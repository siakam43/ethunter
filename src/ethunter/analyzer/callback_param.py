"""Module 3: Callback function (parameter passing) analysis."""

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
    """Detect function pointers passed as arguments and track their call sites."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            args = node.child_by_field_name('arguments')
            if args:
                caller = _find_enclosing_function(node, tree.root_node)
                for arg in args.children:
                    if arg.type == 'identifier' and arg.text:
                        func_name = arg.text.decode('utf-8')
                        if func_name in symbol_names:
                            edges.append(CallEdge(
                                caller=caller or '<unknown>',
                                callee=func_name,
                                caller_file=filepath,
                                callee_file='',
                                type=CallType.INDIRECT,
                                indirect_kind='callback_param',
                                caller_line=node.start_point[0] + 1,
                            ))
                            dataflow.assign(func_name, func_name)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _find_enclosing_function(node: ts.Node, root: ts.Node) -> str | None:
    result = [None]

    def _search(n: ts.Node, target_line: int) -> None:
        if result[0] is not None:
            return
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if decl:
                ident = _find_child(decl, 'identifier')
                if ident and ident.text:
                    result[0] = ident.text.decode('utf-8')
        for child in n.children:
            if child.start_point[0] <= target_line <= child.end_point[0]:
                _search(child, target_line)

    _search(root, node.start_point[0])
    return result[0]


def _find_child(node: ts.Node, type_name: str) -> ts.Node | None:
    for child in node.children:
        if child.type == type_name:
            return child
    return None
