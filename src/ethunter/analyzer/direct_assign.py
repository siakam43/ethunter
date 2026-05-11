"""Direct function pointer assignment tracking.

Handles simple assignment patterns:
- fp = func_name
- void (*fp)(void) = func_name
- fp2 = fp1 (alias chain)
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
    """Track direct function pointer assignments."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # assignment_expression: fp = func_name
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if not lhs or not rhs:
                return
            if lhs.type == 'identifier' and lhs.text:
                var_name = lhs.text.decode('utf-8')
                if rhs.type == 'identifier' and rhs.text:
                    target = rhs.text.decode('utf-8')
                    if target in symbol_names:
                        dataflow.assign(var_name, target)
                    else:
                        # Alias chain: fp2 = fp1
                        targets = dataflow.resolve(target)
                        if targets:
                            for t in targets:
                                dataflow.assign(var_name, t)

        # init_declarator: void (*fp)(void) = func_name or *var = &target
        if node.type == 'init_declarator':
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
                        else:
                            # Track struct pointer alias (e.g., Curl_ssl -> Curl_ssl_openssl)
                            dataflow.assign(var_name, target)
                return
            if value.type == 'identifier' and value.text:
                target = value.text.decode('utf-8')
                if target in symbol_names:
                    dataflow.assign(var_name, target)
                else:
                    targets = dataflow.resolve(target)
                    if targets:
                        for t in targets:
                            dataflow.assign(var_name, t)

        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
