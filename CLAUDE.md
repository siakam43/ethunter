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

- **`--analyze DIR`**: Full analysis pipeline, outputs JSON only (default output: `callgraph.json`)
- **`--from-json FILE`**: Load pre-generated JSON, then `--query`, `--to-dot`, or `--find-entry` (these three are mutually exclusive with each other and require `--from-json`)

The `--format` parameter has been removed.

## Architecture

### 5-Phase Pipeline (`src/ethunter/cli.py`)

1. **Scan** — recursively discover `.c`/`.h` files via `scanner.py`, excluding build directories and patterns in `.ethunterignore`
2. **Parse** — build tree-sitter ASTs for each file via `ast_builder.py`
3. **Symbol Table** — extract function definitions/declarations into a `SymbolTable`, initialize `VariableState` for dataflow tracking
4. **Analyze** — run analyzer modules through the orchestrator, merge into a single `CallGraph`
5. **Output** — emit JSON (analyze mode), or DOT/query/entry-points (from-json mode)

### Data Model (`src/ethunter/graph/model.py`)

- **`CallType`** — `DIRECT` or `INDIRECT`
- **`Function`** — name, file, line, signature, is_definition, return_type, parameters. Has `key` property: `"{file}:{name}:{line}"`
- **`CallEdge`** — caller, callee, caller_file, callee_file, type, `indirect_kind`, caller_line
- **`CallGraph`** — dict of `Function` by key, list of `CallEdge`, source_files. Provides `add_function()`, `add_edge()`, `query_callers()`, `query_callees()`, `from_dict()`, `to_dict()`

### Analyzer Modules (`src/ethunter/analyzer/orchestrator.py`)

The orchestrator runs analyzers in a **two-phase pipeline** plus independent modules:

**Phase 1a: Metadata Collection** — cross-file pre-scan, no edges:
| Module | Detects |
|---|---|
| `param_helpers.prepare()` | Collect func_params, func_fp_params, param_usage, param/ret fields |

**Phase 1: Target Resolution** — writes function pointer targets to `dataflow` (no edges returned):
| Module | Detects |
|---|---|
| `direct_assign` | `fp = func` direct variable assignment |
| `initializer_assign` | `void (*fp)(void) = func` declaration with initializer |
| `cast_assign` | `(void (*)(void))func` cast-style assignment |
| `param_helpers` | pre-scan: collects func_params, func_fp_params, param_usage metadata |
| `param_binding` | call-site argument binding: writes dataflow + registration_sites, no edges |

**Phase 1b: Callback Detection** — produces callback edges:
| Module | Detects |
|---|---|
| `callback_reg` | callback_reg edges from registration_sites (3-stage: behavior check → coverage check → heuristic fallback) |
| `param_dispatch` | callback_param edges: callee-body calls through fnptr params + call-site propagation |

**Phase 2: Call Detection** — reads from `dataflow` to produce call edges:
| Module | Detects |
|---|---|
| `direct_call_fp` | `fp()` calls where fp was resolved in Phase 1 |
| `field_call` | `obj->func_ptr()` struct field function pointer calls |
| `array_call` | `fp_array[i]()` function pointer array dispatch calls |

**Independent modules** (don't depend on the two-phase pipeline):
| Module | Detects |
|---|---|
| `direct_call` | `foo()` style direct calls (runs first, uses `symbol_names` set only) |
| `dlsym_fp` | `dlsym()`-based dynamic loading patterns |

### Shared State

- **`SymbolTable`** (`src/ethunter/analyzer/symbol_table.py`) — project-wide function name → `Function` list, typedef resolution, struct member tracking
- **`VariableState`** (`src/ethunter/analyzer/dataflow.py`) — variable name → set of function targets, callback registry. Used by analyzers to track where function pointers flow across assignments and aliases.

### Parser (`src/ethunter/parser/`)

- **`scanner.py`** — file discovery with `.ethunterignore` support, build directory exclusion
- **`ast_builder.py`** — tree-sitter parsing of individual C files
- **`preprocessor.py`** — C preprocessor integration

### Query Engine (`src/ethunter/query/engine.py`)

- **`query_callers(graph, func_name)`** — find all functions that call `func_name`
- **`query_callees(graph, func_name)`** — find all functions called by `func_name`

### Output (`src/ethunter/output/`)

- **`json_output.py`** — serializes `CallGraph` to JSON with summary statistics
- **`dot_output.py`** — converts `CallGraph` to Graphviz DOT format

### Directory Structure

```
src/ethunter/
  analyzer/           — analysis modules + orchestrator + helpers
    helpers.py        — shared AST utilities (find_enclosing_function, extract_identifier)
    dataflow.py       — VariableState for variable → function target tracking
    symbol_table.py   — SymbolTable + extract_functions from tree-sitter AST
    orchestrator.py   — run_all_analyses, two-phase pipeline + deduplication
    direct_call.py    — direct call detection (foo())
    direct_assign.py  — direct function pointer assignment (fp = func)
    initializer_assign.py — declaration with initializer
    cast_assign.py    — cast-style assignment
    param_helpers.py  — pre-scan metadata collection
    param_binding.py  — call-site argument binding (Phase 1)
    param_dispatch.py — fnptr param call detection (Phase 2)
    callback_reg.py   — callback registration edge emission (Phase 3)
    direct_call_fp.py  — indirect calls through resolved function pointers
    field_call.py     — struct field function pointer calls
    array_call.py     — function pointer array dispatch
    dlsym_fp.py       — dlsym-based dynamic loading
  graph/
    model.py          — Function, CallEdge, CallType, CallGraph
  output/
    json_output.py    — JSON serialization
    dot_output.py     — DOT/Graphviz serialization
  parser/
    scanner.py        — file discovery, .ethunterignore
    ast_builder.py    — tree-sitter parsing
    preprocessor.py   — preprocessor integration
  query/
    engine.py         — caller/callee lookup
  cli.py              — CLI entry point, 5-phase pipeline orchestration

tests/
  fixtures/           — minimal C files (simple + _complex variants)
  fixtures/cross_file/ — multi-file C fixtures per analyzer
  benchmark/          — real C projects with ground_truth.json
    et_bench/         — ET-Bench benchmark suite (fnptr-callback, fnptr-cast, fnptr-dynamic-call, fnptr-global-array, fnptr-global-struct, fnptr-global-struct-array, fnptr-library, fnptr-only, fnptr-struct, fnptr-varargs, fnptr-virtual)
  test_analyzers.py   — per-module unit tests
  test_cross_file.py  — cross-file call detection tests
  test_benchmark.py   — benchmark accuracy tests
  test_et_bench.py    — ET-Bench integration tests
  test_query_json.py  — query and JSON round-trip tests
  test_scanner.py     — file scanner tests
```