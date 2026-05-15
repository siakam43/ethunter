"""Field-expression-based function pointer call detection.

Detects calls through struct field access:
- obj.field()
- ptr->field()
- ptr->chain->field()  (chain access)

Also tracks field assignments (obj.field = func) for dataflow lookup.
Handles macro-expanded field calls: #define MACRO(...) obj.field(...)
"""

from __future__ import annotations

import re

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import (
    find_enclosing_function, extract_field_path, collect_field_assignments,
    collect_pointer_resolutions,
)
from ethunter.analyzer.local_fp_tracker import collect_local_fp_assignments



def _collect_macros(tree: ts.Tree) -> dict[str, str]:
    """Collect preproc_def/preproc_function_def macros and their bodies."""
    macros: dict[str, str] = {}

    def _scan(n: ts.Node) -> None:
        if n.type in ('preproc_def', 'preproc_function_def'):
            name_node = None
            body_text = None
            for child in n.children:
                if child.type == 'identifier' and child.text:
                    name_node = child
                elif child.type == 'preproc_arg' and child.text:
                    body_text = child.text.decode('utf-8')
            if name_node and name_node.text and body_text:
                macros[name_node.text.decode('utf-8')] = body_text
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros


def _extract_field_path_from_macro_body(body: str) -> str | None:
    """Extract struct_var.field pattern from macro body text."""
    match = re.search(r'(\w+)\s*(?:\.|->)\s*(\w+)', body)
    if match:
        return f'{match.group(1)}.{match.group(2)}'
    return None


def collect(tree: ts.Tree, filepath: str, dataflow, symbol_table,
            symbol_names: set[str]) -> None:
    """Phase 1a*: collect field assignments, write struct_fields entries.

    Runs across ALL files before Phase 2 so cross-file assignments are visible.
    """
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None:
            # Old store: only for known function names

            # New store: ALL resolved values (functions + struct vars)
            if hasattr(dataflow, 'store'):
                base_var = fa.field_path.split('.')[0]
                field_tail = dataflow.store.compute_field_tail(fa.field_path)
                dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}',
                                                   fa.resolved_value, filepath)
                struct_type = symbol_table.get_func_var_type(fa.enclosing_func, base_var)
                if struct_type:
                    dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}',
                                                       fa.resolved_value, filepath)
    _collect_local_var_types(tree, symbol_table)
    _collect_cast_types(tree, symbol_table)


def _collect_local_var_types(tree, symbol_table):
    """Scan function bodies for local struct pointer declarations.

    struct my_type *ptr;  →  (func, "ptr") → "my_type"
    my_type *ptr;         →  (func, "ptr") → "my_type" (via typedef)
    """
    def _extract_func_name(node):
        decl = None
        for c in node.children:
            if c.type == 'function_declarator':
                decl = c; break
            if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                for cc in c.children:
                    if cc.type == 'function_declarator':
                        decl = cc; break
        if decl:
            for c in decl.children:
                if c.type == 'identifier' and c.text:
                    return c.text.decode('utf-8')
        return None

    def _scan(node, current_func):
        if node.type == 'function_definition':
            fname = _extract_func_name(node)
            if fname:
                current_func = fname
        if node.type == 'declaration' and current_func:
            type_name = None
            var_name = None
            for c in node.children:
                if c.type == 'type_identifier' and c.text:
                    type_name = c.text.decode('utf-8')
                elif c.type == 'struct_specifier':
                    for sc in c.children:
                        if sc.type == 'type_identifier' and sc.text:
                            type_name = sc.text.decode('utf-8'); break
                elif c.type == 'pointer_declarator':
                    for pc in c.children:
                        if pc.type == 'identifier' and pc.text:
                            var_name = pc.text.decode('utf-8'); break
                elif c.type == 'init_declarator':
                    inner_decl = c.child_by_field_name('declarator')
                    if inner_decl:
                        from ethunter.analyzer.helpers import extract_identifier_from_declarator
                        var_name = extract_identifier_from_declarator(inner_decl)
                elif c.type in ('field_identifier', 'identifier') and c.text:
                    var_name = c.text.decode('utf-8')
            if type_name and var_name:
                symbol_table.record_func_var_type(current_func, var_name, type_name)
        for child in node.children:
            _scan(child, current_func)

    _scan(tree.root_node, None)


