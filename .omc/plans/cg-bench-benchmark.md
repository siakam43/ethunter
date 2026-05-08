# Plan: CG-Bench Benchmark Conversion to ethunter Test Harness

## RALPLAN-DR Summary

### Principles
1. **Preserve real-project complexity** — CG-Bench examples reflect real-world indirect call patterns; simplification should only remove unrelated code
2. **Measurement over pass/fail** — report recall ratios, don't gate on thresholds
3. **Category-level granularity** — one ground_truth.json per CG-Bench category for debuggability
4. **No new analyzers** — evaluate ethunter as-is; don't add detection logic during benchmark creation

### Decision Drivers
1. 104 examples across 11 categories from 7 external projects — scale is significant
2. ethunter expects directory-based project input with `.c`/`.h` files
3. CG-Bench provides structured data (cgbench.json) and code snippets (markdown files)

### Viable Options

#### Option A: Per-category directory with example subdirs (Recommended)
```
tests/benchmark/cg_bench/
├── fnptr-callback/
│   ├── example_01/ (curl_share_lockfunc)
│   │   ├── caller.c
│   │   └── callee.c
│   ├── example_02/
│   │   └── ...
│   └── ground_truth.json
├── fnptr-global-array/
│   └── ...
```
**Pros:** Matches spec exactly, preserves cross-file structure, per-category ground_truth enables category-level recall reporting
**Cons:** 104+ directories to create, some examples have complex dependencies

#### Option B: Single monolithic directory
All 104 examples in one large C project directory with one ground_truth.json.
**Pros:** Simple to run ethunter once
**Cons:** Loses category-level granularity, cross-example naming collisions, hard to debug which category has low recall
**Invalidation:** Violates the per-category ground_truth constraint from the spec.

### Approach
Use Option A. Create `tests/benchmark/cg_bench/<category>/example_<NN>/` directories with C fixture files extracted from CG-Bench markdown code snippets. Generate `ground_truth.json` per category from `cgbench.json`. Write a new `tests/test_cg_bench.py` benchmark test module.

## Requirements Summary

Convert CG-Bench's 104 indirect call examples into an ethunter benchmark harness. Each example gets its own fixture directory. ground_truth.json per category records expected indirect edges. A pytest test computes and reports recall per category and overall.

## Acceptance Criteria

- [ ] `tests/benchmark/cg_bench/` directory exists with 11 category subdirectories
- [ ] Each category subdirectory contains example_<NN>/ subdirectories (104 total)
- [ ] Each example directory contains compilable `.c`/`.h` fixture files
- [ ] 11 `ground_truth.json` files (one per category) with expected indirect edges
- [ ] `tests/test_cg_bench.py` exists and runs via pytest
- [ ] Test reports recall per category and overall (via print/capfd)
- [ ] `pytest tests/` runs without errors

## Implementation Steps

### Step 1: Create directory structure
Create `tests/benchmark/cg_bench/` and 11 category subdirectories:
- `fnptr-callback`, `fnptr-cast`, `fnptr-dynamic-call`, `fnptr-global-array`,
- `fnptr-global-struct-array`, `fnptr-global-struct`, `fnptr-library`,
- `fnptr-only`, `fnptr-struct`, `fnptr-varargs`, `fnptr-virtual`

**Files:** `mkdir` commands

### Step 2: Extract CG-Bench examples into fixture directories
For each of the 104 examples in `tests/benchmark/CG-Bench/`:
- Parse the markdown file to extract code snippets for each example
- Write the code snippets as `.c`/`.h` files into `tests/benchmark/cg_bench/<category>/example_<NN>/`
- Preserve original code structure; trim only clearly unrelated code

**Approach:** Write a Python extraction script `tests/benchmark/cg_bench/extract_fixtures.py` that:
1. Reads `cgbench.json` for the structured metadata (callsites, fnptr, targets, chain_summary)
2. Reads the corresponding `fnptr-*.md` files to extract code snippets
3. Creates directory per example and writes C files
4. **Phase 2 — Parse verification:** After extraction, run `parse_file()` on each generated `.c` fixture. Collect any parse failures and fix them iteratively. Tree-sitter is tolerant of missing includes but may fail on syntax errors from incomplete snippets.

**Files:** `tests/benchmark/cg_bench/extract_fixtures.py`

### Step 3: Generate ground_truth.json per category
For each category, generate a `ground_truth.json` with:
```json
{
  "category": "fnptr-callback",
  "examples": [
    {
      "name": "example_01",
      "caller": "function_name",
      "callee": "target_function"
    }
  ]
}
```

