"""Cross-file tests for all analyzer modules."""

import os
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.direct_call import analyze as direct_analyze
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import DataflowEngine


FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures', 'cross_file')


def _make_cross_file_env(dir_name, files):
    """Create symbol_table + dataflow for cross-file fixture directory."""
    base = os.path.join(FIXTURES, dir_name)
    trees = {}
    st = SymbolTable()
    df = DataflowEngine()
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


def test_cross_file_direct_assign():
    from ethunter.analyzer import direct_assign, direct_call_fp
    trees, st, df = _make_cross_file_env('fp_assign', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        direct_assign.analyze(tree, path, st, df)
        edges.extend(direct_call_fp.analyze(tree, path, st, df))
    assert any(e.callee == 'actual_handler' for e in edges)


def test_cross_file_param_assign():
    from ethunter.analyzer import param_helpers, param_binding, callback_reg
    trees, st, df = _make_cross_file_env('callback_reg', ['callee.c', 'caller.c'])
    for path, tree in trees.items():
        param_helpers.prepare(tree, path, df, st)
    for path, tree in trees.items():
        param_binding.analyze(tree, path, st, df)
    df.covered_callees = set()
    edges = []
    for path, tree in trees.items():
        edges.extend(callback_reg.analyze(tree, path, df))
    assert any(e.callee for e in edges)


def test_cross_file_initializer_assign():
    from ethunter.analyzer import initializer_assign, array_call
    trees, st, df = _make_cross_file_env('fp_array', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        initializer_assign.analyze(tree, path, st, df)
        edges.extend(array_call.analyze(tree, path, st, df))
    assert any('cmd' in e.callee.lower() for e in edges)


def test_cross_file_field_call():
    from ethunter.analyzer import initializer_assign, field_call
    trees, st, df = _make_cross_file_env('vtable', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        initializer_assign.analyze(tree, path, st, df)
        field_call.collect(tree, path, df, st, st.all_function_names)
        edges.extend(field_call.analyze(tree, path, st, df))
    assert any('init' in e.callee.lower() or 'read' in e.callee.lower() for e in edges)


def test_cross_file_dlsym_fp():
    from ethunter.analyzer import dlsym_fp
    trees, st, df = _make_cross_file_env('dlsym_fp', ['caller.c', 'callee.h'])
    edges = []
    for path, tree in trees.items():
        edges.extend(dlsym_fp.analyze(tree, path, st, df))
    callees = {e.callee for e in edges}
    assert 'plugin_func_a' in callees or 'plugin_func_b' in callees
