# Scoped Dataflow Architecture Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the global flat `VariableState.targets` dict with a function-scoped `ScopedStore`, unify dataflow key conventions, replace field_call's 15-layer fallback stack with a strategy chain, complete the param_assign→param_binding migration, and add a confidence model to CallEdge.

**Architecture:** Four-phase migration. Phase A adds ScopedStore with dual-write to maintain backward compat. Phase B unifies key formats and adds type tracking. Phase C implements the FieldResolver strategy chain, removes suffix scans, and deletes param_assign.analyze(). Phase D adds confidence/evidence fields to CallEdge with confidence-based dedup.

**Tech Stack:** Python 3.11, pytest, tree-sitter-c

---

## File Map

| File | Phase | Role |
|---|---|---|
| `src/ethunter/analyzer/scoped_store.py` | A (new) | ScopedStore dataclass — func_vars, struct_fields, global_arrays, vtable_entries |
| `src/ethunter/analyzer/dataflow.py` | A | Add `store: ScopedStore` to DataflowEngine |
| `src/ethunter/analyzer/direct_assign.py` | A, D | Write `func_vars[(func, var)]`; D: remove bare-name backward compat |
| `src/ethunter/analyzer/cast_assign.py` | A, D | Same as direct_assign |
| `src/ethunter/analyzer/initializer_assign.py` | A, B | Write struct_fields + global_arrays; B: use field_tail |
| `src/ethunter/analyzer/param_binding.py` | A, B | Write func_vars; B: use field_tail in _resolve_fields |
| `src/ethunter/analyzer/field_call.py` | A, B, C, D | Split Pass 1/2; B: field_tail; C: FieldResolver; D: confidence |
| `src/ethunter/analyzer/direct_call_fp.py` | A, D | Read func_vars; D: confidence |
| `src/ethunter/analyzer/array_call.py` | A, D | Read global_arrays; D: confidence |
| `src/ethunter/analyzer/param_dispatch.py` | A, D | Read func_vars + call_site_targets; D: confidence |
| `src/ethunter/analyzer/orchestrator.py` | A, B, C, D | Pipeline reorder; C: remove param_assign.analyze(); D: confidence dedup |
| `src/ethunter/analyzer/symbol_table.py` | B | Add `_func_var_types`, `record_func_var_type()`, `get_func_var_type()` |
| `src/ethunter/analyzer/param_helpers.py` | B | Add `_collect_param_types()` |
| `src/ethunter/analyzer/field_resolver.py` | C (new) | FieldResolver + 8 ResolutionStrategy classes |
| `src/ethunter/analyzer/param_assign.py` | C | Delete `analyze()` (~400 lines); retain `_register_phase()` |
| `src/ethunter/graph/model.py` | D | Add `confidence` + `evidence` fields to CallEdge |
| `src/ethunter/analyzer/direct_call.py` | D | Add confidence annotation |
| `src/ethunter/analyzer/callback_reg.py` | D | Add confidence annotation |
| `src/ethunter/analyzer/dlsym_fp.py` | D | Add confidence annotation |
| `tests/test_et_bench.py` | A-D | FPR ceiling updates; min_confidence assertions (Phase D) |
| `tests/test_scoped_store.py` | A (new) | Unit tests for ScopedStore |
| `tests/test_field_resolver.py` | C (new) | Unit tests for each ResolutionStrategy |

---

## Phase A: ScopedStore — New Data Model

### Task A1: Create ScopedStore + Add to DataflowEngine

**Files:**
- Create: `src/ethunter/analyzer/scoped_store.py`
- Modify: `src/ethunter/analyzer/dataflow.py:32-67`
- Create: `tests/test_scoped_store.py`

- [ ] **Step 1: Write ScopedStore dataclass**

```python
# src/ethunter/analyzer/scoped_store.py
"""Function-scoped dataflow storage.

Replaces the flat VariableState.targets dict with four separate stores:
  - func_vars: function-scoped variable → targets
  - struct_fields: struct field → targets (global, inherently cross-function)
  - global_arrays: global array → targets
  - vtable_entries: vtable field → targets (reserved for future vtable support)
"""

from __future__ import annotations
from dataclasses import dataclass, field


@dataclass
class ScopedStore:
    """Function-scoped variable → targets mapping.

    Keys are ALWAYS (func_name, var_name) tuples for func_vars.
    Cross-function information flows through explicit bridges
    (call_site_targets, param_fields, ret_fields), not through
    shared variable names.
    """
    # (func_name, var_name) -> {target_functions}
    func_vars: dict[tuple[str, str], set[str]] = field(default_factory=dict)

    # Struct field targets: "gstruct:<path>" -> {target_functions}
    # Path is either "<base_var>.<field_tail>" or "<struct_type>.<field_tail>"
    # where field_tail is the field path WITHOUT the base variable name
    struct_fields: dict[str, set[str]] = field(default_factory=dict)

    # Global array targets: "garray:<var_name>" -> {target_functions}
    global_arrays: dict[str, set[str]] = field(default_factory=dict)

    # Vtable entries: "vtable:<struct_type>.<field_name>" -> {target_functions}
    vtable_entries: dict[str, set[str]] = field(default_factory=dict)

    # Struct variable aliases: var_name -> struct_type_or_resolved_name
    # Populated by initializer_assign during global struct initialization
    aliases: dict[str, str] = field(default_factory=dict)

    # --- func_vars helpers ---

    def assign_func_var(self, func: str, var: str, target: str) -> None:
        """Assign a target to a function-scoped variable."""
        key = (func, var)
        if key not in self.func_vars:
            self.func_vars[key] = set()
        self.func_vars[key].add(target)

    def resolve_func_var(self, func: str, var: str) -> set[str]:
        """Resolve targets for a function-scoped variable."""
        return self.func_vars.get((func, var), set()).copy()

    # --- struct_fields helpers ---

    def assign_struct_field(self, key: str, target: str) -> None:
        """Assign a target to a struct field. Key format: 'gstruct:<path>'."""
        if key not in self.struct_fields:
            self.struct_fields[key] = set()
        self.struct_fields[key].add(target)

    def resolve_struct_field(self, key: str) -> set[str]:
        """Resolve targets for a struct field key."""
        return self.struct_fields.get(key, set()).copy()

    # --- global_arrays helpers ---

    def assign_global_array(self, name: str, target: str) -> None:
        """Assign a target to a global array. Key format: 'garray:<name>'."""
        key = f'garray:{name}'
        if key not in self.global_arrays:
            self.global_arrays[key] = set()
        self.global_arrays[key].add(target)

    def resolve_global_array(self, name: str) -> set[str]:
        """Resolve targets for a global array name."""
        return self.global_arrays.get(f'garray:{name}', set()).copy()

    # --- vtable_entries helpers ---

    def assign_vtable_entry(self, struct_type: str, field: str, target: str) -> None:
        """Assign a target to a vtable entry."""
        key = f'vtable:{struct_type}.{field}'
        if key not in self.vtable_entries:
            self.vtable_entries[key] = set()
        self.vtable_entries[key].add(target)

    def resolve_vtable_entry(self, struct_type: str, field: str) -> set[str]:
        """Resolve targets for a vtable field."""
        return self.vtable_entries.get(f'vtable:{struct_type}.{field}', set()).copy()

    # --- utility ---

    def compute_field_tail(self, field_path: str) -> str:
        """Extract field_tail from a full field path.

        'handler.cb' -> 'cb'
        'ctx.ext.alpn_select_cb' -> 'ext.alpn_select_cb'
        """
        if '.' in field_path:
            return field_path.split('.', 1)[1]
        return field_path
```

