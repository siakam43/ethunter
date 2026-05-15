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
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path, collect_field_assignments
import re

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


def _find_func_name_from_decl(decl):
    """Extract function name and inner declarator from a function_declarator.

    Returns (name, inner_decl) or (None, None).
    The inner_decl is the function_declarator containing the actual params.
    """
    ident = _find_child(decl, 'identifier')
    if ident and ident.text:
        return ident.text.decode('utf-8'), decl

    # Recursively search for inner function_declarator through
    # parenthesized/pointer declarator wrappers
    def _search_inner(node):
        if node.type == 'function_declarator':
            inner_ident = _find_child(node, 'identifier')
            if inner_ident and inner_ident.text:
                return inner_ident.text.decode('utf-8'), node
        for c in node.children:
            result = _search_inner(c)
            if result[0]:
                return result
        return None, None

    for c in decl.children:
        if c.type in ('parenthesized_declarator', 'pointer_declarator'):
            name, inner = _search_inner(c)
            if name:
                return name, inner
    return None, None


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


def _collect_fnptr_typedefs(tree) -> set[str]:
    """Collect typedef names that are function pointer types from the AST.

    e.g. 'typedef void (*cb_fn)(int x);' -> adds 'cb_fn'
    AST: type_definition -> function_declarator -> parenthesized_declarator ->
         pointer_declarator -> type_identifier: 'cb_fn'
    """
    fnptr_typedefs: set[str] = set()

    def _scan(n) -> None:
        if n.type == 'type_definition':
            for child in n.children:
                if child.type == 'function_declarator':
                    # Extract type_identifier from nested structure
                    def _extract_name(node) -> str | None:
                        if node.type == 'type_identifier' and node.text:
                            return node.text.decode('utf-8')
                        for c in node.children:
                            result = _extract_name(c)
                            if result:
                                return result
                        return None
                    name = _extract_name(child)
                    if name:
                        fnptr_typedefs.add(name)
                    break
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return fnptr_typedefs


def _has_fnptr_declarator(node, fnptr_typedefs: set[str] | None = None) -> bool:
    """Check if a parameter_declaration subtree contains a function_declarator (fnptr param).

    Also checks for typedef-based fnptr params using fnptr_typedefs set.
    """
    if node.type == 'function_declarator':
        return True
    if fnptr_typedefs is not None:
        for c in node.children:
            if c.type in ('type_identifier', 'primitive_type') and c.text:
                type_name = c.text.decode('utf-8')
                if type_name in fnptr_typedefs:
                    return True
    for c in node.children:
        if _has_fnptr_declarator(c, fnptr_typedefs):
            return True
    return False


def _collect_simple_macros(tree) -> dict[str, tuple[str, list[str]]]:
    """Collect function-wrapper macros: macro_name -> (real_func_name, [param_names]).

    Only matches macros of the form: #define MACRO(a,b) real_func(a,b)
    Skips constant macros, expression macros, and multi-statement macros.
    """
    macros: dict[str, tuple[str, list[str]]] = {}

    def _scan(n) -> None:
        if n.type == 'preproc_function_def':
            name_node = None
            body_text = None
            param_idents = []
            for child in n.children:
                if child.type == 'identifier' and child.text and name_node is None:
                    name_node = child
                elif child.type == 'preproc_params':
                    for pc in child.children:
                        if pc.type == 'identifier' and pc.text:
                            param_idents.append(pc.text.decode('utf-8'))
                elif child.type == 'preproc_arg' and child.text:
                    body_text = child.text.decode('utf-8')
            if name_node and name_node.text and body_text:
                macro_name = name_node.text.decode('utf-8')
                func_match = re.match(r'\s*(\w+)\s*\(', body_text)
                if func_match and func_match.group(1) != macro_name:
                    macros[macro_name] = (func_match.group(1), param_idents)
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros


