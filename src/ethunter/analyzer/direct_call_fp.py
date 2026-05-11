"""Direct identifier-based function pointer call detection.

Detects calls through function pointers identified by simple identifiers:
- fp() where fp has been assigned via dataflow
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through function pointer identifiers."""
    edges: list[CallEdge] = []

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
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
                            indirect_kind='direct_assign',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
