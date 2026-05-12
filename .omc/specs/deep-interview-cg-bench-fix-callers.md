# Deep Interview Spec: Fix CG-Bench Fixture Caller/Callee Pairs

## Metadata
- Interview ID: cg-bench-fix-callers-001
- Rounds: 3
- Final Ambiguity Score: 15.5%
- Type: brownfield
- Generated: 2026-05-08
- Threshold: 20%
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.9 | 0.35 | 0.315 |
| Constraint Clarity | 0.8 | 0.25 | 0.200 |
| Success Criteria Clarity | 0.7 | 0.25 | 0.175 |
| Context Clarity | 0.6 | 0.15 | 0.090 |
| **Total Clarity** | | | **0.780** |
| **Ambiguity** | | | **0.220** |

## Goal

Fix all 84 CG-Bench fixtures where the ground_truth (caller, callee) pairs cannot be verified by ethunter because either the caller function doesn't exist, the callee functions aren't defined, or the function pointer is never actually invoked. For each fixture, add the necessary code (stub callees, wrapper callers, callsites) so that ethunter can detect the indirect call relationship, and update ground_truth.json to match.

## Constraints

- **Stub format**: Missing callee functions should be added as minimal `static` stub implementations matching the fnptr's expected signature. E.g., `static void callee_name(void) { }` or with appropriate return types/parameters.
- **Wrapper functions**: For MISSING_CALLER cases where the fnptr is a variable (not a function), add a wrapper function with the fnptr name that directly calls the target. E.g., for `static void (*fp)(int) = target;`, add `void fp(int x) { target(x); }`.
- **Callsites**: For MISSING_CALL cases where the fnptr is never invoked, add a callsite inside the existing caller function that invokes the fnptr.
- **No behavior changes to existing code**: Only add new code, don't modify existing fixture code logic.
- **ground_truth consistency**: After fixture modifications, ground_truth.json (caller, callee) pairs must match what ethunter can actually detect from the fixture.
- **Preserve real-project complexity**: Don't simplify or remove existing fixture code. Additions should integrate naturally with the existing code style.

## Non-Goals

- Do not fix ethunter's analyzers — only fix the test fixtures
- Do not change the benchmark test structure (test_cg_bench.py) except if ground_truth format needs adjustment
- Do not add features or analyzers to ethunter

## Acceptance Criteria

- [ ] All 84 problematic fixtures have been fixed: caller function defined, callee stubs defined, fnptr callsite present
- [ ] ground_truth.json for each category is updated to reflect actual (caller, callee) pairs detectable in the fixtures
- [ ] `pytest tests/test_cg_bench.py` passes and shows improved recall compared to baseline (~0.16%)
- [ ] All existing tests pass: `pytest tests/ -q` (90 tests)
- [ ] extract_fixtures.py is updated so that re-running it produces the same fixed fixtures (or is disabled/deprecated)

## Assumptions Exposed & Resolved

| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| ground_truth caller = fnptr name | fnptr is often a variable, not a function — ethunter can't detect "calls" from variables | Add wrapper functions with the fnptr name that call the target |
| Callees exist in fixtures | CG-Bench extracts fragments where callees are defined in external projects | Add static stub implementations for each missing callee |
| fnptr is always called | 3 examples have fnptr assigned but never invoked | Add callsites inside existing caller functions |
| One-size-fits-all stub format | Different examples have wildly different numbers of callees (1 to 256+) | Use uniform stub format for all; simple `static` functions matching signature |

## Technical Context

### Current Issue Types (84 of 104 examples affected)

1. **MISSING_CALL (3)**: fnptr assigned but never invoked. Examples: fnptr-callback/example_13 (valueize), fnptr-cast/example_05 (funs->memory), fnptr-only/example_10 (Curl_cfree).

2. **MISSING_CALLER (12)**: ground_truth uses fnptr name as caller but no function definition exists. Examples: fnptr-callback/example_04 (x), fnptr-only/example_01 (zmalloc_oom_handler), fnptr-only/example_03 (md_final_raw), etc.

