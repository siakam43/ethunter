"""Shared helpers for parametric function pointer tracking.

Provides AST scanning utilities and the prepare() entry point
used by param_binding, param_dispatch, and callback_reg modules.
"""

from __future__ import annotations

import re
import tree_sitter as ts

REG_PATTERNS = [
    'register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_',
    'once', 'submit', 'post', 'work', 'spawn', 'scandir', 'sort', 'filter',
    'notify', 'watch', 'dispatch', 'schedule',
]


def _is_registration(name: str) -> bool:
    lower = name.lower()
    return any(p in lower for p in REG_PATTERNS)


def _find_child(node, type_name: str):
    for c in node.children:
        if c.type == type_name:
            return c
    return None


def _find_func_name_from_decl(decl):
    """Extract function name and inner declarator from a function_declarator."""
    ident = _find_child(decl, 'identifier')
    if ident and ident.text:
        return ident.text.decode('utf-8'), decl

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


def _extract_field_operand(field_expr) -> str | None:
    """Extract the base identifier from a field_expression."""
    for child in field_expr.children:
        if child.type == 'field_expression':
            return _extract_field_operand(child)
        if child.type == 'identifier' and child.text:
            return child.text.decode('utf-8')
    return None


def _collect_fnptr_typedefs(tree) -> set[str]:
    """Collect typedef names that are function pointer types from the AST."""
    fnptr_typedefs: set[str] = set()

    def _scan(n) -> None:
        if n.type == 'type_definition':
            for child in n.children:
                if child.type == 'function_declarator':
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
    """Check if a parameter_declaration subtree contains a function_declarator."""
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
    """Collect function-wrapper macros: macro_name -> (real_func_name, [param_names])."""
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


def _classify_param_usage(node, func_fp_params, func_params, param_usage):
    """Classify each fnptr param's usage: 'caller', 'forwarder', or 'storage'.

    Caller: param(args) or (*param)(args) in function body
    Forwarder: other_func(param) in function body (param forwarded as arg)
    Storage: handled by prepare() via param_fields registration
    """
    def _scan(n):
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if not decl:
                for c in n.children:
                    if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                        d = _find_child(c, 'function_declarator')
                        if d:
                            decl = d
                            break
            if decl:
                fname, _ = _find_func_name_from_decl(decl)
                if fname and fname in func_fp_params:
                    fp_positions = func_fp_params[fname]
                    body = _find_child(n, 'compound_statement')
                    if body:
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

                        params = func_params.get(fname, [])
                        for pos in fp_positions:
                            if pos >= len(params):
                                continue
                            pname = params[pos]
                            role = 'unknown'
                            for called_name, arg_names in calls:
                                if called_name == pname or called_name == '*' + pname:
                                    role = 'caller'
                                    break
                                if pname in arg_names:
                                    if role != 'caller':
                                        role = 'forwarder'
                            key = (fname, pos)
                            if key not in param_usage:
                                param_usage[key] = role

        for child in n.children:
            _scan(child)

    _scan(node)


def prepare(tree: ts.Tree, filepath: str, dataflow, symbol_table=None) -> None:
    """Phase 1a: Cross-file pre-scan. Collect function metadata and register
    param->field / return->field mappings. Writes to engine only, no edges.

    Populates: engine.func_params, engine.state.func_fp_params,
               engine.state.param_usage, engine.param_fields, engine.ret_fields
    """
    from ethunter.analyzer.helpers import extract_field_path, collect_field_assignments

    func_params: dict[str, list[str]] = {}
    func_fp_params: dict[str, set[int]] = {}
    fnptr_typedefs = _collect_fnptr_typedefs(tree)
    _collect_func_params(tree.root_node, func_params, func_fp_params, fnptr_typedefs)

    # Store func_params on engine (cross-file accumulation)
    dataflow.func_params.update(func_params)

    # Store func_fp_params on state (migration: still on state to keep hasattr fallback working)
    if not hasattr(dataflow.state, 'func_fp_params'):
        dataflow.state.func_fp_params = {}
    dataflow.state.func_fp_params.update(func_fp_params)

    # Scan for field = param patterns -> register_param_mapping
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

    # Classify fnptr param usage — stored on state during migration
    param_usage: dict[tuple[str, int], str] = {}
    if func_fp_params:
        _classify_param_usage(tree.root_node, func_fp_params, func_params, param_usage)
        if not hasattr(dataflow.state, 'param_usage'):
            dataflow.state.param_usage = {}
        dataflow.state.param_usage.update(param_usage)

    # Collect parameter types (new — Phase B)
    if symbol_table is not None:
        _collect_param_types(tree.root_node, symbol_table)


def _collect_param_types(root_node, symbol_table) -> None:
    """Scan function definitions and record parameter struct types.

    For each function parameter declared as 'struct type_name *ptr',
    record (func_name, param_name) -> 'type_name' in symbol_table.
    """
    def _scan(node):
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
                    _scan(child)
                return

            fname, inner_decl = _find_func_name_from_decl(decl)
            if not fname:
                for child in node.children:
                    _scan(child)
                return

            plist = _find_child(inner_decl, 'parameter_list')
            if plist:
                for p in plist.children:
                    if p.type == 'parameter_declaration':
                        pname = _extract_param_name(p)
                        if not pname:
                            continue
                        for tc in p.children:
                            if tc.type == 'type_identifier' and tc.text:
                                type_name = tc.text.decode('utf-8')
                                symbol_table.record_func_var_type(fname, pname, type_name)
                                break
                            if tc.type == 'struct_specifier':
                                for sc in tc.children:
                                    if sc.type == 'type_identifier' and sc.text:
                                        type_name = sc.text.decode('utf-8')
                                        symbol_table.record_func_var_type(fname, pname, type_name)
                                        break

        for child in node.children:
            _scan(child)

    _scan(root_node)
