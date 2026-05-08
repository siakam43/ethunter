"""Module 4: Function pointer return value analysis."""

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
    """Detect functions that return function pointers and track calls through return values.

    Handles: get_handler()() — a function returning a function pointer that is immediately called.
    Note: the pattern `x = get_handler(); x()` requires inter-procedural analysis beyond
    this tool's single-file tree-sitter approach.
    """
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'call_expression':
                caller = find_enclosing_function(node, tree.root_node)
                inner_func = func_node.child_by_field_name('function') or func_node.children[0]
                if inner_func and inner_func.text:
                    inner_name = inner_func.text.decode('utf-8')
                    if inner_name in symbol_names:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=inner_name,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='fp_return',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges

