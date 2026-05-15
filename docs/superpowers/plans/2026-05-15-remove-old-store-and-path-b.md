# Remove Old Store and Path B — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete `VariableState.targets` (old store) and Path B (legacy suffix scan in `field_call._visit()`), migrating all consumers to unified resolution methods on `DataflowEngine` backed by `ScopedStore`.

**Architecture:** Build 5 semantic query methods on `DataflowEngine` (`resolve_variable`, `resolve_struct_field_call`, `resolve_global_array`, `rebuild_param_mappings`, and updated `resolve_returned_field`). All 12 analyzer files switch from `dataflow.resolve(key)` / `dataflow.targets` to these methods. Then delete the old store, `VariableState.assign/merge/resolve`, and backward compat wrappers.

**Tech Stack:** Python 3.11, tree-sitter, pytest

---

### Task 1: Add unified resolution API to DataflowEngine

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`

- [ ] **Step 1: Add `resolve_variable()` method**

Add after `merge()` (line 83). Insert before `register_callback()` (line 89):

```python
    def resolve_variable(self, var_name: str, caller_func: str | None = None,
                         local_fp_mapping: dict | None = None) -> set[str]:
        """Resolve a variable name to function targets.

        Priority: func-scoped > global > local_fp_mapping.
        """
        if caller_func:
            targets = self.store.resolve_func_var(caller_func, var_name)
            if targets:
                return targets
            targets = self.store.resolve_func_var('<global>', var_name)
            if targets:
                return targets
        targets = self.store.resolve_func_var('<global>', var_name)
        if targets:
            return targets
        if local_fp_mapping:
            targets = local_fp_mapping.get(var_name, set())
            if targets:
                return targets
        return set()
```

- [ ] **Step 2: Add `resolve_global_array()` method**

Append after `resolve_variable()`:

```python
    def resolve_global_array(self, name: str) -> set[str]:
        """Resolve a global function pointer array name to targets."""
        return self.store.resolve_global_array(name)
```

- [ ] **Step 3: Add `resolve_struct_field_call()` method**

Append after `resolve_global_array()`. This method needs a `FieldResolver` — construct it lazily:

```python
    def resolve_struct_field_call(self, field_path: str, base_var: str,
                                  caller_func: str | None, filepath: str,
                                  symbol_table, local_fp_mapping: dict | None = None,
                                  pointer_resolutions: dict | None = None) \
            -> tuple[set[str], 'Confidence | None', 'Evidence | None']:
        """Resolve a struct field function pointer call.

        Uses FieldResolver 4-tier chain + garray fallback.
        All backed by ScopedStore — no old-store dependency.
        """
        from ethunter.analyzer.field_resolver import FieldResolver
        from ethunter.graph.model import Confidence, Evidence

        resolver = FieldResolver(
            store=self.store,
            dataflow=self,
            symbol_table=symbol_table,
            local_fp_mapping=local_fp_mapping or {},
            pointer_resolutions=pointer_resolutions or {},
        )
        targets, confidence, evidence = resolver.resolve_field_call(
            field_path, base_var, caller_func, filepath)
        # garray fallback (was Path B line 275)
        garray_targets = self.store.resolve_global_array(base_var)
        if garray_targets:
            targets.update(garray_targets)
            if confidence is None:
                confidence, evidence = Confidence.LOW, Evidence('garray_fallback')
        return targets, confidence, evidence
```

- [ ] **Step 4: Add `rebuild_param_mappings()` method**

Append after `resolve_struct_field_call()`:

```python
    def rebuild_param_mappings(self) -> dict[str, set[str]]:
        """Rebuild param_name -> {targets} mapping from func_vars.

        Replaces: for key, vals in dataflow.targets.items():
                     if ':' in key and not key.startswith('<'):
                         param_name = key.split(':')[-1]
                         ...
        """
        result: dict[str, set[str]] = {}
        for (func, var), vals in self.store.func_vars.items():
            result.setdefault(var, set()).update(vals)
        return result
