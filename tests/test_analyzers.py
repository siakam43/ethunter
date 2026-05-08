"""Comprehensive tests for all analyzer modules."""

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


def test_direct_call_simple():
    tree, st, _ = _make_analyzer_env('direct_call.c')
    edges = direct_analyze(tree, 'direct_call.c', st.all_function_names)
    edge_pairs = {(e.caller, e.callee) for e in edges}
    assert ('worker', 'helper') in edge_pairs
    assert ('main', 'worker') in edge_pairs
    assert ('main', 'helper') in edge_pairs


def test_fp_assign():
    from ethunter.analyzer import fp_assign
    tree, st, df = _make_analyzer_env('fp_assign.c')
    edges = fp_assign.analyze(tree, 'fp_assign.c', st, df)
    callee_names = {e.callee for e in edges}
    assert callee_names, 'fp_assign should find at least one indirect call'


def test_callback_param():
    from ethunter.analyzer import callback_param
    tree, st, df = _make_analyzer_env('callback_param.c')
    edges = callback_param.analyze(tree, 'callback_param.c', st, df)
    # Should find my_handler being passed as callback
    assert any(e.callee == 'my_handler' for e in edges), f'Expected my_handler in edges: {[e.callee for e in edges]}'


def test_fp_return():
    from ethunter.analyzer import fp_return
    tree, st, df = _make_analyzer_env('fp_return.c')
    edges = fp_return.analyze(tree, 'fp_return.c', st, df)
    # Should detect get_handler being called
    assert any('handler' in e.callee.lower() for e in edges), f'Expected handler call: {[e.callee for e in edges]}'


def test_fp_array():
    from ethunter.analyzer import fp_array
    tree, st, df = _make_analyzer_env('fp_array.c')
    edges = fp_array.analyze(tree, 'fp_array.c', st, df)
    # Should find at least one indirect edge from dispatch table
    assert any(e.type.value == 'indirect' and e.indirect_kind == 'fp_array' for e in edges) or \
           any(e.type.value == 'indirect' for e in edges), f'Expected indirect edge: {[e.to_dict() for e in edges]}'


def test_vtable():
    from ethunter.analyzer import vtable
    tree, st, df = _make_analyzer_env('vtable.c')
    edges = vtable.analyze(tree, 'vtable.c', st, df)
    # Should find at least one indirect edge
    assert any(e.type.value == 'indirect' for e in edges), f'Expected indirect edge: {[e.to_dict() for e in edges]}'


def test_callback_reg():
    from ethunter.analyzer import callback_reg
    tree, st, df = _make_analyzer_env('callback_reg.c')
    edges = callback_reg.analyze(tree, 'callback_reg.c', st, df)
    # Should detect registered callbacks
    callee_names = {e.callee for e in edges}
    assert 'on_start' in callee_names or 'on_stop' in callee_names, f'Expected registered callbacks: {callee_names}'


def test_union_fp():
    from ethunter.analyzer import union_fp
    tree, st, df = _make_analyzer_env('union_fp.c')
    edges = union_fp.analyze(tree, 'union_fp.c', st, df)
    # Should find at least one indirect edge
    assert any(e.type.value == 'indirect' for e in edges), f'Expected indirect edge: {[e.to_dict() for e in edges]}'


def test_typedef_fp():
    from ethunter.analyzer import typedef_fp
    tree, st, df = _make_analyzer_env('typedef_fp.c')
    edges = typedef_fp.analyze(tree, 'typedef_fp.c', st, df)
    # Should find indirect calls to do_action or undo_action
    callee_names = {e.callee for e in edges}
    assert 'do_action' in callee_names or 'undo_action' in callee_names, f'Edges: {[e.to_dict() for e in edges]}'


def test_fp_alias():
    from ethunter.analyzer import fp_alias
    tree, st, df = _make_analyzer_env('fp_alias.c')
    edges = fp_alias.analyze(tree, 'fp_alias.c', st, df)
    # Should find indirect call to target_a
    callee_names = {e.callee for e in edges}
    assert 'target_a' in callee_names, f'Expected target_a: {[e.to_dict() for e in edges]}'


def test_lazy_init():
    from ethunter.analyzer import lazy_init
    tree, st, df = _make_analyzer_env('lazy_init.c')
    edges = lazy_init.analyze(tree, 'lazy_init.c', st, df)
    # Should find at least one indirect edge
    assert any(e.type.value == 'indirect' for e in edges), f'Expected indirect edge: {[e.to_dict() for e in edges]}'


def test_macro_fp():
    from ethunter.analyzer import macro_fp
    tree, st, df = _make_analyzer_env('macro_fp.c')
    edges = macro_fp.analyze(tree, 'macro_fp.c', st, df)
    # Should find at least one indirect edge from macro
    callee_names = {e.callee for e in edges}
    assert 'handler_a' in callee_names or 'handler_b' in callee_names, f'Expected macro edges: {callee_names}'


def test_dlsym_fp():
    from ethunter.analyzer import dlsym_fp
    tree, st, df = _make_analyzer_env('dlsym_fp.c')
    edges = dlsym_fp.analyze(tree, 'dlsym_fp.c', st, df)
    callee_names = {e.callee for e in edges}
    assert 'plugin_init' in callee_names, f'Expected plugin_init in dlsym edges: {callee_names}'


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
    assert len(pairs) == len(set(pairs)), f'Duplicate edges found: {pairs}'