def _collect_func_params(node, func_params: dict, func_fp_params: dict | None = None,
                        fnptr_typedefs: set[str] | None = None) -> None:
    """Collect function parameter lists and optionally fnptr parameter positions."""
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
            fname, inner_decl = _find_func_name_from_decl(decl)
            if fname:
                params = []
                fp_positions = set()
                plist = _find_child(inner_decl, 'parameter_list')
                if plist:
                    pos = 0
                    for p in plist.children:
                        if p.type == 'parameter_declaration':
                            pname = _extract_param_name(p)
                            if pname:
                                params.append(pname)
                                if func_fp_params is not None and _has_fnptr_declarator(p, fnptr_typedefs):
                                    fp_positions.add(pos)
                                pos += 1
                func_params[fname] = params
                if func_fp_params is not None and fp_positions:
                    func_fp_params[fname] = fp_positions
    for child in node.children:
        _collect_func_params(child, func_params, func_fp_params, fnptr_typedefs)


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
    func_fp_params: dict[str, set[int]] = {}
    fnptr_typedefs = _collect_fnptr_typedefs(tree)
    _collect_func_params(tree.root_node, func_params, func_fp_params, fnptr_typedefs)
    # Store on engine (cross-file accumulation)
    if hasattr(dataflow, 'state'):
        if not hasattr(dataflow.state, 'func_fp_params'):
            dataflow.state.func_fp_params = {}
        dataflow.state.func_fp_params.update(func_fp_params)
    else:
        if not hasattr(dataflow, 'func_fp_params'):
            dataflow.func_fp_params = {}
        dataflow.func_fp_params.update(func_fp_params)

    # Scan for field = param patterns -> register_param_mapping
    if hasattr(dataflow, 'register_param_mapping'):
        for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
            if fa.enclosing_func is None or fa.enclosing_func not in func_params:
                continue
            if fa.resolved_value is None:
                continue
            params = func_params[fa.enclosing_func]
            if fa.resolved_value not in params:
                continue
            param_idx = params.index(fa.resolved_value)
            dataflow.register_param_mapping(
                fa.enclosing_func, param_idx, fa.field_path
            )

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
                fname, inner_decl = _find_func_name_from_decl(decl)
                if not fname:
                    for child in node.children:
                        _scan_returns(child)
                    return
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

    # === Classify fnptr param usage ===
    param_usage: dict[tuple[str, int], str] = {}
    if func_fp_params:
        _classify_param_usage(tree.root_node, func_fp_params, func_params, param_usage)
        # Store on engine (cross-file accumulation)
        if hasattr(dataflow, 'state'):
            if not hasattr(dataflow.state, 'param_usage'):
                dataflow.state.param_usage = {}
            dataflow.state.param_usage.update(param_usage)
        else:
            if not hasattr(dataflow, 'param_usage'):
                dataflow.param_usage = {}
            dataflow.param_usage.update(param_usage)


def _classify_param_usage(node, func_fp_params, func_params, param_usage):
    """Classify each fnptr param's usage: 'caller', 'forwarder', or 'storage'.

    Caller: param(args) or (*param)(args) in function body
    Forwarder: other_func(param) in function body (param forwarded as arg)
    Storage: field = param or already in param_fields (handled by _register_phase)
    """
    def _scan(n):
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if not decl:
                for c in n.children:
                    if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                        d = _find_child(c, 'function_declarator')
                        if d: decl = d; break
            if decl:
                fname, _ = _find_func_name_from_decl(decl)
                if fname and fname in func_fp_params:
                    fp_positions = func_fp_params[fname]
                    body = _find_child(n, 'compound_statement')
                    if body:
                        # Find all call expressions in body
                        def _scan_calls(cn, results):
                            if cn.type == 'call_expression':
                                func_node = cn.child_by_field_name('function') or cn.children[0]
                                called_name = None
                                if func_node and func_node.type == 'identifier' and func_node.text:
                                    called_name = func_node.text.decode('utf-8')
                                elif func_node and func_node.type == 'parenthesized_expression':
                                    for cc in func_node.children:
                                        if cc.type == 'pointer_expression' and cc.children:
                                            inner = cc.children[-1]
                                            if inner.type == 'identifier' and inner.text:
                                                called_name = '*' + inner.text.decode('utf-8')
                                elif func_node and func_node.type == 'pointer_expression' and func_node.children:
                                    inner = func_node.children[-1]
                                    if inner.type == 'identifier' and inner.text:
                                        called_name = '*' + inner.text.decode('utf-8')

                                args = cn.child_by_field_name('arguments')
                                arg_names = []
                                if args:
                                    for cc in args.children:
                                        if cc.type == 'identifier' and cc.text:
                                            arg_names.append(cc.text.decode('utf-8'))

                                results.append((called_name, arg_names))
                            for child in cn.children:
                                _scan_calls(child, results)

                        calls = []
                        _scan_calls(body, calls)

                        # Classify each fnptr param position
                        params = func_params.get(fname, [])
                        for pos in fp_positions:
                            if pos >= len(params):
                                continue
                            pname = params[pos]
                            role = 'unknown'
                            for called_name, arg_names in calls:
                                # Check if fnptr is directly called: cb(args) or (*cb)(args)
                                if called_name == pname or called_name == '*' + pname:
                                    role = 'caller'
                                    break
                                # Check if fnptr is forwarded: other_func(cb)
                                if pname in arg_names:
                                    if role != 'caller':
                                        role = 'forwarder'
                            key = (fname, pos)
                            if key not in param_usage:
                                param_usage[key] = role

        for child in n.children:
            _scan(child)

    _scan(node)


