"""Module 9: Typedef-hidden function pointer analysis."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable


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
            name_node = _find_child(node, 'type_identifier')
            if name_node and name_node.text:
                type_node = _find_child(node, 'type_descriptor') or node.children[0]
                if type_node:
                    # Check if typedef involves function pointer syntax
                    text = type_node.text.decode('utf-8')
                    if '(' in text or '*' in text:
                        symbol_table.add_typedef(name_node.text.decode('utf-8'), text)
        for child in node.children:
            _collect_typedefs(child)

    _collect_typedefs(tree.root_node)

    # Analyze assignments involving typedef'd types
    def _visit(node: ts.Node) -> None:
        if node.type == 'declaration':
            type_desc = _find_child(node, 'type_descriptor')
            if type_desc and type_desc.text:
                type_text = type_desc.text.decode('utf-8')
                resolved = symbol_table.resolve_typedef(type_text)
                if resolved and ('*' in resolved or '(' in resolved):
                    # This is a function pointer declaration via typedef
                    ident = _find_child(node, 'identifier')
                    if ident and ident.text:
                        var_name = ident.text.decode('utf-8')
                        init = _find_child(node, 'init_declarator')
                        if init:
                            init_val = _find_child(init, 'identifier')
                            if init_val and init_val.text:
                                target = init_val.text.decode('utf-8')
                                if target in symbol_names:
                                    dataflow.assign(var_name, target)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier':
                var_name = func_node.text.decode('utf-8')
                targets = dataflow.resolve(var_name)
                if targets:
                    caller = _find_enclosing_function(node, tree.root_node)
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


def _find_enclosing_function(node: ts.Node, root: ts.Node) -> str | None:
    result = [None]
    def _search(n: ts.Node, line: int) -> None:
        if result[0] is not None: return
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if decl:
                ident = _find_child(decl, 'identifier')
                if ident and ident.text:
                    result[0] = ident.text.decode('utf-8')
        for c in n.children:
            if c.start_point[0] <= line <= c.end_point[0]:
                _search(c, line)
    _search(root, node.start_point[0])
    return result[0]

def _find_child(node: ts.Node, type_name: str) -> ts.Node | None:
    for c in node.children:
        if c.type == type_name: return c
    return None