3. **MISSING_CALLEE (69)**: callee target functions not defined in fixture. This is the most common issue. CG-Bench's original data focuses on fnptr+targets assignment relationships, not actual callsites.

### Affected Categories Summary

| Category | Total | MISSING_CALL | MISSING_CALLER | MISSING_CALLEE |
|----------|-------|-------------|----------------|----------------|
| fnptr-callback | 13 | 1 | 1 | 9 |
| fnptr-cast | 7 | 1 | 0 | 4 |
| fnptr-dynamic-call | 3 | 0 | 2 | 1 |
| fnptr-global-array | 6 | 0 | 1 | 4 |
| fnptr-global-struct | 8 | 0 | 0 | 7 |
| fnptr-global-struct-array | 12 | 0 | 2 | 9 |
| fnptr-library | 14 | 0 | 0 | 11 |
| fnptr-only | 12 | 1 | 4 | 4 |
| fnptr-struct | 9 | 1 | 0 | 7 |
| fnptr-varargs | 1 | 0 | 0 | 1 |
| fnptr-virtual | 1 | 0 | 0 | 1 |

### Key Implementation Detail

The fix approach for each issue type:
- **MISSING_CALL**: Inside the existing caller function, add a line that invokes the fnptr. E.g., if `convert_to_ascii` is a parameter, add `convert_to_ascii(buf, buf2);` inside the function body.
- **MISSING_CALLER**: Add a wrapper function definition that calls through the fnptr variable to its target. Also ensure the wrapper is invoked somewhere.
- **MISSING_CALLEE**: Add `static` stub function definitions for each missing callee, matching the expected signature from the fnptr declaration.

### Extract Script Consideration

`extract_fixtures.py` generates fixtures from CG-Bench markdown source. After manual fixes, re-running it would overwrite changes. The script should either be updated to produce the fixed output, or the fixed fixtures should be decoupled from the script.

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| fixture | core domain | fixture.c, ground_truth.json | fixture has ground_truth |
| ground_truth_entry | core domain | fnptr_name, caller, callee, callsite | ground_truth_entry references fixture |
| missing_callee | supporting | stub_implementation | missing_callee needs stub in fixture |
| missing_caller | supporting | wrapper_function | missing_caller wraps fnptr invocation |
| validation | supporting | recall_improvement, test_pass_rate | validation measures fixture correctness |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 1 | 3 | 3 | - | - | - |
| 2 | 4 | 1 | 0 | 3 | 100% |
| 3 | 5 | 1 | 0 | 4 | 100% |

Ontology has converged — entities stable across rounds 2-3 with 100% stability.

## Interview Transcript

<details>
<summary>Full Q&A (3 rounds)</summary>

### Round 1
**Q:** For the 69 MISSING_CALLEE examples where callee target functions are not defined in the fixture, how should we add them? The examples vary widely — some have 1-5 targets, others have 50-256+.
**A:** Add stub implementations for each missing callee
**Ambiguity:** 75% (Goal: 0.8, Constraints: 0.4, Criteria: 0.3)

### Round 2
**Q:** How should we verify that the fixes are correct? The current test_cg_bench.py reports recall by comparing detected edges against ground_truth.json. After adding stubs and callsites, what constitutes a successful fix?
**A:** All of the above: test recall, existing tests, and per-fixture validation.
**Ambiguity:** 46% (Goal: 0.8, Constraints: 0.4, Criteria: 0.7)

### Round 3
**Q:** For MISSING_CALLER examples where the fnptr is a variable (not a function), how should we fix the caller? Two approaches: (A) add a wrapper function with the fnptr name that calls through to the target, or (B) change ground_truth to use the actual enclosing function as caller?
**A:** Add a wrapper function that calls the fnptr target
**Ambiguity:** 15.5% (Goal: 0.9, Constraints: 0.8, Criteria: 0.7)

</details>
