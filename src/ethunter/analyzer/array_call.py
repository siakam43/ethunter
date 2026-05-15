"""Subscript-expression-based function pointer call detection.

Detects calls through array indexing:
- arr[i]()
- structs[i].field()
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through array subscript expressions."""
    edges: list[CallEdge] = []

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'subscript_expression':
                caller = find_enclosing_function(node, tree.root_node)
                arr_node = func_node.children[0] if func_node.children else None
                if arr_node and arr_node.text:
                    arr_name = arr_node.text.decode('utf-8')
                    # Phase A: try ScopedStore first
                    targets = set()
                    if hasattr(dataflow, 'store'):
                        targets = dataflow.store.resolve_global_array(arr_name)
                    # Fallback: old dataflow
                    if not targets:
                        targets = dataflow.resolve(f'<garray:{arr_name}>')
                    if not targets:
                        targets = dataflow.resolve(arr_name)
                    if not targets:
                        targets = dataflow.resolve('<initializer>')

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='array_call',
                            caller_line=node.start_point[0] + 1,
                            confidence=Confidence.MEDIUM,
                            evidence=Evidence('array_dispatch'),
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
