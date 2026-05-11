"""Cast-based function pointer assignment tracking.

Handles cast_expression patterns:
- Init: fn_t *fp = (type)func_name
- Assignment: fp = (type)func_name
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import extract_identifier_from_declarator


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list:
    """Track function pointer assignments via cast expressions."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _extract_cast_target(node: ts.Node) -> str | None:
        """Extract function name from inside a cast_expression."""
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
                if name in symbol_names:
                    return name
        return None

    def _visit(node: ts.Node) -> None:
        # init_declarator with cast: fn_t *fp = (type)func_name
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            value = node.child_by_field_name('value')
            if declarator and value:
                target = _extract_cast_target(value)
                if target:
                    var_name = extract_identifier_from_declarator(declarator)
                    if var_name:
                        dataflow.assign(var_name, target)

        # assignment_expression with cast: fp = (type)func_name
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'identifier' and lhs.text:
                target = _extract_cast_target(rhs)
                if target:
                    var_name = lhs.text.decode('utf-8')
                    dataflow.assign(var_name, target)

        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
