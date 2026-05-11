"""ET-Bench benchmark: evaluate ethunter's indirect call detection recall.

Runs ethunter on each ET-Bench example fixture and compares detected indirect edges against per-example ground_truth.json.
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


def _run_fixture(example_dir):
    """Helper: run ethunter on a fixture directory and return graph."""
    return _run_analysis_on_fixture(example_dir)


def test_et_bench_fnptr_struct_example_2():
    """cpp_pop_definition -> dump_queued_macros (two-pass field_call fix)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_2')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('cpp_pop_definition', 'dump_queued_macros') in pairs


def test_et_bench_fnptr_struct_example_13():
    """CRYPTO_gcm128_encrypt -> aesni_encrypt (cast unwrap + param propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_13')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('CRYPTO_gcm128_encrypt', 'aesni_encrypt') in pairs


def test_et_bench_fnptr_struct_example_12():
    """s_server_main -> alpn_cb (param->field registration + call-site propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_12')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('s_server_main', 'alpn_cb') in pairs


@pytest.mark.xfail(reason="example_9 return value tracking chain needs further investigation")
def test_et_bench_fnptr_struct_example_9():
    """security_callback_debug -> ssl_security_default_callback (return value tracking)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_9')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('security_callback_debug', 'ssl_security_default_callback') in pairs


@pytest.mark.xfail(reason="example_5 cast+param propagation needs further investigation")
def test_et_bench_fnptr_struct_example_5():
    """iterate_through_spacemap_logs_cb -> count_unflushed_space_cb et al (cast + param propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_5')
    gt = _load_example_ground_truth(ex_dir)
    expected_pairs = {(e['caller'], e['callee']) for e in gt}
    graph = _run_fixture(ex_dir)
    found_pairs = {(e.caller, e.callee) for e in graph.edges}
    matched = found_pairs & expected_pairs
    assert len(matched) == len(expected_pairs), f"Missing: {expected_pairs - matched}"


@pytest.mark.xfail(reason="examples 5 and 9 still need fixes; recall at 71.43% currently")
def test_et_bench_fnptr_struct_full_recall():
    """fnptr-struct category should achieve 100% recall."""
    cat_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct')
    total_matched = 0
    total_expected = 0
    for example in sorted(os.listdir(cat_dir)):
        if not example.startswith('example_'):
            continue
        example_dir = os.path.join(cat_dir, example)
        expected = _load_example_ground_truth(example_dir)
        if not expected:
            continue
        total_expected += len(expected)
        graph = _run_analysis_on_fixture(example_dir)
        indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']
        _, matched = compute_recall(indirect_edges, expected)
        total_matched += len(matched)
    recall = total_matched / total_expected if total_expected > 0 else 1.0
    assert recall == 1.0, f"fnptr-struct recall is {recall:.2%}, expected 100%"


def test_cross_file_param_registration():
    """Verify Phase 1a: registration function in file A, call site in file B."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source_a = b'''
void SSL_CTX_set_alpn_select_cb(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
'''
    source_b = b'''
void alpn_cb(void *ctx) {}
void s_server_main(void) {
    SSL_CTX_set_alpn_select_cb(ctx, alpn_cb);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree_a = parser.parse(source_a)
    tree_b = parser.parse(source_b)

    trees = {'file_a.c': tree_a, 'file_b.c': tree_b}
    st = SymbolTable()
    df = VariableState()
    for fp in trees:
        for func in extract_functions(trees[fp], fp):
            st.add_function(func)

    graph = run_all_analyses(trees, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('s_server_main', 'alpn_cb') in pairs, f"Missing cross-file edge. Got: {pairs}"
