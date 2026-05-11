"""Tests for all analyzer modules (new architecture)."""

import os
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.direct_call import analyze as direct_analyze
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState

FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures')


def _make_analyzer_env(fixture_name):
    """Create symbol_table + dataflow for a fixture file."""
    path = os.path.join(FIXTURES, fixture_name)
    tree = parse_file(path)
    st = SymbolTable()
    for func in extract_functions(tree, fixture_name):
        st.add_function(func)
    df = VariableState()
    return tree, st, df


# === Core tests ===

def test_direct_call_simple():
    tree, st, _ = _make_analyzer_env('direct_call.c')
    edges = direct_analyze(tree, 'direct_call.c', st.all_function_names)
    edge_pairs = {(e.caller, e.callee) for e in edges}
    assert ('worker', 'helper') in edge_pairs
    assert ('main', 'worker') in edge_pairs
    assert ('main', 'helper') in edge_pairs


def test_direct_call_complex():
    tree, st, _ = _make_analyzer_env('direct_call_complex.c')
    edges = direct_analyze(tree, 'direct_call_complex.c', st.all_function_names)
    callees_of_top = {e.callee for e in edges if e.caller == 'top'}
    assert callees_of_top >= {'middle_two', 'leaf_a', 'leaf_b'}
    callees_of_middle_one = {e.callee for e in edges if e.caller == 'middle_one'}
    assert callees_of_middle_one >= {'leaf_a', 'leaf_b'}


# === Target Resolution tests ===

def test_direct_assign_simple():
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('fp_assign.c')
    direct_assign.analyze(tree, 'fp_assign.c', st, df)
    assert len(df.targets) > 0
    assert 'fp' in df.targets


def test_direct_assign_alias_chain():
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    direct_assign.analyze(tree, 'long_alias_chain.c', st, df)
    assert 'fp1' in df.targets
    assert 'target_func' in df.targets['fp1']
    assert 'fp4' in df.targets
    assert 'target_func' in df.targets.get('fp4', set())


def test_direct_assign_complex():
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('fp_assign_complex.c')
    direct_assign.analyze(tree, 'fp_assign_complex.c', st, df)
    assert len(df.targets) >= 2


def test_initializer_assign_simple():
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign.c')
    initializer_assign.analyze(tree, 'initializer_assign.c', st, df)
    assert any(k.startswith('<gstruct:') for k in df.targets)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert 'fs_init' in all_targets
    assert 'fs_read' in all_targets


def test_initializer_assign_complex():
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign_complex.c')
    initializer_assign.analyze(tree, 'initializer_assign_complex.c', st, df)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert len(all_targets) >= 4
    assert 'start_a' in all_targets and 'start_b' in all_targets


def test_cast_assign_simple():
    from ethunter.analyzer import cast_assign
    tree, st, df = _make_analyzer_env('cast_assign.c')
    cast_assign.analyze(tree, 'cast_assign.c', st, df)
    assert 'fp_update' in df.targets
    assert 'update_impl' in df.targets.get('fp_update', set())


def test_cast_assign_complex():
    from ethunter.analyzer import cast_assign
    tree, st, df = _make_analyzer_env('cast_assign_complex.c')
    cast_assign.analyze(tree, 'cast_assign_complex.c', st, df)
    assert 'g_init' in df.targets
    assert 'my_md5_init' in df.targets.get('g_init', set())


def test_param_assign_simple():
    from ethunter.analyzer import param_assign
    tree, st, df = _make_analyzer_env('param_assign.c')
    edges = param_assign.analyze(tree, 'param_assign.c', st, df)
    assert len(df.targets) > 0
    assert any(k.startswith('<struct:') for k in df.targets)