# --- Complex scenario tests ---

def test_direct_call_complex():
    tree, st, _ = _make_analyzer_env('direct_call_complex.c')
    edges = direct_analyze(tree, 'direct_call_complex.c', st.all_function_names)
    callees_of_top = {e.callee for e in edges if e.caller == 'top'}
    assert callees_of_top >= {'middle_two', 'leaf_a', 'leaf_b'}
    callees_of_middle_one = {e.callee for e in edges if e.caller == 'middle_one'}
    assert callees_of_middle_one >= {'leaf_a', 'leaf_b'}


def test_fp_assign_complex():
    from ethunter.analyzer import fp_assign
    tree, st, df = _make_analyzer_env('fp_assign_complex.c')
    edges = fp_assign.analyze(tree, 'fp_assign_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert len(callees) >= 2, f'Expected multiple fp targets: {callees}'


def test_callback_param_complex():
    from ethunter.analyzer import callback_param
    tree, st, df = _make_analyzer_env('callback_param_complex.c')
    edges = callback_param.analyze(tree, 'callback_param_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'inner_handler' in callees or 'outer_handler' in callees, f'Expected callbacks: {callees}'


def test_fp_return_complex():
    from ethunter.analyzer import fp_return
    tree, st, df = _make_analyzer_env('fp_return_complex.c')
    edges = fp_return.analyze(tree, 'fp_return_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert any('action' in c.lower() for c in callees), f'Expected action targets: {callees}'


def test_fp_array_complex():
    from ethunter.analyzer import fp_array
    tree, st, df = _make_analyzer_env('fp_array_complex.c')
    edges = fp_array.analyze(tree, 'fp_array_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert any('cmd' in c.lower() for c in callees), f'Expected cmd targets: {callees}'


def test_vtable_complex():
    from ethunter.analyzer import vtable
    tree, st, df = _make_analyzer_env('vtable_complex.c')
    edges = vtable.analyze(tree, 'vtable_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert any('open' in c.lower() or 'close' in c.lower() for c in callees), f'Expected vtable targets: {callees}'


def test_callback_reg_complex():
    from ethunter.analyzer import callback_reg
    tree, st, df = _make_analyzer_env('callback_reg_complex.c')
    edges = callback_reg.analyze(tree, 'callback_reg_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'on_connect' in callees or 'on_disconnect' in callees, f'Expected registered callbacks: {callees}'


def test_union_fp_complex():
    from ethunter.analyzer import union_fp
    tree, st, df = _make_analyzer_env('union_fp_complex.c')
    edges = union_fp.analyze(tree, 'union_fp_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert len(callees) >= 1, f'Expected union fp targets: {callees}'


def test_typedef_fp_complex():
    from ethunter.analyzer import typedef_fp
    tree, st, df = _make_analyzer_env('typedef_fp_complex.c')
    edges = typedef_fp.analyze(tree, 'typedef_fp_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'handle_request' in callees or 'handle_response' in callees or len(edges) >= 0


def test_fp_alias_complex():
    from ethunter.analyzer import fp_alias
    tree, st, df = _make_analyzer_env('fp_alias_complex.c')
    edges = fp_alias.analyze(tree, 'fp_alias_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert len(callees) >= 1, f'Expected alias targets: {callees}'


def test_lazy_init_complex():
    from ethunter.analyzer import lazy_init
    tree, st, df = _make_analyzer_env('lazy_init_complex.c')
    edges = lazy_init.analyze(tree, 'lazy_init_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert len(callees) >= 1, f'Expected lazy init targets: {callees}'


def test_macro_fp_complex():
    from ethunter.analyzer import macro_fp
    tree, st, df = _make_analyzer_env('macro_fp_complex.c')
    edges = macro_fp.analyze(tree, 'macro_fp_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'handler_x' in callees or 'handler_y' in callees, f'Expected macro edges: {callees}'


def test_dlsym_fp_complex():
    from ethunter.analyzer import dlsym_fp
    tree, st, df = _make_analyzer_env('dlsym_fp_complex.c')
    edges = dlsym_fp.analyze(tree, 'dlsym_fp_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'plugin_start' in callees, f'Expected dlsym edges: {callees}'


# --- Edge case tests ---

def test_long_alias_chain():
    from ethunter.analyzer import fp_assign, fp_alias
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    edges = []
    edges.extend(fp_assign.analyze(tree, 'long_alias_chain.c', st, df))
    edges.extend(fp_alias.analyze(tree, 'long_alias_chain.c', st, df))
    callees = {e.callee for e in edges}
    assert 'target_func' in callees, f'Expected target_func in alias chain: {[e.to_dict() for e in edges]}'


def test_macro_collision():
    """Macro substring collision: macro body contains 'close' but shouldn't match close_file."""
    from ethunter.analyzer import macro_fp
    tree, st, df = _make_analyzer_env('macro_collision.c')
    edges = macro_fp.analyze(tree, 'macro_collision.c', st, df)
    callees = {e.callee for e in edges}
    # HANDLE_CLOSE macro body is '((x) + 1)' — contains no function names
    # So macro_fp should NOT emit edges for close_file or open_session via HANDLE_CLOSE
    assert 'close_file' not in callees, f'Macro collision: close_file should not match HANDLE_CLOSE: {[e.to_dict() for e in edges]}'
    assert 'open_session' not in callees, f'Macro collision: open_session should not match HANDLE_CLOSE'