```

- [ ] **Step 5: Run tests to verify no regressions**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures (unchanged).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "feat: add unified resolution API to DataflowEngine

- resolve_variable(): func-scoped > global > local_fp_mapping
- resolve_global_array(): ScopedStore + initializer fallback
- resolve_struct_field_call(): FieldResolver 4-tier + garray
- rebuild_param_mappings(): func_vars iteration

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Migrate local_fp_tracker to resolve_struct_field_call

**Files:**
- Modify: `src/ethunter/analyzer/local_fp_tracker.py`

- [ ] **Step 1: Replace `_resolve_and_store` old store calls**

In `_resolve_and_store()` (lines 68-96), replace the resolve chain with a single call to `dataflow.resolve_struct_field_call()`. The function also needs `symbol_table` to pass through:

```python
    def _resolve_and_store(
        var_name: str,
        field_expr: ts.Node,
        mapping: dict[str, set[str]],
        dataflow: VariableState,
        symbol_table=None,
    ) -> None:
        """Build dataflow key from field expression and resolve targets."""
        field_path = extract_field_path(field_expr)
        if not field_path:
            return
        base_var = field_path.split('.')[0]
        targets, _, _ = dataflow.resolve_struct_field_call(
            field_path, base_var, None, '',
            symbol_table=symbol_table,
        )
        if targets:
            if var_name not in mapping:
                mapping[var_name] = set()
            mapping[var_name].update(targets)
```

- [ ] **Step 2: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/local_fp_tracker.py
git commit -m "refactor: migrate local_fp_tracker to resolve_struct_field_call()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Delete Path B and unify field_call resolution

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py`

- [ ] **Step 1: Remove Path B and else branch in `_visit()`**

In `analyze()` function, `_visit()` inner function (lines 256-361):

Replace lines 264-289 (from `targets = set()` through the end of the else branch):

Old code (lines 264-289):
```python
                    targets = set()
                    confidence = Confidence.MEDIUM
                    evidence = Evidence('field_call_resolution')
                    base_var = field_path.split('.')[0]

                    # 4-tier resolver
                    if resolver is not None:
                        targets, confidence, evidence = \
                            resolver.resolve_field_call(field_path, base_var, caller, filepath)
                        # Legacy fallback: suffix scan for data not in new store
                        if '.' in field_path:
                            garray_targets = dataflow.resolve(f'<garray:{base_var}>')
                            if garray_targets:
                                targets.update(garray_targets)
                            parts = field_path.split('.')
                            for i in range(1, len(parts)):
                                sfx = '.'.join(parts[i:])
                                for key, vals in dataflow.targets.items():
                                    if key.endswith(f'.{sfx}>') and vals:
                                        targets.update(vals)
                            if targets and confidence is None:
                                confidence, evidence = Confidence.LOW, Evidence('legacy_fallback')
                    else:
                        targets = dataflow.resolve(f'<gstruct:{field_path}>')
                        if not targets:
                            targets = dataflow.resolve(f'<struct:{field_path}>')
```

New code:
```python
                    base_var = field_path.split('.')[0]
                    targets, confidence, evidence = dataflow.resolve_struct_field_call(
                        field_path, base_var, caller, filepath,
                        symbol_table=symbol_table,
                        local_fp_mapping=local_fp_mapping,
                        pointer_resolutions=pointer_resolutions,
                    )
                    if confidence is None:
                        confidence = Confidence.MEDIUM
                        evidence = Evidence('field_call_resolution')
```

- [ ] **Step 2: Remove FieldResolver construction in analyze()**

Delete lines 244-253 (the `resolver = None` / `if hasattr(dataflow, 'store'):` block that constructs FieldResolver):

Old code to delete:
```python
    # Build FieldResolver if store is available
    resolver = None
    if hasattr(dataflow, 'store'):
        resolver = FieldResolver(
            store=dataflow.store,
            dataflow=dataflow,
            symbol_table=symbol_table,
            local_fp_mapping=local_fp_mapping,
            pointer_resolutions=pointer_resolutions,
        )
```

- [ ] **Step 3: Remove unused import**

Delete line 26 (the `FieldResolver` import):
```python
from ethunter.analyzer.field_resolver import FieldResolver
```

- [ ] **Step 4: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "refactor: delete Path B, unify field_call via resolve_struct_field_call()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Migrate direct_call_fp to resolve_variable

**Files:**
- Modify: `src/ethunter/analyzer/direct_call_fp.py`

- [ ] **Step 1: Replace `_get_targets` resolve chain**

In `_get_targets()` (lines 31-57), replace the multi-step resolve chain with a single call:

Old code (lines 36-57):
```python
        targets = set()
        confidence, evidence = Confidence.MEDIUM, Evidence('flat_fp', source='dataflow')
        if caller_func:
            if hasattr(dataflow, 'store'):
                targets = dataflow.store.resolve_func_var(caller_func, var_name)
                if targets:
                    confidence, evidence = Confidence.HIGH, Evidence('scoped_fp', source='scoped_store')
                if not targets:
                    targets = dataflow.store.resolve_func_var('<global>', var_name)
                    if targets:
                        confidence, evidence = Confidence.HIGH, Evidence('global_fp', source='scoped_store')
            if not targets:
                targets = dataflow.resolve(f'<var>:{caller_func}:{var_name}')
                if targets:
                    confidence, evidence = Confidence.HIGH, Evidence('scoped_fp', source='dataflow')
        if not targets:
            targets = dataflow.resolve(var_name)
        if not targets:
            targets = local_mapping.get(var_name, set()).copy()
            if targets:
                confidence, evidence = Confidence.MEDIUM, Evidence('struct_field_init', source='local_fp_mapping')
        return targets, confidence, evidence
```

