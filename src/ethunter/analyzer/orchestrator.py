"""Orchestrator: runs all analyzer modules and merges results into a single CallGraph."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallGraph, CallType, CallEdge
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer import (
    direct_call,
    fp_assign,
    callback_param,
    fp_return,
    fp_array,
    vtable,
    callback_reg,
    union_fp,
    typedef_fp,
    fp_alias,
    lazy_init,
    macro_fp,
    dlsym_fp,
)

# Analyzers that use the standard interface (symbol_table + dataflow)
STANDARD_ANALYZERS = [
    fp_assign,
    callback_param,
    fp_return,
    fp_array,
    vtable,
    callback_reg,
    union_fp,
    typedef_fp,
    fp_alias,
    lazy_init,
    macro_fp,
    dlsym_fp,
]


def run_all_analyses(
    trees: dict[str, ts.Tree],
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> CallGraph:
    """Run all analyzer modules on the parsed trees and build the CallGraph."""
    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    # Add all functions to the graph
    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer uses symbol_names (set) not symbol_table
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # All other analyzers use the standard interface
    for filepath, tree in trees.items():
        for analyzer in STANDARD_ANALYZERS:
            edges = analyzer.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=dataflow,
            )
            for edge in edges:
                graph.add_edge(edge)

    # Deduplicate edges: same caller+callee = one edge, prefer direct over indirect
    edge_map: dict[tuple[str, str], dict] = {}
    for edge in graph.edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge.to_dict()
        else:
            # If existing is indirect and new is direct, replace
            existing = edge_map[key]
            if existing.get('type') == 'indirect' and edge.type == CallType.DIRECT:
                edge_map[key] = edge.to_dict()

    graph.edges = [CallEdge(
        caller=d['caller'],
        callee=d['callee'],
        caller_file=d.get('caller_file', ''),
        callee_file=d.get('callee_file', ''),
        type=CallType(d.get('type', 'direct')),
        indirect_kind=d.get('indirect_kind', ''),
        caller_line=d.get('caller_line', 0),
    ) for d in edge_map.values()]

    return graph
