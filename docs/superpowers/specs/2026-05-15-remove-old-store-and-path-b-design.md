# Design: Remove Old Store and Path B

**Date**: 2026-05-15
**Goal**: Completely delete `VariableState.targets` (old store) and Path B (legacy suffix scan in `field_call._visit()`), migrating all consumers to a unified resolution API on `DataflowEngine` backed by `ScopedStore`.

## Architecture Overview

### Current State (Problem)

Two parallel stores and two parallel resolution paths:

```
Writes ──→ VariableState.targets (old)     Path B reads (field_call._visit)
        └→ ScopedStore (new)                Path A reads (FieldResolver)
```

### Target State

Single store, single resolution path:

```
Writes ──→ ScopedStore (new) ──→ DataflowEngine semantic API ──→ all analyzers
```

## Unified Resolution API

All analyzers query through `DataflowEngine` methods instead of `dataflow.resolve(key)` or `dataflow.targets.items()`:

| Method | Purpose | Replaces |
|---|---|---|
| `resolve_variable(var, caller)` | Variable → function targets | `<var>:caller:name`, bare `name` resolve |
| `resolve_struct_field_call(field_path, base_var, caller, filepath)` | Struct field → targets + metadata | Path A (FieldResolver) + Path B + garray fallback |
| `resolve_global_array(name)` | Global array → targets | `<garray:name>` resolve |
| `rebuild_param_mappings()` | Param name → targets from func_vars | `dataflow.targets.items()` iteration |
| `resolve_returned_field(func)` | Return value → field targets (already exists, needs de-old-store) | Old store suffix fallback |

### Method Details

**`resolve_variable(var_name, caller_func=None) -> set[str]`**
1. `store.resolve_func_var(caller_func, var_name)` — function-scoped
2. `store.resolve_func_var('<global>', var_name)` — global fallback
3. `local_fp_mapping.get(var_name)` — local assignments (caller passes via context or kwarg)

**`resolve_struct_field_call(field_path, base_var, caller_func, filepath) -> (set[str], Confidence, Evidence)`**
1. FieldResolver 4-tier chain (Tier 1-2 exact, chain decomp, Tier 3 same-file suffix, Tier 4 cross-file suffix) — all backed by `ScopedStore.struct_fields`
2. `resolve_global_array(base_var)` — garray fallback (was Path B line 275)
3. Returns empty set + None confidence if nothing found (no fallback to old store)

**`resolve_global_array(name) -> set[str]`**
1. `store.resolve_global_array(name)` — scoped store
2. If empty, check `<initializer>` (backward compat for array_call)

**`rebuild_param_mappings() -> dict[str, set[str]]`**
- Iterates `func_vars` entries where key's var portion matches `*:param_name` pattern
- Returns `{param_name: {targets}}` just like old `dataflow.targets.items()` iteration

## Migration Steps

### Step 1: Build unified API on DataflowEngine
- File: `dataflow.py`
- Add `resolve_variable()`, `resolve_struct_field_call()`, `resolve_global_array()`, `rebuild_param_mappings()`
- `resolve_struct_field_call()` takes a FieldResolver (constructed once and stored or passed in)
- ~60 lines new code

### Step 2: local_fp_tracker — switch to `resolve_struct_field_call()`
- File: `local_fp_tracker.py` (`_resolve_and_store`)
- Old: `dataflow.resolve('<gstruct>:' + key)`, `dataflow.resolve('<gstruct:' + key)`, etc.
- New: `dataflow.resolve_struct_field_call(field_path, base_var, ...)`
- ~15 lines changed

### Step 3: field_call — delete Path B, unify resolution
- File: `field_call.py` (`analyze`, `_visit`)
- Delete: Path B lines 274-285 (garray suffix + `dataflow.targets` progressive suffix)
- Delete: else branch lines 286-289 (`dataflow.resolve('<gstruct:...>')` + `<struct:...>`)
- Replace: resolver call with `dataflow.resolve_struct_field_call()`
- FieldResolver is passed into analyze() or created once via engine
- ~15 lines deleted, ~10 lines changed

### Step 4: direct_call_fp — switch to `resolve_variable()`
- File: `direct_call_fp.py` (`_get_targets`)
- Old: `dataflow.store.resolve_func_var()` + `dataflow.resolve('<var>:...')` + `dataflow.resolve(var)`
- New: `dataflow.resolve_variable(var_name, caller_func)` — single call
- ~15 lines simplified

### Step 5: array_call — switch to `resolve_global_array()`
- File: `array_call.py`
- Old: `dataflow.store.resolve_global_array()` → `dataflow.resolve('<garray:...>')` → `dataflow.resolve(name)` → `dataflow.resolve('<initializer>')`
- New: `dataflow.resolve_global_array(name)` — single call
- ~10 lines simplified

### Step 6: direct_assign, cast_assign, helpers — switch to `resolve_variable()`
- Old: `dataflow.resolve(target)` for alias chain resolution
- New: `dataflow.resolve_variable(target)` — same semantics, unified path
- ~5 lines each

### Step 7: param_dispatch — switch to `rebuild_param_mappings()`
- File: `param_dispatch.py`
- Old: `for key, vals in dataflow.targets.items()` iteration
- New: `dataflow.rebuild_param_mappings()`
- ~10 lines changed

### Step 8: param_assign, param_binding — switch to `resolve_variable()` + `resolve_global_array()`
- Files: `param_assign.py`, `param_binding.py`
- Old: `dataflow.resolve('caller:param')`, `dataflow.resolve(param)`, `dataflow.resolve('<garray:param>')`
- New: `dataflow.resolve_variable(param, caller)`, `dataflow.resolve_global_array(param)`
- ~15 lines each

### Step 9: dataflow.resolve_returned_field — remove old store suffix fallback
- File: `dataflow.py`
- Old: `self.state.resolve(...)` + `self.state.targets.items()` suffix scan
- New: Only `self.store.resolve_struct_field(...)` + `self.store.struct_fields.items()` suffix scan
- ~8 lines removed

### Step 10: Delete old store and backward compat methods
- File: `dataflow.py`
- Delete: `VariableState.targets` field, `assign()`, `merge()`, `resolve()`
- Delete: `DataflowEngine.assign()`, `resolve()`, `merge()`, `targets` property
- `VariableState.var_types` is kept (still used by other modules)
- ~30 lines deleted

## Files Touched

| File | Change |
|---|---|
| `dataflow.py` | +60 lines new API, -30 lines deleted old store |
| `local_fp_tracker.py` | ~15 lines changed |
| `field_call.py` | -15 lines deleted (Path B + else), ~10 lines changed |
| `direct_call_fp.py` | ~15 lines simplified |
| `array_call.py` | ~10 lines simplified |
| `direct_assign.py` | ~5 lines changed |
| `cast_assign.py` | ~5 lines changed |
| `helpers.py` | ~5 lines changed |
| `param_dispatch.py` | ~10 lines changed |
| `param_assign.py` | ~15 lines changed |
| `param_binding.py` | ~15 lines changed |
| `initializer_assign.py` | ~5 lines changed |

Total: 12 files, ~180 lines changed, ~60 lines deleted.

## Verification

After each step, run `pytest tests/ -q` to verify no regressions. The two pre-existing failures (`fnptr-global-struct recall=98.53%`, `test_et_bench_report`) are unrelated to this change and should remain unchanged.

## Scope

This design covers only the removal of old store and Path B. It does NOT:
- Add new detection capabilities
- Change the orchestrator pipeline ordering
- Modify SymbolTable or AST parsing
- Change test fixtures or ground truth
