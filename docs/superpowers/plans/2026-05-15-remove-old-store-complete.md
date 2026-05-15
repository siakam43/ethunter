# Complete Old Store Removal — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete `VariableState.targets` dict entirely. Path B is already deleted; this removes the storage layer.

**Architecture:** Add `_param_bindings` dict to `DataflowEngine` for the one key pattern not covered by ScopedStore (`call_name:param_name` mappings). Replace 6 remaining old store consumers one by one. Then delete old store + all backward compat wrappers. Remove all `else` branches in hasattr guards.

**Tech Stack:** Python 3.11, pytest

---

### Task 1: Add `_param_bindings` store to DataflowEngine

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`

- [ ] **Step 1: Add `_param_bindings` field and `add_param_binding()` method**

Add field to `DataflowEngine` (after `call_site_targets` on line 67):

```python
    # Param binding storage: (call_name, param_name) -> {targets}
    _param_bindings: dict[tuple[str, str], set[str]] = field(default_factory=dict)
```

Add method after `register_callback()` (current line ~89):

```python
    def add_param_binding(self, call_name: str, param_name: str, target: str) -> None:
        """Register a call-site param binding. Replaces dataflow.assign('fn:param', target)."""
        key = (call_name, param_name)
        if key not in self._param_bindings:
            self._param_bindings[key] = set()
        self._param_bindings[key].add(target)
```

- [ ] **Step 2: Run tests to verify no regressions**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "feat: add _param_bindings store to DataflowEngine

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Migrate param_binding writers to _param_bindings

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py`

- [ ] **Step 1: Replace `dataflow.assign(f'{call_name}:{pname}', target)` calls**

All instances of `dataflow.assign(f'{call_name}:{pname}', target)` → `dataflow.add_param_binding(call_name, pname, target)`. Also remove the adjacent `dataflow.assign(pname, target)` lines (bare keys — only needed for old store lookups).

Lines 93-94:
```python
# Old:
dataflow.assign(f'{call_name}:{pname}', target)
dataflow.assign(pname, target)
# New:
dataflow.add_param_binding(call_name, pname, target)
```

Lines 102-103: same pattern.

Lines 122-123:
```python
# Old:
dataflow.assign(f'{call_name}:{pname}', t)
dataflow.assign(pname, t)
# New:
dataflow.add_param_binding(call_name, pname, t)
```

Lines 195-196: same pattern → `dataflow.add_param_binding(call_name, pname, target)`.

- [ ] **Step 2: Replace `dataflow.assign(f'<gstruct:...>')` writes in `_resolve_fields()`**

Remove the old store writes for `<gstruct:...>` (line 243), `<struct:...>` (lines 253, 270-271) — the new store equivalents already exist in the same blocks.

Line 243: delete `dataflow.assign(f'<gstruct:{field_path}>', t)`
Lines 253: delete `dataflow.assign(f'<struct:{field_path}>', t)`
Lines 270-271: delete `dataflow.assign(f'<struct:{field_path}>', t)` and `dataflow.assign(f'<struct:{field_name}>', t)`

- [ ] **Step 3: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py
git commit -m "refactor: migrate param_binding writers to _param_bindings

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Migrate param_assign writers to _param_bindings

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py`

- [ ] **Step 1: Replace `dataflow.assign(f'{call_name}:{pname}', target)` calls**

Same pattern as Task 2. Replace at lines 548-549, 556-557, 657-658:

```python
# Old:
dataflow.assign(f'{call_name}:{pname}', target)
dataflow.assign(pname, target)
# New:
dataflow.add_param_binding(call_name, pname, target)
```

- [ ] **Step 2: Remove old store `<gstruct:...>` and `<struct:...>` writes**

Remove old store assigns at lines 690, 699, 714-715 — new store equivalents already exist:

Line 690: delete `dataflow.assign(f'<gstruct:{field_path}>', t)`
Line 699: delete `dataflow.assign(f'<struct:{field_path}>', t)`
Lines 714-715: delete `dataflow.assign(f'<struct:{field_path}>', t)` and `dataflow.assign(f'<struct:{field_name}>', t)`

- [ ] **Step 3: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py
git commit -m "refactor: migrate param_assign writers to _param_bindings

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Migrate rebuild_param_mappings to _param_bindings

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`

