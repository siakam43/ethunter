"""Shared helper functions used across analyzer modules."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState


def find_enclosing_function(node: ts.Node, root: ts.Node) -> str | None:
    """Find the function name that contains the given AST node."""
    result = [None]

    def _search(n: ts.Node, target_line: int) -> None:
        if result[0] is not None:
            return
        if n.type == 'function_definition':
            decl = find_child(n, 'function_declarator')
            if not decl:
                pd = find_child(n, 'pointer_declarator')
                if pd:
                    decl = find_child(pd, 'function_declarator')
            if decl:
                ident = find_child(decl, 'identifier')
                if ident and ident.text:
                    result[0] = ident.text.decode('utf-8')
        for c in n.children:
            if c.start_point[0] <= target_line <= c.end_point[0]:
                _search(c, target_line)

    _search(root, node.start_point[0])
    return result[0]


def find_child(node: ts.Node, type_name: str) -> ts.Node | None:
    """Find the first direct child of the given AST node type."""
    for c in node.children:
        if c.type == type_name:
            return c
    return None


def extract_identifier_from_declarator(declarator: ts.Node) -> str | None:
    """Extract the variable name from a pointer/function-pointer declarator."""
    if declarator.type == 'identifier' and declarator.text:
        return declarator.text.decode('utf-8')
    if declarator.type in ('parenthesized_declarator', 'function_declarator', 'array_declarator'):
        for c in declarator.children:
            if c.type not in ('(', ')', ';'):
                result = extract_identifier_from_declarator(c)
                if result:
                    return result
    if declarator.type == 'pointer_declarator':
        return extract_identifier_from_declarator(declarator.children[-1])
    return None


def extract_field_path(node: ts.Node) -> str | None:
    """Recursively extract the full path string from a field_expression.

    Supports . and -> operators, chain access, and subscript expressions.
    Examples: c->funcs->read -> "c.funcs.read"
              obj.field -> "obj.field"
              arr[i].field -> "arr.field"
              ((struct ctx*)ptr)->field -> "ptr.field"
    """
    if node.type == 'field_expression':
        parts = []
        for child in node.children:
            if child.type in ('identifier', 'field_identifier') and child.text:
                parts.append(child.text.decode('utf-8'))
            elif child.type == 'field_expression':
                inner = extract_field_path(child)
                if inner:
                    parts.extend(inner.split('.'))
            elif child.type == 'subscript_expression' and child.children:
                # Handle arr[i].field -> extract arr name
                base = child.children[0]
                if base.type == 'identifier' and base.text:
                    parts.append(base.text.decode('utf-8'))
            elif child.type == 'parenthesized_expression':
                # Handle ((struct foo*)ptr)->field -> find identifier inside cast
                for pc in child.children:
                    if pc.type == 'cast_expression':
                        for cc in pc.children:
                            if cc.type == 'identifier' and cc.text:
                                parts.append(cc.text.decode('utf-8'))
                            elif cc.type == 'field_expression':
                                inner = extract_field_path(cc)
                                if inner:
                                    parts.extend(inner.split('.'))
        return '.'.join(parts) if parts else None
    return None


def handle_init_declarator(
    node: ts.Node,
    dataflow: VariableState,
    symbol_names: set[str],
) -> None:
    """Handle init_declarators like void (*fp)(void) = func_name or *var = &target."""
    declarator = node.child_by_field_name('declarator')
    value = node.child_by_field_name('value')
    if not declarator or not value:
        return
    var_name = extract_identifier_from_declarator(declarator)
    if not var_name:
        return

    # Handle pointer_expression: &target
    if value.type == 'pointer_expression' and value.children:
        target_node = value.children[-1]
        if target_node.type == 'identifier' and target_node.text:
            target = target_node.text.decode('utf-8')
            if target in symbol_names:
                dataflow.assign(var_name, target)
            else:
                targets = dataflow.resolve(target)
                if targets:
                    for t in targets:
                        dataflow.assign(var_name, t)
        return

    if value.type == 'identifier' and value.text:
        target = value.text.decode('utf-8')
        if target in symbol_names:
            dataflow.assign(var_name, target)
        else:
            # fp2 = fp1 (alias chain) — resolve fp1's targets
            targets = dataflow.resolve(target)
            if targets:
                for t in targets:
                    dataflow.assign(var_name, t)


# --- Unified Field Assignment Collector ---

from collections import namedtuple

FieldAssignment = namedtuple('FieldAssignment', [
    'field_path',
    'value_node',
    'resolved_value',
    'form',
    'enclosing_func',
    'line',
])


def _unwrap_identifier(node: ts.Node, unwrap_fn=None) -> str | None:
    """Extract identifier text from a node, unwrapping cast expressions."""
    if node.type == 'identifier' and node.text:
        return node.text.decode('utf-8')
    if node.type == 'cast_expression':
        if unwrap_fn:
            result = unwrap_fn(node)
            if result:
                return result
        for c in reversed(node.children):
            result = _unwrap_identifier(c, unwrap_fn)
            if result:
                return result
    return None


def collect_field_assignments(tree: ts.Tree, unwrap_fn=None) -> list[FieldAssignment]:
    """Collect all struct-field function pointer assignments from an AST.

    Handles:
    1. assignment_expression: ptr->field = rhs
    2. designated_initializer: .field = val (inside init_declarator → initializer_list)
    """
    results: list[FieldAssignment] = []

    def _extract_pair(pair_node: ts.Node, var_name: str, enclosing_func: str | None) -> None:
        """Extract a single initializer_pair: .field = value.
        Children are always [field_designator, '=', value].
        """
        field_name = None
        for c in pair_node.children:
            if c.type == 'field_designator':
                for cc in c.children:
                    if cc.type == 'field_identifier' and cc.text:
                        field_name = cc.text.decode('utf-8')
        value = pair_node.children[-1] if pair_node.children else None
        if field_name and value:
            field_path = f'{var_name}.{field_name}'
            resolved = _unwrap_identifier(value, unwrap_fn)
            results.append(FieldAssignment(
                field_path=field_path,
                value_node=value,
                resolved_value=resolved,
                form='designated_init',
                enclosing_func=enclosing_func,
                line=pair_node.start_point[0] + 1,
            ))

    def _unwrap_lhs(lhs: ts.Node) -> ts.Node | None:
        """Unwrap parenthesized_expression / cast_expression to find field_expression."""
        node = lhs
        while node.type in ('parenthesized_expression', 'cast_expression'):
            # parenthesized_expression: ( cast_expression ( type_descriptor ) field_expression )
            # cast_expression: ( type_descriptor ) field_expression
            if node.type == 'parenthesized_expression':
                for c in node.children:
                    if c.type in ('cast_expression', 'field_expression'):
                        node = c
                        break
                else:
                    return None
            elif node.type == 'cast_expression':
                for c in node.children:
                    if c.type == 'field_expression':
                        node = c
                        break
                else:
                    return None
        return node if node.type == 'field_expression' else None

    def _scan(node: ts.Node) -> None:
        # Form 1: assignment_expression
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or (node.children[0] if node.children else None)
            rhs = node.child_by_field_name('right') or (
                node.children[-1] if len(node.children) >= 2 else None
            )
            if lhs and rhs:
                field_expr = lhs if lhs.type == 'field_expression' else _unwrap_lhs(lhs)
                if field_expr:
                    field_path = extract_field_path(field_expr)
                    if field_path:
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        resolved = _unwrap_identifier(rhs, unwrap_fn)
                        results.append(FieldAssignment(
                            field_path=field_path,
                            value_node=rhs,
                            resolved_value=resolved,
                            form='assign',
                            enclosing_func=enclosing_func,
                            line=node.start_point[0] + 1,
                        ))

        # Form 2: designated_initializer inside init_declarator
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            init_list = node.child_by_field_name('value')
            if not init_list:
                for c in node.children:
                    if c.type == 'initializer_list':
                        init_list = c
                        break
            if declarator and init_list and init_list.type == 'initializer_list':
                var_name = extract_identifier_from_declarator(declarator)
                if var_name:
                    enclosing_func = find_enclosing_function(node, tree.root_node)
                    for child in init_list.children:
                        if child.type == 'initializer_pair':
                            _extract_pair(child, var_name, enclosing_func)

        for child in node.children:
            _scan(child)

    _scan(tree.root_node)
    return results


def collect_pointer_resolutions(tree: ts.Tree) -> dict[str, str]:
    """Scan function bodies for ptr = &expr patterns.

    Returns mapping: local_var_name -> resolved_name_or_path

    Handles:
    - ptr = &global_array[i]  ->  var_name -> global_array
    - ptr = &global_struct    ->  var_name -> global_struct
    - ptr = &obj->field       ->  var_name -> obj.field  (field path preserved)
    """
    resolutions: dict[str, str] = {}

    def _scan(n: ts.Node) -> None:
        if n.type == 'assignment_expression':
            _handle_assignment(n, resolutions)
        elif n.type == 'init_declarator':
            _handle_init(n, resolutions)
        for child in n.children:
            _scan(child)

    def _handle_assignment(node: ts.Node, resolutions: dict[str, str]) -> None:
        lhs = node.child_by_field_name('left') or (node.children[0] if node.children else None)
        rhs = node.child_by_field_name('right') or (
            node.children[-1] if len(node.children) >= 2 else None
        )
        if not lhs or not rhs or lhs.type != 'identifier' or not lhs.text:
            return
        var_name = lhs.text.decode('utf-8')
        resolved = _resolve_pointer_target(rhs)
        if resolved:
            resolutions[var_name] = resolved

    def _handle_init(node: ts.Node, resolutions: dict[str, str]) -> None:
        declarator = node.child_by_field_name('declarator')
        value = node.child_by_field_name('value')
        if not declarator or not value or value.type != 'pointer_expression':
            return
        var_name = extract_identifier_from_declarator(declarator)
        if not var_name:
            return
        resolved = _resolve_pointer_target(value)
        if resolved:
            resolutions[var_name] = resolved

    def _resolve_pointer_target(rhs: ts.Node) -> str | None:
        """Extract target name/path from the operand of a pointer_expression (&expr)."""
        if rhs.type != 'pointer_expression' or not rhs.children:
            return None
        inner = rhs.children[-1]
        if inner.type == 'identifier' and inner.text:
            return inner.text.decode('utf-8')
        if inner.type == 'subscript_expression' and inner.children:
            base = inner.children[0]
            if base.type == 'identifier' and base.text:
                return base.text.decode('utf-8')
        if inner.type == 'field_expression':
            field_path = extract_field_path(inner)
            if field_path:
                return field_path
        return None

    _scan(tree.root_node)
    return resolutions
