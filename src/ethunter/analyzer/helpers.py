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
    if declarator.type in ('parenthesized_declarator', 'function_declarator'):
        for c in declarator.children:
            if c.type not in ('(', ')', ';'):
                result = extract_identifier_from_declarator(c)
                if result:
                    return result
    if declarator.type == 'pointer_declarator':
        return extract_identifier_from_declarator(declarator.children[-1])
    return None


def handle_init_declarator(
    node: ts.Node,
    dataflow: VariableState,
    symbol_names: set[str],
) -> None:
    """Handle init_declarators like void (*fp)(void) = func_name."""
    declarator = node.child_by_field_name('declarator')
    value = node.child_by_field_name('value')
    if not declarator or not value:
        return
    if value.type == 'identifier' and value.text:
        target = value.text.decode('utf-8')
        var_name = extract_identifier_from_declarator(declarator)
        if var_name:
            if target in symbol_names:
                dataflow.assign(var_name, target)
            else:
                # fp2 = fp1 (alias chain) — resolve fp1's targets
                targets = dataflow.resolve(target)
                if targets:
                    for t in targets:
                        dataflow.assign(var_name, t)