def _collect_cast_types(tree, symbol_table):
    """Scan for cast expressions that reveal struct pointer types."""
    def _extract_current_func(node):
        if node.type == 'function_definition':
            for c in node.children:
                if c.type == 'function_declarator':
                    for cc in c.children:
                        if cc.type == 'identifier' and cc.text:
                            return cc.text.decode('utf-8')
                if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                    for cc in c.children:
                        if cc.type == 'function_declarator':
                            for ccc in cc.children:
                                if ccc.type == 'identifier' and ccc.text:
                                    return ccc.text.decode('utf-8')
        return None

    def _extract_cast_struct_type(cast_node):
        """Extract struct type name from cast_expression's type_descriptor.

        Parses 'struct ctx*' → 'ctx', 'my_type*' → 'my_type'.
        """
        for c in cast_node.children:
            if c.type == 'type_descriptor':
                text = c.text.decode('utf-8') if c.text else ''
                # Strip '*', 'const', whitespace to get base type
                import re
                m = re.search(r'struct\s+(\w+)', text)
                if m:
                    return m.group(1)
                m = re.search(r'(\w+)\s*\*', text)
                if m and m.group(1) not in ('const', 'volatile'):
                    return m.group(1)
            if c.type == 'struct_specifier':
                for sc in c.children:
                    if sc.type == 'type_identifier' and sc.text:
                        return sc.text.decode('utf-8')
        return None

    def _scan(node, current_func):
        if node.type == 'function_definition':
            fname = _extract_current_func(node)
            if fname:
                current_func = fname
        if node.type == 'field_expression' and current_func:
            base = node.children[0] if node.children else None
            if base and base.type == 'parenthesized_expression':
                inner = base.children[1] if len(base.children) > 1 else None
                if inner and inner.type == 'cast_expression':
                    type_name = _extract_cast_struct_type(inner)
                    # identifier is a direct child of cast_expression (index 3)
                    for cc in inner.children:
                        if cc.type == 'identifier' and cc.text:
                            var_name = cc.text.decode('utf-8')
                            if type_name and var_name:
                                symbol_table.record_func_var_type(current_func, var_name, type_name)
                            break
        for child in node.children:
            _scan(child, current_func)

    _scan(tree.root_node, None)


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through struct field expressions."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names
    macro_map = _collect_macros(tree)

    def _extract_field_expression(node: ts.Node | None) -> ts.Node | None:
        if not node:
            return None
        if node.type == 'field_expression':
            return node
        if node.type == 'parenthesized_expression':
            for c in node.children:
                if c.type == 'pointer_expression':
                    for cc in c.children:
                        if cc.type == 'field_expression':
                            return cc
        return node if node.type == 'field_expression' else None

    # Pass 1: collect field assignments (still needed for macro-expanded calls)
    # Main collection moved to collect() but keep here for direct-test compat
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            if hasattr(dataflow, 'store'):
                base_var = fa.field_path.split('.')[0]
                field_tail = dataflow.store.compute_field_tail(fa.field_path)
                dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}',
                                                   fa.resolved_value, filepath)
                struct_type = symbol_table.get_func_var_type(fa.enclosing_func, base_var)
                if struct_type:
                    dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}',
                                                       fa.resolved_value, filepath)

    pointer_resolutions = collect_pointer_resolutions(tree)
    local_fp_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names, symbol_table)

    # Pass 2: detect call sites
    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            field_expr = _extract_field_expression(func_node)
            if field_expr:
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(field_expr)
                if field_path:
                    base_var = field_path.split('.')[0]
                    targets, confidence, evidence = dataflow.resolve_struct_field_call(
                        field_path, base_var, caller, filepath,
                        symbol_table=symbol_table,
                        local_fp_mapping=local_fp_mapping,
                        pointer_resolutions=pointer_resolutions,
                    )
                    if confidence is None:
                        confidence = Confidence.MEDIUM
                        evidence = Evidence('field_call_resolution')

                    # Callback-of-callback
                    func_fp_params = getattr(dataflow.state, 'func_fp_params', None) if hasattr(dataflow, 'state') else None
                    if func_fp_params:
                        args = node.child_by_field_name('arguments')
                        if args:
                            comma_count = 0
                            arg_values = []
                            for c in args.children:
                                if c.type == ',':
                                    comma_count += 1
                                elif c.type not in ('(', ')'):
                                    arg_values.append((comma_count, c))
                            for ftarget in targets:
                                fp_positions = func_fp_params.get(ftarget, set())
                                for pos, arg_node in arg_values:
                                    if pos in fp_positions:
                                        actual = None
                                        if arg_node.type == 'identifier' and arg_node.text:
                                            actual = arg_node.text.decode('utf-8')
                                        elif arg_node.type == 'pointer_expression' and arg_node.children:
                                            inner = arg_node.children[-1]
                                            if inner.type == 'identifier' and inner.text:
                                                actual = inner.text.decode('utf-8')
                                        if actual and actual in symbol_names:
                                            edges.append(CallEdge(
                                                caller=ftarget,
                                                callee=actual,
                                                caller_file=filepath,
                                                callee_file='',
                                                type=CallType.INDIRECT,
                                                indirect_kind='callback_param',
                                                caller_line=node.start_point[0] + 1,
                                                confidence=Confidence.MEDIUM,
                                                evidence=Evidence('callback_of_callback'),
                                            ))

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='field_call',
                            caller_line=node.start_point[0] + 1,
                            confidence=confidence,
                            evidence=evidence,
                        ))
            elif func_node.type == 'identifier' and func_node.text:
                call_name = func_node.text.decode('utf-8')
                if call_name in macro_map:
                    body = macro_map[call_name]
                    resolved_path = _extract_field_path_from_macro_body(body)
                    if resolved_path:
                        targets = dataflow.resolve(f'<gstruct:{resolved_path}>')
                        if targets:
                            caller = find_enclosing_function(node, tree.root_node)
                            for target in targets:
                                edges.append(CallEdge(
                                    caller=caller or '<unknown>',
                                    callee=target,
                                    caller_file=filepath,
                                    callee_file='',
                                    type=CallType.INDIRECT,
                                    indirect_kind='field_call',
                                    caller_line=node.start_point[0] + 1,
                                    confidence=Confidence.MEDIUM,
                                    evidence=Evidence('macro_expansion'),
                                ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