New code:
```python
        targets = dataflow.resolve_variable(var_name, caller_func, local_fp_mapping=local_mapping)
        if targets:
            if caller_func:
                confidence, evidence = Confidence.HIGH, Evidence('scoped_fp', source='scoped_store')
            else:
                confidence, evidence = Confidence.MEDIUM, Evidence('flat_fp', source='dataflow')
        else:
            confidence, evidence = Confidence.MEDIUM, Evidence('flat_fp', source='dataflow')
        return targets, confidence, evidence
```

- [ ] **Step 2: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/direct_call_fp.py
git commit -m "refactor: migrate direct_call_fp to resolve_variable()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Migrate array_call to resolve_global_array

**Files:**
- Modify: `src/ethunter/analyzer/array_call.py`

- [ ] **Step 1: Read current array_call.py**

The resolve chain in `analyze()` is at lines 35-45. Read the file to get exact current code.

- [ ] **Step 2: Replace the 4-step resolve chain**

Old code (lines 35-45):
```python
                    # Phase A: try ScopedStore first
                    targets = set()
                    if hasattr(dataflow, 'store'):
                        targets = dataflow.store.resolve_global_array(arr_name)
                    if not targets:
                        targets = dataflow.resolve(f'<garray:{arr_name}>')
                    if not targets:
                        targets = dataflow.resolve(arr_name)
                    if not targets:
                        targets = dataflow.resolve('<initializer>')
```

New code:
```python
                    targets = dataflow.resolve_global_array(arr_name)
```

- [ ] **Step 3: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/array_call.py
git commit -m "refactor: migrate array_call to resolve_global_array()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Migrate direct_assign, cast_assign, helpers resolve() calls

**Files:**
- Modify: `src/ethunter/analyzer/direct_assign.py`
- Modify: `src/ethunter/analyzer/cast_assign.py`
- Modify: `src/ethunter/analyzer/helpers.py`
- Modify: `src/ethunter/analyzer/initializer_assign.py`

- [ ] **Step 1: direct_assign.py — replace `dataflow.resolve(target)` calls**

All `dataflow.resolve(target)` calls resolve alias chains (e.g., `fp2 = fp1` where fp1 is a known variable). Replace each with `dataflow.resolve_variable(target)`.

Lines 50, 72, 85, 106, 123 — each `dataflow.resolve(target)` → `dataflow.resolve_variable(target)`.

- [ ] **Step 2: cast_assign.py — check for resolve calls**

`cast_assign.py` does not call `dataflow.resolve()` — no changes needed in this file beyond future Task 10 cleanup.

- [ ] **Step 3: helpers.py — replace `dataflow.resolve(target)` calls**

In `handle_init_declarator()`, lines 118 and 130 — each `dataflow.resolve(target)` → `dataflow.resolve_variable(target)`.

- [ ] **Step 4: initializer_assign.py — replace `dataflow.resolve()` call**

Line 422 — `dataflow.resolve(f'<garray:{arg_name}>')` → `dataflow.resolve_global_array(arg_name)`.

- [ ] **Step 5: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/direct_assign.py src/ethunter/analyzer/cast_assign.py src/ethunter/analyzer/helpers.py src/ethunter/analyzer/initializer_assign.py
git commit -m "refactor: migrate alias chain resolve to resolve_variable()/resolve_global_array()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 7: Migrate param_dispatch to rebuild_param_mappings

**Files:**
- Modify: `src/ethunter/analyzer/param_dispatch.py`

- [ ] **Step 1: Replace `dataflow.targets.items()` iteration**

Lines 33-39 — replace the dict iteration with the new method:

Old code (lines 33-39):
```python
    param_mappings: dict[str, set[str]] = {}
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            param_name = key.split(':')[-1]
            if param_name not in param_mappings:
                param_mappings[param_name] = set()
            param_mappings[param_name].update(vals)
```

New code:
```python
    param_mappings = dataflow.rebuild_param_mappings()
```

- [ ] **Step 2: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/param_dispatch.py
git commit -m "refactor: migrate param_dispatch to rebuild_param_mappings()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 8: Migrate param_assign and param_binding resolve() calls

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py`
- Modify: `src/ethunter/analyzer/param_binding.py`

- [ ] **Step 1: param_assign.py — replace resolve calls**

Lines 569-571:
```python
df_targets = dataflow.resolve(f'{caller}:{target}')
if not df_targets:
    df_targets = dataflow.resolve(target)
