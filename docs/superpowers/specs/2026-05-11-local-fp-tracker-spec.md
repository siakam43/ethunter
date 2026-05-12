# Spec: Local Variable Function Pointer Tracking for fnptr-struct

## Problem

`fnptr-struct` benchmark recall is 42.86% (9/21). Two categories of missed indirect calls share a common root cause: **function pointers first assigned to local variables before being called**.

### Missed Example 1 — example_6 (Redis dict defrag)

```c
/* struct field assignment — already tracked by initializer_assign as <gstruct:defragfns.defragAlloc> */
dictDefragFunctions defragfns = {.defragAlloc = activeDefragAlloc};

/* Local variable inherits the struct field's function pointer type */
dictDefragAllocFunction *defragalloc = defragfns->defragAlloc;

/* Call through the local variable — NOT a field_expression, so field_call.py doesn't match */
newentry = defragalloc(entry);
```

Current state: `initializer_assign` records `<gstruct:defragfns.defragAlloc>` = `activeDefragAlloc`. `direct_call_fp.py` sees `defragalloc(entry)` as an identifier call but only queries `dataflow.resolve("defragalloc")` — which is empty because the `<gstruct:...>` key is never resolved into the local variable name.

### Missed Example 2 — example_13 (OpenSSL GCM AES-NI)

```c
/* struct field assignment — tracked as <gstruct:ctx.block> */
ctx->block = aesni_encrypt;

/* Local variable inherits the struct field's function pointer type */
block128_f block = ctx->block;

/* Call through pointer expression — func_node is pointer_expression, not identifier */
(*block)(ctx->Yi.c, ctx->EKi.c, key);
```

Current state: `direct_call_fp.py` only matches when `func_node.type == 'identifier'`. `(*block)(...)` has `func_node.type == 'pointer_expression'`, so it's silently skipped.

### Impact

Fixing the local variable tracking pattern adds 1 matched edge (example_6: 1). Recall improves from 42.86% (9/21) to 47.62% (10/21).

Note: example_13 (`ctx->block` set through `CRYPTO_gcm128_init(ctx, key, block)`) requires cross-function parameter tracking — `ctx->block` is set via a function parameter, not a global struct initializer. The dataflow for example_13 has zero targets even before local_fp_tracker runs. This requires inter-procedural analysis and is out of scope.

Note: example_8 and example_10 have ground truth callee of `"NULL"` — these are sentinel values meaning "call exists but target unknown at analysis time". They are not matchable by any concrete callee and should be excluded from realistic recall targets. Excluding them: 10/19 = 52.63%.

## Design

### New Module: `src/ethunter/analyzer/local_fp_tracker.py`

**Purpose:** Track local variables that inherit function pointer types from struct field access.

**Responsibilities:**

1. **Scan assignment expressions** where RHS is a field expression:
   - `local = struct_ptr->field` (field_expression with `->`)
   - `local = struct_var.field` (field_expression with `.`)

2. **Scan init_declarators** where initializer is a field expression:
   - `Type local = struct_ptr->field;`
   - `Type local = struct_var.field;`

3. **For each match**, construct the dataflow key from the field path:
   - `struct_ptr->field` → key `<gstruct:struct_ptr.field>`
   - `struct_var.field` → key `<gstruct:struct_var.field>`

4. **Resolve the dataflow key** to get the set of function targets.

5. **Return a mapping**: `{local_var_name: set(function_targets)}`.

**Interface:**

```python
def collect_local_fp_assignments(
    tree: ts.Tree,
    dataflow: VariableState,
    symbol_names: set[str],
) -> dict[str, set[str]]:
    """Collect local variable assignments from struct field function pointers.

    Returns mapping from local variable name to set of resolved function targets.
    """
```

**Not stored in `dataflow`**: This module returns a standalone dict rather than writing to `VariableState`. Reasons:
- Local variables are function-scoped and not meaningful across files
- Keeps the implementation lightweight and testable
- Avoids polluting the global dataflow namespace with temporary keys

### Modified Module: `src/ethunter/analyzer/direct_call_fp.py`

**Changes:**

1. **Call `collect_local_fp_assignments()`** at the start of `analyze()` to get the local variable mapping.

