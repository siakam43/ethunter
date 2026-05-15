# Design: Complete Removal of Old Store (VariableState.targets)

**Date**: 2026-05-15
**Goal**: Delete `VariableState.targets` dict entirely. Path B is already deleted; this removes the storage layer it depended on.

## Problem

After Path B deletion, `VariableState.targets` persists with 6 remaining consumers:

| # | Location | Access | Purpose |
|---|---|---|---|
| R1 | `dataflow.py:167` | `state.targets.items()` | `rebuild_param_mappings`: aggregate `*:param` keys |
| R2 | `dataflow.py:265` | `state.targets.items()` | `resolve_returned_field` suffix fallback |
| R3 | `param_dispatch.py:35` | `dataflow.targets.items()` | backward compat param mappings |
| R4 | `param_binding.py:222` | `dataflow.targets.items()` | `_resolve_fields` rebuild param_mappings |
| R5 | `initializer_assign.py:420` | `dataflow.targets.items()` | check if arg has gstruct entries |
| R6 | `direct_assign.py:109,125` | `dataflow.targets[key]` | Pass2 write/check `<var>:...` keys |

All `dataflow.assign()` writes are also dual-written to ScopedStore. Deleting them removes only redundancy.

## Solution

Add a minimal `_param_bindings` dict to `DataflowEngine` for the one key pattern not covered by ScopedStore (`call_name:param_name`). Replace each consumer individually, then delete old store and all backward compat wrappers.

### New: `_param_bindings` store

```python
# DataflowEngine field:
_param_bindings: dict[tuple[str, str], set[str]]  # (call_name, param_name) -> {targets}

def add_param_binding(self, call_name: str, param_name: str, target: str) -> None:
    key = (call_name, param_name)
    self._param_bindings.setdefault(key, set()).add(target)
```

### Consumer replacements

| Consumer | Old | New |
|---|---|---|
| R1 `rebuild_param_mappings` | iterate `state.targets` | iterate `_param_bindings` |
| R2 `resolve_returned_field` | scan `state.targets` suffix | only ScopedStore suffix (data already dual-written) |
| R3 `param_dispatch` fallback | iterate `dataflow.targets` | use `rebuild_param_mappings()` (only DataflowEngine path) |
| R4 `param_binding._resolve_fields` | iterate `dataflow.targets` | use `rebuild_param_mappings()` |
| R5 `initializer_assign` | iterate `dataflow.targets` for gstruct check | iterate `store.struct_fields` |
| R6 `direct_assign` Pass2 | write/check `dataflow.targets['<var>:...']` | write/check `store.func_vars` |

### Writers to update

All `dataflow.assign(f'{caller}:{pname}', target)` → `dataflow.add_param_binding(caller, pname, target)`
All `dataflow.assign(pname, target)` (bare key) → remove (was only for old store lookups)
All `dataflow.assign(f'<gstruct:...>')` → remove (new store equivalent exists)
All `dataflow.assign(f'<struct:...>')` → remove (new store equivalent exists)
All `dataflow.assign(f'<garray:...>')` → remove (new store equivalent exists)
All `dataflow.assign(f'<var>:...')` → remove (new store equivalent exists)

### Deletions

- `VariableState.targets` dict field
- `VariableState.assign()`, `merge()`, `resolve()`
- `DataflowEngine.assign()`, `resolve()`, `merge()`, `targets` property
- All `else dataflow.resolve(...)` backward compat branches in hasattr guards
- `VariableState` kept only for `var_types` + `register_callback()`

### Files touched

| File | Change |
|---|---|
| `dataflow.py` | +15 (`_param_bindings`), -50 (old store + wrappers) |
| `param_binding.py` | ~15 lines (assign → add_param_binding, targets → rebuild) |
| `param_assign.py` | ~10 lines (assign → add_param_binding) |
| `param_dispatch.py` | -5 lines (remove backward compat branch) |
| `initializer_assign.py` | ~5 lines (targets check → struct_fields) |
| `direct_assign.py` | -8 lines (remove old store writes/checks) |
| `field_call.py` | -4 lines (remove old store assigns) |
| `helpers.py` | -6 lines (remove old store assigns) |
| `cast_assign.py` | -2 lines (remove old store assigns) |

Total: 9 files, ~80 lines changed, ~60 lines deleted.

## Verification

Run `pytest tests/ -q`. Baseline: 196 passed, 2 pre-existing failures. No new failures.
