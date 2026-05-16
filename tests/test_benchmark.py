"""Benchmark integration tests against real C projects."""

import os
import json
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import DataflowEngine
from ethunter.analyzer.orchestrator import run_all_analyses

BENCHMARK_DIR = os.path.join(os.path.dirname(__file__), 'benchmark')


def _run_analysis_on_benchmark(project_dir, source_files):
    """Run full ethunter pipeline on a benchmark project."""
    trees = {}
    st = SymbolTable()
    df = DataflowEngine()
    for f in source_files:
        path = os.path.join(project_dir, f)
        tree = parse_file(path)
        trees[path] = tree
        for func in extract_functions(tree, f):
            st.add_function(func)
    graph = run_all_analyses(trees, st, df)
    return graph


def compute_recall(found_edges, expected_edges):
    """Compute recall: fraction of expected edges found."""
    found_pairs = {(e.caller, e.callee) for e in found_edges}
    expected_pairs = {(e['caller'], e['callee']) for e in expected_edges}
    matched = found_pairs & expected_pairs
    if not expected_pairs:
        return 1.0, matched
    return len(matched) / len(expected_pairs), matched


def test_cjson_benchmark():
    """cJSON v1.7.18: direct recall must be 100%."""
    cjson_dir = os.path.join(BENCHMARK_DIR, 'cjson')
    with open(os.path.join(cjson_dir, 'ground_truth.json')) as f:
        gt = json.load(f)

    graph = _run_analysis_on_benchmark(cjson_dir, gt['source_files'])

    direct_edges = [e for e in graph.edges if e.type.value == 'direct']
    indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']

    direct_recall, matched_direct = compute_recall(direct_edges, gt['direct_edges'])
    indirect_recall, matched_indirect = compute_recall(indirect_edges, gt['indirect_edges'])

    print(f'cJSON direct recall: {direct_recall:.2%} ({len(matched_direct)}/{len(gt["direct_edges"])})')
    print(f'cJSON indirect recall: {indirect_recall:.2%} ({len(matched_indirect)}/{len(gt["indirect_edges"])})')

    assert direct_recall == 1.0, f'Direct recall must be 100%, got {direct_recall:.2%}'
    assert indirect_recall >= 0.8, f'Indirect recall must be >=80%, got {indirect_recall:.2%}'


def test_libuv_benchmark():
    """libuv v1.48.0 core subset: direct recall must be 100%."""
    libuv_dir = os.path.join(BENCHMARK_DIR, 'libuv')
    with open(os.path.join(libuv_dir, 'ground_truth.json')) as f:
        gt = json.load(f)

    graph = _run_analysis_on_benchmark(libuv_dir, gt['source_files'])

    direct_edges = [e for e in graph.edges if e.type.value == 'direct']
    indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']

    direct_recall, matched_direct = compute_recall(direct_edges, gt['direct_edges'])
    indirect_recall, matched_indirect = compute_recall(indirect_edges, gt['indirect_edges'])

    print(f'libuv direct recall: {direct_recall:.2%} ({len(matched_direct)}/{len(gt["direct_edges"])})')
    print(f'libuv indirect recall: {indirect_recall:.2%} ({len(matched_indirect)}/{len(gt["indirect_edges"])})')

    assert direct_recall == 1.0, f'Direct recall must be 100%, got {direct_recall:.2%}'
    assert indirect_recall >= 0.8, f'Indirect recall must be >=80%, got {indirect_recall:.2%}'
