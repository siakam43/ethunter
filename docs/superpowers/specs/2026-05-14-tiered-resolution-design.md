# Tiered Field Resolution Architecture

**Date**: 2026-05-14
**Status**: Draft — awaiting review
**Replaces**: `2026-05-14-scoped-dataflow-arch-redesign.md` Phase C (unfinished portion)
**Builds on**: Phase A (ScopedStore), Phase B (unified keys + type tracking), Phase D (confidence model)

## Motivation

The previous spec attempted to replace field_call's 15-layer fallback stack with a pure exact-match strategy chain. This failed because type tracking was incomplete — when struct types were unknown, exact key lookups returned empty, causing recall regression.

This spec replaces the failed Phase C with a **4-tier resolution chain** that combines type-aware exact matching (design A) with file-scoped suffix fallback (design B). Each tier has a defined FPR risk and confidence level.

**Goal**: Remove the 15-layer fallback stack, replace with tiered resolution chain. Remove `VariableState.targets`. Remove `param_assign.analyze()` from orchestrator. Achieve overall FPR < 20% while maintaining recall ≥ 98.86%.

## Design Principles

1. **Tiered degradation** — each tier degrades gracefully with defined FPR bounds
2. **Type-first** — type-aware exact match is always attempted before any scan
3. **File-scoped safety net** — when type is unknown, limit suffix match to same file
4. **Confidence-aligned** — tier number maps to confidence level (Tier 1-2 = high, Tier 3 = medium, Tier 4 = low)

---

## Section 1: 4-Tier Resolution Chain

### 1.1 Overview

```
┌─────────────────────────────────────────────┐
│ Tier 1: Type-Aware Exact Match              │ O(1), FPR 0%
│ gstruct:<type>.<field_tail>                 │
├─────────────────────────────────────────────┤
│ Tier 2: Exact Path Match                    │ O(1), FPR 0%
│ gstruct:<var>.<field_tail>                  │
├─────────────────────────────────────────────┤
│ Tier 3: Same-File Scoped Suffix             │ O(n_samefile), FPR ~10%
│ endswith(.{field_tail}) ∧ same_file         │
├─────────────────────────────────────────────┤
│ Tier 4: Cross-File Suffix (deprecated)      │ O(n_all), FPR high
│ endswith(.{field_tail})                     │ confidence=low
└─────────────────────────────────────────────┘
```

### 1.2 Resolution Algorithm

```python
def resolve_field_call(field_path: str, base_var: str,
                       caller_func: str | None, filepath: str,
                       store, symbol_table) -> tuple[set[str], str, str]:
    """Resolve a struct field function pointer call.
    
    Returns (targets, confidence, evidence).
    """
    field_tail = store.compute_field_tail(field_path)
    targets = set()
    
    # === Tier 1: Type-aware exact match ===
    struct_type = None
    if caller_func:
        struct_type = symbol_table.get_func_var_type(caller_func, base_var)
    if not struct_type:
        struct_type = symbol_table.get_var_type(base_var)
    if struct_type:
        targets = store.resolve_struct_field(f'gstruct:{struct_type}.{field_tail}')
        if targets:
            return targets, 'high', f'type-aware: {struct_type}.{field_tail}'
    
    # === Tier 2: Exact path match ===
    targets = store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
    if targets:
        return targets, 'high', f'exact path: {base_var}.{field_tail}'
    
    # === Tier 3: Same-file scoped suffix ===
    for key, vals in store.struct_fields.items():
        if not key.endswith(f'.{field_tail}'):
            continue
        if not store.struct_field_files.get(key, set()) & {filepath}:
            continue
        targets.update(vals)
    if targets:
        return targets, 'medium', f'same-file suffix: .{field_tail}'
    
    # === Tier 4: Cross-file suffix (last resort) ===
    for key, vals in store.struct_fields.items():
        if key.endswith(f'.{field_tail}'):
            targets.update(vals)
    if targets:
        return targets, 'low', f'cross-file suffix: .{field_tail}'
    
    return set(), 'none', ''
```

### 1.3 Tier Coverage Estimates

Based on analysis of 104 et_bench fixtures:

| Tier | Mechanism | Est. Coverage | FPR Risk | Confidence |
|---|---|---|---|---|
| 1 | Type-aware exact key | ~65% | 0% | high |
| 2 | Exact path key | ~20% | 0% | high |
| 3 | Same-file suffix | ~12% | ~10% | medium |
| 4 | Cross-file suffix | ~3% | high | low |

Tier 1 + 2 cover ~85% of struct field calls — all with zero FP risk. Tier 3 handles most remaining cases with file-scoped safety. Tier 4 is a rarely-hit safety net.

---

## Section 2: Enhanced Type Tracking

### 2.1 New Type Sources

Two new type collection points fill the largest gaps:

**Source 1: Local variable declarations** — `struct type_name *var;` or `type_name *var;` inside function bodies.

Collected by a new function `_collect_local_var_types()` called from `field_call.collect()`:

```python
def _collect_local_var_types(tree, symbol_table):
    """Scan function bodies for local struct pointer declarations.
    
    struct my_type *ptr;  →  (func, "ptr") → "my_type"
    my_type *ptr;         →  (func, "ptr") → "my_type" (via typedef)
    """
    def _scan(node, current_func):
        if node.type == 'function_definition':
            # ... extract function name ...
            current_func = fname
        if node.type == 'declaration':
            # Look for pointer_declarator with struct type
            _extract_declaration_type(node, current_func, symbol_table)
        for child in node.children:
            _scan(child, current_func)
    
    _scan(tree.root_node, None)
```

**Source 2: Cast expressions to struct pointer** — `(struct my_type *)expr` or `(my_type *)expr`.

Collected by new function `_collect_cast_types()` also called from `field_call.collect()`:

```python
def _collect_cast_types(tree, symbol_table):
    """Scan for cast expressions that reveal struct types.
    
    ((struct ctx*)ctx)->handler → ctx has type "ctx"
    """
    # Walk AST for cast_expression nodes
    # Extract cast target type and record on symbol_table
```

### 2.2 Updated Type Collection Pipeline

```
param_helpers.prepare()         ← function parameter types (existing)
field_call.collect():
  ├── _collect_field_assignments()  ← struct field data (existing)
  ├── _collect_local_var_types()    ← NEW: local var declarations
  └── _collect_cast_types()         ← NEW: cast expression types
```

### 2.3 Type Lookup Resolution Order

```python
def get_var_type_for_resolution(func, var, symbol_table):
    """Resolution order for struct type lookup."""
    # 1. Function-scoped (parameter or local var)
    if func:
        t = symbol_table.get_func_var_type(func, var)
        if t:
            return t
    # 2. Global variable
    t = symbol_table.get_var_type(var)
    if t:
        return t
    # 3. Typedef resolution (bare type name used as variable)
    t = symbol_table.resolve_typedef(var)
    if t:
        return t
    return None
```

---

## Section 3: File-Scoped Index

### 3.1 struct_field_files

To support Tier 3's same-file filter, each struct field entry tracks which files it was written from:

```python
@dataclass
class ScopedStore:
    # ... existing fields ...
    
    # Per-file index for struct field entries
    # key: "gstruct:handler.cb", value: {"fixture.c", "caller.c"}
    struct_field_files: dict[str, set[str]] = field(default_factory=dict)
```

### 3.2 Write-Time File Tracking

All struct field writes record the source file:

```python
def assign_struct_field(self, key: str, target: str, filepath: str = '') -> None:
    if key not in self.struct_fields:
        self.struct_fields[key] = set()
    self.struct_fields[key].add(target)
    if filepath:
        if key not in self.struct_field_files:
            self.struct_field_files[key] = set()
        self.struct_field_files[key].add(filepath)
```

Updated callers pass `filepath`:
- `initializer_assign._assign_gstruct()` — receives `filepath` from `analyze()`
- `field_call.collect()` — receives `filepath` parameter
- `param_binding._resolve_fields()` — receives `filepath` from `analyze()`

### 3.3 Read-Time File Filter

```python
def _from_same_file(store, key, current_file):
    """Check if a struct field key was written from the current file."""
    files = store.struct_field_files.get(key, set())
    return current_file in files
```

---

## Section 4: Pipeline Cleanup

### 4.1 Remove param_assign.analyze() from Orchestrator

`param_assign.analyze()` is currently kept in orchestrator Phase 1c for backward compat. With enhanced type tracking and the tiered resolver, the new modules (`param_binding` + `param_dispatch` + `callback_reg`) can now produce all needed edges.

To safely remove it:
1. Run et_bench with Tier 1-4 field_call AND param_assign.analyze() running → record all edges
2. Remove param_assign.analyze() call
3. Run et_bench again → compare
4. If any `callback_param` edges are missing, fix `param_dispatch` Pass A to cover them

### 4.2 Remove Old Fallback Stack from field_call

After Tier 1-4 chain is verified to match or exceed old stack recall:
1. Delete all 15 old fallback layers from `field_call.analyze()._visit()`
2. Delete `_resolve_with_old_suffix()` debug function
3. `_visit()` now calls only `resolve_field_call()` + callback-of-callback handling

### 4.3 Remove VariableState.targets

After field_call and param_dispatch are fully migrated:
1. Remove `VariableState.targets` dict, `assign()`, `resolve()` methods
2. Remove `DataflowEngine.assign()`, `resolve()`, `targets` backward compat
3. Remove old `dataflow.assign(...)` calls from all producers
4. Remove old `dataflow.resolve(...)` calls from all readers
5. Update `DataflowEngine.resolve_call_site_param()` and `resolve_returned_field()` to use only store