- [ ] **Step 1: Update `rebuild_param_mappings()` to read from `_param_bindings`**

Replace the current implementation that reads from `state.targets`:

```python
    def rebuild_param_mappings(self) -> dict[str, set[str]]:
        """Rebuild param_name -> {targets} mapping from _param_bindings."""
        result: dict[str, set[str]] = {}
        for (call_name, param_name), vals in self._param_bindings.items():
            result.setdefault(param_name, set()).update(vals)
        return result
```

- [ ] **Step 2: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "refactor: migrate rebuild_param_mappings to _param_bindings

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Migrate remaining old store consumers

**Files:**
- Modify: `src/ethunter/analyzer/param_dispatch.py`
- Modify: `src/ethunter/analyzer/param_binding.py`
- Modify: `src/ethunter/analyzer/initializer_assign.py`
- Modify: `src/ethunter/analyzer/direct_assign.py`

- [ ] **Step 1: param_dispatch.py — remove backward compat branch**

Delete the `else` branch (lines 34-39) that iterates `dataflow.targets.items()`. The `if hasattr(dataflow, 'rebuild_param_mappings')` branch now always runs and uses `_param_bindings`:

```python
# Before:
param_mappings: dict[str, set[str]] = {}
if hasattr(dataflow, 'rebuild_param_mappings'):
    param_mappings = dataflow.rebuild_param_mappings()
else:
    for key, vals in dataflow.targets.items():
        ...

# After:
param_mappings = dataflow.rebuild_param_mappings()
```

- [ ] **Step 2: param_binding.py — replace `_resolve_fields` targets.items()**

In `_resolve_fields()` (line 222), replace:

```python
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            p = key.split(':')[-1]
            if p not in param_mappings:
                param_mappings[p] = set()
            param_mappings[p].update(vals)
```

With:
```python
    param_mappings.update(dataflow.rebuild_param_mappings())
```

(Keep the existing `param_mappings: dict[str, set[str]] = {}` init on line 219.)

- [ ] **Step 3: initializer_assign.py — replace `dataflow.targets.items()` check**

Line 420:
```python
# Old:
has_gstruct = any(
    k.startswith(f'<gstruct:{arg_name}.') and bool(v)
    for k, v in dataflow.targets.items()
)
# New:
has_gstruct = any(
    k.startswith(f'gstruct:{arg_name}.') and bool(v)
    for k, v in dataflow.store.struct_fields.items()
)
```

- [ ] **Step 4: direct_assign.py — remove Pass2 `dataflow.targets` writes**

Lines 109 and 125: remove `dataflow.targets[f'<var>:{enclosing}:{var_name}'] = set()` and the check on line 125. The Pass2 re-resolve still works through `dataflow.resolve_variable()` which reads from `func_vars`.

Line 109: delete `dataflow.targets[f'<var>:{enclosing}:{var_name}'] = set()`
Line 110: delete `if hasattr(dataflow, 'store'):` block (lines 110-111) — `func_vars.setdefault` is not needed when we're just re-resolving
Line 125: `if targets and f'<var>:{enclosing}:{var_name}' not in dataflow.targets:` → `if targets:` (remove the targets check, just check targets is non-empty)

- [ ] **Step 5: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_dispatch.py src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/direct_assign.py
git commit -m "refactor: migrate remaining old store consumers

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Remove backward compat branches and redundant old store writes

**Files:**
- Modify: `src/ethunter/analyzer/direct_assign.py`
- Modify: `src/ethunter/analyzer/cast_assign.py`
- Modify: `src/ethunter/analyzer/helpers.py`
- Modify: `src/ethunter/analyzer/field_call.py`
- Modify: `src/ethunter/analyzer/initializer_assign.py`
- Modify: `src/ethunter/analyzer/direct_call_fp.py`
- Modify: `src/ethunter/analyzer/array_call.py`
- Modify: `src/ethunter/analyzer/local_fp_tracker.py`
- Modify: `src/ethunter/analyzer/param_assign.py`
- Modify: `src/ethunter/analyzer/param_binding.py`

- [ ] **Step 1: Remove redundant `dataflow.assign()` calls**

In each file, remove `dataflow.assign(key, target)` lines that have a corresponding new store equivalent on the next line. The pattern is:

