# Type-Filtered Suffix + Path B Removal — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate cross-struct false positives and delete Path B legacy suffix scan by adding reachability-gated suffix filtering to FieldResolver Tier 3/4 and multi-level chain decomposition.

**Architecture:** B.1 adds a 3-case reachability gate to Tier 3/4 suffix scanning in FieldResolver. B.2 removes the type gate that blocked all suffix scanning. B.3 deletes the legacy suffix scan from field_call._visit() (Path B), keeping only the independent garray lookup. A.1 extends chain decomposition to try all prefix lengths (cut=2..N-1).

**Tech Stack:** Python 3.11, tree-sitter, pytest

---

### Task 1: Add reachability gate to Tier 3/4 and remove type gate from FieldResolver

**Files:**
- Modify: `src/ethunter/analyzer/field_resolver.py:193-228`

- [ ] **Step 1: Read the current resolve_field_call from Tier 2 through type gate**

Read line 193-203 of field_resolver.py:
```python
        # === Tier 2: Exact path match ===
        targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

        # === Type gate: known type + Tier 1 miss → skip Tier 3/4 suffix ===
        # Legacy fallback in caller may still find data in old dataflow.targets.
        # FPR reduction depends on enough type-aware keys being populated.
        if struct_type:
            return set(), None, None
```

Read lines 205-228 to see current Tier 3/4 code (same-file + cross-file suffix scans).

- [ ] **Step 2: Replace Tier 2 through Tier 4 with new reachability-gated version**

Replace from Tier 2 comment through the end of Tier 4 return:

```python
        # === Tier 2: Exact path match ===
        targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

        # === Chain decomposition ===
        # Handle s.method.put_cb where s.method resolves to a concrete struct
        parts = field_path.split('.')
        if len(parts) >= 3:
            for cut in range(2, len(parts)):
                prefix = '.'.join(parts[:cut])
                suffix = '.'.join(parts[cut:])
                resolved_vars = self._store.resolve_struct_field(f'gstruct:{prefix}')
                if not resolved_vars:
                    continue
                for var_name in resolved_vars:
                    var_type = self._symbol_table.get_var_type(var_name)
                    if var_type:
                        targets = self._store.resolve_struct_field(
                            f'gstruct:{var_type}.{suffix}')
                        if targets:
                            return targets, Confidence.HIGH, Evidence('chain_resolve', tier=1)
                    targets = self._store.resolve_struct_field(
                        f'gstruct:{var_name}.{suffix}')
                    if targets:
                        return targets, Confidence.HIGH, Evidence('chain_resolve_exact', tier=2)

        # === Tier 3: Reachability-gated same-file suffix ===
        # Type gate REMOVED — Tier 3/4 now have reachability filtering, safe to run.
        suffix = f'.{field_tail}'
        for key, vals in self._store.struct_fields.items():
            if not key.endswith(suffix):
                continue
            if struct_type:
                key_prefix = key[len('gstruct:'):].split('.')[0]
                if key_prefix != struct_type:
                    has_mappings = False
                    reachable = False
                    for sk, sv in self._store.struct_fields.items():
                        if sk.startswith(f'gstruct:{struct_type}.'):
                            has_mappings = True
                            if key_prefix in sv:
                                reachable = True
                                break
                    if has_mappings and not reachable:
                        continue
            files = self._store.struct_field_files.get(key)
            if files and filepath not in files:
                continue
            targets.update(vals)
        if targets:
            return targets, Confidence.MEDIUM, Evidence('same_file_suffix', tier=3)

        # === Tier 4: Reachability-gated cross-file suffix ===
        for key, vals in self._store.struct_fields.items():
            if not key.endswith(suffix):
                continue
            if struct_type:
                key_prefix = key[len('gstruct:'):].split('.')[0]
                if key_prefix != struct_type:
                    has_mappings = False
                    reachable = False
                    for sk, sv in self._store.struct_fields.items():
                        if sk.startswith(f'gstruct:{struct_type}.'):
                            has_mappings = True
                            if key_prefix in sv:
                                reachable = True
                                break
                    if has_mappings and not reachable:
                        continue
            targets.update(vals)
        if targets:
            return targets, Confidence.LOW, Evidence('cross_file_suffix', tier=4)

        return set(), None, None
```

- [ ] **Step 3: Run regression tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (Tier 3/4 now run but with reachability gate)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/field_resolver.py
git commit -m "feat: add reachability gate to Tier 3/4 suffix, remove type gate, multi-level chain decomp (B.1, B.2, A.1)"
```

---

### Task 2: Delete Path B suffix scan from field_call._visit()

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:269-293`