### 4.4 Target Pipeline

```
Phase 0:    direct_call
Phase 1a:   param_helpers.prepare + param_assign.register_phase
Phase 1a*:  field_call.collect (ALL files — writes struct_fields + types)
Phase 1:    param_binding + TARGET_RESOLVERS (write ScopedStore, no edges)
Phase 1b:   param_binding._resolve_fields (ALL files, no edges)
Phase 2:    field_call.analyze (Tier 1-4) + direct_call_fp + array_call + param_dispatch
Phase 3:    callback_reg (with coverage suppression)
Final:     dlsym_fp + confidence-based dedup
```

---

## Section 5: Tier-Aware Confidence Model Extension

### 5.1 field_call Confidence by Tier

| Tier | Confidence | Evidence |
|---|---|---|
| 1 | `high` | `"type-aware exact match: <type>.<field>"` |
| 2 | `high` | `"exact path match: <var>.<field>"` |
| 3 | `medium` | `"same-file suffix match: .<field>"` |
| 4 | `low` | `"cross-file suffix fallback: .<field>"` |

### 5.2 Expected FPR Improvement

Current: 276 FPs / 881 detected = **31.33%**

After Tier 1-4:
- Tier 1-2 edges (zero FP): ~85% of 605 matched + no extra FPs
- Tier 3 edges (~10% FPR): ~12% coverage with file-scoped suffix
- Tier 4 edges (~100% FPR): ~3% coverage with cross-file suffix

Estimated overall FPR: **< 15%** (down from 31.33%)
High-confidence FPR (Tier 1-2 only): **< 5%**

---

## Section 6: Migration Plan

### Task E1: Enhanced Type Tracking (2 TDD tasks)

1. Add `_collect_local_var_types()` to field_call
2. Add `_collect_cast_types()` to field_call
3. Call both from `field_call.collect()`
4. Verify type coverage improvement via new unit tests

**Acceptance**: `test_type_aware_key_isolates_different_struct_types` passes (remove xfail)

### Task E2: File-Scoped Index (1 TDD task)

1. Add `struct_field_files` to `ScopedStore`
2. Update `assign_struct_field()` to accept `filepath`
3. Update all callers to pass `filepath`
4. Add `_from_same_file()` helper

**Acceptance**: All existing tests pass; file index populated correctly

### Task E3: Tiered FieldResolver (3 TDD tasks)

1. Implement `resolve_field_call()` with 4-tier logic
2. Unit tests for each tier independently
3. Integration test: side-by-side comparison with old 15-layer stack
4. Replace `field_call.analyze()._visit()` resolution with tiered resolver

**Acceptance**: et_bench recall ≥ 98.86%; FPR reduced (tier-dependent improvement)

### Task E4: Remove Old Code (2 TDD tasks)

1. Delete old 15 fallback layers from field_call
2. Remove `param_assign.analyze()` from orchestrator
3. Remove `VariableState.targets` and all backward-compat code
4. Run full et_bench: recall must be ≥ 98.86%

**Acceptance**: All tests pass; `VariableState.targets` no longer referenced; `param_assign.analyze()` not called from orchestrator

### Task E5: Confidence Update + FPR Ceilings (1 TDD task)

1. Update FPR ceilings in `test_et_bench.py` to reflect Tier 1-4 improvements
2. Update high-confidence FPR assertion to < 5%
3. Verify high-confidence subset recall

**Acceptance**: New FPR ceilings pass; high-confidence FPR < 5%

---

## Section 7: Risk Analysis

| Risk | Likelihood | Mitigation |
|---|---|---|
| Type coverage still insufficient for Tier 1 | Low | 2 new type sources + existing 3 sources; Tier 2-3 catch remaining |
| File-scoped suffix too restrictive | Low | Tier 4 cross-file suffix as ultimate fallback |
| `param_dispatch` missing edges after removing param_assign.analyze() | Medium | E4 includes side-by-side comparison; fix param_dispatch before removal |
| Tier 3 same-file suffix still produces FPs | Medium | Marked `confidence=medium`; Tier 1-2 provide high-confidence subset |
| Performance regression from file index | Low | O(1) dict lookups for file check; net faster than full-table suffix scan |

---

## Section 8: Success Criteria

1. **Recall**: ≥ 98.86% in all 9 et_bench categories (excluding dynamic-call + virtual)
2. **FPR**: < 20% overall (target < 15%)
3. **High-confidence FPR**: < 5% (Tier 1-2 edges only)
4. **Code quality**: `field_call.py` no longer contains old 15-layer fallback stack or suffix scans of `dataflow.targets`
5. **Module hygiene**: `param_assign.analyze()` not called from orchestrator; `VariableState.targets` removed
6. **Type coverage**: `test_type_aware_key_isolates_different_struct_types` passes (no xfail)
