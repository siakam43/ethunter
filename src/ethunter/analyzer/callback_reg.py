"""Module 7: Callback registration pattern analysis."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, find_child

# Patterns that indicate a callback registration function
REG_PATTERNS = ['register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_']


def _is_registration(name: str) -> bool:
    """Check if a function name matches common callback registration patterns."""
    lower = name.lower()
    return any(p in lower for p in REG_PATTERNS)


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
                if _is_registration(call_name):
                    args = node.child_by_field_name('arguments')
                    if args:
                        caller = find_enclosing_function(node, tree.root_node)
                        for arg in args.children:
                            if arg.type == 'identifier' and arg.text:
                                target = arg.text.decode('utf-8')
                                if target in symbol_names:
                                    dataflow.register_callback(target)
                                    edges.append(CallEdge(
                                        caller=caller or '<registration>',
                                        callee=target,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='callback_reg',
                                        caller_line=node.start_point[0] + 1,
                                    ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)

    return edges

