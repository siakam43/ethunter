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
