"""CLI entry point for ethunter."""

import argparse
import json
import sys
from pathlib import Path

from ethunter.parser.scanner import scan_files
from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.orchestrator import run_all_analyses
from ethunter.graph.model import CallGraph
from ethunter.output.json_output import to_json
from ethunter.output.dot_output import to_dot
from ethunter.query.engine import query_callers, query_callees


def _query_result(graph, func_name: str) -> str:
    """Build JSON query result for a function."""
    callers = query_callers(graph, func_name)
    callees = query_callees(graph, func_name)
    return json.dumps({
        'function': func_name,
        'callers': [{'caller': e.caller, 'file': e.caller_file, 'type': e.type.value} for e in callers],
        'callees': [{'callee': e.callee, 'file': e.callee_file, 'type': e.type.value} for e in callees],
    }, indent=2, ensure_ascii=False)


def _find_entry_points(graph) -> list[dict]:
    """Find functions with implementations that are never called by anyone."""
    callees = {e.callee for e in graph.edges}
    return [
        {"name": f.name, "file": f.file, "line": f.line}
        for f in graph.functions.values()
        if f.is_definition and f.name not in callees
    ]


def main() -> None:
    parser = argparse.ArgumentParser(
        description='ethunter - C source code call graph analyzer',
    )
    parser.add_argument('--analyze', metavar='DIR', help='Analyze a C project directory')
    parser.add_argument('--from-json', metavar='FILE', help='Load call graph from a JSON file instead of analyzing')
    parser.add_argument('--query', metavar='FUNC_NAME', help='Query callers and callees for a specific function')
    parser.add_argument('--to-dot', action='store_true', help='Convert loaded JSON call graph to DOT format')
    parser.add_argument('--find-entry', action='store_true', help='Find functions with implementations that are never called')
    parser.add_argument(
        '--output', '-o',
        metavar='FILE',
        help='Write output to file instead of stdout',
    )

    args = parser.parse_args()

    # Mutual exclusion: --analyze and --from-json cannot be used together
    if args.analyze and args.from_json:
        print('Error: --analyze and --from-json are mutually exclusive', file=sys.stderr)
        sys.exit(1)

    # --query/--to-dot/--find-entry require --from-json
    query_ops = [args.query, args.to_dot, args.find_entry]
    if any(query_ops) and not args.from_json:
        print('Error: --query, --to-dot, and --find-entry require --from-json', file=sys.stderr)
        sys.exit(1)

    if not args.analyze and not args.from_json:
        print('Error: either --analyze or --from-json is required', file=sys.stderr)
        sys.exit(1)

    if args.from_json:
        # --- Mode: Load from JSON ---
        json_path = Path(args.from_json)
        if not json_path.is_file():
            print(f'Error: file not found: {args.from_json}', file=sys.stderr)
            sys.exit(1)

        try:
            data = json.loads(json_path.read_text())
        except json.JSONDecodeError as e:
            print(f'Error: invalid JSON: {e}', file=sys.stderr)
            sys.exit(1)

        # Validate schema: must have 'functions' or 'edges' key
        if 'functions' not in data and 'edges' not in data:
            print('Error: unrecognized JSON format (missing "functions" or "edges" key)', file=sys.stderr)
            sys.exit(1)

        graph = CallGraph.from_dict(data)

        # Mutual exclusion: --query, --to-dot, and --find-entry cannot be used together
        exclusives = [args.query, args.to_dot, args.find_entry]
        if sum(1 for x in exclusives if x) > 1:
            print('Error: --query, --to-dot, and --find-entry are mutually exclusive', file=sys.stderr)
            sys.exit(1)

        if args.to_dot:
            output = to_dot(graph)
        elif args.query:
            output = _query_result(graph, args.query)
        elif args.find_entry:
            output = json.dumps(
                {"uncalled_functions": _find_entry_points(graph)},
                indent=2, ensure_ascii=False,
            )
        else:
            print('Error: --from-json requires --query, --to-dot, or --find-entry', file=sys.stderr)
            sys.exit(1)
    else:
        # --- Mode: Analyze project ---
        project_dir = Path(args.analyze)
        if not project_dir.is_dir():
            print(f'Error: {project_dir} is not a directory', file=sys.stderr)
            sys.exit(1)

        # Phase 1: Scan files
        files, ignored_count = scan_files(project_dir)
        if ignored_count:
            print(f'Ignored {ignored_count} files matching .ethunterignore', file=sys.stderr)
        if not files:
            print('No .c/.h files found', file=sys.stderr)
            sys.exit(1)

        # Phase 2: Parse ASTs
        trees: dict[str, str] = {}
        for f in files:
            try:
                tree = parse_file(f)
                trees[str(f)] = tree
            except Exception as e:
                print(f'Warning: failed to parse {f}: {e}', file=sys.stderr)

        # Phase 3: Build symbol table
        symbol_table = SymbolTable()
        dataflow = VariableState()

        for filepath, tree in trees.items():
            for func in extract_functions(tree, filepath):
                symbol_table.add_function(func)

        # Phase 4: Run all analyzers
        call_graph = run_all_analyses(trees, symbol_table, dataflow)
        call_graph.source_files = [str(f) for f in files]

        # Phase 5: Output (--analyze mode only produces JSON)
        output = to_json(call_graph)

    # Write output
    out_path = args.output
    if not out_path:
        if args.analyze:
            out_path = 'callgraph.json'
        elif args.to_dot:
            out_path = 'output.dot'
        elif args.find_entry:
            out_path = 'entry.json'
        elif args.query:
            out_path = 'query.json'
        else:
            out_path = 'output.json'

    Path(out_path).write_text(output)
    print(f'Output written to {out_path}')


if __name__ == '__main__':
    main()
