# Deep Interview Spec: CG-Bench Test Case Generation

## Metadata
- Interview ID: cg-bench-test-cases-20260509
- Rounds: 6
- Final Ambiguity Score: 9%
- Type: brownfield
- Generated: 2026-05-09
- Threshold: 20%
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.95 | 0.40 | 0.38 |
| Constraint Clarity | 0.85 | 0.30 | 0.255 |
| Success Criteria | 0.90 | 0.30 | 0.27 |
| **Total Clarity** | | | **0.905** |
| **Ambiguity** | | | **9%** |

## Goal

Extract all 104 CG-Bench examples from `tests/benchmark/CG-Bench/fnptr-*.md` files into standalone test fixtures under `tests/benchmark/cg_bench/`, organized as `{category}/example_N/` directories. Each fixture contains the raw C code snippets with wrapper functions where needed, plus a per-example `ground_truth.json`. Modify `test_cg_bench.py` to load per-example ground truth and produce a per-category recall report (measurement only, no assertions).

## Constraints

- **All 104 examples** must be processed, no subset
- **Raw code snippets** from CG-Bench markdown files are used as-is; no compilation required, tree-sitter only parses
- **Caller identification**: For each example, check if the `fnptr` is called within an existing function. If yes → that function is the caller. If no → create a wrapper function named `{fnptr_name}_caller` that calls through the fnptr with correct parameters
- **Callee identification**: `targets` become callee names. If a target has no implementation in the snippet → create a stub wrapper function with the exact target name
- **Per-example ground truth**: Each `example_N/` directory gets its own `ground_truth.json` with `{"examples": [{"caller": "...", "callee": "..."}]}`
- **Only indirect edges** are tracked; direct calls are ignored
- **Test behavior**: Measurement-only report with `print()`, no `assert` statements for pass/fail
- **test_cg_bench.py modification**: Minimal change — load `ground_truth.json` from each `example_N/` directory instead of one per category, aggregate all expected edges

## Non-Goals

- Direct call detection analysis
- Adding assertion-based pass/fail thresholds
- Rewriting or simplifying CG-Bench code snippets (stubs for types/macros are NOT needed)
- Creating a new test file; `test_cg_bench.py` is modified in-place

## Acceptance Criteria

- [ ] `tests/benchmark/cg_bench/` directory exists with 11 subdirectories (one per CG-Bench category)
- [ ] All 104 examples extracted as `example_N/` directories with `.c` files containing raw snippets + wrapper functions
- [ ] Each `example_N/` has a `ground_truth.json` listing expected indirect (caller, callee) edges
- [ ] `test_cg_bench.py` loads per-example ground truth and aggregates results per category
- [ ] Running `pytest tests/test_cg_bench.py -v` produces a recall table per category and overall

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| External types/macros need stubs | tree-sitter doesn't compile, only parses | Keep raw snippets verbatim, no stubs |
| All 104 examples may be overkill | User confirmed comprehensive benchmark needed | Process all 104 examples |
| Shared category-level ground_truth | Per-example avoids name collisions | One `ground_truth.json` per `example_N/` |
| Need new test file | Existing `test_cg_bench.py` just needs schema change | Modify in-place, minimal change |

## Technical Context

### CG-Bench Structure
- 11 categories: fnptr-callback(15), fnptr-cast(7), fnptr-dynamic-call(5), fnptr-global-array(6), fnptr-global-struct-array(12), fnptr-global-struct(11), fnptr-library(20), fnptr-only(12), fnptr-struct(14), fnptr-varargs(1), fnptr-virtual(1)
- Total: 104 examples from 7 projects (gcc, ffmpeg, wrk, zfs, redis, curl, openssh)
- Each example has: callsite path, fnptr name, targets list, code snippets

### ethunter Test Infrastructure
- `test_benchmark.py`: Runs on real C projects (cJSON, libuv) with `ground_truth.json` containing `source_files`, `direct_edges`, `indirect_edges`
- `test_cg_bench.py` (existing): Expects fixtures at `tests/benchmark/cg_bench/`, currently loads category-level ground truth, report-only
- `_run_analysis_on_fixture()`: Walks fixture dir, parses `.c`/`.h`, builds SymbolTable + VariableState, runs orchestrator
- `compute_recall()`: Matches `(caller, callee)` pairs between found edges and expected edges

### ethunter Analyzer Modules
13 analyzers: direct_call, fp_assign, callback_param, fp_return, fp_array, vtable, callback_reg, union_fp, typedef_fp, fp_alias, lazy_init, macro_fp, dlsym_fp

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| Category | core domain | name (fnptr-*), example_count | has many Examples |
| Example | core domain | example_id, fnptr_name, targets[], code_snippets[], caller_func, has_fnptr_call | belongs to Category |
| Wrapper-Caller | supporting | name ({fnptr}_caller), body (calls fnptr), parameters | created when fnptr not called; calls Example.fnptr |
| Wrapper-Target | supporting | name (target_name), body (stub) | created when target has no impl; is callee of Example |
| Fixture | core domain | directory path, .c files, ground_truth.json | contains Example; tested by test_cg_bench.py |
| GroundTruthEdge | supporting | caller, callee | listed in ground_truth.json; matched against analyzer output |
| Test | supporting | recall_per_category, overall_recall, matched_pairs | reads Fixture + runs ethunter; produces report |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 3 | 3 | 3 | - | - | N/A |
| 5 | 5 | 2 (Wrapper-Caller, Wrapper-Target) | - | 3 (Fixture, Test, Category) | 100% |

## Interview Transcript
<details>
<summary>Full Q&A (6 rounds)</summary>

### Round 1
**Q:** Which approach for creating test fixtures from CG-Bench markdown files?
**A:** 1 - Extract & isolate: Extract code snippets into standalone .c fixtures under tests/benchmark/cg_bench/{category}/example_N/, creating minimal reproducible fixtures (104 examples total)
**Ambiguity:** 67% (Goal: 0.6, Constraints: 0.3, Criteria: 0.5)

### Round 2
**Q:** How to handle external dependencies (types, macros, functions from original projects)?
**A:** 3 - Keep as-is: Use raw snippets verbatim; ethunter only needs tree-sitter parsing (not compilation), so undefined types are acceptable
**Ambiguity:** 53% (Goal: 0.7, Constraints: 0.5, Criteria: 0.6)

### Round 3
**Q:** What should the final test behavior look like?
**A:** 1 - Measurement only: Per-category recall report with print() output, no assert statements (like current test_cg_bench.py, just with real data)
**Ambiguity:** 40% (Goal: 0.8, Constraints: 0.6, Criteria: 0.8)

### Round 4
**Q:** All 104 examples or phased approach?
**A:** 1 - All 104: Full extraction, no shortcuts. High effort but comprehensive.
**Ambiguity:** 30% (Goal: 0.85, Constraints: 0.7, Criteria: 0.8)

### Round 5
**Q:** Ground truth schema — shared category-level or per-example?
**A:** 2 - Per-example ground_truth: Each example_N/ has its own ground_truth.json with only its edges. The test script would need modification.
**Ambiguity:** 20% (Goal: 0.9, Constraints: 0.8, Criteria: 0.85)

### Round 6
**Q:** test_cg_bench.py modification scope — minimal change or full refactor?
**A:** 1 - Minimal change: Instead of loading one category-level ground_truth.json, load ground_truth.json from each example_N/ directory, aggregate all expected edges, and keep the existing accumulate-matches-across-examples logic
**Ambiguity:** 9% (Goal: 0.95, Constraints: 0.85, Criteria: 0.9)

</details>
