"""Orchestrator: runs all analyzer modules and merges results into a single CallGraph."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallGraph, CallType, CallEdge
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer import (
    direct_call,
    dlsym_fp,
)
from ethunter.analyzer import (
    direct_assign,
    initializer_assign,
    cast_assign,
    param_assign,
)
from ethunter.analyzer import (
    direct_call_fp,
    field_call,
    array_call,
)

TARGET_RESOLVERS = [
    param_assign,
    direct_assign,
    initializer_assign,
    cast_assign,
]

CALL_DETECTORS = [
    field_call,
    direct_call_fp,
    array_call,
]


def run_all_analyses(
    trees: dict[str, ts.Tree],
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> CallGraph:
    """Run all analyzer modules on the parsed trees and build the CallGraph."""
    from ethunter.analyzer.dataflow import DataflowEngine

    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    # Wrap dataflow in DataflowEngine for cross-function tracking
    engine = DataflowEngine(state=dataflow)

    # Add all functions to the graph
    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # Phase 1a: Pre-scan all files for param->field registrations.
    # This ensures engine.param_fields is fully populated BEFORE any file's
    # Phase 1 call-site propagation tries to use it (fixes cross-file timing).
    for filepath, tree in trees.items():
        param_assign._register_phase(tree, filepath, symbol_table, engine)

    # Phase 1: Target resolution (writes to dataflow via engine)
    for filepath, tree in trees.items():
        for resolver in TARGET_RESOLVERS:
            resolver.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=engine,
            )

    # Phase 1b: param_assign callback detection
    for filepath, tree in trees.items():
        edges = param_assign.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=engine,
        )
        for edge in edges:
            graph.add_edge(edge)

    # Phase 2: Call detection (reads from dataflow via engine)
    for filepath, tree in trees.items():
        for detector in CALL_DETECTORS:
            edges = detector.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=engine,
            )
            for edge in edges:
                graph.add_edge(edge)

    # dlsym_fp (independent)
    for filepath, tree in trees.items():
        edges = dlsym_fp.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=engine,
        )
        for edge in edges:
            graph.add_edge(edge)

    # Fix B+A-1: suppress callback edges where callee is covered by field_call
    # (field_call provides a more precise caller name via struct dispatch).
    field_callees = {e.callee for e in graph.edges
                     if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    if field_callees:
        filtered = []
        for edge in graph.edges:
            if edge.indirect_kind in ('callback_reg', 'callback_param') \
                    and edge.callee in field_callees:
                continue
            filtered.append(edge)
        graph.edges = filtered

    # Deduplicate: same caller+callee = one edge, prefer direct over indirect
    edge_map: dict[tuple[str, str], dict] = {}
    for edge in graph.edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge.to_dict()
        else:
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