def test_param_assign_complex():
    from ethunter.analyzer import param_assign
    tree, st, df = _make_analyzer_env('param_assign_complex.c')
    edges = param_assign.analyze(tree, 'param_assign_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'on_start' in callees or 'on_stop' in callees


# === Call Detection tests ===

def test_direct_call_fp():
    from ethunter.analyzer import direct_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('fp_assign.c')
    direct_assign.analyze(tree, 'fp_assign.c', st, df)
    edges = direct_call_fp.analyze(tree, 'fp_assign.c', st, df)
    callees = {e.callee for e in edges}
    assert 'foo' in callees


def test_direct_call_fp_alias_chain():
    from ethunter.analyzer import direct_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    direct_assign.analyze(tree, 'long_alias_chain.c', st, df)
    edges = direct_call_fp.analyze(tree, 'long_alias_chain.c', st, df)
    callees = {e.callee for e in edges}
    assert 'target_func' in callees


def test_array_call():
    from ethunter.analyzer import initializer_assign, array_call
    tree, st, df = _make_analyzer_env('fp_array.c')
    initializer_assign.analyze(tree, 'fp_array.c', st, df)
    edges = array_call.analyze(tree, 'fp_array.c', st, df)
    callees = {e.callee for e in edges}
    assert 'cmd_help' in callees


def test_array_call_complex():
    from ethunter.analyzer import initializer_assign, array_call
    tree, st, df = _make_analyzer_env('fp_array_complex.c')
    initializer_assign.analyze(tree, 'fp_array_complex.c', st, df)
    edges = array_call.analyze(tree, 'fp_array_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert any('cmd' in c.lower() for c in callees)


def test_field_call_simple():
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call.c')
    initializer_assign.analyze(tree, 'field_call.c', st, df)
    edges = field_call.analyze(tree, 'field_call.c', st, df)
    callees = {e.callee for e in edges}
    assert 'fs_init' in callees
    assert 'fs_read' in callees


def test_field_call_subscript():
    """Test arr[i]->field() pattern — extract_field_path handles subscript."""
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call_subscript.c')
    initializer_assign.analyze(tree, 'field_call_subscript.c', st, df)
    edges = field_call.analyze(tree, 'field_call_subscript.c', st, df)
    callees = {e.callee for e in edges}
    assert 'handler_a' in callees
    assert 'handler_b' in callees


def test_initializer_assign_pointer_field():
    """Test vec->field = func pattern — runtime struct pointer field assignment."""
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('initializer_assign_pointer_field.c')
    initializer_assign.analyze(tree, 'initializer_assign_pointer_field.c', st, df)
    # Verify dataflow targets
    assert any('dispatch_table.process' in k for k in df.targets)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert 'handler_a' in all_targets
    assert 'handler_b' in all_targets
    # Verify field_call detects the indirect calls
    edges = field_call.analyze(tree, 'initializer_assign_pointer_field.c', st, df)
    callees = {e.callee for e in edges}
    assert 'handler_a' in callees
    assert 'handler_b' in callees
    assert 'cleanup_a' in callees
    assert 'cleanup_b' in callees


def test_field_call_chain():
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call_complex.c')
    initializer_assign.analyze(tree, 'field_call_complex.c', st, df)
    edges = field_call.analyze(tree, 'field_call_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'net_read' in callees
    assert 'net_write' in callees


def test_dlsym_fp():
    from ethunter.analyzer import dlsym_fp
    tree, st, df = _make_analyzer_env('dlsym_fp.c')
    edges = dlsym_fp.analyze(tree, 'dlsym_fp.c', st, df)
    callees = {e.callee for e in edges}
    assert 'plugin_init' in callees


def test_dlsym_fp_complex():
    from ethunter.analyzer import dlsym_fp
    tree, st, df = _make_analyzer_env('dlsym_fp_complex.c')
    edges = dlsym_fp.analyze(tree, 'dlsym_fp_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'plugin_start' in callees


# === Integration tests ===

def test_symbol_table_extraction():
    tree, st, _ = _make_analyzer_env('direct_call.c')
    names = st.all_function_names
    assert 'main' in names
    assert 'worker' in names
    assert 'helper' in names


def test_dataflow_assign_merge():
    df = VariableState()
    df.assign('fp', 'foo')
    assert df.resolve('fp') == {'foo'}
    df.merge('fp', 'fp2')
    assert df.resolve('fp2') == {'foo'}


def test_call_graph_dedup():
    from ethunter.graph.model import CallGraph, CallEdge, CallType
    from ethunter.analyzer.orchestrator import run_all_analyses
    files = ['direct_call.c', 'fp_assign.c']
    trees = {}
    st = SymbolTable()
    df = VariableState()
    for f in files:
        path = os.path.join(FIXTURES, f)
        tree = parse_file(path)
        trees[path] = tree
        for func in extract_functions(tree, f):
            st.add_function(func)
    graph = run_all_analyses(trees, st, df)
    graph.source_files = [os.path.join(FIXTURES, f) for f in files]
    pairs = [(e.caller, e.callee) for e in graph.edges]
    assert len(pairs) == len(set(pairs)), f'Duplicate edges: {pairs}'
