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


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Track function pointer parameters and their propagation."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    # Collect function definitions with their parameter lists
    func_params: dict[str, list[str]] = {}  # func_name -> [param_names]

    def _extract_param_name(param_decl: ts.Node) -> str | None:
        """Extract parameter name from parameter_declaration, recursively."""
        def _search(node: ts.Node, depth: int = 0) -> str | None:
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

    def _collect_func_params(node: ts.Node) -> None:
        if node.type == 'function_definition':
            # Find function_declarator (may be nested)
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
            _collect_func_params(child)

    def _find_child(node: ts.Node, type_name: str) -> ts.Node | None:
        for c in node.children:
            if c.type == type_name:
                return c
        return None

    def _count_param_position(args: ts.Node, target_index: int) -> int:
        """Find the 0-based param position of an identifier at the given child index in arguments."""
        # args children: (, arg0, comma, arg1, comma, ..., )
        # Count commas before the target child index
        pos = 0
        for c in args.children:
            if c == target_index:
                return pos
            if c.type == ',':
                pos += 1
            elif c.type != '(':
                pos += 1
        return -1

    _collect_func_params(tree.root_node)

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
        for child in node.children:
            _collect_call_params(child)

    _collect_call_params(tree.root_node)

    # === Pass 2: resolve struct member assignments ===
    def _visit(node: ts.Node) -> None:
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and rhs.type == 'identifier' and rhs.text:
                param_name = rhs.text.decode('utf-8')
                if lhs.type == 'field_expression':
                    field_path = extract_field_path(lhs)
                    if field_path:
                        # Resolve param to actual functions
                        targets = param_mappings.get(param_name, set())
                        for t in targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                        # Also check dataflow (direct_assign may have populated it)
                        df_targets = dataflow.resolve(param_name)
                        # Fallback: check <garray:name> for array-initialized globals
                        if not df_targets:
                            df_targets = dataflow.resolve(f'<garray:{param_name}>')
                        for t in df_targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                            # Also store as <struct:last_component> for alias variables
                            field_name = field_path.split('.')[-1]
                            dataflow.assign(f'<struct:{field_name}>', t)
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
