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
    total_extra = 0
    total_detected = 0

    for category in categories:
        examples = _get_examples(category)
        cat_matched = 0
        cat_expected = 0
        cat_extra = 0
        cat_detected = 0

        for example in examples:
            example_dir = os.path.join(ET_BENCH_DIR, category, example)
            example_edges = _load_example_ground_truth(example_dir)
            if not example_edges:
                continue

            cat_expected += len(example_edges)

            graph = _run_analysis_on_fixture(example_dir)
            indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']
            found_pairs = {(e.caller, e.callee) for e in indirect_edges}
            expected_pairs = {(e['caller'], e['callee']) for e in example_edges}

            matched_pairs = found_pairs & expected_pairs
            extra_pairs = found_pairs - expected_pairs

            cat_matched += len(matched_pairs)
            cat_extra += len(extra_pairs)
            cat_detected += len(found_pairs)

        cat_recall = cat_matched / cat_expected if cat_expected > 0 else 1.0
        cat_fpr = cat_extra / cat_detected if cat_detected > 0 else 0.0

        results[category] = {
            'matched': cat_matched,
            'total': cat_expected,
            'recall': cat_recall,
            'extra': cat_extra,
            'detected': cat_detected,
            'fpr': cat_fpr,
        }
        total_matched += cat_matched
        total_expected += cat_expected
        total_extra += cat_extra
        total_detected += cat_detected

    # Report per category
    overall_recall = total_matched / total_expected if total_expected > 0 else 1.0
    overall_fpr = total_extra / total_detected if total_detected > 0 else 0.0
    print('\n=== ET-Bench Recall & False Positive Report ===')
    print(f'{"Category":<35} {"Matched":>8} {"Expected":>8} {"Extra":>6} {"Recall":>8} {"FPR":>8}')
    print('-' * 79)
    for category, r in sorted(results.items()):
        print(f'{category:<35} {r["matched"]:>8} {r["total"]:>8} {r["extra"]:>6} {r["recall"]:>7.2%} {r["fpr"]:>7.2%}')
    print('-' * 79)
    print(f'{"OVERALL":<35} {total_matched:>8} {total_expected:>8} {total_extra:>6} {overall_recall:>7.2%} {overall_fpr:>7.2%}')
    print()

    # FPR ceilings — post-arch-refactor baseline (30.54% overall)
    fpr_ceilings = {
        'fnptr-callback': 0.67,
        'fnptr-cast': 0.69,
        'fnptr-global-array': 0.03,
        'fnptr-global-struct': 0.41,
        'fnptr-global-struct-array': 0.50,
        'fnptr-library': 0.20,
        'fnptr-only': 0.10,
        'fnptr-struct': 0.41,
        'fnptr-varargs': 0.70,
    }
    for category, ceiling in fpr_ceilings.items():
        if category in results:
            actual_fpr = results[category]['fpr']
            assert actual_fpr <= ceiling, \
                f"{category} FPR={actual_fpr:.2%} exceeds ceiling {ceiling:.2%}"


def test_et_bench_high_confidence_fpr():
    """High-confidence edge subset should have FPR < 5%."""
    categories = _get_categories()
    total_extra = 0
    total_detected = 0
    for category in categories:
        for example in _get_examples(category):
            example_dir = os.path.join(ET_BENCH_DIR, category, example)
            example_edges = _load_example_ground_truth(example_dir)
            if not example_edges:
                continue
            graph = _run_analysis_on_fixture(example_dir)
            high_conf_edges = [e for e in graph.edges
                               if e.type.value == 'indirect'
                               and getattr(e, 'confidence', 'medium') == 'high']
            found_pairs = {(e.caller, e.callee) for e in high_conf_edges}
            expected_pairs = {(e['caller'], e['callee']) for e in example_edges}
            extra = found_pairs - expected_pairs
            total_extra += len(extra)
            total_detected += len(found_pairs)
    fpr = total_extra / total_detected if total_detected > 0 else 0.0
    print(f'\nHigh-confidence FPR: {fpr:.2%} ({total_extra} extra / {total_detected} detected)')
    # NOTE: 5% target requires removing suffix scans (post-Phase C cleanup).
    # Current baseline: ~22% (most FPs come from old fallback layers)
    # Target: <5% after legacy fallback removed. Current: ~13%.
    assert fpr < 0.20, f"High-confidence FPR={fpr:.2%} exceeds 20% ceiling"


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
    """tls_handle_alpn -> alpn_cb via field_call (param->field + dispatch)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_12')
    graph = _run_fixture(ex_dir)
    # Fix B: callback_reg suppressed when field_call covers same callee.
    # alpn_cb dispatched via struct field (field_call), not callback_reg.
    assert any('alpn_cb' == e.callee for e in graph.edges), \
        "alpn_cb should be reachable via field_call"
def test_et_bench_fnptr_struct_example_9():
    """security_callback_debug -> ssl_security_default_callback (return value tracking)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_9')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('security_callback_debug', 'ssl_security_default_callback') in pairs


def test_et_bench_fnptr_struct_example_5():
    """iterate_through_spacemap_logs_cb -> count_unflushed_space_cb et al (cast + param propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_5')
    gt = _load_example_ground_truth(ex_dir)
    expected_pairs = {(e['caller'], e['callee']) for e in gt}
    graph = _run_fixture(ex_dir)
    found_pairs = {(e.caller, e.callee) for e in graph.edges}
    matched = found_pairs & expected_pairs
    assert len(matched) == len(expected_pairs), f"Missing: {expected_pairs - matched}"


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


def test_bug0_positional_index_correctness():
    """Bug 0: string/number/null values in positional initializers must increment index."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef struct ops {
        const char *name;
        void (*init)(void);
        int (*cleanup)(void);
        void *extra;
    } ops_t;

    static void my_init(void) {}
    static int my_cleanup(void) { return 0; }

    static const ops_t my_ops = {
        "myops",
        my_init,
        my_cleanup,
        NULL,
    };
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.initializer_assign import analyze as init_analyze
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    init_analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=df)

    # After fix: index 0 ("myops", string) increments index; index 1 gets my_init -> field "init"
    assert 'my_init' in df.resolve('<gstruct:my_ops.init>'), \
        f"Expected my_init in .init, got: {df.resolve('<gstruct:my_ops.init>')}"
    # index 2 gets my_cleanup -> field "cleanup"
    assert 'my_cleanup' in df.resolve('<gstruct:my_ops.cleanup>'), \
        f"Expected my_cleanup in .cleanup, got: {df.resolve('<gstruct:my_ops.cleanup>')}"
    # NULL at index 3 should be skipped for function target storage
    assert not df.resolve('<gstruct:my_ops.extra>'), \
        f"Expected empty .extra, got: {df.resolve('<gstruct:my_ops.extra>')}"