- [ ] **Step 1: Read the current legacy fallback block**

Read the `_visit()` function around the resolver call and legacy fallback (lines 269-293):
```python
                    # 4-tier resolver
                    if resolver is not None:
                        targets, confidence, evidence = \
                            resolver.resolve_field_call(field_path, base_var, caller, filepath)
                        # Legacy fallback: suffix scan for remaining data gaps
                        # (chain access not resolvable via struct_fields alone,
                        #  array-of-structs with positional init, cross-module data)
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

- [ ] **Step 2: Delete suffix scan, keep only garray**

Replace with:

```python
                    # 4-tier resolver
                    if resolver is not None:
                        targets, confidence, evidence = \
                            resolver.resolve_field_call(field_path, base_var, caller, filepath)
                        # Garray fallback: array-of-structs with positional init
                        if '.' in field_path:
                            garray_targets = dataflow.resolve(f'<garray:{base_var}>')
                            if garray_targets:
                                targets.update(garray_targets)
                    else:
                        targets = dataflow.resolve(f'<gstruct:{field_path}>')
                        if not targets:
                            targets = dataflow.resolve(f'<struct:{field_path}>')
```

- [ ] **Step 3: Run regression tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (recall ≥ 98.86%, FPR may decrease)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "fix: delete Path B suffix scan — Tier 3/4 reachability gate covers all cases (B.3)"
```

---

### Task 3: Add reachability gate test

**Files:**
- Modify: `tests/test_et_bench.py` (append new test at end)

- [ ] **Step 1: Write the test**

Append to `tests/test_et_bench.py`:

```python
def test_reachability_gate_blocks_cross_struct_suffix():
    """Tier 3 suffix should NOT match keys from a different struct type
    when the base variable's struct_type has no field mapping to that type."""
    from ethunter.analyzer.field_resolver import FieldResolver
    from ethunter.analyzer.scoped_store import ScopedStore
    from ethunter.analyzer.symbol_table import SymbolTable

    store = ScopedStore()
    st = SymbolTable()
    # Simulate: ctx has type region_model_context, with known field mappings
    # but NO mapping that points to decorator_vtable or noop_vtable
    st.record_func_var_type('get_fd_map', 'ctx', 'region_model_context')
    st.record_var_type('ctx_local', 'region_model_context')
    # Populate struct_fields with type-aware data for a DIFFERENT struct
    store.assign_struct_field('gstruct:decorator_vtable.get_state_map_by_name', 'decorator_fn')
    store.assign_struct_field('gstruct:noop_vtable.get_state_map_by_name', 'noop_fn')
    # No gstruct:region_model_context.* keys that reference decorator_vtable

    resolver = FieldResolver(store, None, st, {}, {})
    targets, conf, ev = resolver.resolve_field_call(
        'ctx.vtable.get_state_map_by_name', 'ctx', 'get_fd_map', 'fixture.c')

    # Region_model_context has NO field mapping to decorator_vtable →
    # reachability gate should block the suffix match
    assert 'decorator_fn' not in targets, \
        "Cross-struct suffix match should be blocked by reachability gate"
    assert 'noop_fn' not in targets
```

- [ ] **Step 2: Run the test**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_reachability_gate_blocks_cross_struct_suffix -v`
Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add reachability gate cross-struct suffix blocking test"
```

---

### Task 4: Verify recall, FPR, and run full regression

- [ ] **Step 1: Run full test suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS

- [ ] **Step 2: Check recall is maintained**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_fnptr_struct_full_recall -v`
Expected: PASS (recall 100%)

- [ ] **Step 3: Check FPR report**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s`
Expected: FPR ≤ 31.33% (may decrease from fnptr-virtual reduction)

- [ ] **Step 4: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: type-filtered suffix + Path B removal complete — recall maintained, FPR verified"
```

---

## Summary

| Task | File | Changes | Est. Time |
|------|------|---------|-----------|
| 1 | `field_resolver.py:193-228` | Reachability gate Tier 3/4, remove type gate, multi-level chain decomp | 20 min |
| 2 | `field_call.py:269-293` | Delete Path B suffix scan, keep garray | 10 min |
| 3 | `tests/test_et_bench.py` | Reachability gate test | 10 min |
| 4 | (verification) | Full regression + FPR/recall check | 10 min |
| **Total** | **3 files** | **~50 LoC** | **~50 min** |
