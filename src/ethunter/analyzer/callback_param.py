"""Module 3: Callback function (parameter passing) analysis."""

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
    """Detect function pointers passed as arguments and track their call sites."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            args = node.child_by_field_name('arguments')
            if args:
                caller = find_enclosing_function(node, tree.root_node)
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

