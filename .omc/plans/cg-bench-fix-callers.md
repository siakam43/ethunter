# Plan: Fix CG-Bench Fixtures for Valid Caller/Callee Pairs

## Requirements Summary

84 of 104 CG-Bench fixtures have invalid ground_truth (caller, callee) pairs that ethunter cannot verify. Three issue types:
- **MISSING_CALL (3)**: fnptr never invoked
- **MISSING_CALLER (12)**: fnptr is a variable, not a function — no caller definition exists
- **MISSING_CALLEE (69)**: target functions not defined in fixture

## RALPLAN-DR Summary

### Principles
1. **Additive-only**: Never remove or modify existing fixture code; only add stubs/wrappers/callsites
2. **Signature-matching**: All stubs must match the fnptr's expected signature (return type, parameters)
3. **Ground-truth fidelity**: Preserve original CG-Bench intent (fnptr→targets mapping) while making it detectable by ethunter

### Decision Drivers
1. **Scale**: 84 fixtures across 11 categories — needs automated/scripted approach, not manual edits
2. **Signature extraction**: Stubs need correct signatures derived from fnptr declarations in fixture code
3. **Test verification**: Each fix must be verifiable by ethunter's analysis pipeline

### Viable Options

**Option A: Hybrid regex+string fixer script (Recommended)**
- Write a Python script that reads each fixture.c + ground_truth.json, uses regex to extract fnptr signatures from the fixture text, generates stubs/wrappers/callsites as string insertions, writes back fixture.c and updates ground_truth.json. Per-category special-case handling for complex patterns (vtable lookups, struct member fnptrs, array dispatch).
- Pros: Automated, consistent, handles all 84 cases, reproducible, simpler than AST-based approach
- Cons: Regex parsing of C is fragile for arbitrary code; per-category special cases needed for complex patterns
- **Invalidation of alternatives**: Manual editing (Option B) is infeasible at 84 fixtures. Full AST-based generation (Option C with tree-sitter) is overkill — the complexity of AST traversal + code generation outweighs the benefit for this task. Template-based fixture generator (Option D) loses real-project complexity.

**Option B: Manual fixture editing**
- Pros: Precise control per example
- Cons: 84× manual edits, error-prone, not reproducible, time-consuming

**Option C: Template-based fixture generator**
- Generate entirely new minimal fixtures from ground_truth.json
- Pros: Clean, consistent
- Cons: Loses real-project complexity (violates spec constraint), discards existing fixture code

## Implementation Steps

### Step 1: Build the fixer script (`tests/benchmark/cg_bench/fix_fixtures.py`)

The script uses a hybrid approach: regex-based signature extraction from fixture text, with per-category special-case handling for complex fnptr patterns.

**Pass 1 — Analysis**: For each fixture.c + ground_truth.json:
- Use regex to extract all function definitions (name, return type, params) from fixture text
- Use regex to extract fnptr declarations/assignments and their signatures from fixture text
- Use regex to find all function calls (pattern: `\w+\s*\(` excluding known declarations)
- Identify issue type by cross-referencing ground_truth with extracted code

**Pass 2 — Code generation** (per issue type):
- **MISSING_CALLEE**: Extract parameter types from the fnptr declaration regex match. Generate `static <return_type> callee_name(<params>) { }` stubs. Append all stubs at end of file before the final newline.
- **MISSING_CALLER**: Extract return type and parameters from fnptr declaration. Generate wrapper `return_type fnptr_name(params) { return target_fn(param_names); }`. Insert after the fnptr variable declaration line. If no callsite exists for the wrapper, also add one inside the nearest enclosing function.
- **MISSING_CALL**: Find the existing caller function's body (from `{` to `}` of the function definition). Insert `fnptr_name(args);` call before the closing `}`.

**Per-category special cases**:
- `fnptr-virtual`: vtable method calls via `ops->method()` — need struct member pattern matching
- `fnptr-global-struct-array`: dispatch table patterns like `arr[i].func()` — need array indexing pattern
- `fnptr-cast`: cast-based indirection `(type)fnptr()` — need to handle explicit casts
- `fnptr-only`: simple variable-style fnptrs — straightforward wrapper generation

