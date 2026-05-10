# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Environment Setup (IMPORTANT)

- **Use `.venv/bin/python`** (Python 3.11), NOT system python (3.6 — missing tree_sitter_c)
- **`PYTHONPATH=src`** is required for CLI runs. pyproject.toml's `pythonpath` only applies to pytest
- **Install packages**: venv 由 uv 创建，无 pip 模块。使用 `uv pip install --python .venv/bin/python <package>`
- **Editable install**: `uv pip install --python .venv/bin/python -e .`（已在环境中执行过）
- **源码变更后**：Python 代码修改后直接运行即可生效（editable install），无需重新 install
- **依赖变更后**：如果 `pyproject.toml` 中的依赖有变化，重新运行 editable install 命令

## Project Overview

**ethunter** is a static analysis tool for C source code call graph generation. It uses tree-sitter to parse C projects and runs multiple analyzer modules to detect both direct and indirect (function pointer) call relationships.

## Commands

```bash
# Run all tests
.venv/bin/python -m pytest tests/ -q

# Run a single test
.venv/bin/python -m pytest tests/test_analyzers.py::test_fp_assign -v

# Run tests for a specific area
.venv/bin/python -m pytest tests/test_cross_file.py -v
.venv/bin/python -m pytest tests/test_query_json.py -v

# Analyze a C project (output: JSON)
PYTHONPATH=src .venv/bin/python -m ethunter.cli --analyze /path/to/c/project
PYTHONPATH=src .venv/bin/python -m ethunter.cli --analyze /path/to/c/project -o output.json

# Query from a pre-generated JSON file (no re-analysis)
PYTHONPATH=src .venv/bin/python -m ethunter.cli --from-json graph.json --query my_function

# Convert a pre-generated JSON file to DOT format (no re-analysis)
PYTHONPATH=src .venv/bin/python -m ethunter.cli --from-json graph.json --to-dot

# Find uncalled functions with implementations from a pre-generated JSON file
PYTHONPATH=src .venv/bin/python -m ethunter.cli --from-json graph.json --find-entry
```

`PYTHONPATH=src` is required — `pyproject.toml` only sets `pythonpath` for pytest, not for `python -m`.

No linter is installed. Use `python -m pytest` from the `.venv` (Python 3.11) for all test runs.

The CLI has two mutually exclusive modes:

- **`--analyze DIR`**: Full analysis pipeline, outputs JSON only
- **`--from-json FILE`**: Load pre-generated JSON, then `--query`, `--to-dot`, or `--find-entry` (these three are mutually exclusive with each other and require `--from-json`)

The `--format` parameter has been removed.

## Architecture

### 5-Phase Pipeline (`src/ethunter/cli.py`)

1. **Scan** — recursively discover `.c`/`.h` files, excluding build directories and patterns in `.ethunterignore`
2. **Parse** — build tree-sitter ASTs for each file
3. **Symbol Table** — extract function definitions/declarations into a `SymbolTable`, initialize `VariableState` for dataflow tracking
4. **Analyze** — run 12 analyzer modules + direct call analysis, merge into a single `CallGraph`
5. **Output** — emit JSON, DOT, or a function-level query

### Data Model (`src/ethunter/graph/model.py`)

- **`CallType`** — `DIRECT` or `INDIRECT`
- **`Function`** — name, file, line, signature, is_definition, return_type, parameters. Has `key` property: `"{file}:{name}:{line}"`
- **`CallEdge`** — caller, callee, caller_file, callee_file, type, `indirect_kind`, caller_line
- **`CallGraph`** — dict of `Function` by key, list of `CallEdge`, source_files. Provides `add_function()`, `add_edge()`, `query_callers()`, `query_callees()`

### Analyzer Modules

All analyzers (except `direct_call`) implement the standard interface:

```python
def analyze(tree, filepath, symbol_table, dataflow) -> list[CallEdge]
```

| Module | Detects | indirect_kind |
|---|---|---|
| `direct_call` | `foo()` style calls | (none, CallType.DIRECT) |
| `fp_assign` | `fp = func` + `fp()` calls | `fp_assign` |
| `callback_param` | functions passed as callback args | `callback_param` |
| `fp_return` | functions returning function pointers | `fp_return` |
| `fp_array` | function pointer arrays / dispatch tables | `fp_array` |
| `vtable` | struct-based vtable indirection | `vtable` |
| `callback_reg` | callback registration APIs | `callback_reg` |
| `union_fp` | function pointers stored in unions | `union_fp` |
| `typedef_fp` | calls through typedef'd function pointer types | `typedef_fp` |
| `fp_alias` | `fp2 = fp1` alias chains | `fp_alias` |
| `lazy_init` | lazily-initialized function pointers | `lazy_init` |
| `macro_fp` | function pointer assignments inside macros | `macro_fp` |
| `dlsym_fp` | `dlsym()`-based dynamic loading | `dlsym_fp` |

### Shared State

- **`SymbolTable`** (`src/ethunter/analyzer/symbol_table.py`) — project-wide function name → `Function` list, typedef resolution, struct member tracking
- **`VariableState`** (`src/ethunter/analyzer/dataflow.py`) — variable name → set of function targets, callback registry. Used by analyzers to track where function pointers flow across assignments and aliases.

### Orchestrator (`src/ethunter/analyzer/orchestrator.py`)

Runs `direct_call` first (uses symbol_names set), then all standard analyzers (use symbol_table + dataflow). Deduplicates edges at the end: same caller+callee pair produces one edge, preferring direct over indirect.

### Directory Structure

```
src/ethunter/
  analyzer/       — 13 analysis modules + orchestrator + helpers.py
    helpers.py    — shared AST utilities (find_enclosing_function, extract_identifier)
    dataflow.py   — VariableState for variable → function target tracking
    symbol_table.py — SymbolTable + extract_functions from tree-sitter AST
    orchestrator.py — run_all_analyses, deduplication
    __init__.py   — re-exports all analyzer modules
  graph/          — CallGraph, CallEdge, Function, CallType
  output/         — JSON and DOT serialization
  parser/         — file scanning, tree-sitter parsing, #include tracking
  query/          — caller/callee lookup on the graph

tests/
  fixtures/       — minimal C files (simple + _complex variants)
  fixtures/cross_file/ — multi-file C fixtures per analyzer
  benchmark/      — real C projects with ground_truth.json
    cg_bench/     — CG-Bench benchmark suite
  test_analyzers.py     — per-module unit tests
  test_cross_file.py    — cross-file call detection tests
  test_benchmark.py     — benchmark accuracy tests
  test_cg_bench.py      — CG-Bench integration tests
  test_query_json.py    — query and JSON round-trip tests
  test_scanner.py       — file scanner tests
```