- [ ] **Step 2: Write unit tests for ScopedStore**

```python
# tests/test_scoped_store.py
"""Unit tests for ScopedStore."""
import pytest
from ethunter.analyzer.scoped_store import ScopedStore


class TestFuncVars:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_func_var("my_func", "cb", "handler_a")
        store.assign_func_var("my_func", "cb", "handler_b")
        assert store.resolve_func_var("my_func", "cb") == {"handler_a", "handler_b"}

    def test_different_funcs_isolated(self):
        store = ScopedStore()
        store.assign_func_var("func1", "cb", "handler_a")
        store.assign_func_var("func2", "cb", "handler_b")
        assert store.resolve_func_var("func1", "cb") == {"handler_a"}
        assert store.resolve_func_var("func2", "cb") == {"handler_b"}

    def test_unresolved_returns_empty(self):
        store = ScopedStore()
        assert store.resolve_func_var("nonexistent", "x") == set()


class TestStructFields:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        assert store.resolve_struct_field("gstruct:my_type.cb") == {"handler_a"}

    def test_unresolved_returns_empty(self):
        store = ScopedStore()
        assert store.resolve_struct_field("gstruct:nonexistent.field") == set()


class TestGlobalArrays:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_global_array("global_hooks", "hook_a")
        assert store.resolve_global_array("global_hooks") == {"hook_a"}


class TestVtableEntries:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_vtable_entry("my_vtable", "get_state", "state_impl")
        assert store.resolve_vtable_entry("my_vtable", "get_state") == {"state_impl"}


class TestFieldTail:
    def test_simple_field(self):
        store = ScopedStore()
        assert store.compute_field_tail("handler.cb") == "cb"

    def test_chained_field(self):
        store = ScopedStore()
        assert store.compute_field_tail("ctx.ext.alpn_select_cb") == "ext.alpn_select_cb"

    def test_no_dot(self):
        store = ScopedStore()
        assert store.compute_field_tail("cb") == "cb"
```

- [ ] **Step 3: Run tests to verify they fail (ScopedStore not imported yet)**

Run: `.venv/bin/python -m pytest tests/test_scoped_store.py -v`
Expected: 8 passed (the module is created in Step 1, tests exercise it)

- [ ] **Step 4: Add ScopedStore to DataflowEngine**

In `src/ethunter/analyzer/dataflow.py`, add to the `DataflowEngine` dataclass:

```python
# dataflow.py — add import at top
from ethunter.analyzer.scoped_store import ScopedStore

# In DataflowEngine dataclass, add after existing fields:
    # Scoped dataflow store (new — Phase A migration)
    store: ScopedStore = field(default_factory=ScopedStore)
```

- [ ] **Step 5: Run all tests to verify no regression**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All 56 et_bench tests pass (new store is present but no writer uses it yet)

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/scoped_store.py tests/test_scoped_store.py src/ethunter/analyzer/dataflow.py
git commit -m "feat: add ScopedStore to DataflowEngine with unit tests"
```

---

### Task A2: Dual-Write in All Producers

**Files:**
- Modify: `src/ethunter/analyzer/direct_assign.py:30-31`
- Modify: `src/ethunter/analyzer/cast_assign.py:29-30`
- Modify: `src/ethunter/analyzer/initializer_assign.py:30,34,224-249,399-403`
- Modify: `src/ethunter/analyzer/param_binding.py:90-91,98-99,115-116,184-185,229,234,241-242`
- Modify: `src/ethunter/analyzer/field_call.py:89-91`

**Strategy:** Each producer continues writing to old `dataflow.targets` AND adds a write to `dataflow.store`. No reader changes yet — et_bench output must be identical.

- [ ] **Step 1: Dual-write in direct_assign.py**

In `direct_assign.py`, after each `dataflow.assign(f'<var>:{enclosing}:{var_name}', target)` (line 30), add:

```python
# In the analyze() function, after the existing scoped write on line 30:
        dataflow.assign(f'<var>:{enclosing}:{var_name}', target)
        dataflow.assign(var_name, target)  # backward compat (existing)
        # NEW: dual-write to ScopedStore
        dataflow.store.assign_func_var(enclosing, var_name, target)
```

And on line 107 where alias chains write:
```python
                    dataflow.targets[f'<var>:{enclosing}:{var_name}'] = set()
                    # NEW: ensure ScopedStore entry exists
                    if (enclosing, var_name) not in dataflow.store.func_vars:
                        dataflow.store.func_vars[(enclosing, var_name)] = set()
```

- [ ] **Step 2: Dual-write in cast_assign.py**

Same pattern as direct_assign. After line 29:
```python
        dataflow.assign(f'<var>:{enclosing}:{var_name}', target)
        dataflow.assign(var_name, target)  # backward compat (existing)
        # NEW: dual-write to ScopedStore
        dataflow.store.assign_func_var(enclosing, var_name, target)
```

- [ ] **Step 3: Dual-write in initializer_assign.py**

For struct field writes (line 30):
```python
        dataflow.assign(f'<gstruct:{field_path}>', target)  # existing
        # NEW: dual-write to ScopedStore
        field_tail = dataflow.store.compute_field_tail(field_path)
        dataflow.store.assign_struct_field(f'gstruct:{field_path}', target)
```

For type-aware struct field writes (line 34):
```python
            dataflow.assign(f'<gstruct>:{struct_type}.{field_path}>', target)  # existing
            # NEW: dual-write
            dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_path}', target)
```

For global array writes (lines 224, 233, 242, 249):
```python
        dataflow.assign(f'<garray:{var_name}>', target)  # existing
        # NEW: dual-write
        dataflow.store.assign_global_array(var_name, target)
```

- [ ] **Step 4: Dual-write in param_binding.py**

In `analyze()`, for each `dataflow.assign(pname, target)` (lines 91, 99, 116, 185):

```python
                dataflow.assign(pname, target)  # existing backward compat
                # NEW: dual-write to ScopedStore func_vars
                dataflow.store.assign_func_var(call_name, pname, target)
```

Note: `call_name` is the callee function name (param_binding.py line 52). This is the same function that param_dispatch will use as `enclosing_func` when resolving.

In `_resolve_fields()`, for struct field writes (lines 229, 234, 241):
```python
                dataflow.assign(f'<gstruct:{field_path}>', t)  # existing
                # NEW: dual-write
                dataflow.store.assign_struct_field(f'gstruct:{field_path}', t)
```

And for `<struct:>` writes (lines 234, 241-242):
```python
                dataflow.assign(f'<struct:{field_path}>', t)  # existing (will be removed in Phase B)
                # NEW: dual-write to same gstruct key
                dataflow.store.assign_struct_field(f'gstruct:{field_path}', t)
```

- [ ] **Step 5: Dual-write in field_call.py Pass 1**

In `analyze()`, line 91:
```python
            dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)  # existing
            # NEW: dual-write
            dataflow.store.assign_struct_field(f'gstruct:{fa.field_path}', fa.resolved_value)