**Pass 3 — ground_truth update**:
- Verify modified fixture still parses (use tree-sitter parse_file)
- Write updated ground_truth.json with confirmed (caller, callee) pairs

### Step 2: Run the fixer script

```bash
.venv/bin/python tests/benchmark/cg_bench/fix_fixtures.py
```

### Step 3: Verify all fixtures parse cleanly

```bash
.venv/bin/python -c "
from ethunter.parser.ast_builder import parse_file
import os, glob
for f in glob.glob('tests/benchmark/cg_bench/**/*.c', recursive=True):
    tree = parse_file(f)
    assert tree is not None, f'Failed to parse {f}'
print(f'All fixtures parse OK')
"
```

### Step 4: Run benchmark test

```bash
.venv/bin/python -m pytest tests/test_cg_bench.py -v
```

### Step 5: Run full test suite

```bash
.venv/bin/python -m pytest tests/ -q
```

### Step 6: Deprecate extract_fixtures.py

Add a comment at the top of `extract_fixtures.py` noting that fixtures have been manually fixed and re-running the script will overwrite changes.

## Files Modified

| File | Action |
|------|--------|
| `tests/benchmark/cg_bench/fix_fixtures.py` | **NEW** — main fixer script |
| `tests/benchmark/cg_bench/<category>/example_NN/fixture.c` | **MODIFIED** — 84 files get stubs/wrappers/callsites added |
| `tests/benchmark/cg_bench/<category>/ground_truth.json` | **MODIFIED** — updated (caller, callee) pairs |
| `tests/benchmark/cg_bench/extract_fixtures.py` | **MODIFIED** — deprecation comment |

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| tree-sitter C fails to parse modified fixtures | Validate parsing after each fixture modification; abort on failure |
| Stub signatures don't match fnptr type | Extract signature from fnptr declaration AST node, not guess |
| Some fixtures have too complex patterns (e.g., 256 callees) | Generate stubs programmatically in a loop; no manual per-callee code |
| ethunter analyzers still can't detect some indirect patterns | Document which categories remain undetectable; focus on fixable ones first |

## Verification Steps

1. All 104 fixtures parse with tree-sitter (no syntax errors)
2. `pytest tests/test_cg_bench.py` passes with improved recall
3. `pytest tests/ -q` — all 90 existing tests still pass
4. Spot-check 3-5 representative fixtures manually to verify code quality

## Acceptance Criteria

- [ ] All 84 problematic fixtures fixed: caller defined, callee stubs defined, fnptr called
- [ ] ground_truth.json updated per category
- [ ] `pytest tests/test_cg_bench.py` passes, recall improved from ~0.16%
- [ ] `pytest tests/ -q` — all 90 tests pass
- [ ] extract_fixtures.py deprecated with warning comment

## ADR

### Decision
Use a hybrid regex+string fixer script with per-category special-case handling to fix all 84 CG-Bench fixtures.

### Drivers
1. Scale (84 fixtures) requires automation
2. Need to preserve real-project code complexity
3. Signature extraction from C declarations is achievable with regex on this constrained domain

### Alternatives considered
- Full tree-sitter AST-based generation: rejected as overkill, too complex for the benefit
- Manual editing: rejected as infeasible at scale
- Template-based fixture generator: rejected as it loses real-project complexity

### Why chosen
Best balance of automation, precision, and preservation of fixture quality.

### Consequences
- Regex parsing is fragile — if fixture patterns change significantly, the script may need updates
- Per-category special cases mean adding new categories requires additional handling
- The script becomes part of the test infrastructure and needs maintenance

### Follow-ups
1. Add per-fixture validation script that checks each fixture independently
2. Document the signature extraction regex patterns for future maintenance
3. Consider updating extract_fixtures.py to produce fixed fixtures directly

## Changelog

| Change | Source |
|--------|--------|
| Changed fixer from tree-sitter AST to hybrid regex+string approach | Architect review feedback |
| Added per-category special-case handling section | Architect review feedback |
| Strengthened Pass 2 code generation with regex extraction details | Critic self-review |
| Added ADR section | Consensus mode requirement |
