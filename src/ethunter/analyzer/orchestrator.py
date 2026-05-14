"""Orchestrator: runs all analyzer modules and merges results into a single CallGraph.

Hybrid pipeline (migration-in-progress):
  Phase 1a: Cross-file pre-scan (param_helpers.prepare) — metadata
  Phase 1:  Target Resolution — write dataflow (param_assign + direct_assign + initializer_assign + cast_assign)
  Phase 1b: param_assign callback detection — callback_reg edges
  Phase 2:  Call Detection — direct_call_fp + field_call + array_call + param_dispatch
  Phase 3:  callback_reg with covered_callees — suppress redundant callback_reg edges
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallGraph, CallType, CallEdge
from ethunter.analyzer.dataflow import VariableState, DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer import (
    direct_call,
    dlsym_fp,
)
from ethunter.analyzer import (
    param_assign,
    direct_assign,
    initializer_assign,
    cast_assign,
)
from ethunter.analyzer import (
    direct_call_fp,
    field_call,
    array_call,
)
from ethunter.analyzer import (
    param_helpers,
    param_binding,
    param_dispatch,
    callback_reg,
)

TARGET_RESOLVERS = [
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
    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    engine = DataflowEngine(state=dataflow)

    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # Phase 1a: Cross-file pre-scan for metadata
    for filepath, tree in trees.items():
        param_helpers.prepare(tree, filepath, engine, symbol_table)

    # Phase 1a (cont'd): param_assign pre-scan for cross-file state
    for filepath, tree in trees.items():
        param_assign.register_phase(tree, filepath, symbol_table, engine)

    # Phase 1a*: field_call Pass 1 — ALL files (collect struct field assignments)
    for filepath, tree in trees.items():
        field_call.collect(tree, filepath, engine, symbol_table, symbol_names)

    # Phase 1 Pass 1: param_binding call params (must run first, before direct_assign)
    for filepath, tree in trees.items():
        param_binding.analyze(tree, filepath, symbol_table, engine)

    # Phase 1 Pass 1b: TARGET_RESOLVERS (write dataflow, no edges)
    for filepath, tree in trees.items():
        for resolver in TARGET_RESOLVERS:
            resolver.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=engine,
            )

    # Phase 1 Pass 2: param_binding field resolution (after all resolvers)
    for filepath, tree in trees.items():
        param_binding._resolve_fields(tree, filepath, symbol_table, engine)

    # Phase 1c (deprecated): param_assign.analyze() — legacy edges, replaced by
    # param_dispatch + callback_reg but kept for backward compat while migration completes.
    for filepath, tree in trees.items():
        edges = param_assign.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=engine,
        )
        for edge in edges:
            graph.add_edge(edge)

    # Phase 2: Call Detection (reads from dataflow via engine)
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

    # param_dispatch: additional callback_param edges
    for filepath, tree in trees.items():
        edges = param_dispatch.analyze(tree, filepath, engine)
        for edge in edges:
            graph.add_edge(edge)

    # Build covered_callees from field_call edges
    covered_callees = {e.callee for e in graph.edges
                       if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    engine.covered_callees = covered_callees

    # Phase 3: callback_reg with covered_callees + param_usage checks
    for filepath, tree in trees.items():
        edges = callback_reg.analyze(tree, filepath, engine)
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

    # Fix B: suppress callback edges where callee is covered by field_call
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