```

- [ ] **Step 6: Run et_bench to verify no regression**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: All 56 pass (identical output — dual-write doesn't change readers)

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/direct_assign.py src/ethunter/analyzer/cast_assign.py src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/field_call.py
git commit -m "feat: dual-write all producers to ScopedStore (Phase A)"
```

---

### Task A3: Switch Readers to ScopedStore

**Files:**
- Modify: `src/ethunter/analyzer/direct_call_fp.py:41-43`
- Modify: `src/ethunter/analyzer/array_call.py:36-42`
- Modify: `src/ethunter/analyzer/param_dispatch.py:31-37`
- Modify: `src/ethunter/analyzer/field_call.py:108-218`
- Modify: `src/ethunter/analyzer/dataflow.py` — remove `VariableState.targets`

**Strategy:** Switch readers one at a time, verify after each. Readers use ScopedStore FIRST, fall back to old `dataflow.targets` until all readers are switched. Then remove old targets.

- [ ] **Step 1: Switch direct_call_fp.py (simplest reader)**

Replace the resolution logic in `_get_targets()` (lines 41-43):

```python
    def _get_targets(var_name: str, caller_func: str | None = None) -> set[str]:
        """Resolve function targets for a variable name."""
        targets = set()
        if caller_func:
            # Phase A: try ScopedStore first, fall back to old targets
            targets = dataflow.store.resolve_func_var(caller_func, var_name)
            if not targets:
                targets = dataflow.resolve(f'<var>:{caller_func}:{var_name}')
        if not targets:
            targets = dataflow.resolve(var_name)
        if not targets:
            targets = local_mapping.get(var_name, set()).copy()
        return targets
```

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: All 56 pass

- [ ] **Step 2: Switch array_call.py**

Replace the resolution (lines 36-42):

```python
            targets = set()
            # Phase A: try ScopedStore first, fall back to old targets
            targets = dataflow.store.resolve_global_array(arr_name)
            if not targets:
                targets = dataflow.resolve(f'<garray:{arr_name}>')
            if not targets:
                targets = dataflow.resolve(arr_name)
            if not targets:
                targets = dataflow.resolve('<initializer>')
```

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: All 56 pass

- [ ] **Step 3: Switch param_dispatch.py**

Replace the param_mappings reconstruction (lines 31-37). Add ScopedStore lookup BEFORE the old dataflow iteration:

```python
    # Reconstruct param_mappings from ScopedStore func_vars (new) + old dataflow
    param_mappings: dict[str, set[str]] = {}
    # New: scan ScopedStore for (callee, param_name) entries
    for (func, pname), targets in dataflow.store.func_vars.items():
        if pname not in param_mappings:
            param_mappings[pname] = set()
        param_mappings[pname].update(targets)
    # Old: scan dataflow.targets for 'callee:pname' keys (dual-write safety)
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            p = key.split(':')[-1]
            if p not in param_mappings:
                param_mappings[p] = set()
            param_mappings[p].update(vals)
```

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: All 56 pass

- [ ] **Step 4: Switch field_call.py (most complex reader)**

In `_visit()`, for each resolution layer, add store lookup BEFORE the old dataflow lookup. For example, Layer 0 (type-aware, lines 110-116):

```python
                # Layer 0: type-aware key
                base_var = field_path.split('.')[0]
                struct_type = symbol_table.get_var_type(base_var)
                if struct_type:
                    # Phase A: try ScopedStore first
                    targets = dataflow.store.resolve_struct_field(
                        f'gstruct:{struct_type}.{field_path}')
                    if not targets:
                        targets = dataflow.resolve(f'<gstruct>:{struct_type}.{field_path}>')
                    if targets:
                        pass  # found — skip fallbacks
```

Similarly for Layer 1 (`<gstruct:path>`), Layer 2 (`<struct:path>`), etc. — add `dataflow.store.resolve_struct_field(key)` before each `dataflow.resolve(key)` call.

**Important**: Do NOT change the suffix-scan fallbacks yet. Those are removed in Phase C.

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: All 56 pass

- [ ] **Step 5: Remove old VariableState.targets AND fix param_dispatch scope**

After all readers are switched and verified, remove the backward-compat fallbacks:

1. In `direct_call_fp.py`: remove `dataflow.resolve(f'<var>:{caller_func}:{var_name}')` and `dataflow.resolve(var_name)` fallbacks; use only `dataflow.store.resolve_func_var(caller_func, var_name)`
2. In `array_call.py`: remove `dataflow.resolve(f'<garray:{arr_name}>')`, `dataflow.resolve(arr_name)`, `dataflow.resolve('<initializer>')` fallbacks; use only `dataflow.store.resolve_global_array(arr_name)`
3. In `param_dispatch.py`: **replace the ScopedStore aggregation loop from Step 3 with targeted lookup**:
```python
    # Replace the func_vars.items() loop with targeted lookup:
    param_mappings: dict[str, set[str]] = {}
    # Scan func_vars for the CURRENT enclosing function only
    for (func, pname), targets in dataflow.store.func_vars.items():
        if func == '<global>':
            continue  # skip non-function entries
        if pname not in param_mappings:
            param_mappings[pname] = set()
        param_mappings[pname].update(targets)
```
Note: this still aggregates across all functions (interim). The final fix replaces this entirely with:
```python
    # Phase C final: use call_site_targets + targeted func_vars lookup only
    # Remove the param_mappings aggregation loop
```
See Task C2 Step 2 for the final implementation.
4. In `field_call.py`: remove old `dataflow.resolve(...)` fallbacks (keep suffix scans for now)
5. In `dataflow.py`: remove `VariableState.targets` dict and its `assign`/`resolve` methods
6. In all producers: remove old `dataflow.assign(...)` calls (keep only ScopedStore writes)

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q`
Expected: All tests pass; et_bench recall ≥ 98.86%; FPR unchanged from baseline

- [ ] **Step 6: Update FPR baselines if needed**

Run `test_et_bench_report` and record the exact FPR values. If FPR is lower than current ceilings (due to cleaner data), update `fpr_ceilings` in `tests/test_et_bench.py` to match.

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/direct_call_fp.py src/ethunter/analyzer/array_call.py src/ethunter/analyzer/param_dispatch.py src/ethunter/analyzer/field_call.py src/ethunter/analyzer/dataflow.py src/ethunter/analyzer/direct_assign.py src/ethunter/analyzer/cast_assign.py src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/param_binding.py tests/test_et_bench.py
git commit -m "feat: switch all readers to ScopedStore, remove old VariableState.targets"
```

---

## Phase B: Unified Keys + Type Tracking

### Task B1: Unify Key Formats (gstruct: field_tail, remove struct:)

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py:30,34`
- Modify: `src/ethunter/analyzer/param_binding.py:229,234,241-242`
- Modify: `src/ethunter/analyzer/field_call.py:89-91,108-218`

**Strategy:** Replace `<struct:path>` keys with `gstruct:<var>.<field_tail>` format. Update all struct field writes to use the `field_tail` convention. Remove `<struct:>` reads from field_call.

- [ ] **Step 1: Update initializer_assign.py key format**

Change line 30 to use field_tail:
```python
        field_tail = dataflow.store.compute_field_tail(field_path)
        dataflow.store.assign_struct_field(f'gstruct:{var_name}.{field_tail}', target)
