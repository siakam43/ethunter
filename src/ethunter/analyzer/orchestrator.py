"""Orchestrator: runs all analyzer modules and merges results into a single CallGraph.

Pipeline phases for DataflowEngine mutations:
  Phase 1a: Cross-file pre-scan (param_helpers.prepare) — metadata
  Phase 1:  Target Resolution — write dataflow (direct_assign + initializer_assign + cast_assign)
  Phase 1b: Callback detection (callback_reg edges from registration sites)
  Phase 2:  Call Detection — direct_call_fp + field_call + array_call + param_dispatch
  Phase 3:  callback_reg with covered_callees — suppress redundant callback_reg edges

Pipeline phase contracts for SymbolTable mutation:

  Phase 1a WRITERS:
    param_helpers.prepare()               → _func_var_types, _func_return_types
    initializer_assign.collect_var_types() → _var_types
    field_call.collect()                  → _func_var_types (via _collect_local_var_types,
                                            _collect_cast_types)

  Phase 1 WRITERS:
    initializer_assign.analyze()          → _struct_fields, _var_types
    (direct_assign, cast_assign, param_binding: no SymbolTable writes)

  Phase 2 READERS (no SymbolTable writes):
    field_call.analyze()                  ← _func_var_types, _var_types
    param_binding._resolve_fields()       ← _func_var_types

  CONTRACTS:
    - All Phase 1a/1 writers MUST complete before any Phase 2 reader runs.
    - _func_var_types: param_helpers + field_call.collect write disjoint
      populations (params vs locals), no conflict.
    - _var_types: collect_var_types + initializer_assign.analyze write
      disjoint populations (globals vs file-scoped), no conflict.
    - Changing pipeline ordering requires updating this table.
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallGraph, CallType, CallEdge, Confidence
from ethunter.analyzer.dataflow import DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer import (
    direct_call,
    dlsym_fp,
)
from ethunter.analyzer import (
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
    dataflow: DataflowEngine,
) -> CallGraph:
    """Run all analyzer modules on the parsed trees and build the CallGraph."""
    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    engine = dataflow

    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # === Phase 1a WRITERS (SymbolTable: _func_var_types, _func_return_types, _var_types) ===
    # All must complete before Phase 2 readers. See module docstring for contracts.
    for filepath, tree in trees.items():
        param_helpers.prepare(tree, filepath, engine, symbol_table)

    # Phase 1a (cont'd): collect struct variable types BEFORE field assignments
    for filepath, tree in trees.items():
        initializer_assign.collect_var_types(tree, filepath, symbol_table, engine)

    # Phase 1a*: field_call Pass 1 — ALL files (collect struct field assignments)
    for filepath, tree in trees.items():
        field_call.collect(tree, filepath, engine, symbol_table, symbol_names)

    # === Phase 1 WRITERS (SymbolTable: _struct_fields, _var_types via initializer_assign) ===
    # param_binding + TARGET_RESOLVERS: DataflowEngine writes only, no SymbolTable mutations.
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

    # Phase 1 Pass 2: second param_binding pass (catch local-var assignments
    # now that TARGET_RESOLVERS have populated dataflow).
    for filepath, tree in trees.items():
        param_binding.analyze(tree, filepath, symbol_table, engine)

    # Phase 1 Pass 3: param_binding field resolution (after all resolvers)
    for filepath, tree in trees.items():
        param_binding._resolve_fields(tree, filepath, symbol_table, engine)

    # === Phase 2 READERS (SymbolTable: read-only — no writes beyond this point) ===
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
    if covered_callees:
        removed = graph.remove_edges(
            lambda e: e.indirect_kind in ('callback_reg', 'callback_param')
                      and e.callee in covered_callees
        )

    return graph
