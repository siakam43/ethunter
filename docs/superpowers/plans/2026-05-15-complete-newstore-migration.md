# Complete New Store Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate ALL data writes to new store, reorder pipeline so type info is available when needed, then delete Path B and old store writes.

**Architecture:** Three steps executed in order. Step 1 fills remaining write gaps (param_assign Pass 2, param_binding field_name). Step 2 makes `collect()` write ALL assignments to new store (not just function names) for chain decomposition. Step 3 splits `initializer_assign` to collect var_types before `field_call.collect()` in the orchestrator. Each step is independently verifiable via Path B gap counting.

**Tech Stack:** Python 3.11, tree-sitter, pytest

---

### Task 1: Patch param_assign Pass 2 to use field_tail format in new store writes

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:688, 697, 708`

- [ ] **Step 1: Read the current write sites**

Read the three `assign_struct_field` calls in `param_assign.py` around lines 688, 697, 708:
```python
dataflow.store.assign_struct_field(f'gstruct:{field_path}', t)
# or
dataflow.store.assign_struct_field(f'gstruct:{field_path}', t, filepath)  # after Task 18
```
These currently use raw `field_path` — need to use `compute_field_tail` like `param_binding` does.

- [ ] **Step 2: Update all three write sites to use compute_field_tail**

Replace each `assign_struct_field(f'gstruct:{field_path}', t, filepath)` with:
```python
base_var = field_path.split('.')[0]
field_tail = dataflow.store.compute_field_tail(field_path)
dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', t, filepath)
```
Apply to all three call sites in `param_assign.analyze()` Pass 2.

- [ ] **Step 3: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (198 + 1 xfailed)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py
git commit -m "fix: use compute_field_tail in param_assign Pass 2 struct_fields writes"
```

---

### Task 2: Add new store write for field_name-only fallback in param_binding

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py:258`

- [ ] **Step 1: Read the current field_name write**

Read `_resolve_fields()` around line 258:
```python
dataflow.assign(f'<struct:{field_name}>', t)
```
No new store equivalent exists.

- [ ] **Step 2: Add new store write**

Add after the old store write:
```python
dataflow.assign(f'<struct:{field_name}>', t)
if hasattr(dataflow, 'store'):
    dataflow.store.assign_struct_field(f'gstruct:{field_name}', t, filepath)
```

- [ ] **Step 3: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py
git commit -m "fix: write field_name-only key to new store in _resolve_fields"
```

---

### Task 3: Make collect() write ALL resolved values to new store

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:65-83`

- [ ] **Step 1: Read the current collect() filter**

Read lines 65-83:
```python
    for fa in collect_field_assignments(tree, ...):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            dataflow.assign(...)  # old store
            if hasattr(dataflow, 'store'):
                ...  # new store (gstruct:{base_var}.{field_tail})
                if struct_type:
                    ...  # type-aware key
```

- [ ] **Step 2: Restructure — old store conditional, new store unconditional**

Replace the block with:
```python
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None:
            # Old store: only for known function names
            if fa.resolved_value in symbol_names:
                dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)
            # New store: ALL resolved values (functions + struct vars)
            if hasattr(dataflow, 'store'):
                base_var = fa.field_path.split('.')[0]
                field_tail = dataflow.store.compute_field_tail(fa.field_path)
                dataflow.store.assign_struct_field(
                    f'gstruct:{base_var}.{field_tail}', fa.resolved_value, filepath)
                struct_type = symbol_table.get_func_var_type(fa.enclosing_func, base_var)
                if struct_type:
                    dataflow.store.assign_struct_field(
                        f'gstruct:{struct_type}.{field_tail}', fa.resolved_value, filepath)
```

- [ ] **Step 3: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (FPR may increase slightly — acceptable for this step)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "feat: write all resolved values to new store unconditionally in collect()"
```

---