```python
# Old (remove the dataflow.assign line):
dataflow.assign(f'<gstruct:{field_path}>', target)
dataflow.store.assign_struct_field(f'gstruct:{base}.{tail}', target, filepath)

# New (keep only new store):
dataflow.store.assign_struct_field(f'gstruct:{base}.{tail}', target, filepath)
```

Files and lines to clean:
- `field_call.py`: remove `dataflow.assign('<gstruct:...>', ...)` at lines 69, 230
- `initializer_assign.py`: remove `dataflow.assign('<gstruct:...>', ...)` at lines 70, 77; remove `dataflow.assign('<garray:...>', ...)` at lines 269, 280, 291, 300, 452, 459
- `helpers.py`: remove all `dataflow.assign(var_name, target)` and `dataflow.assign(var_name, t)` at lines 116, 121, 127, 133
- `direct_assign.py`: remove `dataflow.assign(f'<var>:..., ...)', ...)` at line 30; remove `dataflow.assign(var_name, target)` at line 31
- `cast_assign.py`: remove `dataflow.assign(f'<var>:..., ...)', ...)` at line 29; remove `dataflow.assign(var_name, target)` at line 30

- [ ] **Step 2: Remove `else dataflow.resolve(...)` backward compat branches in hasattr guards**

In each file, simplify the hasattr guard pattern:

```python
# Old:
targets = (dataflow.resolve_variable(target) if hasattr(dataflow, 'resolve_variable') else dataflow.resolve(target))

# New:
targets = dataflow.resolve_variable(target)
```

Files and lines:
- `direct_assign.py`: lines 50, 72, 85, 106, 123 — simplify all 5 hasattr guards
- `helpers.py`: lines 118, 130 — simplify
- `direct_call_fp.py`: remove the entire `else` backward compat block (lines ~48-66), keep only the `if hasattr(dataflow, 'resolve_variable'):` path
- `array_call.py`: remove `else` backward compat block, keep only `if hasattr` path
- `local_fp_tracker.py`: remove `else` backward compat block in `_resolve_and_store`
- `field_call.py`: remove `else` backward compat block in `_visit()`, keep only `if hasattr` path
- `param_assign.py`: remove backward compat branches at lines ~571-573 and ~708-712
- `param_binding.py`: remove backward compat branches at lines ~113-115 and ~264-268
- `initializer_assign.py`: remove backward compat branch at line 422

- [ ] **Step 3: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/
git commit -m "refactor: remove backward compat branches and redundant old store writes

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 7: Delete VariableState.targets and backward compat methods

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`

- [ ] **Step 1: Verify no remaining old store consumers**

```bash
grep -rn "dataflow\.targets\b" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "dataflow\.resolve(" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "dataflow\.assign(" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "dataflow\.merge(" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "state\.targets\b" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "state\.resolve(" src/ethunter/ --include="*.py" | grep -v __pycache__
```

Expected: only matches in `dataflow.py` itself (the methods being deleted, and `resolve_returned_field`).

- [ ] **Step 2: Delete VariableState.targets and related methods**

Replace `VariableState` class (lines 10-31):

```python
@dataclass
class VariableState:
    """Track variable type info and callback registrations."""
    var_types: dict[str, str] = field(default_factory=dict)

    def register_callback(self, func_name: str) -> None:
        """No-op: registered_callbacks was dead code."""
```

- [ ] **Step 3: Delete DataflowEngine backward compat wrappers**

Delete `assign()` (lines ~76-78), `resolve()` (lines ~79-81), `merge()` (lines ~82-84), `targets` property (lines ~85-88).

- [ ] **Step 4: Remove old store reads from resolve_returned_field**

Delete the `self.state.resolve(f'<gstruct:{field_path}>')` and `self.state.targets.items()` suffix fallback lines. Keep only ScopedStore reads.

- [ ] **Step 5: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "refactor: delete VariableState.targets and old store

Remove targets dict, assign/merge/resolve from VariableState.
Remove backward compat wrappers from DataflowEngine.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Verification

After all tasks complete, run:

```bash
.venv/bin/python -m pytest tests/ -q
```

Baseline: 196 passed, 2 pre-existing failures (`fnptr-global-struct recall=98.53%`, `test_et_bench_report`).

Acceptance: same count, same 2 pre-existing failures. No new failures.