```
→ `df_targets = dataflow.resolve_variable(target, caller)`

Lines 701-705:
```python
df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
if not df_targets:
    df_targets = dataflow.resolve(param_name)
if not df_targets:
    df_targets = dataflow.resolve(f'<garray:{param_name}>')
```
→
```python
df_targets = dataflow.resolve_variable(param_name, fa.enclosing_func)
if not df_targets:
    df_targets = dataflow.resolve_global_array(param_name)
```

- [ ] **Step 2: param_binding.py — same replacements as param_assign**

Lines 111-113 → `dataflow.resolve_variable(target, caller)`

Lines 257-261:
```python
df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
if not df_targets:
    df_targets = dataflow.resolve(param_name)
if not df_targets:
    df_targets = dataflow.resolve(f'<garray:{param_name}>')
```
→
```python
df_targets = dataflow.resolve_variable(param_name, fa.enclosing_func)
if not df_targets:
    df_targets = dataflow.resolve_global_array(param_name)
```

- [ ] **Step 3: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py src/ethunter/analyzer/param_binding.py
git commit -m "refactor: migrate param_assign/binding to resolve_variable()+resolve_global_array()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 9: Remove old store from resolve_returned_field

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`

- [ ] **Step 1: Update `resolve_returned_field()`**

In `resolve_returned_field()` (lines 160-190), remove the old store reads (`self.state.resolve` and `self.state.targets.items()` fallback):

Old code (lines 166-190):
```python
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            # Exact match via store first, then old state fallback
            results.update(self.store.resolve_struct_field(f'gstruct:{field_path}'))
            targets = self.state.resolve(f'<gstruct:{field_path}>')
            results.update(targets)

            # Suffix fallback via store first, then old state
            parts = field_path.split('.')
            for i in range(1, len(parts)):
                suffix = '.'.join(parts[i:])
                before = len(results)
                for key, vals in self.store.struct_fields.items():
                    if key.endswith(f'.{suffix}') and vals:
                        results.update(vals)
                for key, vals in self.state.targets.items():
                    if key.endswith(f'.{suffix}>') and vals:
                        results.update(vals)
                if len(results) > before:
                    break

        return results
```

New code:
```python
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            results.update(self.store.resolve_struct_field(f'gstruct:{field_path}'))

            parts = field_path.split('.')
            for i in range(1, len(parts)):
                suffix = '.'.join(parts[i:])
                before = len(results)
                for key, vals in self.store.struct_fields.items():
                    if key.endswith(f'.{suffix}') and vals:
                        results.update(vals)
                if len(results) > before:
                    break

        return results
```

- [ ] **Step 2: Run tests**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures.

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "refactor: remove old store reads from resolve_returned_field()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 10: Delete old store and backward compat methods

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`
- Modify: `src/ethunter/analyzer/orchestrator.py` (possibly)

- [ ] **Step 1: grep for any remaining old store consumers**

```bash
grep -rn "dataflow\.targets\b" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "dataflow\.resolve(" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "dataflow\.assign(" src/ethunter/ --include="*.py" | grep -v __pycache__
grep -rn "dataflow\.merge(" src/ethunter/ --include="*.py" | grep -v __pycache__
```

Expected: no matches outside of dataflow.py itself. If any matches found, fix them first (migrate to the new API methods) before proceeding.

- [ ] **Step 2: Remove VariableState.targets, assign, merge, resolve**

Delete from `VariableState` (lines 13-28 in dataflow.py):
- Field: `targets: dict[str, set[str]] = field(default_factory=dict)` (line 13)
- Method: `assign()` (lines 16-19)
- Method: `merge()` (lines 21-25)
- Method: `resolve()` (lines 27-28)

Keep: `var_types: dict[str, str]` (line 15) and `register_callback()` (line 30).

`VariableState` should become:
```python
@dataclass
class VariableState:
    """Track variable type info and callback registrations."""
    var_types: dict[str, str] = field(default_factory=dict)

    def register_callback(self, func_name: str) -> None:
        """No-op: registered_callbacks was dead code."""
```

- [ ] **Step 3: Remove DataflowEngine backward compat wrappers**

Delete (lines 76-87 in dataflow.py):
- `assign()` method (lines 76-77)
- `resolve()` method (lines 79-80)
- `merge()` method (lines 82-83)
- `targets` property (lines 85-87)

- [ ] **Step 4: Run full test suite**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: 196 passed, 2 pre-existing failures (unchanged).

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "refactor: delete VariableState.targets and old store backward compat

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