### Task 4: Add collect_var_types() to initializer_assign

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py` (add new function)

- [ ] **Step 1: Find _resolve_struct_type helper**

Read `_resolve_struct_type` or equivalent logic in `initializer_assign.py` around line 40-50. This resolves struct type from `init_declarator` context.

- [ ] **Step 2: Add collect_var_types() function**

Add before `analyze()` function (around line 18):

```python
def collect_var_types(tree: ts.Tree, filepath: str,
                      symbol_table, dataflow) -> None:
    """Phase 1a: collect struct variable types from init_declarators.
    Must run BEFORE field_call.collect() so var_types are available.
    Returns no edges — metadata only.
    """
    def _resolve_type(decl_node):
        """Extract struct type name from declaration."""
        for c in decl_node.children:
            if c.type == 'type_identifier' and c.text:
                type_id = c.text.decode('utf-8')
                resolved = symbol_table.resolve_typedef(type_id)
                return resolved if resolved else type_id
            if c.type == 'struct_specifier':
                for sc in c.children:
                    if sc.type == 'type_identifier' and sc.text:
                        return sc.text.decode('utf-8')
        return None

    def _scan(node):
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            value = node.child_by_field_name('value')
            if declarator and value:
                var_name = extract_identifier_from_declarator(declarator)
                # Walk up: check if this init_declarator's sibling (the
                # declaration type) has a struct type
                if var_name:
                    # Reuse struct_type from _assign_gstruct's approach:
                    # scan parent declaration's children for type info
                    pass  # handled below via declaration scan
        if node.type == 'declaration':
            # Extract type and variable name from declaration
            type_name = _resolve_type(node)
            for c in node.children:
                if c.type == 'init_declarator':
                    declarator = c.child_by_field_name('declarator')
                    if declarator:
                        var_name = extract_identifier_from_declarator(declarator)
                        if var_name and type_name:
                            symbol_table.record_var_type(var_name, type_name)
                elif c.type == 'pointer_declarator' and type_name:
                    for pc in c.children:
                        if pc.type == 'identifier' and pc.text:
                            symbol_table.record_var_type(
                                pc.text.decode('utf-8'), type_name)
        for child in node.children:
            _scan(child)
    _scan(tree.root_node)
```

Note: `extract_identifier_from_declarator` is imported from `ethunter.analyzer.helpers`. `_resolve_type` mirrors the struct type extraction logic from `_assign_gstruct()` in `initializer_assign.analyze()`. Uses `declaration` node scan (not parent traversal) to find the declaration context.

- [ ] **Step 3: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (function added but not yet called)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py
git commit -m "feat: add collect_var_types() for early struct type collection"
```

---

### Task 5: Reorder orchestrator pipeline — call collect_var_types before field_call.collect()

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py:75-84`

- [ ] **Step 1: Read the current Phase 1a ordering**

Read lines 75-84 of `orchestrator.py`:
```python
    # Phase 1a: Cross-file pre-scan for metadata
    for filepath, tree in trees.items():
        param_helpers.prepare(tree, filepath, engine, symbol_table)

    # Phase 1a (cont'd): param_assign pre-scan
    for filepath, tree in trees.items():
        param_assign.register_phase(tree, filepath, symbol_table, engine)

    # Phase 1a*: field_call Pass 1 — ALL files (collect struct field assignments)
    for filepath, tree in trees.items():
        field_call.collect(tree, filepath, engine, symbol_table, symbol_names)
```

- [ ] **Step 2: Insert collect_var_types between register_phase and collect**

Replace the Phase 1a* comment and field_call.collect block with:
```python
    # Phase 1a (cont'd): collect struct variable types BEFORE field assignments
    for filepath, tree in trees.items():
        initializer_assign.collect_var_types(tree, filepath, symbol_table, engine)

    # Phase 1a*: field_call Pass 1 — ALL files (collect struct field assignments)
    for filepath, tree in trees.items():
        field_call.collect(tree, filepath, engine, symbol_table, symbol_names)
```

- [ ] **Step 3: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (pipeline order change, more type-aware keys in struct_fields)

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py
git commit -m "feat: reorder pipeline — collect var_types before field_call.collect()"
```

---

### Task 6: Verify Path B gap reduction

- [ ] **Step 1: Run gap analysis script**

Run the gap audit (same script from analysis phase):
```python
# Expected output: Path B gaps should decrease from 54
```
Verify gaps decrease with each task.

- [ ] **Step 2: Run full test suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS

- [ ] **Step 3: If Path B gaps = 0, delete Path B suffix scan**

If gap count is zero, remove the legacy suffix scan from `field_call._visit()` (lines 288-295):
```python
# Delete: for i in range(1, len(parts)): ... targets.update(vals)
# Keep: garray lookup
```

- [ ] **Step 4: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: new store migration complete — Path B gaps verified"
```

---

## Summary

| Task | File | Changes | Est. Time |
|------|------|---------|-----------|
| 1 | `param_assign.py:688,697,708` | field_tail format in new store | 10 min |
| 2 | `param_binding.py:258` | field_name new store write | 5 min |
| 3 | `field_call.py:65-83` | unconditional new store writes | 10 min |
| 4 | `initializer_assign.py` | new `collect_var_types()` | 15 min |
| 5 | `orchestrator.py:75-84` | pipeline reorder | 5 min |
| 6 | (verification) | gap audit + regression | 10 min |
| **Total** | **6 files** | **~55 LoC** | **~55 min** |