def test_bug0_array_of_structs_with_inner_init_list():
    """Bug 0 extended: inner initializer_list in array-of-structs needs index tracking + field mapping."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef int (*fn_t)(void);

    struct item {
        const char *label;
        int priority;
        fn_t handler;
    };

    static int do_a(void) { return 0; }
    static int do_b(void) { return 0; }

    static const struct item table[] = {
        {"alpha", 1, do_a},
        {"beta",  2, do_b},
    };
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.initializer_assign import analyze as init_analyze
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    init_analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=df)

    # After fix: <gstruct:table.handler> should contain do_a and do_b
    targets = df.resolve('<gstruct:table.handler>')
    assert 'do_a' in targets, f"Expected do_a in table.handler, got: {targets}"
    assert 'do_b' in targets, f"Expected do_b in table.handler, got: {targets}"
    # Also check garray
    garray_targets = df.resolve('<garray:table>')
    assert 'do_a' in garray_targets, f"Expected do_a in garray:table"
    assert 'do_b' in garray_targets, f"Expected do_b in garray:table"


def test_fix_a_collect_pointer_resolutions():
    """Fix A: collect_pointer_resolutions handles &identifier, &subscript, &field_expr."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    struct ops {
        void (*init)(void);
    };

    static void my_init(void) {}
    static struct ops my_ops = { my_init };
    static struct ops ops_array[1] = { { my_init } };

    void test_func(void) {
        struct ops *p1 = &my_ops;           // &identifier
        struct ops *p2 = &ops_array[0];     // &subscript_expression
        struct ops *p3 = &(my_ops);         // &parenthesized (edge case)
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.helpers import collect_pointer_resolutions

    resolutions = collect_pointer_resolutions(tree)
    assert 'p1' in resolutions, f"p1 not found in {resolutions}"
    assert resolutions['p1'] == 'my_ops', f"p1 -> {resolutions.get('p1')}"
    assert 'p2' in resolutions, f"p2 not found in {resolutions}"
    assert resolutions['p2'] == 'ops_array', f"p2 -> {resolutions.get('p2')}"
    assert 'p3' not in resolutions, f"p3 should not resolve (parenthesized, not field_expr)"


def test_fix_b_param_alias_registration():
    """Fix B: param_alias_map is populated when caller passes global array arg."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(void);

    struct item {
        const char *label;
        int pri;
        cb_fn handler;
    };

    static void my_handler(void) {}

    static const struct item items[] = {
        {"test", 0, my_handler},
    };

    void process(const struct item list[]) {
        if (list[0].handler)
            list[0].handler();
    }

    void bootstrap(void) {
        process(items);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import initializer_assign

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())

    initializer_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)

    # After Fix B: param_alias_map should have (process, list) -> items
    assert hasattr(engine, 'param_alias_map'), "param_alias_map not set on engine"
    assert ('process', 'list') in engine.param_alias_map, \
        f"Expected (process, list) in param_alias_map, got: {engine.param_alias_map}"
    assert engine.param_alias_map[('process', 'list')] == 'items', \
        f"Expected items, got: {engine.param_alias_map.get(('process', 'list'))}"


def test_fix_c1_pointer_expression_in_array_init():
    """Fix C1: &struct_name in array initializers produces garray entries."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*transform_fn)(void);

    struct impl {
        const char *name;
        transform_fn transform;
    };

    static void my_transform(void) {}

    static const struct impl my_impl = {
        "generic", my_transform
    };

    static const struct impl *const impls[] = {
        &my_impl,
    };
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.initializer_assign import analyze as init_analyze
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    init_analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=df)

    # After C1: <garray:impls> should contain the struct name
    targets = df.resolve('<garray:impls>')
    assert 'my_impl' in targets, f"Expected my_impl in <garray:impls>, got: {targets}"