2. **Handle `pointer_expression` calls** (`(*block)(...)`):
   - When `func_node.type == 'pointer_expression'`, unwrap to find the inner identifier.
   - If inner node is an identifier, look it up in the local variable mapping.

3. **Handle identifier calls** (`defragalloc(entry)`):
   - After querying `dataflow.resolve(var_name)` (existing behavior), also check the local variable mapping.

**Detection order per call:**
1. `dataflow.resolve(var_name)` — existing direct assignment (e.g., `fp = func`)
2. `local_fp_mapping.get(var_name)` — new: local variable from struct field

### Modified Module: `src/ethunter/analyzer/orchestrator.py`

**Change:** Run `local_fp_tracker.collect_local_fp_assignments()` as part of the Phase 2 call detection pipeline. Since it produces a per-file dict consumed by `direct_call_fp`, the simplest approach is to call it inside `direct_call_fp.analyze()` rather than as a separate orchestrator step.

**Decision:** Call `local_fp_tracker` from within `direct_call_fp.analyze()` to minimize changes to the orchestrator. This keeps the dependency explicit and local.

## Test Plan

### TDD Approach

1. **Write test fixture `tests/fixtures/local_fp/example_6/fixture.c`** — based on the real ET-Bench example_6 (Redis dict defrag). Tests:
   - `dictDefragBucket` → `activeDefragAlloc` detected as indirect call

2. **Write test fixture `tests/fixtures/local_fp/example_13/fixture.c`** — based on the real ET-Bench example_13 (OpenSSL GCM AES-NI). Tests:
   - `CRYPTO_gcm128_encrypt` → `aesni_encrypt` detected as indirect call

3. **Write test fixture `tests/fixtures/local_fp/combined.c`** — minimal test covering both patterns in one file:
   - Pattern A: `local = struct->field; local(args)`
   - Pattern B: `local = struct.field; (*local)(args)`

4. **Add unit tests in `tests/test_analyzers.py`**:
   - `test_local_fp_tracker_assignment`: verifies `collect_local_fp_assignments` returns correct mapping for `ptr->field` assignment
   - `test_local_fp_tracker_init_declarator`: verifies handling of `Type local = struct.field`
   - `test_local_fp_tracker_pointer_expression`: verifies `(*block)(...)` call detection
   - `test_direct_call_fp_with_local_mapping`: verifies end-to-end detection through `direct_call_fp`

5. **Run ET-Bench** to verify `fnptr-struct` recall improves to ≥ 52.38% (11/21).

### Expected Test Results

| Test | Before | After |
|---|---|---|
| `test_local_fp_*` (new) | N/A (not written yet) | Pass |
| example_6 detection | 0/1 | 1/1 |
| example_13 detection | 0/1 | 1/1 |
| `fnptr-struct` overall recall | 9/21 (42.86%) | 11/21 (52.38%) |
| Other categories | unchanged | unchanged |
| All existing tests | Pass | Pass |

## File Changes

| File | Change |
|---|---|
| `src/ethunter/analyzer/local_fp_tracker.py` | **NEW** — local variable function pointer tracking |
| `src/ethunter/analyzer/direct_call_fp.py` | **MODIFY** — integrate local mapping, handle pointer_expression calls |
| `tests/fixtures/local_fp/example_6/fixture.c` | **NEW** — test fixture |
| `tests/fixtures/local_fp/example_13/fixture.c` | **NEW** — test fixture |
| `tests/fixtures/local_fp/combined.c` | **NEW** — combined test fixture |
| `tests/fixtures/local_fp/ground_truth.json` | **NEW** — ground truth for combined fixture |
| `tests/test_analyzers.py` | **MODIFY** — add unit tests for new module |
| `tests/test_et_bench.py` | **NO CHANGE** — existing ET-Bench test validates end-to-end |

## Scope Boundaries

**This spec does NOT cover:**

- Parameter-to-struct-field propagation (example_5): requires cross-function parameter tracking
- Chain access beyond 2 levels (example_12): requires multi-level struct chain resolution
- NULL-guarded calls (example_8, example_10): ground truth marks callee as `NULL`, not a detection issue
- Function return value tracking (example_9): requires inter-procedural analysis
- Callback registration patterns: already handled by `param_assign.py`

These should be addressed in separate specs if needed.