def register_phase(tree: ts.Tree, filepath: str, symbol_table: SymbolTable,
                   dataflow) -> None:
    """Phase 1a: pre-scan for param→field registrations. No edges."""
    _register_phase(tree, filepath, symbol_table, dataflow)


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
    func_fp_params: dict[str, set[int]] = {}  # func_name -> {fnptr_param_positions}

    fnptr_typedefs = _collect_fnptr_typedefs(tree)
    _collect_func_params(tree.root_node, func_params, func_fp_params, fnptr_typedefs)

    # Collect function-wrapper macros for call-site expansion
    macros = _collect_simple_macros(tree)

    # Store func_fp_params in dataflow for field_call callback-of-callback handling
    if hasattr(dataflow, 'state'):
        if not hasattr(dataflow.state, 'func_fp_params'):
            dataflow.state.func_fp_params = {}
        dataflow.state.func_fp_params.update(func_fp_params)
    else:
        if not hasattr(dataflow, 'func_fp_params'):
            dataflow.func_fp_params = {}
        dataflow.func_fp_params.update(func_fp_params)

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
                fname, inner_decl = _find_func_name_from_decl(decl)
                if not fname:
                    for child in node.children:
                        _collect_returns(child)
                    return
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
    call_site_targets: dict[tuple[str, str, int], set[str]] = {}  # (caller, callee, arg_idx) -> {target_func, ...}

    def _collect_call_params(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)

                    # Macro expansion: replace macro call with real function name
                    if call_name not in func_params and call_name in macros:
                        real_func, _ = macros[call_name]
                        if real_func in func_params:
                            call_name = real_func

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
                                    fp_params = getattr(dataflow, 'func_fp_params', None)
                                    if fp_params is None and hasattr(dataflow, 'state'):
                                        fp_params = getattr(dataflow.state, 'func_fp_params', None)
                                    fp_positions = fp_params.get(call_name, set()) if fp_params else set()
                                    if not fp_positions or arg_idx in fp_positions:
                                        # Fix 3: check param_usage — suppress for forwarder/storage
                                        usage = None
                                        pu = getattr(dataflow, 'param_usage', None)
                                        if pu is None and hasattr(dataflow, 'state'):
                                            pu = getattr(dataflow.state, 'param_usage', None)
                                        if pu:
                                            usage = pu.get((call_name, arg_idx), 'unknown')
                                        if usage not in ('forwarder', 'storage'):
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
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        # Per-call-site tracking (P0)
                                        cs_key = (caller or '<unknown>', call_name, arg_idx)
                                        if cs_key not in call_site_targets:
                                            call_site_targets[cs_key] = set()
                                        call_site_targets[cs_key].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
                            else:
                                # Fallback: check dataflow for local var assigned to fnptr
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
                                if df_targets and arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].update(df_targets)
                                    # Also track per-call-site (P0)
                                    cs_key = (caller or '<unknown>', call_name, arg_idx)
                                    if cs_key not in call_site_targets:
                                        call_site_targets[cs_key] = set()
                                    call_site_targets[cs_key].update(df_targets)
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
                                    fp_params = getattr(dataflow, 'func_fp_params', None)
                                    if fp_params is None and hasattr(dataflow, 'state'):
                                        fp_params = getattr(dataflow.state, 'func_fp_params', None)
                                    fp_positions = fp_params.get(call_name, set()) if fp_params else set()
                                    if not fp_positions or arg_idx in fp_positions:
                                        usage = None
                                        pu = getattr(dataflow, 'param_usage', None)
                                        if pu is None and hasattr(dataflow, 'state'):
                                            pu = getattr(dataflow.state, 'param_usage', None)
                                        if pu:
                                            usage = pu.get((call_name, arg_idx), 'unknown')
                                        if usage not in ('forwarder', 'storage'):
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
                        elif c.type == 'pointer_expression' and c.children:
                            # Extract &func from pointer_expression
                            inner = c.children[-1]
                            if inner.type == 'identifier' and inner.text:
                                target = inner.text.decode('utf-8')
                                if target in symbol_names:
                                    arg_idx = comma_count
                                    if _is_registration(call_name):
                                        fp_params = getattr(dataflow, 'func_fp_params', None)
                                        if fp_params is None and hasattr(dataflow, 'state'):
                                            fp_params = getattr(dataflow.state, 'func_fp_params', None)
                                        fp_positions = fp_params.get(call_name, set()) if fp_params else set()
                                        if not fp_positions or arg_idx in fp_positions:
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
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        # Per-call-site tracking (P0)
                                        cs_key = (caller or '<unknown>', call_name, arg_idx)
                                        if cs_key not in call_site_targets:
                                            call_site_targets[cs_key] = set()
                                        call_site_targets[cs_key].add(target)
                                    _propagate_call_site(
                                        call_name, arg_idx, target,
                                        dataflow, symbol_names
                                    )
        for child in node.children:
            _collect_call_params(child)

    _collect_call_params(tree.root_node)

    # === Pass 2: resolve struct member assignments ===
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.enclosing_func is None:
            continue
        field_path = fa.field_path
        field_name = field_path.split('.')[-1]

        if fa.value_node.type == 'call_expression':
            # === Case B: RHS is call_expression (return value tracking) ===
            call_func = fa.value_node.child_by_field_name('function') or fa.value_node.children[0]
            if call_func and call_func.type == 'identifier' and call_func.text:
                func_name = call_func.text.decode('utf-8')
                if hasattr(dataflow, 'resolve_returned_field'):
                    ret_targets = dataflow.resolve_returned_field(func_name)
                    for t in ret_targets:
                        dataflow.assign(f'<gstruct:{field_path}>', t)
                        if hasattr(dataflow, 'store'):
                            dataflow.store.assign_struct_field(f'gstruct:{field_path}', t, filepath)
        elif fa.resolved_value is not None:
            # === Case A: RHS is identifier or cast_expression ===
            param_name = fa.resolved_value
            # Prong 1: resolve via param_mappings (call-site arg propagation)
            targets = param_mappings.get(param_name, set())
            for t in targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                if hasattr(dataflow, 'store'):
                    dataflow.store.assign_struct_field(f'gstruct:{field_path}', t, filepath)
            # Prong 2: resolve via dataflow
            df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
            if not df_targets:
                df_targets = dataflow.resolve(param_name)
            if not df_targets:
                df_targets = dataflow.resolve(f'<garray:{param_name}>')
            for t in df_targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                dataflow.assign(f'<struct:{field_name}>', t)
                if hasattr(dataflow, 'store'):
                    dataflow.store.assign_struct_field(f'gstruct:{field_path}', t, filepath)
            # Prong 3: register for cross-function propagation
            if hasattr(dataflow, 'register_param_mapping') and fa.enclosing_func in func_params:
                params = func_params[fa.enclosing_func]
                if param_name in params:
                    param_idx = params.index(param_name)
                    dataflow.register_param_mapping(
                        fa.enclosing_func, param_idx, field_path
                    )

    # === Pass 3: detect calls through function pointer parameters ===
    call_site_edges: list[tuple[str, str, str, int]] = []  # (caller, target, filepath, line)

    def _detect_param_calls(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            call_target_name = None
            if func_node and func_node.type == 'identifier' and func_node.text:
                call_target_name = func_node.text.decode('utf-8')
            elif func_node and func_node.type == 'parenthesized_expression':
                for c in func_node.children:
                    if c.type == 'pointer_expression' and c.children:
                        inner = c.children[-1]
                        if inner.type == 'identifier' and inner.text:
                            call_target_name = inner.text.decode('utf-8')
                            break
            elif func_node and func_node.type == 'pointer_expression' and func_node.children:
                inner = func_node.children[-1]
                if inner.type == 'identifier' and inner.text:
                    call_target_name = inner.text.decode('utf-8')

            if call_target_name:
                enclosing_func = find_enclosing_function(node, tree.root_node)
                targets = set()

                # Per-call-site resolution: query call_site_targets by callee + arg_idx
                if enclosing_func and enclosing_func in func_params:
                    params = func_params[enclosing_func]
                    if call_target_name in params:
                        arg_idx = params.index(call_target_name)
                        for (clr, cn, ai), tgs in call_site_targets.items():
                            if cn == enclosing_func and ai == arg_idx:
                                targets.update(tgs)

                # Always merge param_mappings (per-call-site may have partial results)
                pm_targets = param_mappings.get(call_target_name, set())
                if pm_targets:
                    targets = targets | pm_targets

                if targets:
                    for target in targets:
                        call_site_edges.append(
                            (enclosing_func or '<unknown>', target, filepath,
                             node.start_point[0] + 1))

        for child in node.children:
            _detect_param_calls(child)

    _detect_param_calls(tree.root_node)

    # Emit Pass 3 edges: calls through parameters inside callee body
    for (caller, target, fp, line) in call_site_edges:
        edges.append(CallEdge(
            caller=caller,
            callee=target,
            caller_file=fp,
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_param',
            caller_line=line,
        ))

    # === Pass 4: emit edges from call-site to actual targets ===
    # Uses per-call-site resolution via call_site_targets (no NxM merge)
    seen_pass4: set[tuple[str, str]] = set()
    for (caller, callee, arg_idx), targets in call_site_targets.items():
        for target in targets:
            key = (caller, target)
            if key not in seen_pass4:
                seen_pass4.add(key)
                edges.append(CallEdge(
                    caller=caller,
                    callee=target,
                    caller_file=filepath,
                    callee_file='',
                    type=CallType.INDIRECT,
                    indirect_kind='callback_param',
                    caller_line=0,
                ))

    return edges
