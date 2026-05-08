"""Module 7: Callback registration pattern analysis."""

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
    """Detect callback registration patterns and emit edges for registered callbacks."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                if 'register' in call_name.lower():
                    args = node.child_by_field_name('arguments')
                    if args:
                        for arg in args.children:
                            if arg.type == 'identifier' and arg.text:
                                target = arg.text.decode('utf-8')
                                if target in symbol_names:
                                    dataflow.register_callback(target)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)

    # Emit edges: any registered callback is a potential target from registration sites
    for cb in dataflow.registered_callbacks:
        edges.append(CallEdge(
            caller='<registration>',
            callee=cb,
            caller_file='',
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_reg',
        ))

    return edges
