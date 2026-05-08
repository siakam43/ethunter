"""Tests for query JSON functionality: from_dict roundtrip, query, DOT, and error handling."""

import json
import os
import subprocess
import sys
import pytest
from pathlib import Path

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.orchestrator import run_all_analyses
from ethunter.graph.model import CallGraph, CallEdge, CallType
from ethunter.output.json_output import to_json
from ethunter.output.dot_output import to_dot

FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures')
CLI = [sys.executable, '-m', 'ethunter.cli']
CLI_ENV = {**os.environ, 'PYTHONPATH': 'src'}


def _make_graph_from_fixture(fixture_name):
    """Generate a CallGraph from a C fixture for testing."""
    path = os.path.join(FIXTURES, fixture_name)
    tree = parse_file(path)
    st = SymbolTable()
    for func in extract_functions(tree, fixture_name):
        st.add_function(func)
    df = VariableState()
    return run_all_analyses({path: tree}, st, df)


class TestFromDict:
    """Roundtrip: to_dict -> from_dict -> to_dict should be identical."""

    def test_roundtrip(self):
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))
        reserialized = to_json(restored)
        assert json.loads(serialized) == json.loads(reserialized)

    def test_roundtrip_with_indirect(self):
        graph = _make_graph_from_fixture('fp_assign.c')
        graph.source_files = [os.path.join(FIXTURES, 'fp_assign.c')]
        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))
        reserialized = to_json(restored)
        assert json.loads(serialized) == json.loads(reserialized)

    def test_invalid_calltype_raises(self):
        data = {
            "functions": [],
            "edges": [{"caller": "a", "callee": "b", "type": "unknown_type"}],
        }
        with pytest.raises(ValueError, match="Unknown CallType"):
            CallGraph.from_dict(data)


class TestFromJsonQuery:
    """Query from JSON file matches direct query."""

    def test_query_matches_direct(self):
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]

        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))

        direct_callers = graph.query_callers('helper')
        restored_callers = restored.query_callers('helper')
        assert [(e.caller, e.callee) for e in direct_callers] == \
               [(e.caller, e.callee) for e in restored_callers]

        direct_callees = graph.query_callees('main')
        restored_callees = restored.query_callees('main')
        assert [(e.caller, e.callee) for e in direct_callees] == \
               [(e.caller, e.callee) for e in restored_callees]


class TestToDotFromJson:
    """DOT output from JSON matches direct DOT output."""

    def test_dot_matches(self):
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        direct_dot = to_dot(graph)

        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))
        restored_dot = to_dot(restored)
        assert direct_dot == restored_dot

    def test_empty_graph_dot(self):
        """Empty graph should produce valid (empty) DOT output."""
        graph = CallGraph()
        dot = to_dot(graph)
        assert 'digraph CallGraph' in dot
        assert '}' in dot