```

Change line 34 to use field_tail (struct type replaces var name):
```python
            field_tail = dataflow.store.compute_field_tail(field_path)
            dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}', target)
```

- [ ] **Step 2: Update param_binding._resolve_fields() key format**

Replace all `<struct:>` writes with `gstruct:` format using field_tail:

```python
        for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
            if fa.enclosing_func is None:
                continue
            field_path = fa.field_path
            field_name = field_path.split('.')[-1]
            field_tail = dataflow.store.compute_field_tail(field_path)

            if fa.value_node and fa.value_node.type == 'call_expression':
                # ... return value tracking (unchanged) ...
                for t in ret_targets:
                    dataflow.store.assign_struct_field(f'gstruct:{field_path}', t)
            elif fa.resolved_value is not None:
                param_name = fa.resolved_value
                targets = param_mappings.get(param_name, set())
                for t in targets:
                    dataflow.store.assign_struct_field(f'gstruct:{fa.enclosing_func}.{field_tail}', t)
                # ... resolve df_targets ...
                for t in df_targets:
                    dataflow.store.assign_struct_field(f'gstruct:{fa.enclosing_func}.{field_tail}', t)
                    dataflow.store.assign_struct_field(f'gstruct:{field_name}', t)
                # ... register_param_mapping (unchanged) ...
```

Note: the `<struct:{field_name}>` short key (bare field name) is removed. It was a source of FPs. The exact `gstruct:<var>.<field_tail>` key replaces it.

- [ ] **Step 3: Update field_call.py Pass 1 key format**

```python
            field_tail = dataflow.store.compute_field_tail(fa.field_path)
            dataflow.store.assign_struct_field(f'gstruct:{fa.field_path.split(".")[0]}.{field_tail}',
                                               fa.resolved_value)
```

- [ ] **Step 4: Remove `<struct:>` reads from field_call**

In `_visit()`, remove or comment out the `<struct:>` resolution layers (lines 121-122, 160-180). These keys are no longer written.

- [ ] **Step 4b: Populate aliases in initializer_assign**

When initializer_assign processes a global struct init like `Curl_ssl = Curl_ssl_openssl`, record the alias:

```python
# In initializer_assign, where struct alias assignments are processed:
dataflow.store.aliases[var_name] = resolved_type
```

This populates `ScopedStore.aliases` for `StructAliasLookup`.

- [ ] **Step 5: Run et_bench and verify**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All 56 pass; recall unchanged; FPR unchanged or slightly decreased

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/field_call.py
git commit -m "refactor: unify key format to gstruct:<var>.<field_tail>, remove <struct:> keys"
```

---

### Task B2: Add Type Tracking (SymbolTable + param_helpers + field_call)

**Files:**
- Modify: `src/ethunter/analyzer/symbol_table.py:112-132` — add `_func_var_types`, `record_func_var_type()`, `get_func_var_type()`
- Modify: `src/ethunter/analyzer/param_helpers.py` — add `_collect_param_types()`
- Modify: `src/ethunter/analyzer/field_call.py` — Pass 1 type collection
- Modify: `src/ethunter/analyzer/orchestrator.py` — pass `symbol_table` to `param_helpers.prepare()`
- Modify: `tests/test_et_bench.py` — `test_type_aware_key_isolates_different_struct_types` should now PASS

- [ ] **Step 1: Add _func_var_types to SymbolTable**

```python
# symbol_table.py — add to SymbolTable class

    def __init__(self):
        # ... existing fields ...
        self._func_var_types: dict[tuple[str, str], str] = {}

    def record_func_var_type(self, func: str, var: str, struct_type: str) -> None:
        """Record a function-scoped variable's struct type."""
        self._func_var_types[(func, var)] = struct_type

    def get_func_var_type(self, func: str | None, var: str) -> str | None:
        """Get struct type for a variable, checking func-scoped first,
        then global var types."""
        if func:
            result = self._func_var_types.get((func, var))
            if result:
                return result
        return self._var_types.get(var)
```

- [ ] **Step 2: Add _collect_param_types to param_helpers.py**

```python
# param_helpers.py — new function

def _collect_param_types(root_node, filepath, symbol_table):
    """Scan function definitions and record parameter struct types.

    For each function parameter declared as 'struct type_name *ptr',
    record (func_name, param_name) -> 'type_name' in symbol_table.
    """
    def _scan(node):
        if node.type == 'function_definition':
            # Find function declarator
            decl = _find_child(node, 'function_declarator')
            if not decl:
                for c in node.children:
                    if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                        d = _find_child(c, 'function_declarator')
                        if d:
                            decl = d
                            break
            if not decl:
                for child in node.children:
                    _scan(child)
                return

            fname, inner_decl = _find_func_name_from_decl(decl)
            if not fname:
                for child in node.children:
                    _scan(child)
                return

            # Extract parameter types
            plist = _find_child(inner_decl, 'parameter_list')
            if plist:
                for p in plist.children:
                    if p.type == 'parameter_declaration':
                        pname = _extract_param_name(p)
                        if not pname:
                            continue
                        # Look for struct type in the declaration
                        for tc in p.children:
                            if tc.type == 'type_identifier' and tc.text:
                                type_name = tc.text.decode('utf-8')
                                # Check if it's a struct type (via typedef or known struct)
                                if symbol_table.resolve_typedef(type_name):
                                    symbol_table.record_func_var_type(
                                        fname, pname, type_name)
                                break
                            # Handle 'struct type_name' pattern
                            if tc.type == 'struct_specifier':
                                for sc in tc.children:
                                    if sc.type == 'type_identifier' and sc.text:
                                        type_name = sc.text.decode('utf-8')
                                        symbol_table.record_func_var_type(
                                            fname, pname, type_name)
                                        break

        for child in node.children:
            _scan(child)

    _scan(root_node)
```

- [ ] **Step 3: Update param_helpers.prepare() to accept and call type collection**

```python
# param_helpers.py — update prepare() signature
def prepare(tree, filepath, engine, symbol_table=None):
    # ... existing func_params, func_fp_params collection ...

    # NEW: collect parameter types
    if symbol_table is not None:
        _collect_param_types(tree.root_node, filepath, symbol_table)
```

- [ ] **Step 4: Update orchestrator to pass symbol_table to prepare()**

```python
# orchestrator.py — in run_all_analyses()
    for filepath, tree in trees.items():
        param_helpers.prepare(tree, filepath, engine, symbol_table)
```

- [ ] **Step 5: Add type collection in field_call Pass 1**

In `field_call.py` Pass 1, when processing a field assignment, resolve the base variable's struct type and write a type-aware key:

```python
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            base_var = fa.field_path.split('.')[0]
            field_tail = dataflow.store.compute_field_tail(fa.field_path)

            # Always write exact key
            dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}',
                                               fa.resolved_value)

            # If type is known, also write type-aware key
            struct_type = symbol_table.get_func_var_type(fa.enclosing_func, base_var)
            if struct_type:
                dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}',
                                                   fa.resolved_value)
```

- [ ] **Step 6: Update test_type_aware_key_isolates_different_struct_types**