Recall computation uses `(caller, callee)` pairs matching the `compute_recall()` pattern in `test_benchmark.py`. The `indirect_kind` is NOT part of the matching — ground_truth records expected caller→callee pairs, and the test checks whether ethunter detects them regardless of which analyzer found them.

**Edge count verification:** Each ground_truth.json should contain exactly the number of indirect edges listed in the corresponding CG-Bench examples. The extraction script validates this.

**Files:** Added to `extract_fixtures.py` or separate script

### Step 4: Write benchmark test module
Create `tests/test_cg_bench.py` that:
1. Iterates over each category directory
2. For each category, runs ethunter's pipeline on each example directory
3. Compares detected indirect edges against `ground_truth.json`
4. Reports recall per category and overall via `print()` statements

Reuses `_run_analysis_on_benchmark()` and `compute_recall()` patterns from `test_benchmark.py`.

**Files:** `tests/test_cg_bench.py`

### Step 5: Handle unmapped categories
For `fnptr-cast` and `fnptr-varargs` (no ethunter analyzer mapping):
- Still create fixture directories and ground_truth.json
- The test module skips these categories in recall computation (or reports 0% with a warning)
- Include them in the total example count

### Step 6: Run and verify
- Run `pytest tests/test_cg_bench.py -v` to verify no import/parse errors
- Verify recall output is printed per category and overall

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| CG-Bench code snippets are incomplete (missing type definitions, includes) | Tree-sitter tolerates missing includes. For missing types, add minimal stub declarations. Two-phase extraction verifies parse success |
| 104 examples requires significant effort | Automated extraction handles 80%+ of examples (basic patterns like fnptr-only, fnptr-global-array). Manual fix for remaining 20% (complex library/callback examples) |
| Some examples span multiple files with complex include relationships | Preserve cross-file structure in the example directory; use ethunter's cross-file analysis support |
| CG-Bench paths in cgbench.json reference absolute paths on the original author's machine | Use markdown file parsing as the source of code snippets; use cgbench.json only for targets metadata |

## Verification Steps

1. `ls tests/benchmark/cg_bench/` — verify 11 category directories exist
2. `find tests/benchmark/cg_bench/ -name "*.c" | wc -l` — verify C files exist
3. `find tests/benchmark/cg_bench/ -name "ground_truth.json" | wc -l` — verify 11 ground_truth files
4. `.venv/bin/python -m pytest tests/test_cg_bench.py -v` — verify test runs
5. Check printed output for per-category recall numbers

## ADR

### Decision
Create a per-category benchmark harness under `tests/benchmark/cg_bench/` with:
- 104 example fixture directories (one per CG-Bench example)
- 11 ground_truth.json files (one per category, using caller→callee pairs)
- One `test_cg_bench.py` module that reports recall per category and overall

### Drivers
- ethunter evaluates overall indirect call detection capability, not per-analyzer coverage
- Per-category granularity enables targeted debugging of low-recall categories
- CG-Bench provides structured metadata (cgbench.json) for ground_truth generation

### Alternatives considered
1. **Single monolithic directory + one ground_truth.json** — Rejected: loses category-level granularity, makes it impossible to identify which patterns ethunter misses
2. **Per-analyzer fixture files in tests/fixtures/** — Rejected: ethunter's analyzer set is incomplete; 1:1 mapping would bias evaluation toward existing capabilities
3. **No C fixtures, metadata-only benchmark** — Rejected: ethunter requires parseable C input; can't evaluate detection without running the tool

### Why chosen
Option A balances debuggability (per-category recall), fidelity (preserved real-project complexity), and feasibility (automated extraction + parse verification).

### Consequences
- 104+ directories to maintain — future CG-Bench updates require re-extraction
- Two categories (fnptr-cast, fnptr-varargs) have no ethunter analyzer mapping — will report 0% recall, which is expected and informative
- ground_truth.json uses (caller, callee) pairs without indirect_kind — matches ethunter's existing benchmark pattern but doesn't verify WHICH analyzer detected the edge

### Follow-ups
- Consider adding per-edge indirect_kind to ground_truth for per-analyzer recall breakdown
- Consider adding a `--print-edges` debug mode to ethunter CLI for fixture troubleshooting
- Consider automating fixture updates when CG-Bench releases new versions

## Changelog (Post-Review Improvements)
- Added two-phase extraction (automated + parse-verify) to Step 2
- Changed ground_truth.json schema: removed `indirect_kind` field; recall uses `(caller, callee)` pairs only
- Added edge count verification to Step 3
- Updated risk table: merged "incomplete snippets" and "external types" risks; made "104 examples" risk concrete with 80% automation target
