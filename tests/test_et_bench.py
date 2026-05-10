"""ET-Bench benchmark: evaluate ethunter's indirect call detection recall.

Runs ethunter on each ET-Bench example fixture (syntax-clean rewritten CG-Bench
fixtures) and compares detected indirect edges against per-example ground_truth.json.
Reports recall per category and overall.
"""

import os
import json
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.orchestrator import run_all_analyses

ET_BENCH_DIR = os.path.join(os.path.dirname(__file__), 'benchmark', 'et_bench')


def _run_analysis_on_fixture(fixture_dir):
    """Run ethunter's full pipeline on a fixture directory."""
    trees = {}
    st = SymbolTable()
    df = VariableState()

    for root, dirs, files in os.walk(fixture_dir):
        for f in files:
            if f.endswith(('.c', '.h')):
                path = os.path.join(root, f)
                tree = parse_file(path)
                trees[path] = tree
                for func in extract_functions(tree, path):
                    st.add_function(func)

    graph = run_all_analyses(trees, st, df)
    return graph


def compute_recall(found_edges, expected_edges):
    """Compute recall: fraction of expected (caller, callee) pairs found."""
    found_pairs = {(e.caller, e.callee) for e in found_edges}
    expected_pairs = {(e['caller'], e['callee']) for e in expected_edges}
    matched = found_pairs & expected_pairs
    if not expected_pairs:
        return 1.0, matched
    return len(matched) / len(expected_pairs), matched


def _get_categories():
    """Return list of category directory names."""
    if not os.path.isdir(ET_BENCH_DIR):
        return []
    return sorted(
        d for d in os.listdir(ET_BENCH_DIR)
        if os.path.isdir(os.path.join(ET_BENCH_DIR, d))
    )


def _get_examples(category):
    """Return list of example directory names for a category."""
    cat_dir = os.path.join(ET_BENCH_DIR, category)
    if not os.path.isdir(cat_dir):
        return []
    return sorted(
        d for d in os.listdir(cat_dir)
        if d.startswith('example_') and os.path.isdir(os.path.join(cat_dir, d))
    )


def _load_example_ground_truth(example_dir):
    """Load ground_truth.json from a single example directory."""
    gt_path = os.path.join(example_dir, 'ground_truth.json')
    if not os.path.isfile(gt_path):
        return []
    with open(gt_path) as f:
        gt = json.load(f)
    return gt.get('examples', [])


def test_et_bench_report():
    """Run ethunter on all ET-Bench fixtures and report recall per category and overall."""
    categories = _get_categories()
    assert categories, f'No category directories found in {ET_BENCH_DIR}'

    results = {}
    total_matched = 0
    total_expected = 0

    for category in categories:
        examples = _get_examples(category)
        cat_matched = 0
        cat_expected = 0

        for example in examples:
            example_dir = os.path.join(ET_BENCH_DIR, category, example)
            example_edges = _load_example_ground_truth(example_dir)
            if not example_edges:
                continue

            cat_expected += len(example_edges)

            graph = _run_analysis_on_fixture(example_dir)
            indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']
            _, matched = compute_recall(indirect_edges, example_edges)
            cat_matched += len(matched)

        cat_recall = cat_matched / cat_expected if cat_expected > 0 else 1.0

        results[category] = {
            'matched': cat_matched,
            'total': cat_expected,
            'recall': cat_recall,
        }
        total_matched += cat_matched
        total_expected += cat_expected

    # Report per category
    overall_recall = total_matched / total_expected if total_expected > 0 else 1.0
    print('\n=== ET-Bench Recall Report ===')
    print(f'{"Category":<35} {"Matched":>10} {"Expected":>10} {"Recall":>10}')
    print('-' * 67)
    for category, r in sorted(results.items()):
        print(f'{category:<35} {r["matched"]:>10} {r["total"]:>10} {r["recall"]:>10.2%}')
    print('-' * 67)
    print(f'{"OVERALL":<35} {total_matched:>10} {total_expected:>10} {overall_recall:>10.2%}')
    print()
