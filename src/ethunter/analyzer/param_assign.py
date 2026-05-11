"""Parameter-based function pointer tracking.

Handles function pointer parameter passing:
- void fn(void (*cb)(void)) + fn(callback_func) → track cb → callback_func
- Callback registration functions (register/hook/attach/subscribe)
- Parameter stored in struct field: handler.cb = param → resolve param to actual function
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path

REG_PATTERNS = [
    'register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_',
    'once', 'submit', 'post', 'work', 'spawn', 'scandir', 'sort', 'filter',
    'notify', 'watch', 'dispatch', 'schedule',
]


def _is_registration(name: str) -> bool:
    lower = name.lower()
    return any(p in lower for p in REG_PATTERNS)


def _find_child(node, type_name: str):
    """Find a direct child of the given type."""
    for c in node.children:
        if c.type == type_name:
            return c
    return None


def _extract_field_operand(field_expr) -> str | None:
    """Extract the base identifier from a field_expression.

    e.g., 'ctx->ext.alpn_select_cb' -> 'ctx'
    """
    for child in field_expr.children:
        if child.type == 'field_expression':
            return _extract_field_operand(child)
        if child.type == 'identifier' and child.text:
            return child.text.decode('utf-8')
    return None


def _try_register_param_to_field(
    lhs, rhs, param_name: str, field_path: str,
    enclosing_func: str | None, func_params: dict,
    dataflow,
) -> None:
    """Register param->field mapping if RHS is a function parameter.

    Called from _visit when we detect: field_expression = identifier(param_name).
    """
    if not enclosing_func or enclosing_func not in func_params:
        return
    params = func_params[enclosing_func]
    if param_name not in params:
        return
    param_idx = params.index(param_name)
    lhs_operand = _extract_field_operand(lhs)
    if not lhs_operand or lhs_operand not in params:
        return
    struct_param_idx = params.index(lhs_operand)
    if hasattr(dataflow, 'register_param_mapping'):
        dataflow.register_param_mapping(
            enclosing_func, param_idx, field_path, struct_param_idx
        )


def _collect_func_params(node, func_params: dict) -> None:
    """Collect function parameter lists from function definitions."""
    if node.type == 'function_definition':
        decl = _find_child(node, 'function_declarator')
        if not decl:
            for c in node.children:
                if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                    d = _find_child(c, 'function_declarator')
                    if d:
                        decl = d
                        break
        if decl:
            func_name_node = _find_child(decl, 'identifier')
            if func_name_node and func_name_node.text:
                fname = func_name_node.text.decode('utf-8')
                params = []
                plist = _find_child(decl, 'parameter_list')
                if plist:
                    for p in plist.children:
                        if p.type == 'parameter_declaration':
                            pname = _extract_param_name(p)
                            if pname:
                                params.append(pname)
                func_params[fname] = params
    for child in node.children:
        _collect_func_params(child, func_params)


def _extract_param_name(param_decl) -> str | None:
    """Extract parameter name from parameter_declaration, recursively."""
    def _search(node, depth: int = 0) -> str | None:
        if node.type == 'identifier' and node.text and depth < 10:
            return node.text.decode('utf-8')
        for c in node.children:
            if c.type in ('parenthesized_declarator', 'pointer_declarator',
                          'array_declarator', 'function_declarator'):
                result = _search(c, depth + 1)
                if result:
                    return result
            if c.type == 'identifier' and c.text:
                return c.text.decode('utf-8')
        return None
    return _search(param_decl)


def _register_phase(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow,
) -> None:
    """Phase 1a: pre-scan for param->field registrations only.

    This populates engine.param_fields and engine.ret_fields across ALL files
    BEFORE any call-site propagation runs. No edges are emitted, no dataflow writes.

    Called from orchestrator.run_all_analyses() before Phase 1.
    """
    if not hasattr(dataflow, 'register_param_mapping') and not hasattr(dataflow, 'register_return'):
        return  # VariableState passed — nothing to register

    func_params: dict[str, list[str]] = {}
    _collect_func_params(tree.root_node, func_params)

    # Scan for field = param patterns -> register_param_mapping
    if hasattr(dataflow, 'register_param_mapping'):
        def _scan_field_assigns(node: ts.Node) -> None:
            if node.type == 'assignment_expression':
                lhs = node.child_by_field_name('left') or node.children[0]
                rhs = node.child_by_field_name('right') or node.children[1]
                if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                    param_name = rhs.text.decode('utf-8')
                    field_path = extract_field_path(lhs)
                    if field_path:
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        _try_register_param_to_field(
                            lhs, rhs, param_name, field_path,
                            enclosing_func, func_params, dataflow
                        )
            for child in node.children:
                _scan_field_assigns(child)

        _scan_field_assigns(tree.root_node)

    # Scan for return field_expression -> register_return
    if hasattr(dataflow, 'register_return'):
        def _scan_returns(node: ts.Node) -> None:
            if node.type == 'function_definition':
                decl = _find_child(node, 'function_declarator')
                if not decl:
                    for c in node.children:
                        if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                            d = _find_child(c, 'function_declarator')
                            if d:
                                decl = d
                                break
                if not decl:
                    for child in node.children:
                        _scan_returns(child)
                    return
                fname_node = _find_child(decl, 'identifier')
                if not fname_node or not fname_node.text:
                    for child in node.children:
                        _scan_returns(child)
                    return
                fname = fname_node.text.decode('utf-8')
                params = func_params.get(fname, [])
                body = _find_child(node, 'compound_statement')
                if body:
                    def _scan_body(n: ts.Node) -> None:
                        if n.type == 'return_statement':
                            for c in n.children:
                                if c.type == 'field_expression':
                                    fp = extract_field_path(c)
                                    if fp:
                                        operand = _extract_field_operand(c)
                                        if operand and operand in params:
                                            dataflow.register_return(fname, fp)
                        for child in n.children:
                            _scan_body(child)
                    _scan_body(body)
            for child in node.children:
                _scan_returns(child)

        _scan_returns(tree.root_node)


def _propagate_call_site(
    call_name: str, arg_idx: int, target: str,
    dataflow, symbol_names: set[str],
) -> None:
    """Propagate a call-site argument target to registered field paths.

    Uses DataflowEngine.resolve_call_site_param if available (hasattr guard).
    """
    if hasattr(dataflow, 'resolve_call_site_param'):
        dataflow.resolve_call_site_param(
            call_name, arg_idx, target, symbol_names=symbol_names
        )


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Track function pointer parameters and their propagation."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    func_params: dict[str, list[str]] = {}  # func_name -> [param_names]

    _collect_func_params(tree.root_node, func_params)

    # === Collect return value tracking ===
    if hasattr(dataflow, 'register_return'):
        def _collect_returns(node) -> None:
            if node.type == 'function_definition':
                decl = _find_child(node, 'function_declarator')
                if not decl:
                    for c in node.children:
                        if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                            d = _find_child(c, 'function_declarator')
                            if d:
                                decl = d
                                break
                if not decl:
                    for child in node.children:
                        _collect_returns(child)
                    return
                fname_node = _find_child(decl, 'identifier')
                if not fname_node or not fname_node.text:
                    for child in node.children:
                        _collect_returns(child)
                    return
                fname = fname_node.text.decode('utf-8')
                params = func_params.get(fname, [])

                body = _find_child(node, 'compound_statement')
                if body:
                    def _scan_returns(n) -> None:
                        if n.type == 'return_statement':
                            for c in n.children:
                                if c.type == 'field_expression':
                                    field_path = extract_field_path(c)
                                    if field_path:
                                        operand = _extract_field_operand(c)
                                        if operand and operand in params:
                                            dataflow.register_return(fname, field_path)
                        for child in n.children:
                            _scan_returns(child)
                    _scan_returns(body)
            for child in node.children:
                _collect_returns(child)

        _collect_returns(tree.root_node)

    # === Pass 1: collect param mappings from call sites ===
    param_mappings: dict[str, set[str]] = {}  # param_name -> {target_func, ...}

    def _collect_call_params(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)
                    param_names = func_params.get(call_name, [])
                    # Count commas to determine argument position
                    # args children: (, arg0, comma, arg1, comma, ..., )
                    comma_count = 0
                    for c in args.children:
                        if c.type == ',':
                            comma_count += 1
                        elif c.type == 'identifier' and c.text:
                            arg_idx = comma_count
                            target = c.text.decode('utf-8')
                            if target in symbol_names:
                                if _is_registration(call_name):
                                    dataflow.register_callback(target)
                                    edges.append(CallEdge(
                                        caller=caller or '<registration>',
                                        callee=target,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='callback_reg',
                                        caller_line=node.start_point[0] + 1,
                                    ))
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
                        elif c.type == 'cast_expression':
                            # Extract identifier from nested cast
                            extracted = None
                            if hasattr(dataflow, 'unwrap_cast'):
                                extracted = dataflow.unwrap_cast(c)
                            if not extracted:
                                for cc in reversed(c.children):
                                    if cc.type == 'identifier' and cc.text:
                                        extracted = cc.text.decode('utf-8')
                                        break
                            if extracted and extracted in symbol_names:
                                arg_idx = comma_count
                                target = extracted
                                if _is_registration(call_name):
                                    dataflow.register_callback(target)
                                    edges.append(CallEdge(
                                        caller=caller or '<registration>',
                                        callee=target,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='callback_reg',
                                        caller_line=node.start_point[0] + 1,
                                    ))
                                elif arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
        for child in node.children:
            _collect_call_params(child)

    _collect_call_params(tree.root_node)

    # === Pass 2: resolve struct member assignments ===
    def _visit(node: ts.Node) -> None:
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression':
                field_path = extract_field_path(lhs)
                if field_path:
                    # === Case A: RHS is identifier (existing + registration) ===
                    if rhs.type == 'identifier' and rhs.text:
                        param_name = rhs.text.decode('utf-8')
                        # EXISTING: resolve param to actual functions
                        targets = param_mappings.get(param_name, set())
                        for t in targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                        df_targets = dataflow.resolve(param_name)
                        if not df_targets:
                            df_targets = dataflow.resolve(f'<garray:{param_name}>')
                        for t in df_targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                            field_name = field_path.split('.')[-1]
                            dataflow.assign(f'<struct:{field_name}>', t)
                        # NEW: register for cross-function propagation
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        _try_register_param_to_field(
                            lhs, rhs, param_name, field_path,
                            enclosing_func, func_params, dataflow
                        )
                    # === Case B: RHS is call_expression (return value tracking) ===
                    elif rhs.type == 'call_expression':
                        call_func = rhs.child_by_field_name('function') or rhs.children[0]
                        if call_func and call_func.type == 'identifier' and call_func.text:
                            func_name = call_func.text.decode('utf-8')
                            if hasattr(dataflow, 'resolve_returned_field'):
                                ret_targets = dataflow.resolve_returned_field(func_name)
                                for t in ret_targets:
                                    dataflow.assign(f'<gstruct:{field_path}>', t)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)

    # === Pass 3: detect calls through function pointer parameters ===
    # When a call-site passes a function as arg N, and the callee calls that param,
    # emit edges from the call-site's enclosing function to the actual target.
    call_site_edges: list[tuple[str, str, str, int]] = []  # (caller, callee, filepath, line)

    def _detect_param_calls(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
                fname = func_node.text.decode('utf-8')
                targets = param_mappings.get(fname)
                if targets:
                    caller = find_enclosing_function(node, tree.root_node)
                    for target in targets:
                        call_site_edges.append((caller or '<unknown>', target, filepath, node.start_point[0] + 1))
        for child in node.children:
            _detect_param_calls(child)

    _detect_param_calls(tree.root_node)

    # === Pass 4: emit edges from call-site to actual targets ===
    call_targets: dict[tuple[str, str], tuple[str, int]] = {}  # (outer_caller, target) -> (filepath, line)

    def _collect_call_args_pass4(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)
                    param_names = func_params.get(call_name, [])
                    # Build arg list with proper positions
                    arg_idx = 0
                    for c in args.children:
                        if c.type == '(':
                            continue
                        if c.type == ')':
                            break
                        if c.type == ',':
                            continue
                        if c.type == 'identifier' and c.text:
                            target = c.text.decode('utf-8')
                            if target in symbol_names and arg_idx < len(param_names):
                                pname = param_names[arg_idx]
                                targets = param_mappings.get(pname)
                                if targets:
                                    for t in targets:
                                        key = (caller or '<unknown>', t)
                                        if key not in call_targets:
                                            call_targets[key] = (filepath, node.start_point[0] + 1)
                        arg_idx += 1
        for child in node.children:
            _collect_call_args_pass4(child)

    _collect_call_args_pass4(tree.root_node)

    for (caller, target), (fp, line) in call_targets.items():
        edges.append(CallEdge(
            caller=caller,
            callee=target,
            caller_file=fp,
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_param',
            caller_line=line,
        ))

    return edges