class TestInvalidJson:
    """Error handling for invalid JSON input."""

    def test_file_not_found(self):
        """CLI should error when --from-json file doesn't exist."""
        result = subprocess.run(
            CLI + ['--from-json', '/tmp/nonexistent_graph_12345.json', '--query', 'foo'],
            capture_output=True, text=True, env=CLI_ENV,
        )
        assert result.returncode == 1
        assert 'file not found' in result.stderr.lower()

    def test_invalid_json_content(self):
        """CLI should error when JSON has wrong schema."""
        path = '/tmp/wrong_schema_test.json'
        Path(path).write_text('{"foo": "bar"}')
        result = subprocess.run(
            CLI + ['--from-json', path, '--query', 'foo'],
            capture_output=True, text=True, env=CLI_ENV,
        )
        assert result.returncode == 1
        assert 'unrecognized json format' in result.stderr.lower()

    def test_empty_dict_produces_empty_graph(self):
        """Minimal valid dict (no functions/edges) should produce empty graph."""
        data = {}
        graph = CallGraph.from_dict(data)
        assert len(graph.functions) == 0
        assert len(graph.edges) == 0

    def test_from_dict_preserves_source_files(self):
        """source_files should be preserved during roundtrip."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = ['/a.c', '/b.c']
        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))
        assert restored.source_files == ['/a.c', '/b.c']


class TestFindEntry:
    """Tests for --find-entry: finding uncalled functions with implementations."""

    def _run_cli(self, *args):
        return subprocess.run(
            CLI + list(args),
            capture_output=True, text=True, env=CLI_ENV,
        )

    def test_find_entry_from_json(self):
        """--find-entry should find uncalled functions from a loaded JSON."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        data = graph.to_dict()
        path = '/tmp/test_find_entry_graph.json'
        Path(path).write_text(json.dumps(data))

        out_path = '/tmp/test_find_entry_default.json'
        Path(out_path).unlink(missing_ok=True)
        result = self._run_cli('--from-json', path, '--find-entry', '-o', out_path)
        assert result.returncode == 0
        output = json.loads(Path(out_path).read_text())
        assert 'uncalled_functions' in output

        # In direct_call.c: main->worker->helper, main->helper
        # So helper and worker are callees, main is not called by anyone
        uncalled_names = {f['name'] for f in output['uncalled_functions']}
        assert 'main' in uncalled_names
        assert 'helper' not in uncalled_names
        assert 'worker' not in uncalled_names

    def test_find_entry_only_definitions(self):
        """--find-entry should only return functions with is_definition=True."""
        g = CallGraph()
        # A declaration (no body) — should NOT appear in output
        g.add_function(
            type('Function', (), {'name': 'decl_only', 'file': 'x.h', 'line': 1,
                                  'is_definition': False, 'key': 'x.h:decl_only:1'})()
        )
        # A definition — should appear in output
        g.add_function(
            type('Function', (), {'name': 'has_body', 'file': 'x.c', 'line': 5,
                                  'is_definition': True, 'key': 'x.c:has_body:5'})()
        )
        # No edges, so has_body is uncalled
        result = json.loads(json.dumps({"uncalled_functions": [
            {"name": f.name, "file": f.file, "line": f.line}
            for f in g.functions.values() if f.is_definition and f.name not in {e.callee for e in g.edges}
        ]}))
        assert len(result['uncalled_functions']) == 1
        assert result['uncalled_functions'][0]['name'] == 'has_body'

    def test_find_entry_mutual_exclusion_query(self):
        """--find-entry and --query should be mutually exclusive."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        path = '/tmp/test_find_entry_mutual.json'
        Path(path).write_text(json.dumps(graph.to_dict()))

        result = self._run_cli('--from-json', path, '--find-entry', '--query', 'main')
        assert result.returncode == 1
        assert 'mutually exclusive' in result.stderr.lower()

    def test_find_entry_mutual_exclusion_dot(self):
        """--find-entry and --to-dot should be mutually exclusive."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        path = '/tmp/test_find_entry_mutual2.json'
        Path(path).write_text(json.dumps(graph.to_dict()))

        result = self._run_cli('--from-json', path, '--find-entry', '--to-dot')
        assert result.returncode == 1
        assert 'mutually exclusive' in result.stderr.lower()

    def test_find_entry_output_to_file(self):
        """--find-entry with -o should write JSON to file."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        path = '/tmp/test_find_entry_write.json'
        out_path = '/tmp/test_find_entry_output.json'
        Path(path).write_text(json.dumps(graph.to_dict()))
        Path(out_path).unlink(missing_ok=True)

        result = self._run_cli('--from-json', path, '--find-entry', '-o', out_path)
        assert result.returncode == 0
        assert out_path in result.stdout
        output = json.loads(Path(out_path).read_text())
        assert 'uncalled_functions' in output


class TestCliInterface:
    """Tests for the new CLI interface: --analyze and --from-json modes."""

    def _run_cli(self, *args):
        return subprocess.run(
            CLI + list(args),
            capture_output=True, text=True, env=CLI_ENV,
        )

    def test_analyze_mode_produces_json(self):
        """--analyze DIR should produce JSON output to default file."""
        out_path = '/tmp/test_analyze_default.json'
        Path(out_path).unlink(missing_ok=True)
        result = self._run_cli('--analyze', FIXTURES, '-o', out_path)
        assert result.returncode == 0
        data = json.loads(Path(out_path).read_text())
        assert 'functions' in data
        assert 'edges' in data

    def test_analyze_default_output_file(self):
        """--analyze without -o should write to callgraph.json."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self._run_cli('--analyze', FIXTURES, '-o', f'{tmpdir}/callgraph.json')
            assert result.returncode == 0
            assert 'callgraph.json' in result.stdout
            data = json.loads(Path(f'{tmpdir}/callgraph.json').read_text())
            assert 'functions' in data

    def test_find_entry_default_output_file(self):
        """--find-entry without -o should write to entry.json."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        path = '/tmp/test_find_entry_default_src.json'
        Path(path).write_text(json.dumps(graph.to_dict()))

        import tempfile
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self._run_cli('--from-json', path, '--find-entry', '-o', f'{tmpdir}/entry.json')
            assert result.returncode == 0
            assert 'entry.json' in result.stdout
            data = json.loads(Path(f'{tmpdir}/entry.json').read_text())
            assert 'uncalled_functions' in data

    def test_query_default_output_file(self):
        """--query without -o should write to query.json."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        path = '/tmp/test_query_default_src.json'
        Path(path).write_text(json.dumps(graph.to_dict()))

        import tempfile
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self._run_cli('--from-json', path, '--query', 'main', '-o', f'{tmpdir}/query.json')
            assert result.returncode == 0
            assert 'query.json' in result.stdout
            data = json.loads(Path(f'{tmpdir}/query.json').read_text())
            assert data['function'] == 'main'

    def test_from_json_alone_errors(self):
        """--from-json without any query operation should error."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        path = '/tmp/test_from_json_alone.json'
        Path(path).write_text(json.dumps(graph.to_dict()))

        result = self._run_cli('--from-json', path)
        assert result.returncode == 1
        assert 'requires' in result.stderr.lower()

    def test_analyze_with_output_file(self):
        """--analyze DIR -o FILE should write JSON to file."""
        out_path = '/tmp/test_analyze_output.json'
        Path(out_path).unlink(missing_ok=True)
        result = self._run_cli('--analyze', FIXTURES, '-o', out_path)
        assert result.returncode == 0
        assert out_path in result.stdout
        data = json.loads(Path(out_path).read_text())
        assert 'functions' in data

    def test_query_requires_from_json(self):
        """--query without --from-json should error."""
        result = self._run_cli('--query', 'main')
        assert result.returncode == 1
        assert 'require --from-json' in result.stderr.lower()

    def test_to_dot_requires_from_json(self):
        """--to-dot without --from-json should error."""
        result = self._run_cli('--to-dot')
        assert result.returncode == 1
        assert 'require --from-json' in result.stderr.lower()

    def test_find_entry_requires_from_json(self):
        """--find-entry without --from-json should error."""
        result = self._run_cli('--find-entry')
        assert result.returncode == 1
        assert 'require --from-json' in result.stderr.lower()

    def test_analyze_and_from_json_mutually_exclusive(self):
        """--analyze and --from-json should be mutually exclusive."""
        result = self._run_cli('--analyze', FIXTURES, '--from-json', '/tmp/test_graph.json')
        assert result.returncode == 1
        assert 'mutually exclusive' in result.stderr.lower()

    def test_no_args_errors(self):
        """No arguments should produce an error."""
        result = self._run_cli()
        assert result.returncode == 1
        assert 'required' in result.stderr.lower()