Remove `@pytest.mark.xfail` from the test (line 52 of test_et_bench.py). The type-aware key isolation should now work.

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_type_aware_key_isolates_different_struct_types -v`
Expected: PASS (no longer xfail)

- [ ] **Step 7: Run full et_bench**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: All 57 pass (56 + 1 previously xfailed test now passing)

- [ ] **Step 8: Commit**

```bash
git add src/ethunter/analyzer/symbol_table.py src/ethunter/analyzer/param_helpers.py src/ethunter/analyzer/field_call.py src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "feat: add function-scoped type tracking for params and local vars"
```

---

## Phase C: field_call Rewrite + Remove param_assign.analyze()

### Task C1: Implement FieldResolver with Strategy Chain

**Files:**
- Create: `src/ethunter/analyzer/field_resolver.py`
- Create: `tests/test_field_resolver.py`

- [ ] **Step 1: Implement ResolutionContext and ResolutionStrategy base**

```python
# src/ethunter/analyzer/field_resolver.py
"""Strategy-chain field resolver for struct field function pointer calls.

Replaces the 15-layer fallback stack in field_call._visit() with a chain of
ResolutionStrategy classes, each doing exact key lookups only.
"""

from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol


@dataclass
class ResolutionContext:
    """Immutable context passed to each strategy."""
    field_path: str       # e.g., "ctx.ext.alpn_select_cb"
    base_var: str         # e.g., "ctx"
    caller_func: str | None = None  # enclosing function


class ResolutionStrategy(Protocol):
    """Protocol for field resolution strategies.

    Each strategy does exact key lookups only. No suffix scans,
    no iteration over all dataflow entries.
    """

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        """Resolve targets. Returns empty set if unresolvable."""
        ...
```

- [ ] **Step 2: Implement TypeAwareStructLookup**

```python
class TypeAwareStructLookup:
    """Query: struct_fields['gstruct:<type>.<field_tail>']"""

    def __init__(self, store, symbol_table):
        self._store = store
        self._symbol_table = symbol_table

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        struct_type = self._symbol_table.get_func_var_type(ctx.caller_func, ctx.base_var)
        if not struct_type:
            return set()
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{struct_type}.{field_tail}')
```

- [ ] **Step 3: Implement ExactPathStructLookup**

```python
class ExactPathStructLookup:
    """Query: struct_fields['gstruct:<base_var>.<field_tail>']"""

    def __init__(self, store):
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{ctx.base_var}.{field_tail}')
```

- [ ] **Step 4: Implement remaining strategies**

```python
class TypeAwareVtableLookup:
    """Query: vtable_entries['vtable:<type>.<field_name>']"""

    def __init__(self, store, symbol_table):
        self._store = store
        self._symbol_table = symbol_table

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        struct_type = self._symbol_table.get_func_var_type(ctx.caller_func, ctx.base_var)
        if not struct_type:
            return set()
        field_name = ctx.field_path.split('.')[-1]
        return self._store.resolve_vtable_entry(struct_type, field_name)


class GlobalArrayLookup:
    """Query: global_arrays['garray:<base_var>']"""

    def __init__(self, store):
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        return self._store.resolve_global_array(ctx.base_var)


class StructAliasLookup:
    """Resolve base_var via alias map, then query struct_fields.

    Uses ScopedStore.aliases dict populated by initializer_assign.
    """

    def __init__(self, store):
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        alias = self._store.aliases.get(ctx.base_var)
        if not alias:
            return set()
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{alias}.{field_tail}')


class ParamAliasLookup:
    """Query: param_alias_map[(caller_func, base_var)] -> field_path -> struct_fields"""

    def __init__(self, dataflow):
        self._dataflow = dataflow

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        if not ctx.caller_func:
            return set()
        alias_key = (ctx.caller_func, ctx.base_var)
        if alias_key not in self._dataflow.param_alias_map:
            return set()
        global_name = self._dataflow.param_alias_map[alias_key]
        field_tail = self._dataflow.store.compute_field_tail(ctx.field_path)
        return self._dataflow.store.resolve_struct_field(f'gstruct:{global_name}.{field_tail}')


class LocalFpLookup:
    """Query: local_fp_mapping[base_var] -> targets"""

    def __init__(self, local_fp_mapping):
        self._mapping = local_fp_mapping

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        return self._mapping.get(ctx.base_var, set()).copy()


class PointerAliasLookup:
    """Query: pointer_resolutions[base_var] -> resolved_base -> struct_fields"""

    def __init__(self, pointer_resolutions, store):
        self._resolutions = pointer_resolutions
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        if ctx.base_var not in self._resolutions:
            return set()
        resolved_base = self._resolutions[ctx.base_var]
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{resolved_base}.{field_tail}')
```

- [ ] **Step 5: Implement FieldResolver**

```python
class FieldResolver:
    """Chain of resolution strategies for struct field function pointer calls."""

    def __init__(self, store, dataflow, symbol_table,
                 local_fp_mapping, pointer_resolutions):
        self._strategies: list = [
            TypeAwareStructLookup(store, symbol_table),
            ExactPathStructLookup(store),
            TypeAwareVtableLookup(store, symbol_table),
            GlobalArrayLookup(store),
            StructAliasLookup(store),
            ParamAliasLookup(dataflow),
            LocalFpLookup(local_fp_mapping),
            PointerAliasLookup(pointer_resolutions, store),
        ]

    def resolve(self, field_path: str, base_var: str,
                caller_func: str | None = None) -> set[str]:
        ctx = ResolutionContext(
            field_path=field_path,
            base_var=base_var,
            caller_func=caller_func,
        )
        for strategy in self._strategies:
            targets = strategy.resolve(ctx)
            if targets:
                return targets
        return set()
```

- [ ] **Step 6: Write unit tests for each strategy**

```python
# tests/test_field_resolver.py
"""Unit tests for FieldResolver and ResolutionStrategy classes."""
import pytest
from ethunter.analyzer.scoped_store import ScopedStore
from ethunter.analyzer.field_resolver import (
    ResolutionContext, TypeAwareStructLookup, ExactPathStructLookup,
    GlobalArrayLookup, FieldResolver,
)


class FakeSymbolTable:
    def __init__(self, types=None):
        self._types = types or {}
    def get_func_var_type(self, func, var):
        return self._types.get((func, var))
    def get_var_type(self, var):
        return None


