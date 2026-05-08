"""Module 9: Typedef-hidden function pointer analysis."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, find_child


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Unwrap typedef chains and detect hidden function pointer assignments/calls."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    # Collect typedefs from this file
    def _collect_typedefs(node: ts.Node) -> None:
        if node.type == 'type_definition':
            # Find the typedef name — it's a type_identifier inside pointer_declarator or function_declarator
            name_node = _find_typedef_name(node)
            if name_node:
                # Check if the typedef involves function pointer syntax
                if node.text and ('*' in node.text.decode('utf-8') or '(' in node.text.decode('utf-8')):
                    symbol_table.add_typedef(name_node, '<function_pointer>')
        for child in node.children:
            _collect_typedefs(child)

    def _find_typedef_name(node: ts.Node) -> str | None:
        """Find the typedef name from a type_definition node."""
        if node.type == 'type_identifier' and node.text:
            return node.text.decode('utf-8')
        for c in node.children:
            name = _find_typedef_name(c)
            if name:
                return name
        return None

    _collect_typedefs(tree.root_node)

    # Analyze assignments involving typedef'd types
    def _visit(node: ts.Node) -> None:
        if node.type == 'declaration':
            type_desc = find_child(node, 'type_descriptor')
            if type_desc and type_desc.text:
                type_text = type_desc.text.decode('utf-8')
                resolved = symbol_table.resolve_typedef(type_text)
                if resolved and ('*' in resolved or '(' in resolved):
                    ident = find_child(node, 'identifier')
                    if ident and ident.text:
                        var_name = ident.text.decode('utf-8')
                        init = find_child(node, 'init_declarator')
                        if init:
                            init_val = find_child(init, 'identifier')
                            if init_val and init_val.text:
                                target = init_val.text.decode('utf-8')
                                if target in symbol_names:
                                    dataflow.assign(var_name, target)
            # Also check for typedef'd type: declaration with type_identifier
            if not type_desc or not type_desc.text:
                type_id = find_child(node, 'type_identifier')
                if type_id and type_id.text:
                    type_name = type_id.text.decode('utf-8')
                    if symbol_table.resolve_typedef(type_name):
                        init = find_child(node, 'init_declarator')
                        if init:
                            var_node = find_child(init, 'identifier')
                            if var_node and var_node.text:
                                var_name = var_node.text.decode('utf-8')
                                value = find_child(init, 'identifier')
                                # find the last identifier in init_declarator (the RHS)
                                for child in init.children:
                                    if child.type == 'identifier' and child.text:
                                        value = child
                                if value and value.text:
                                    target = value.text.decode('utf-8')
                                    if target in symbol_names:
                                        dataflow.assign(var_name, target)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier':
                var_name = func_node.text.decode('utf-8')
                targets = dataflow.resolve(var_name)
                if targets:
                    caller = find_enclosing_function(node, tree.root_node)
                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='typedef_fp',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges

