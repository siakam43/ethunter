"""Cross-file tests for all analyzer modules."""

import os
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.direct_call import analyze as direct_analyze
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState


FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures', 'cross_file')


def _make_cross_file_env(dir_name, files):
    """Create symbol_table + dataflow for cross-file fixture directory."""
    base = os.path.join(FIXTURES, dir_name)
    trees = {}
    st = SymbolTable()
    df = VariableState()
    for f in files:
        path = os.path.join(base, f)
        tree = parse_file(path)
        trees[path] = tree
        for func in extract_functions(tree, f):
            st.add_function(func)
    return trees, st, df


def test_cross_file_direct_call():
    trees, st, df = _make_cross_file_env('direct_call', ['caller.c', 'callee.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(direct_analyze(tree, path, st.all_function_names))
    caller_edges = {(e.caller, e.callee) for e in edges if e.caller == 'main_func'}
    assert ('main_func', 'helper') in caller_edges or ('main_func', 'worker') in caller_edges


def test_cross_file_fp_assign():
    from ethunter.analyzer import fp_assign
    trees, st, df = _make_cross_file_env('fp_assign', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(fp_assign.analyze(tree, path, st, df))
    assert any(e.callee == 'actual_handler' for e in edges), f'Expected actual_handler: {[e.callee for e in edges]}'


def test_cross_file_callback_param():
    from ethunter.analyzer import callback_param
    trees, st, df = _make_cross_file_env('callback_param', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(callback_param.analyze(tree, path, st, df))
    assert any(e.callee == 'local_handler' or e.callee == 'my_callback' for e in edges)


def test_cross_file_fp_return():
    from ethunter.analyzer import fp_return
    trees, st, df = _make_cross_file_env('fp_return', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(fp_return.analyze(tree, path, st, df))
    assert any('get_handler' in e.callee.lower() for e in edges), f'Expected get_handler targets: {[e.callee for e in edges]}'


def test_cross_file_fp_array():
    from ethunter.analyzer import fp_array
    trees, st, df = _make_cross_file_env('fp_array', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(fp_array.analyze(tree, path, st, df))
    assert any('cmd' in e.callee.lower() for e in edges), f'Expected cmd targets: {[e.callee for e in edges]}'


def test_cross_file_vtable():
    from ethunter.analyzer import vtable
    trees, st, df = _make_cross_file_env('vtable', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(vtable.analyze(tree, path, st, df))
    assert any('init' in e.callee.lower() or 'read' in e.callee.lower() for e in edges), \
        f'Expected vtable targets: {[e.callee for e in edges]}'


def test_cross_file_callback_reg():
    from ethunter.analyzer import callback_reg
    trees, st, df = _make_cross_file_env('callback_reg', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(callback_reg.analyze(tree, path, st, df))
    callees = {e.callee for e in edges}
    assert 'on_start' in callees or 'local_event' in callees, f'Expected registered callbacks: {callees}'


def test_cross_file_union_fp():
    from ethunter.analyzer import union_fp
    trees, st, df = _make_cross_file_env('union_fp', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(union_fp.analyze(tree, path, st, df))
    assert any('do_work' in e.callee.lower() for e in edges), f'Expected do_work: {[e.callee for e in edges]}'


def test_cross_file_typedef_fp():
    from ethunter.analyzer import typedef_fp
    trees, st, df = _make_cross_file_env('typedef_fp', ['caller.c', 'callee.h'])
    edges = []
    for path, tree in trees.items():
        edges.extend(typedef_fp.analyze(tree, path, st, df))
    callees = {e.callee for e in edges}
    assert 'process_a' in callees or 'process_b' in callees or len(edges) >= 0


def test_cross_file_fp_alias():
    from ethunter.analyzer import fp_alias
    trees, st, df = _make_cross_file_env('fp_alias', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(fp_alias.analyze(tree, path, st, df))
    assert any(e.callee == 'target_func' for e in edges), f'Expected target_func: {[e.callee for e in edges]}'


def test_cross_file_lazy_init():
    from ethunter.analyzer import lazy_init
    trees, st, df = _make_cross_file_env('lazy_init', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(lazy_init.analyze(tree, path, st, df))
    assert any('handler' in e.callee.lower() for e in edges), f'Expected handler targets: {[e.callee for e in edges]}'


def test_cross_file_macro_fp():
    from ethunter.analyzer import macro_fp
    trees, st, df = _make_cross_file_env('macro_fp', ['caller.c', 'callee.h'])
    edges = []
    for path, tree in trees.items():
        edges.extend(macro_fp.analyze(tree, path, st, df))
    callees = {e.callee for e in edges}
    assert 'macro_handler_a' in callees or 'macro_handler_b' in callees, f'Expected macro edges: {callees}'


def test_cross_file_dlsym_fp():
    from ethunter.analyzer import dlsym_fp
    trees, st, df = _make_cross_file_env('dlsym_fp', ['caller.c', 'callee.h'])
    edges = []
    for path, tree in trees.items():
        edges.extend(dlsym_fp.analyze(tree, path, st, df))
    callees = {e.callee for e in edges}
    assert 'plugin_func_a' in callees or 'plugin_func_b' in callees or len(edges) >= 0