class TestTypeAwareStructLookup:
    def test_matches_type_aware_key(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        sym = FakeSymbolTable({("caller", "h"): "my_type"})
        strategy = TypeAwareStructLookup(store, sym)
        ctx = ResolutionContext(field_path="h.cb", base_var="h", caller_func="caller")
        assert strategy.resolve(ctx) == {"handler_a"}

    def test_no_type_info_returns_empty(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        sym = FakeSymbolTable()
        strategy = TypeAwareStructLookup(store, sym)
        ctx = ResolutionContext(field_path="h.cb", base_var="h", caller_func="caller")
        assert strategy.resolve(ctx) == set()


class TestExactPathStructLookup:
    def test_matches_exact_var_name(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a")
        strategy = ExactPathStructLookup(store)
        ctx = ResolutionContext(field_path="handler.cb", base_var="handler")
        assert strategy.resolve(ctx) == {"handler_a"}

    def test_different_var_name_returns_empty(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a")
        strategy = ExactPathStructLookup(store)
        ctx = ResolutionContext(field_path="h.cb", base_var="h")
        assert strategy.resolve(ctx) == set()


class TestGlobalArrayLookup:
    def test_matches_global_array(self):
        store = ScopedStore()
        store.assign_global_array("hooks", "hook_a")
        strategy = GlobalArrayLookup(store)
        ctx = ResolutionContext(field_path="hooks.dispatch", base_var="hooks")
        assert strategy.resolve(ctx) == {"hook_a"}


class TestFieldResolver:
    def test_resolves_via_first_matching_strategy(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        store.assign_struct_field("gstruct:h.cb", "handler_b")
        sym = FakeSymbolTable({("caller", "h"): "my_type"})
        resolver = FieldResolver(store, None, sym, {}, {})
        # TypeAwareStructLookup should match first
        targets = resolver.resolve("h.cb", "h", "caller")
        assert targets == {"handler_a"}

    def test_falls_back_to_exact_when_no_type(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:h.cb", "handler_b")
        sym = FakeSymbolTable()  # no type info
        resolver = FieldResolver(store, None, sym, {}, {})
        targets = resolver.resolve("h.cb", "h", "caller")
        assert targets == {"handler_b"}
```

Run: `.venv/bin/python -m pytest tests/test_field_resolver.py -v`
Expected: All tests pass

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/field_resolver.py tests/test_field_resolver.py
git commit -m "feat: add FieldResolver with 8-strategy chain and unit tests"
```

---

### Task C2: Replace field_call Resolution with FieldResolver

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py` — split Pass 1/Pass 2

- [ ] **Step 1: Split field_call into collect() and analyze()**

Extract Pass 1 (field assignment collection) from `analyze()` into a new `collect()` function:

```python
# field_call.py — new function (extracted from existing analyze())
def collect(tree: ts.Tree, filepath: str, dataflow, symbol_table,
            symbol_names: set[str]) -> None:
    """Phase 1a*: collect field assignments, write struct_fields entries.

    Runs across ALL files before Phase 2 so cross-file assignments are visible.
    """
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            base_var = fa.field_path.split('.')[0]
            field_tail = dataflow.store.compute_field_tail(fa.field_path)

            # Always write exact key
            dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}',
                                               fa.resolved_value)

            # If type is known, also write type-aware key
            struct_type = symbol_table.get_func_var_type(fa.enclosing_func, base_var)
            if struct_type:
                dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}',
                                                   fa.resolved_value)
```

- [ ] **Step 2: Rewrite analyze() to use FieldResolver**

Replace the `_visit()` function (lines 100-288) with a version that uses `FieldResolver`:

```python
# field_call.py — rewrite analyze()
def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table,
    dataflow,
) -> list[CallEdge]:
    """Phase 2: detect indirect calls through struct field expressions."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names
    macro_map = _collect_macros(tree)
    pointer_resolutions = collect_pointer_resolutions(tree)
    local_fp_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names, symbol_table)

    resolver = FieldResolver(
        store=dataflow.store,
        dataflow=dataflow,
        symbol_table=symbol_table,
        local_fp_mapping=local_fp_mapping,
        pointer_resolutions=pointer_resolutions,
    )

    func_fp_params = getattr(dataflow.state, 'func_fp_params', None) if hasattr(dataflow, 'state') else None

    def _extract_field_expression(node):
        """Unwrap parenthesized/pointer expressions to find field_expression."""
        if not node:
            return None
        if node.type == 'field_expression':
            return node
        if node.type == 'parenthesized_expression':
            for c in node.children:
                if c.type == 'pointer_expression':
                    for cc in c.children:
                        if cc.type == 'field_expression':
                            return cc
        return None

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            field_expr = _extract_field_expression(func_node)
            if field_expr:
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(field_expr)
                if field_path:
                    base_var = field_path.split('.')[0]

                    # Resolve via strategy chain (no suffix scans)
                    targets = resolver.resolve(field_path, base_var, caller)

                    # Callback-of-callback
                    if func_fp_params and targets:
                        _resolve_callback_of_callback(
                            targets, node, func_fp_params, symbol_names,
                            edges, filepath)

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='field_call',
                            caller_line=node.start_point[0] + 1,
                        ))
            elif (func_node and func_node.type == 'identifier'
                  and func_node.text and func_node.text.decode('utf-8') in macro_map):
                # Macro-expanded field call (unchanged logic)
                call_name = func_node.text.decode('utf-8')
                body = macro_map[call_name]
                resolved_path = _extract_field_path_from_macro_body(body)
                if resolved_path:
                    base_var = resolved_path.split('.')[0]
                    targets = resolver.resolve(resolved_path, base_var,
                                               find_enclosing_function(node, tree.root_node))
                    if targets:
                        caller = find_enclosing_function(node, tree.root_node)
                        for target in targets:
                            edges.append(CallEdge(
                                caller=caller or '<unknown>',
                                callee=target,
                                caller_file=filepath,
                                callee_file='',
                                type=CallType.INDIRECT,
                                indirect_kind='field_call',
                                caller_line=node.start_point[0] + 1,
                            ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _resolve_callback_of_callback(targets, call_node, func_fp_params,
                                   symbol_names, edges, filepath):
    """For each resolved target with fnptr params, check if arguments
    are known function names and emit callback_param edges."""
    args = call_node.child_by_field_name('arguments')
    if not args:
        return
    comma_count = 0
    arg_values = []
    for c in args.children:
        if c.type == ',':
            comma_count += 1
        elif c.type not in ('(', ')'):
            arg_values.append((comma_count, c))
    for ftarget in targets:
        fp_positions = func_fp_params.get(ftarget, set())
        for pos, arg_node in arg_values:
            if pos in fp_positions:
                actual = None
                if arg_node.type == 'identifier' and arg_node.text:
                    actual = arg_node.text.decode('utf-8')
                elif arg_node.type == 'pointer_expression' and arg_node.children:
                    inner = arg_node.children[-1]
                    if inner.type == 'identifier' and inner.text:
                        actual = inner.text.decode('utf-8')
                if actual and actual in symbol_names:
                    edges.append(CallEdge(
                        caller=ftarget,
                        callee=actual,
                        caller_file=filepath,
                        callee_file='',
                        type=CallType.INDIRECT,
                        indirect_kind='callback_param',
                        caller_line=call_node.start_point[0] + 1,
                    ))
```

- [ ] **Step 3: Safety net — run side-by-side with old suffix logic**

Before removing suffix scans, add a debug comparison that reconstructs the old suffix behavior from `dataflow.store.struct_fields` (no longer uses `dataflow.targets` — it was removed in Phase A):

```python
# Temporary: in field_call.analyze(), add debug comparison
def _resolve_with_old_suffix(field_path, store):
    """OLD suffix scan logic — for comparison only. Remove after validation.
    Uses ScopedStore.struct_fields instead of old dataflow.targets."""
    targets = set()
    if '.' in field_path:
        parts = field_path.split('.')
        for i in range(1, len(parts)):
            suffix = '.'.join(parts[i:])
            for key, vals in store.struct_fields.items():
                if key.endswith(f'.{suffix}') and vals:
                    targets.update(vals)
    return targets

# In _visit(), after resolver.resolve():
# old_targets = _resolve_with_old_suffix(field_path, dataflow.store)
# missed = old_targets - targets
# if missed:
#     print(f"DEBUG: FieldResolver missed: {field_path} -> {missed}")
```

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s 2>&1 | grep "DEBUG:"`
Expected: No output (or only actionable gaps). If gaps found:
- Classify each gap (type tracking, structural pattern, cross-file)
- Return to Phase B scope if type tracking gap
- Add new strategy if structural pattern
- If cross-file with unknown types: defer to Phase D (confidence:low safety net)

- [ ] **Step 4: Remove suffix scans and old _resolve_with_old_suffix debug code**

After confirming no regressions, delete the old suffix scan logic and the debug comparison code.

- [ ] **Step 5: Run full et_bench**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All 57 pass; recall ≥ 98.86%; FPR < 20%

- [ ] **Step 6: Update orchestrator to use field_call.collect() + field_call.analyze()**

```python
# orchestrator.py — in run_all_analyses()

    # Phase 1a*: field_call Pass 1 — ALL files (collect assignments)
    for filepath, tree in trees.items():
        field_call.collect(tree, filepath, engine, symbol_table, symbol_names)

    # ... Phase 1, Phase 1b ...

    # Phase 2: field_call Pass 2 + other detectors
    # field_call.analyze() is already in CALL_DETECTORS — it now only does resolution
```

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/field_call.py src/ethunter/analyzer/orchestrator.py
git commit -m "refactor: split field_call into collect()/analyze(), use FieldResolver"
```

---

### Task C3: Remove param_assign.analyze()

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` — delete `analyze()` function
- Modify: `src/ethunter/analyzer/orchestrator.py` — remove `param_assign.analyze()` call

- [ ] **Step 1: Delete param_assign.analyze()**

Remove the `analyze()` function from `param_assign.py` (lines 415-786, the entire function including its helper `_propagate_call_site`). Keep:
- `_register_phase()` (renamed to `register_phase` at module level)
- `REG_PATTERNS` (deprecated — param_helpers has its own copy, but keep for backward compat during cleanup)
- All utility functions that are referenced elsewhere

- [ ] **Step 2: Rename _register_phase to register_phase**

```python
# param_assign.py — at module level
def register_phase(tree, filepath, symbol_table, dataflow):
    """Phase 1a: pre-scan for param→field registrations. No edges."""
    _register_phase(tree, filepath, symbol_table, dataflow)
```

- [ ] **Step 3: Remove param_assign.analyze() from orchestrator**

```python
# orchestrator.py — remove lines 101-108 (the old param_assign.analyze() call)
# Keep: param_assign.register_phase() in Phase 1a (renamed from _register_phase)
```

Update the import at the top of orchestrator.py:
```python
# Remove 'param_assign,' from the Phase 1b import if analyze() was the only consumer
# Keep param_assign imported for register_phase
```

- [ ] **Step 4: Run full et_bench**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All 57 pass; recall ≥ 98.86%; FPR unchanged from Task C1

If recall drops: the new modules (param_binding + param_dispatch + callback_reg) must produce all edges that param_assign.analyze() used to produce. Check `test_et_bench_report` for which categories dropped, then investigate the specific missing edges. The safety net from Task C1 step 3 provides the comparison baseline.

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py src/ethunter/analyzer/orchestrator.py
git commit -m "refactor: remove param_assign.analyze(), complete module migration"
```

---

## Phase D: Confidence Model

### Task D1: Add confidence + evidence to CallEdge

**Files:**
- Modify: `src/ethunter/graph/model.py` — extend CallEdge dataclass
- Modify: `src/ethunter/output/json_output.py` — verify serialization (should be automatic via to_dict)

- [ ] **Step 1: Extend CallEdge**

```python
# model.py — add to CallEdge dataclass

@dataclass
class CallEdge:
    caller: str
    callee: str
    caller_file: str = ''
    callee_file: str = ''
    type: CallType = CallType.DIRECT
    indirect_kind: str = ''
    caller_line: int = 0
    # NEW: confidence and evidence
    confidence: str = 'medium'   # 'high' | 'medium' | 'low'
    evidence: str = ''           # human-readable evidence description

    def to_dict(self) -> dict:
        d = {
            'caller': self.caller,
            'callee': self.callee,
            'caller_file': self.caller_file,
            'callee_file': self.callee_file,
            'type': self.type.value,
            'indirect_kind': self.indirect_kind,
            'caller_line': self.caller_line,
            'confidence': self.confidence,
            'evidence': self.evidence,
        }
        return d

    @classmethod
    def from_dict(cls, d: dict) -> 'CallEdge':
        return cls(
            caller=d.get('caller', ''),
            callee=d.get('callee', ''),
            caller_file=d.get('caller_file', ''),
            callee_file=d.get('callee_file', ''),
            type=CallType(d.get('type', 'direct')),
            indirect_kind=d.get('indirect_kind', ''),
            caller_line=d.get('caller_line', 0),
            confidence=d.get('confidence', 'medium'),
            evidence=d.get('evidence', ''),
        )
```

- [ ] **Step 2: Verify no test breakage**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests pass (new fields have defaults, no existing assertions break)

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/graph/model.py
git commit -m "feat: add confidence + evidence fields to CallEdge"
```

---

### Task D2: Annotate All Edge Producers + Confidence-Based Dedup

**Files:**
- Modify: `src/ethunter/analyzer/direct_call.py` — add `confidence='high', evidence='direct call expression'`
- Modify: `src/ethunter/analyzer/direct_call_fp.py` — add confidence based on match type
- Modify: `src/ethunter/analyzer/field_call.py` — add confidence based on which strategy matched
- Modify: `src/ethunter/analyzer/array_call.py` — add `confidence='high', evidence='global array dispatch'`
- Modify: `src/ethunter/analyzer/param_dispatch.py` — Pass A: high, Pass B: medium
- Modify: `src/ethunter/analyzer/callback_reg.py` — Stage 1: medium, Stage 3: low
- Modify: `src/ethunter/analyzer/dlsym_fp.py` — add `confidence='low', evidence='dlsym string literal match'`
- Modify: `src/ethunter/analyzer/orchestrator.py` — replace dedup with confidence-based version

- [ ] **Step 1: Annotate direct_call_fp.py**

```python
# In _add_edges(), after resolution:
        targets = _get_targets(func_name, caller)
        if targets:
            first_target = next(iter(targets))
            # Determine confidence based on resolution path
            if caller_func and func_name in dataflow.store.func_vars.get((caller_func, func_name), set()):
                confidence, evidence = 'high', 'scoped variable resolution'
            elif local_mapping.get(func_name):
                confidence, evidence = 'medium', 'local fp from struct field'
            else:
                confidence, evidence = 'medium', 'direct_assign resolution'
            for target in targets:
                edges.append(CallEdge(
                    caller=caller or '<unknown>',
                    callee=target,
                    caller_file=filepath,
                    callee_file='',
                    type=CallType.INDIRECT,
                    indirect_kind='direct_assign',
                    caller_line=call_node.start_point[0] + 1,
                    confidence=confidence,
                    evidence=evidence,
                ))
```

- [ ] **Step 2: Annotate field_call.py with strategy-aware confidence**

Update `_visit()` in field_call.analyze() to get confidence from the strategy chain. Also update `_resolve_callback_of_callback()` — callback-of-callback edges are `callback_param` with `confidence='medium'`:

```python
# In _resolve_callback_of_callback(), for each edge:
                    edges.append(CallEdge(
                        # ... existing fields ...
                        indirect_kind='callback_param',
                        caller_line=call_node.start_point[0] + 1,
                        confidence='medium',
                        evidence='callback-of-callback via field_call',
                    ))
```

```python
# Modify FieldResolver.resolve() to return (targets, strategy_name)
# or add a method that identifies which strategy matched:

class FieldResolver:
    # ... existing code ...

    def resolve_with_evidence(self, field_path, base_var, caller_func=None):
        """Resolve targets and return (targets, strategy_name)."""
        ctx = ResolutionContext(field_path=field_path, base_var=base_var,
                                caller_func=caller_func)
        for strategy in self._strategies:
            targets = strategy.resolve(ctx)
            if targets:
                return targets, type(strategy).__name__
        return set(), 'none'
```

Then in `_visit()`:
```python
                targets, strategy_name = resolver.resolve_with_evidence(
                    field_path, base_var, caller)

                # Map strategy to confidence
                strategy_confidence = {
                    'TypeAwareStructLookup': ('high', f'type-aware gstruct match: {field_path}'),
                    'ExactPathStructLookup': ('high', f'exact gstruct match: {base_var}.{field_path}'),
                    'TypeAwareVtableLookup': ('high', f'vtable match: {field_path}'),
                    'GlobalArrayLookup': ('high', f'global array dispatch: {base_var}'),
                    'StructAliasLookup': ('medium', f'struct alias resolution: {base_var}'),
                    'ParamAliasLookup': ('medium', f'param alias resolution: {base_var}'),
                    'LocalFpLookup': ('medium', f'local fp mapping: {base_var}'),
                    'PointerAliasLookup': ('medium', f'pointer alias resolution: {base_var}'),
                }
                confidence, evidence = strategy_confidence.get(
                    strategy_name, ('medium', f'field_call: {strategy_name}'))

                for target in targets:
                    edges.append(CallEdge(
                        # ... existing fields ...
                        confidence=confidence,
                        evidence=evidence,
                    ))
```

- [ ] **Step 3: Annotate param_dispatch.py**

```python
# Pass A edges (lines 88-97):
            indirect_kind='callback_param',
            caller_line=line,
            confidence='high',
            evidence='fnptr call in callee body',
        ))

# Pass B edges (lines 111-119):
            indirect_kind='callback_param',
            caller_line=0,
            confidence='medium',
            evidence='call-site caller -> target',
        ))
```

- [ ] **Step 4: Annotate callback_reg.py**

```python
# In analyze(), for each edge:
        confidence = 'medium' if usage == 'caller' else 'low'
        evidence = (f'behavioral: fnptr called in callee body'
                    if usage == 'caller'
                    else f'heuristic: registration name match ({callee})')
        edges.append(CallEdge(
            # ... existing fields ...
            confidence=confidence,
            evidence=evidence,
        ))
```

- [ ] **Step 5: Annotate remaining modules (direct_call, array_call, dlsym_fp)**

- `direct_call.py`: add `confidence='high', evidence='direct call expression'`
- `array_call.py`: add `confidence='high', evidence='global array dispatch'`
- `dlsym_fp.py`: add `confidence='low', evidence='dlsym string literal match'`

- [ ] **Step 6: Replace orchestrator dedup with confidence-based dedup**

```python
# orchestrator.py — replace lines 164-182

def _dedup_with_confidence(edges: list[CallEdge]) -> list[CallEdge]:
    """Keep highest-confidence edge for each (caller, callee) pair."""
    edge_map: dict[tuple[str, str], CallEdge] = {}
    confidence_rank = {'high': 3, 'medium': 2, 'low': 1}
    for edge in edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge
        elif confidence_rank.get(edge.confidence, 0) > \
             confidence_rank.get(edge_map[key].confidence, 0):
            edge_map[key] = edge
    return list(edge_map.values())

# In run_all_analyses(), replace the dedup block with:
    graph.edges = _dedup_with_confidence(graph.edges)
```

- [ ] **Step 7: Remove field_callees post-hoc filter**

In orchestrator.py, remove lines 152-161 (the `field_callees` filter). This is now redundant — confidence-based dedup handles the same scenario (field_call edges have confidence='high', callback_reg edges have confidence='low' — dedup keeps the high one).

- [ ] **Step 8: Update FPR ceilings in tests + add high-confidence assertion**

Update `fpr_ceilings` in `tests/test_et_bench.py` to reflect Phase C/D FPR targets:

```python
    fpr_ceilings = {
        'fnptr-callback': 0.55,
        'fnptr-cast': 0.55,
        'fnptr-global-array': 0.03,
        'fnptr-global-struct': 0.25,
        'fnptr-global-struct-array': 0.35,
        'fnptr-library': 0.12,
        'fnptr-only': 0.06,
        'fnptr-struct': 0.30,
        'fnptr-varargs': 0.53,
    }
```

Add a new test for high-confidence FPR:

```python
def test_et_bench_high_confidence_fpr():
    """High-confidence edge subset should have FPR < 5%."""
    categories = _get_categories()
    total_extra = 0
    total_detected = 0
    for category in categories:
        for example in _get_examples(category):
            example_dir = os.path.join(ET_BENCH_DIR, category, example)
            example_edges = _load_example_ground_truth(example_dir)
            if not example_edges:
                continue
            graph = _run_analysis_on_fixture(example_dir)
            high_conf_edges = [e for e in graph.edges
                               if e.type.value == 'indirect'
                               and getattr(e, 'confidence', 'medium') == 'high']
            found_pairs = {(e.caller, e.callee) for e in high_conf_edges}
            expected_pairs = {(e['caller'], e['callee']) for e in example_edges}
            extra = found_pairs - expected_pairs
            total_extra += len(extra)
            total_detected += len(found_pairs)
    fpr = total_extra / total_detected if total_detected > 0 else 0.0
    assert fpr < 0.05, f"High-confidence FPR={fpr:.2%} exceeds 5% ceiling"
```

- [ ] **Step 9: Run full et_bench**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All tests pass; overall recall ≥ 98.86%; overall FPR < 20%; high-confidence FPR < 5%

- [ ] **Step 10: Commit**

```bash
git add src/ethunter/analyzer/direct_call.py src/ethunter/analyzer/direct_call_fp.py src/ethunter/analyzer/field_call.py src/ethunter/analyzer/array_call.py src/ethunter/analyzer/param_dispatch.py src/ethunter/analyzer/callback_reg.py src/ethunter/analyzer/dlsym_fp.py src/ethunter/analyzer/orchestrator.py src/ethunter/analyzer/field_resolver.py tests/test_et_bench.py
git commit -m "feat: add confidence annotations + confidence-based dedup, remove field_callees filter"
```

---

## Completion Checklist

- [ ] `test_et_bench_report` — recall ≥ 98.86% in all categories (excluding dynamic-call + virtual)
- [ ] `test_et_bench_report` — overall FPR < 20%
- [ ] `test_et_bench_high_confidence_fpr` — high-confidence FPR < 5%
- [ ] `test_type_aware_key_isolates_different_struct_types` — PASS (no longer xfail)
- [ ] All 57+ et_bench tests pass
- [ ] `field_call.py` no longer contains suffix scans or `dataflow.targets` iteration
- [ ] `param_assign.py` no longer contains `analyze()` function
- [ ] `orchestrator.py` no longer imports/calls `param_assign.analyze()`
- [ ] `CallEdge` has `confidence` and `evidence` fields serialized in JSON output
