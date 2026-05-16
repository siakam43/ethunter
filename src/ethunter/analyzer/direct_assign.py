"""Direct function pointer assignment tracking.

Handles simple assignment patterns:
- fp = func_name
- void (*fp)(void) = func_name
- fp2 = fp1 (alias chain)
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import extract_identifier_from_declarator, find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: DataflowEngine,
) -> list:
    """Track direct function pointer assignments."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _assign(var_name: str, target: str, node: ts.Node) -> None:
        enclosing = find_enclosing_function(node, tree.root_node) or '<global>'
        dataflow.store.assign_func_var(enclosing, var_name, target)

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
                        _assign(var_name, target, node)
                    else:
                        # Alias chain: fp2 = fp1
                        targets = dataflow.resolve_variable(target)
                        if targets:
                            for t in targets:
                                _assign(var_name, t, node)

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
                        _assign(var_name, target, node)
                    else:
                        targets = dataflow.resolve_variable(target)
                        if targets:
                            for t in targets:
                                _assign(var_name, t, node)
                        else:
                            # Track struct pointer alias (e.g., Curl_ssl -> Curl_ssl_openssl)
                            _assign(var_name, target, node)
                return
            if value.type == 'identifier' and value.text:
                target = value.text.decode('utf-8')
                if target in symbol_names:
                    _assign(var_name, target, node)
                else:
                    targets = dataflow.resolve_variable(target)
                    if targets:
                        for t in targets:
                            _assign(var_name, t, node)

        for child in node.children:
            _visit(child)

    _visit(tree.root_node)

    # Pass 2: re-resolve alias chains where first pass had unresolved RHS
    # (e.g., tmp_handler = log_handler where log_handler is assigned later in tree)
    def _visit_pass2(node: ts.Node) -> None:
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if (lhs and rhs and lhs.type == 'identifier' and lhs.text
                    and rhs.type == 'identifier' and rhs.text):
                var_name = lhs.text.decode('utf-8')
                target = rhs.text.decode('utf-8')
                if target not in symbol_names:
                    targets = dataflow.resolve_variable(target)
                    if targets:
                        enclosing = find_enclosing_function(node, tree.root_node) or '<global>'
                        for t in targets:
                            _assign(var_name, t, node)
            # Re-check init_declarators with deferred resolution
            if node.type == 'init_declarator':
                declarator = node.child_by_field_name('declarator')
                value = node.child_by_field_name('value')
                if (declarator and value and value.type == 'identifier'
                        and value.text):
                    var_name = extract_identifier_from_declarator(declarator)
                    target = value.text.decode('utf-8')
                    if var_name and target not in symbol_names:
                        targets = dataflow.resolve_variable(target)
                        enclosing = find_enclosing_function(node, tree.root_node) or '<global>'
                        if targets:
                            for t in targets:
                                _assign(var_name, t, node)
        for child in node.children:
            _visit_pass2(child)

    _visit_pass2(tree.root_node)
    return edges