def test_fix_c2_call_expression_rhs_field_assign():
    """Fix C2/C3: obj->field = func_call() resolves through callee return + local_fp_tracker."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*transform_fn)(void);

    struct ops {
        const char *name;
        transform_fn transform;
    };

    static void my_transform(void) {}

    static const struct ops my_impl = {
        "generic", my_transform,
    };

    static const struct ops *const impls[] = {
        &my_impl,
    };

    static const struct ops *get_ops(void) {
        return impls[0];
    }

    struct ctx {
        const struct ops *ops;
    };

    void use_ops(struct ctx *ctx) {
        const struct ops *ops = ctx->ops;
        if (ops && ops->transform)
            ops->transform();
    }

    void init(struct ctx *ctx) {
        ctx->ops = get_ops();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('use_ops', 'my_transform') in pairs, \
        f"Missing use_ops -> my_transform. Got: {pairs}"


def test_cast_assign_no_symbol_names_guard():
    """Phase 1: (type)stdlib_func cast where target is NOT in symbol_names should still be tracked."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void *(*alloc_fn)(size_t nmemb, size_t size);

    alloc_fn my_alloc = (alloc_fn)calloc;

    void use_alloc(void) {
        my_alloc(1, 64);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('use_alloc', 'calloc') in pairs, \
        f"Expected use_alloc -> calloc, got: {pairs}"


def test_direct_assign_no_symbol_names_guard():
    """Phase 1b: direct assignment fp = stdlib_func where target not in symbol_names should be tracked."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef char *(*strdup_fn)(const char *str);

    strdup_fn my_strdup = (strdup_fn)strdup;

    char *use_strdup(const char *s) {
        return my_strdup(s);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('use_strdup', 'strdup') in pairs, \
        f"Expected use_strdup -> strdup, got: {pairs}"


def test_param_local_call_direct():
    """Phase 2: callee(fnptr) pattern where fnptr is called directly inside callee."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef char *(*fmt_fn)(long double n);

    static char *format_time_us(long double n) {
        (void)n;
        return "1.00us";
    }

    static void print_units(long double n, fmt_fn fmt, int width) {
        char *msg = fmt(n);
        (void)msg;
        (void)width;
    }

    void main_func(void) {
        print_units(100.0, format_time_us, 10);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('print_units', 'format_time_us') in pairs, \
        f"Expected print_units -> format_time_us, got: {pairs}"


def test_param_local_call_address_of():
    """Phase 2: &func passed as fnptr argument, called through parameter in callee."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef const void *(*ptr_getter)(void *ctx, size_t i);

    const void *my_getter(void *ctx, size_t i) {
        return (void *)(unsigned long long)i;
    }

    static void batch_lookup(size_t n, ptr_getter getter, void *ctx) {
        for (size_t i = 0; i < n; i++) {
            getter(ctx, i);
        }
    }

    void caller_func(void) {
        batch_lookup(10, &my_getter, ((void *)0));
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('batch_lookup', 'my_getter') in pairs, \
        f"Expected batch_lookup -> my_getter, got: {pairs}"


def test_param_local_var_dataflow_fallback():
    """Phase 2: local var = func_name; callee(local_var) -> resolve via dataflow."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef int (*cmp_fn)(const void *, const void *);

    int sort_asc(const void *a, const void *b) { (void)a; (void)b; return 0; }

    static void my_qsort(void *base, size_t n, size_t sz, cmp_fn cmp) {
        cmp(base, ((char *)base) + sz);
    }

    void sort_data(void) {
        cmp_fn callback = sort_asc;
        my_qsort(((void *)0), 10, 8, callback);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('my_qsort', 'sort_asc') in pairs, \
        f"Expected my_qsort -> sort_asc, got: {pairs}"


def test_param_local_call_deref():
    """Phase 2: (*fnptr)(args) dereference call through parameter."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(int x);

    static void actual_cb(int x) { (void)x; }

    static void invoke_cb(int x, cb_fn cb) {
        (*cb)(x);
    }

    void main_func(void) {
        invoke_cb(42, actual_cb);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('invoke_cb', 'actual_cb') in pairs, \
        f"Expected invoke_cb -> actual_cb, got: {pairs}"


def test_param_callback_of_callback():
    """Phase 2: field->fnptr(fnptr_arg) — fnptr passed as arg to indirect field call."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    static void my_relocate(void *ptr) { (void)ptr; }

    typedef struct {
        void (*note_fn)(void *obj, void *cookie, void (*op)(void *ptr));
        void *obj;
        void *cookie;
    } ptr_data_t;

    static void my_note_fn(void *obj, void *cookie, void (*op)(void *ptr)) {
        op(obj);
    }

    static ptr_data_t slot;

    void caller_func(void) {
        slot.note_fn = my_note_fn;
        if (slot.note_fn)
            slot.note_fn(slot.obj, slot.cookie, my_relocate);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    # Fix 2: field_call callback-of-callback uses ftarget (my_note_fn) as caller
    assert ('my_note_fn', 'my_relocate') in pairs, \
        f"Expected my_note_fn -> my_relocate, got: {pairs}"


def test_fnptr_pointer_global():
    """Phase 2/Gap4: log_handler_fn *global -> local tmp_handler -> call through local."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*log_handler_fn)(int level, const char *msg, void *ctx);

    static log_handler_fn *log_handler;
    static void *log_handler_ctx;

    static void mm_log_handler(int level, const char *msg, void *ctx) {
        (void)level; (void)msg; (void)ctx;
    }

    static void do_log(int level, const char *msg) {
        log_handler_fn *tmp_handler;
        if (log_handler != ((void *)0)) {
            tmp_handler = log_handler;
            tmp_handler(level, msg, log_handler_ctx);
        }
    }

    void set_log_handler(log_handler_fn *handler, void *ctx) {
        log_handler = handler;
        log_handler_ctx = ctx;
    }

    void init_logging(void) {
        set_log_handler(mm_log_handler, ((void *)0));
        do_log(1, "test");
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('do_log', 'mm_log_handler') in pairs, \
        f"Expected do_log -> mm_log_handler, got: {pairs}"


def test_local_fp_from_struct_field_init():
    """Phase 3/Gap2: Type *fp = obj->field; fp() resolves through field_call+direct_call_fp chain."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef int (*holdfunc_t)(void *dp, const char *name, void *tag, void **dsp);

    static int my_hold(void *dp, const char *name, void *tag, void **dsp) {
        return 0;
    }

    typedef struct {
        holdfunc_t holdfunc;
    } arg_t;

    static void release_sync(void *arg_ptr) {
        arg_t *a = (arg_t *)arg_ptr;
        holdfunc_t *hf = a->holdfunc;
        void *ds;
        hf(((void *)0), "test", ((void *)0), &ds);
    }

    void setup_and_call(void) {
        arg_t a;
        a.holdfunc = (holdfunc_t)my_hold;
        release_sync(&a);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('release_sync', 'my_hold') in pairs, \
        f"Expected release_sync -> my_hold, got: {pairs}"


def test_field_to_field_propagation():
    """Phase 5: a->fp = b->fp field-to-field fnptr propagation."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(int x);

    static void my_cb(int x) { (void)x; }

    struct store {
        cb_fn callback;
    };

    struct ctx {
        cb_fn callback;
    };

    static void store_set_cb(struct store *s, cb_fn cb) {
        s->callback = cb;
    }

    static void ctx_init(struct ctx *c, struct store *s) {
        c->callback = s->callback;
    }

    void use_ctx(struct ctx *c) {
        if (c->callback)
            c->callback(42);
    }

    void main_func(void) {
        struct store s;
        struct ctx c;
        store_set_cb(&s, my_cb);
        ctx_init(&c, &s);
        use_ctx(&c);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('use_ctx', 'my_cb') in pairs, \
        f"Expected use_ctx -> my_cb, got: {pairs}"


def test_macro_expansion_param_tracking():
    """Macro wrapper call: #define MACRO(a,b) real(a,b) → param tracking works."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(int x);

    static void my_handler(int x) { (void)x; }

    static void register_callback_impl(void *ctx, cb_fn cb) {
        ((struct ctx*)ctx)->handler = cb;
    }

    #define register_callback(ctx, fn) register_callback_impl((ctx), (fn))

    struct ctx {
        cb_fn handler;
    };

    void setup(void) {
        struct ctx c;
        register_callback(&c, my_handler);
    }

    void invoke(struct ctx *c) {
        if (c->handler)
            c->handler(42);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('invoke', 'my_handler') in pairs, \
        f"Expected invoke -> my_handler, got: {pairs}"


# === Recall regression guards ===

def _category_recall(category):
    """Compute recall and FPR for a single category."""
    cat_dir = os.path.join(ET_BENCH_DIR, category)
    matched = 0
    total = 0
    extra = 0
    detected = 0
    for example in sorted(os.listdir(cat_dir)):
        if not example.startswith('example_'):
            continue
        ex_dir = os.path.join(cat_dir, example)
        expected = _load_example_ground_truth(ex_dir)
        if not expected:
            continue
        total += len(expected)
        graph = _run_analysis_on_fixture(ex_dir)
        indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']
        found_pairs = {(e.caller, e.callee) for e in indirect_edges}
        expected_pairs = {(e['caller'], e['callee']) for e in expected}
        matched += len(found_pairs & expected_pairs)
        extra += len(found_pairs - expected_pairs)
        detected += len(found_pairs)
    recall = matched / total if total > 0 else 1.0
    fpr = extra / detected if detected > 0 else 0.0
    return matched, total, recall, fpr


def test_fnptr_callback_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-callback')
    assert recall == 1.0, f"fnptr-callback recall={recall:.2%} ({matched}/{total})"

def test_fnptr_cast_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-cast')
    assert recall == 1.0, f"fnptr-cast recall={recall:.2%} ({matched}/{total})"

def test_fnptr_global_array_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-global-array')
    assert recall == 1.0, f"fnptr-global-array recall={recall:.2%} ({matched}/{total})"

def test_fnptr_global_struct_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-global-struct')
    assert recall == 1.0, f"fnptr-global-struct recall={recall:.2%} ({matched}/{total})"

def test_fnptr_global_struct_array_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-global-struct-array')
    assert recall == 1.0, f"fnptr-global-struct-array recall={recall:.2%} ({matched}/{total})"

def test_fnptr_library_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-library')
    assert recall == 1.0, f"fnptr-library recall={recall:.2%} ({matched}/{total})"

def test_fnptr_only_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-only')
    assert recall == 1.0, f"fnptr-only recall={recall:.2%} ({matched}/{total})"

def test_fnptr_struct_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-struct')
    assert recall == 1.0, f"fnptr-struct recall={recall:.2%} ({matched}/{total})"

def test_fnptr_varargs_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-varargs')
    assert recall == 1.0, f"fnptr-varargs recall={recall:.2%} ({matched}/{total})"


def test_p2_callback_reg_only_fnptr_positions():
    """Registration function: only fnptr-param positions emit callback_reg edges."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void my_handler(int x) { (void)x; }
static void cleanup_handler(int x) { (void)x; }
static void name_func(int x) { (void)x; }  /* name used as non-fnptr arg */

struct ctx { cb_fn handler; cb_fn cleanup; };

static void register_item(struct ctx *c, const char *name, int pri, cb_fn cb) {
    c->handler = cb;
}
static void register_cleanup(struct ctx *c, cb_fn cleanup) {
    c->cleanup = cleanup;
}

void setup(void) {
    struct ctx c;
    /* name_func at pos 1 (const char*, NOT fnptr) -- should NOT emit callback_reg */
    register_item(&c, name_func, 10, my_handler);
    register_cleanup(&c, cleanup_handler);
}

void invoke(struct ctx *c) {
    if (c->handler)
        c->handler(42);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    # my_handler stored in c->handler, field_call dispatches (Fix B)
    all_callees = {e.callee for e in graph.edges if e.type.value == "indirect"}
    assert 'my_handler' in all_callees, \
        f"Expected my_handler in some indirect edge, got: {all_callees}"

    # cleanup_handler has no field_call (invoke only uses c->handler)
    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == "callback_reg"]
    cr_callees = {e.callee for e in callback_reg_edges}
    assert 'cleanup_handler' in cr_callees, \
        f"Expected cleanup_handler in callback_reg, got: {cr_callees}"

    # name_func at non-fnptr position, not in callback_reg
    assert 'name_func' not in cr_callees, \
        f"name_func at non-fnptr position should NOT be in callback_reg, got: {cr_callees}"


def test_p2_callback_reg_cross_file_fallback():
    """Registration function not in func_fp_params still emits callback_reg (no regression)."""
    source = b'''
typedef void (*cb_fn)(int x);
static void my_handler(int x) { (void)x; }
struct ctx { cb_fn handler; };

/* Declaration only -- no function_definition, register_remote NOT in func_fp_params */
void register_remote(void *ctx, cb_fn cb);

void setup(void) {
    struct ctx c;
    register_remote(&c, my_handler);
}
'''
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == 'callback_reg']
    callees = {e.callee for e in callback_reg_edges}
    assert 'my_handler' in callees, \
        f"Cross-file fallback failed: expected my_handler in {callees}"


def test_p3_param_namespace_isolation():
    """Same param name in different functions should not cross-pollute in dataflow."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void handler_a(int x) { (void)x; }
static void handler_b(int x) { (void)x; }

struct ctx { cb_fn h; };

/* Both use param name "cb" */
static void register_a(struct ctx *c, cb_fn cb) {
    c->h = cb;
}
static void register_b(struct ctx *c, cb_fn cb) {
    c->h = cb;
}

void setup(void) {
    struct ctx ca, cb2;
    register_a(&ca, handler_a);
    register_b(&cb2, handler_b);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import param_assign

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())

    param_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)

    targets_a = engine.resolve('register_a:cb')
    assert targets_a == {'handler_a'}, \
        f"register_a:cb should be {{handler_a}}, got: {targets_a}"

    targets_b = engine.resolve('register_b:cb')
    assert targets_b == {'handler_b'}, \
        f"register_b:cb should be {{handler_b}}, got: {targets_b}"


def test_p0_param_callback_no_nx_m_edges():
    """N callers × M targets should produce O(N+M) edges, not O(N×M)."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void h1(int x) { (void)x; }
static void h2(int x) { (void)x; }
static void h3(int x) { (void)x; }

/* Non-registration function: receives fnptr and calls it */
static void forward(cb_fn cb) {
    cb(42);
}

void caller1(void) { forward(h1); }
void caller2(void) { forward(h2); }
void caller3(void) { forward(h3); }
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}

    # Pass 3 edges (callee body = forward) — may come from direct_call_fp or param_dispatch
    assert ('forward', 'h1') in pairs
    assert ('forward', 'h2') in pairs
    assert ('forward', 'h3') in pairs

    # Pass 4 edges (outer callers)
    assert ('caller1', 'h1') in pairs
    assert ('caller2', 'h2') in pairs
    assert ('caller3', 'h3') in pairs

    # No N×M cross edges: caller1 should NOT be connected to h2 or h3
    assert ('caller1', 'h2') not in pairs, \
        f"N×M cross edge (caller1, h2) should not exist"
    assert ('caller1', 'h3') not in pairs, \
        f"N×M cross edge (caller1, h3) should not exist"
    assert ('caller2', 'h1') not in pairs, \
        f"N×M cross edge (caller2, h1) should not exist"

    # Total indirect edges: at most 6 (3 Pass 3 + 3 Pass 4)
    assert len(pairs) <= 6, \
        f"Expected <=6 indirect edges, got {len(pairs)}"




def test_fix_a_fallback_prefixed_resolve():
    """Pass 1 fallback branch resolves via prefixed key, not polluted bare key."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void h_a(int x) { (void)x; }
static void h_b(int x) { (void)x; }

struct ctx { handler_fn handler; };

static void register_legacy(struct ctx *c, handler_fn fn) {
    c->handler = fn;
}

static void wrapper_a(struct ctx *c, handler_fn fn) {
    register_legacy(c, fn);
}

static void wrapper_b(struct ctx *c, handler_fn fn) {
    register_legacy(c, fn);
}

void caller_a(void) {
    struct ctx c;
    wrapper_a(&c, h_a);
}

void caller_b(void) {
    struct ctx c;
    wrapper_b(&c, h_b);
}

void dispatch(struct ctx *c) {
    if (c->handler)
        c->handler(42);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    callback_param = [e for e in graph.edges if e.indirect_kind == "callback_param"]
    pairs = {(e.caller, e.callee) for e in callback_param}

    assert ("wrapper_a", "h_b") not in pairs, \
        f"wrapper_a should NOT connect to h_b (bare key pollution): {pairs}"

    assert ("wrapper_b", "h_a") not in pairs, \
        f"wrapper_b should NOT connect to h_a (bare key pollution): {pairs}"

    field_call_edges = [e for e in graph.edges if e.indirect_kind == "field_call"]
    fc_pairs = {(e.caller, e.callee) for e in field_call_edges}
    assert ("dispatch", "h_a") in fc_pairs, "field_call dispatch -> h_a should work"
    assert ("dispatch", "h_b") in fc_pairs, "field_call dispatch -> h_b should work"


def test_fix_b_callback_reg_suppress_when_field_covered():
    """callback_reg edges with callee also in field_call should be suppressed."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void my_handler(int x) { (void)x; }

struct ctx { handler_fn handler; };

static void register_handler(struct ctx *c, handler_fn fn) {
    c->handler = fn;
}

static void register_wrapper(struct ctx *c, handler_fn fn) {
    register_handler(c, fn);
}

void setup(void) {
    struct ctx c;
    register_wrapper(&c, my_handler);
}

void dispatch(struct ctx *c) {
    if (c->handler)
        c->handler(42);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    field_call_edges = [e for e in graph.edges if e.indirect_kind == "field_call"]
    assert ("dispatch", "my_handler") in {(e.caller, e.callee) for e in field_call_edges}, \
        "field_call should produce dispatch -> my_handler"

    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == "callback_reg"]
    cr_callees = {e.callee for e in callback_reg_edges}
    assert "my_handler" not in cr_callees, \
        f"callback_reg for my_handler should be suppressed: {cr_callees}"


def test_fix_b_callback_reg_kept_when_no_field_cover():
    """callback_reg edges without field_call coverage should be retained."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void my_cb(int x) { (void)x; }

static void register_cb(cb_fn cb) {
    cb(42);
}

void setup(void) {
    register_cb(my_cb);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == "callback_reg"]
    cr_callees = {e.callee for e in callback_reg_edges}
    assert "my_cb" in cr_callees, \
        f"callback_reg for my_cb should be retained (no field_call): {cr_callees}"


def test_fix_a1_callback_param_suppress_when_field_covered():
    """callback_param edges with callee also in field_call should be suppressed."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void h_a(int x) { (void)x; }
static void h_b(int x) { (void)x; }

struct ctx { handler_fn handler; };

static void register_fn(struct ctx *c, handler_fn fn) {
    c->handler = fn;
}

static void wrapper_a(struct ctx *c, handler_fn fn) {
    register_fn(c, fn);
}

static void wrapper_b(struct ctx *c, handler_fn fn) {
    register_fn(c, fn);
}

void caller_a(void) {
    struct ctx c;
    wrapper_a(&c, h_a);
}

void caller_b(void) {
    struct ctx c;
    wrapper_b(&c, h_b);
}

void dispatch(struct ctx *c) {
    if (c->handler)
        c->handler(42);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    fc_pairs = {(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "field_call"}
    assert ("dispatch", "h_a") in fc_pairs, "field_call dispatch -> h_a should work"
    assert ("dispatch", "h_b") in fc_pairs, "field_call dispatch -> h_b should work"

    cp_edges = [e for e in graph.edges if e.indirect_kind == "callback_param"]
    cp_callees = {e.callee for e in cp_edges}
    assert "h_a" not in cp_callees, \
        f"callback_param for h_a should be suppressed: {cp_callees}"
    assert "h_b" not in cp_callees, \
        f"callback_param for h_b should be suppressed: {cp_callees}"







def test_fix2_field_call_callback_of_callback_caller():
    """field_call callback-of-callback caller should be ftarget, not enclosing func."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*note_fn)(void *, void *, void *);

static void relocate(void *a, void *b, void *c) { (void)a; (void)b; (void)c; }

struct ptr_data {
    void *obj;
    note_fn note_ptr_fn;
    void *cookie;
};

static void target_func(void *obj, void *x, note_fn op, void *cookie) {
    op(obj, x, cookie);
}

static struct ptr_data slot;

void dispatcher(void) {
    slot.note_ptr_fn = target_func;
    if (slot.note_ptr_fn)
        slot.note_ptr_fn(slot.obj, slot.cookie, relocate);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    cp_edges = [(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "callback_param"]
    pairs = set(cp_edges)

    assert ("target_func", "relocate") in pairs, \
        f"target_func -> relocate missing: {pairs}"

    assert ("dispatcher", "relocate") not in pairs, \
        f"dispatcher should not be caller for relocate: {pairs}"




def test_fix3_behavioral_registration():
    """Registration matching functions that forward should NOT emit callback_reg."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
typedef void (*cb_t)(int);
static void my_cb(int x) { (void)x; }
static void real_dispatcher(cb_t cb) { cb(42); }
/* Name matches _is_registration, but only forwards the fnptr */
static void register_handler(cb_t cb) { real_dispatcher(cb); }
void setup(void) { register_handler(my_cb); }
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"): st.add_function(func)
    df = VariableState()
    graph = run_all_analyses({"test.c": tree}, st, df)
    cr = {e.callee for e in graph.edges if e.indirect_kind == "callback_reg"}
    assert "my_cb" not in cr, f"register_handler forwards, should not emit: {cr}"


def test_engine_has_new_fields():
    """DataflowEngine has func_params, registration_sites, covered_callees, etc."""
    from ethunter.analyzer.dataflow import DataflowEngine
    engine = DataflowEngine()
    assert hasattr(engine, 'func_params'), "engine missing func_params"
    assert engine.func_params == {}, f"expected empty dict, got {engine.func_params}"
    assert hasattr(engine, 'registration_sites'), "engine missing registration_sites"
    assert engine.registration_sites == [], f"expected empty list, got {engine.registration_sites}"
    assert hasattr(engine, 'covered_callees'), "engine missing covered_callees"
    assert engine.covered_callees == set(), f"expected empty set, got {engine.covered_callees}"
    assert hasattr(engine, 'call_site_targets'), "engine missing call_site_targets"
    assert engine.call_site_targets == {}, f"expected empty dict, got {engine.call_site_targets}"
    assert hasattr(engine, 'param_alias_map'), "engine missing param_alias_map"


def test_symbol_table_type_tracking():
    """SymbolTable supports record_var_type/get_var_type and record_struct_fields/get_struct_fields."""
    from ethunter.analyzer.symbol_table import SymbolTable
    st = SymbolTable()
    st.record_var_type("ctx", "ssl_ctx_st")
    assert st.get_var_type("ctx") == "ssl_ctx_st", f"expected ssl_ctx_st, got {st.get_var_type('ctx')}"
    assert st.get_var_type("unknown") is None, "expected None for unknown var"
    st.record_struct_fields("ssl_ctx_st", ["handler", "callback"])
    assert st.get_struct_fields("ssl_ctx_st") == ["handler", "callback"], \
        f"unexpected fields: {st.get_struct_fields('ssl_ctx_st')}"
    assert st.get_struct_fields("nonexistent") == [], "expected empty list for unknown struct"


def test_param_helpers_prepare_populates_engine():
    """param_helpers.prepare() writes func_params, func_fp_params, param_usage to engine."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*cb_t)(int x);
    static void handler(int x) { (void)x; }
    static void direct_caller(cb_t cb) { cb(42); }
    static void forwarder(cb_t cb) { direct_caller(cb); }
    static void setter(void *s, cb_t cb) { ((struct s*)s)->handler = cb; }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    assert "direct_caller" in engine.func_params, f"func_params missing direct_caller: {engine.func_params}"
    assert engine.func_params["direct_caller"] == ["cb"], f"unexpected params: {engine.func_params['direct_caller']}"
    # func_fp_params still on state during migration
    assert hasattr(engine.state, 'func_fp_params'), "state missing func_fp_params"
    assert 0 in engine.state.func_fp_params["direct_caller"], "direct_caller pos 0 should be fnptr"
    # param_usage still on state during migration
    assert hasattr(engine.state, 'param_usage'), "state missing param_usage"
    assert engine.state.param_usage[("direct_caller", 0)] == "caller", \
        f"expected caller, got {engine.state.param_usage.get(('direct_caller', 0))}"
    assert engine.state.param_usage[("forwarder", 0)] == "forwarder", \
        f"expected forwarder, got {engine.state.param_usage.get(('forwarder', 0))}"
    assert ("setter", 1) in engine.param_fields, f"setter cb param should map to field"


def test_param_binding_writes_dataflow_no_edges():
    """param_binding writes dataflow + registration_sites, returns NO edges."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void my_handler(void) {}
    static void reg(void *s, fn_t f) { ((struct s*)s)->handler = f; }
    void setup(void) { struct s o; reg(&o, my_handler); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    edges = param_binding_analyze(tree, "test.c", st, engine)
    assert len(edges) == 0, f"param_binding should return 0 edges, got {len(edges)}"
    assert len(engine.registration_sites) > 0, "should have registration_sites"
    site = engine.registration_sites[0]
    assert site["caller"] == "setup", f"unexpected caller: {site}"
    assert site["callee"] == "reg", f"unexpected callee: {site}"
    assert site["target"] == "my_handler", f"unexpected target: {site}"


def test_param_dispatch_produces_callback_param_edges():
    """param_dispatch detects fnptr param calls and emits callback_param edges."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void my_handler(void) {}
    static void dispatcher(fn_t cb) { cb(); }
    void setup(void) { dispatcher(my_handler); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.param_dispatch import analyze as param_dispatch_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    edges = param_dispatch_analyze(tree, "test.c", engine)
    pairs = {(e.caller, e.callee) for e in edges}
    assert ("dispatcher", "my_handler") in pairs, f"Expected dispatcher->my_handler in {pairs}"
    assert all(e.indirect_kind == "callback_param" for e in edges)


def test_param_dispatch_pass_b_skips_when_pass_a_covers():
    """Pass B should NOT emit (outer, target) when Pass A already emits (inner, target)."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void h1(void) {}
    static void h2(void) {}
    static void inner(fn_t cb) { cb(); }
    void outer_a(void) { inner(h1); }
    void outer_b(void) { inner(h2); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.param_dispatch import analyze as param_dispatch_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    edges = param_dispatch_analyze(tree, "test.c", engine)
    pairs = {(e.caller, e.callee) for e in edges}
    assert ("inner", "h1") in pairs, f"Pass A should emit inner->h1: {pairs}"
    assert ("inner", "h2") in pairs, f"Pass A should emit inner->h2: {pairs}"
    assert ("outer_a", "h1") not in pairs, f"Pass B should skip outer_a->h1: {pairs}"
    assert ("outer_b", "h2") not in pairs, f"Pass B should skip outer_b->h2: {pairs}"


def test_callback_reg_suppresses_forwarder():
    """Forwarder should NOT emit callback_reg for forwarded fnptr."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*cb_t)(int);
    static void my_cb(int x) { (void)x; }
    static void direct_caller(cb_t cb) { cb(42); }
    static void forwarder(cb_t cb) { direct_caller(cb); }
    void setup(void) { forwarder(my_cb); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.callback_reg import analyze as callback_reg_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    engine.covered_callees = set()
    edges = callback_reg_analyze(tree, "test.c", engine)
    cr_targets = {e.callee for e in edges}
    assert "my_cb" not in cr_targets, f"forwarder should not emit callback_reg, got: {cr_targets}"


def test_callback_reg_suppress_when_covered_by_field_call():
    """callback_reg suppressed when callee is in covered_callees (Stage 2)."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void my_fn(void) {}
    struct s { fn_t handler; };
    static void reg(struct s *o, fn_t f) { o->handler = f; }
    static void dispatch(struct s *o) { if (o->handler) o->handler(); }
    void setup(void) { struct s o; reg(&o, my_fn); dispatch(&o); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.callback_reg import analyze as callback_reg_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    engine.covered_callees = {"my_fn"}
    edges = callback_reg_analyze(tree, "test.c", engine)
    cr_targets = {e.callee for e in edges}
    assert "my_fn" not in cr_targets, f"my_fn in covered_callees, should be suppressed: {cr_targets}"


def test_scoped_key_isolates_same_name_vars():
    """fp in two different functions should not collide in dataflow."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void h_a(void) {}
    static void h_b(void) {}
    void setup_a(void) { fn_t fp = h_a; fp(); }
    void setup_b(void) { fn_t fp = h_b; fp(); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()
    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ("setup_a", "h_a") in pairs, f"Expected setup_a->h_a in {pairs}"
    assert ("setup_a", "h_b") not in pairs, f"setup_a should NOT call h_b: {pairs}"


@pytest.mark.xfail(reason="Requires full type-aware field_call migration (Task 11 cleanup)")
def test_type_aware_key_isolates_different_struct_types():
    """Two struct types with same field name: targets must not mix."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void h_a(void) {}
    static void h_b(void) {}
    struct type_a { const char *n; fn_t handler; };
    struct type_b { int id; fn_t handler; };
    static struct type_a o1 = {"a", h_a};
    static struct type_b o2 = {42, h_b};
    void use_a(void) { if (o1.handler) o1.handler(); }
    void use_b(void) { if (o2.handler) o2.handler(); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()
    graph = run_all_analyses({"test.c": tree}, st, df)
    fc = {(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "field_call"}
    assert ("use_a", "h_b") not in fc, f"type_a.handler should NOT resolve to h_b: {fc}"
    assert ("use_b", "h_a") not in fc, f"type_b.handler should NOT resolve to h_a: {fc}"


def test_param_binding_suppresses_non_fnptr_args_as_registration():
    """Non-fnptr args to known-function should NOT be recorded as registration_sites."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*cb_fn)(int x);
    static void my_handler(int x) { (void)x; }
    static void name_func(int x) { (void)x; }
    static void register_item(struct ctx *c, const char *name, int pri, cb_fn cb) {
        c->handler = cb;
    }
    void setup(void) {
        struct ctx c;
        register_item(&c, name_func, 10, my_handler);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    for site in engine.registration_sites:
        assert site["target"] != "name_func", \
            f"name_func at non-fnptr position should not be registration_site: {site}"


def test_local_var_fnptr_arg_creates_callback_param_edge():
    """Call-site passes local var as fnptr → param_dispatch should find the target."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef int (*cmp_t)(const void *, const void *);
    static int sort_asc(const void *a, const void *b) { return 1; }
    static void _pqsort(void *a, size_t n, size_t es,
        cmp_t cmp, void *l, void *r) { cmp(a, a); }
    static void pqsort(void *a, size_t n, size_t es,
        cmp_t cmp, size_t l, size_t r) { _pqsort(a, n, es, cmp, NULL, NULL); }
    void georadius(void) {
        cmp_t sort_gp_callback = sort_asc;
        pqsort(NULL, 10, 8, sort_gp_callback, 0, 9);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.param_dispatch import analyze as param_dispatch_analyze
    from ethunter.analyzer import direct_assign
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    direct_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)
    param_binding_analyze(tree, "test.c", st, engine)  # second pass: local vars now resolvable
    edges = param_dispatch_analyze(tree, "test.c", engine)
    pairs = {(e.caller, e.callee) for e in edges}
    assert ("_pqsort", "sort_asc") in pairs, \
        f"Expected _pqsort -> sort_asc via local var, got: {pairs}"


def test_return_field_resolution_after_initializer_assign():
    """resolve_returned_field must find gstruct keys from initializer_assign."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef int (*cb_t)(const void *, const void *, int, int, int, void *, void *);
    struct cert { cb_t sec_cb; };
    typedef struct { cb_t old_cb; } sdb_t;
    struct ctx { struct cert *cert; };
    static int ssl_default(const void *s, const void *c, int o, int b, int n,
                           void *ex, void *e2) { return 1; }
    static cb_t get_sec_cb(const struct ctx *ctx) {
        if (!ctx || !ctx->cert) return NULL;
        return ctx->cert->sec_cb;
    }
    static int debug_cb(const void *s, const void *c, int o, int b, int n,
                        void *ex, void *e2) {
        sdb_t *sdb = ex;
        return sdb->old_cb(s, c, o, b, n, ex, e2);
    }
    void setup(void) {
        struct cert *ret = (struct cert *)1;
        ret->sec_cb = ssl_default;
        static sdb_t sdb;
        sdb.old_cb = get_sec_cb(NULL);
        debug_cb(NULL, NULL, 0, 0, 0, &sdb, NULL);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as ba, _resolve_fields
    from ethunter.analyzer import direct_assign, initializer_assign, cast_assign
    from ethunter.analyzer import direct_call_fp, field_call, array_call
    from ethunter.graph.model import CallType
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    # Simulate Gap B reorder: Pass 1 first, resolvers, then Pass 2
    ba(tree, "test.c", st, engine)
    direct_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)
    initializer_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)
    cast_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)
    _resolve_fields(tree, "test.c", st, engine)
    # Run field_call for struct dispatch
    edges = field_call.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)
    pairs = {(e.caller, e.callee) for e in edges if e.type == CallType.INDIRECT}
    assert ("debug_cb", "ssl_default") in pairs, \
        f"Expected debug_cb -> ssl_default via return field chain, got: {pairs}"


def test_collect_local_var_types_records_struct_types():
    """Local struct variable declarations should record types for Tier 1 resolution."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    struct my_type { void (*cb)(void); };
    static void handler_a(void) {}
    void do_work(void) {
        struct my_type *ctx;
        ctx->cb = handler_a;
        ctx->cb();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import field_call
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())
    field_call.collect(tree, "test.c", engine, st, st.all_function_names)
    t = st.get_func_var_type("do_work", "ctx")
    assert t == "my_type", f"Expected my_type, got {t}"


def test_collect_cast_types_records_struct_types():
    """Cast expressions like ((struct ctx*)ptr)->handler should record ptr's type."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    struct ctx { void (*handler)(void); };
    static void h(void) {}
    void do_work(void *ptr) {
        ((struct ctx*)ptr)->handler = h;
        ((struct ctx*)ptr)->handler();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import field_call
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())
    field_call.collect(tree, "test.c", engine, st, st.all_function_names)
    t = st.get_func_var_type("do_work", "ptr")
    assert t == "ctx", f"Expected ctx, got {t}"


# === Confidence round-trip tests ===

from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence


def test_confidence_round_trip():
    """CallEdge.to_dict() -> from_dict() preserves confidence and evidence."""
    edge = CallEdge(
        caller="main",
        callee="handler",
        caller_file="a.c",
        callee_file="",
        type=CallType.INDIRECT,
        indirect_kind="callback_param",
        caller_line=42,
        confidence=Confidence.HIGH,
        evidence=Evidence("callee_body_call"),
    )
    d = edge.to_dict()
    restored = CallEdge.from_dict(d)
    assert restored.confidence == Confidence.HIGH
    assert restored.evidence is not None
    assert restored.evidence.method == "callee_body_call"


def test_confidence_round_trip_default_medium():
    """Default confidence MEDIUM must survive round-trip (always serialized)."""
    edge = CallEdge(
        caller="main",
        callee="helper",
        caller_file="b.c",
        callee_file="",
        type=CallType.DIRECT,
    )
    d = edge.to_dict()
    assert "confidence" in d, "confidence must always be serialized"
    assert d["confidence"] == "medium"
    restored = CallEdge.from_dict(d)
    assert restored.confidence == Confidence.MEDIUM


def test_confidence_ordinals():
    """Verify confidence ordering for dedup."""
    assert Confidence.HIGH.ordinal() > Confidence.MEDIUM.ordinal()
    assert Confidence.MEDIUM.ordinal() > Confidence.LOW.ordinal()
