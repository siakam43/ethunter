# Spec: Unified Field Assignment Collector for fnptr-struct 100% Recall

## Background

fnptr-struct category currently at 15/21 (71.43%) recall. 12/14 examples pass; examples 5 and 9 fail. Root causes are two distinct gaps in field assignment detection and cross-function field tracking.

## Problem Analysis

### Example 5: Designated Initializer Not Scanned (5 edges missing)

**Code pattern:**
```c
unflushed_iter_cb_arg_t uic = {
    .uic_cb = (unflushed_iter_fn_t *)(uintptr_t)cb  // designated initializer
};
```

The `cb` parameter is stored into struct field `uic.uic_cb` via designated initializer syntax. Phase 1a `_scan_field_assigns` only inspects `assignment_expression` nodes, missing `designated_initializer` inside `initializer_list`.

**Root cause:** Three independent field-assignment scanners (`param_assign._register_phase._scan_field_assigns`, `param_assign.analyze._visit`, `field_call._collect_assignments`) all only handle `assignment_expression`, duplicating the same gap.

**Additional blocker:** `_try_register_param_to_field` requires `lhs_operand in params`, but `uic` is a local variable, not a parameter. The `struct_param_idx` it computes is never consumed by `resolve_call_site_param` — the gate is dead logic.

### Example 9: Return Value Field Tracking with Variable Name Mismatch (1 edge missing)

**Code flow:**
```
ssl_cert_new:          ret->sec_cb = ssl_security_default_callback  → <gstruct:ret.sec_cb>
SSL_CTX_get_security:  return ctx->cert->sec_cb                      → ret_fields["SSL_CTX_get_security"] = {"ctx.cert.sec_cb"}
ssl_ctx_security_debug: sdb.old_cb = SSL_CTX_get_security_callback(ctx)
                       → resolve_returned_field looks up <gstruct:ctx.cert.sec_cb> → NOT FOUND
```

**Root cause:** `resolve_returned_field` does exact-match lookup on `<gstruct:{field_path}>`. When the field was originally stored under a different variable name (e.g., `ret` vs `ctx`), lookup fails.

## Design

### Part 1: Unified Field Assignment Collector (`helpers.py`)

New function `collect_field_assignments(tree) -> list[FieldAssignment]` replaces the three scattered scanners.

**Collected forms:**

| Form | AST node | Field path source | Value extraction |
|---|---|---|---|
| `ptr->field = rhs` | `assignment_expression` | `extract_field_path(lhs)` | unwrap_cast(rhs) |
| `.field = rhs` | `designated_initializer` | `decl_var` + `.field` from `field_designator` | unwrap_cast(value) |

For `designated_initializer`, the collector walks up to the enclosing `init_declarator`, extracts the variable name via existing `extract_identifier_from_declarator` (already in helpers.py, handles `pointer_declarator`/`array_declarator` wrapping), then concatenates `var.field`.

```python
FieldAssignment = namedtuple('FieldAssignment', [
    'field_path',       # str: "uic.uic_cb", "handler.finalizeResultEmission"
    'value_node',       # ts.Node: the rhs node (identifier or cast_expression)
    'resolved_value',   # str | None: unwrapped identifier text
    'form',             # str: 'assign' | 'designated_init'
    'enclosing_func',   # str | None
    'line',             # int
])
```

**Cast unwrapping note:** The collector accepts an optional `unwrap_fn` callback parameter (avoids circular dependency: `helpers.py` should not import `dataflow.py`). Callers pass `dataflow.unwrap_cast`. Fallback: recursive identifier extraction from innermost cast child.

**Scope filtering:** `_visit` (Phase 1 Pass 2) and `_register_phase` (Phase 1a) only process FieldAssignments where `enclosing_func is not None`. Global designated initializers are skipped — they are already handled by `initializer_assign.analyze`.

### Part 2: Consumer Migration

Three consumers switch from custom AST traversal to iterating `collect_field_assignments` results:

**`param_assign._register_phase` (Phase 1a):**
- For each `FieldAssignment` where `resolved_value` is a parameter of `enclosing_func`: call `dataflow.register_param_mapping(func_name, param_idx, field_path)`
- Remove the old `_scan_field_assigns` helper and `_try_register_param_to_field` entirely
- The old `_try_register_param_to_field` had a gate requiring `lhs_operand in params`, but `struct_param_idx` was never consumed by `resolve_call_site_param`. The replacement inline logic skips this check — only verifies that `resolved_value` (the RHS identifier) is a parameter of `enclosing_func`

**`param_assign.analyze._visit` (Phase 1, Pass 2):**
- Skip FieldAssignments where `enclosing_func is None` (global scope — handled by `initializer_assign`)
- Dispatch by `fa.value_node.type`:
  - `'identifier'` or `'cast_expression'` → **Case A**: three-prong lookup:
    1. `param_mappings.get(resolved_value)` → write `<struct:{field_path}>` targets
    2. `dataflow.resolve(resolved_value)` / `<garray:{resolved_value}>` → write `<struct:{field_path}>` + `<struct:{field_name}>`
    3. (Registration handled by Phase 1a, no duplicate needed)
  - `'call_expression'` → **Case B**: extract call name, call `dataflow.resolve_returned_field(func_name)`, write `<gstruct:{field_path}>` (unchanged from existing)
- Remove old `assignment_expression` matching code from `_visit`

**`field_call._collect_assignments` (Phase 2):**
- For each `FieldAssignment` where `resolved_value` is in symbol_names: write `<gstruct:{field_path}>` to dataflow
- Remove old `_collect_assignments` AST traversal

### Part 3: Return Value Suffix Matching (`dataflow.py`)

In `DataflowEngine.resolve_returned_field`, after exact-match fails:

```
field_path "ctx.cert.sec_cb" → try suffixes: "cert.sec_cb", "sec_cb"
For each suffix, scan dataflow.targets for keys ending with ".<suffix>>"
```

This mirrors the suffix-fallback pattern already in `field_call.py` lines 146-165.

```python
if not results:
    parts = field_path.split('.')
    for i in range(1, len(parts)):
        suffix = '.'.join(parts[i:])
        for key, vals in self.state.targets.items():
            if key.endswith(f'.{suffix}>') and vals:
                results.update(vals)
                break
        if results:
            break
```

## Changes Summary

| File | Change | Est. lines |
|---|---|---|
| `helpers.py` | Add `FieldAssignment`, `collect_field_assignments` (reuses existing `extract_identifier_from_declarator`) | +55 |
| `param_assign.py` | Replace `_scan_field_assigns` / `_visit` field-assign blocks with collector iteration; relax struct-param gate | +25 / -40 |
| `field_call.py` | Replace `_collect_assignments` with collector iteration | +10 / -20 |
| `dataflow.py` | Add suffix matching in `resolve_returned_field` | +15 |
| `tests/test_et_bench.py` | Remove xfail from example_5, example_9, full_recall | ~5 |

Net: ~110 new lines, ~75 deleted.

## Risks & Known Limitations

| Risk | Severity | Mitigation |
|---|---|---|
| Cast unwrapping may extract identifiers from RHS that were previously skipped (e.g., `ptr->field = (T *)func_name`). Could create new field assignments for cast patterns not previously handled. | Low | The extracted identifier must be in `symbol_names` — only valid function names trigger writes. This actually fixes previously-missed legitimate assignments. |
| Suffix matching in `resolve_returned_field` may match wrong field if two different struct types have identically-named fields (e.g., `struct A.cb` and `struct B.cb`). | Low | Existing suffix fallback in `field_call.py:146-165` has the same behavior. ET-Bench fixtures are small enough that field names are unique. |
| Nested designated initializers (`.outer.inner = val`) not handled. | Low | Not present in current ET-Bench fixtures. Collector only extracts one level of variable name from `init_declarator`. |
| Redundant `<struct:>` writes for globals if `_visit` doesn't filter by scope. | Low | Addressed by scope filtering (see Design). |

## Verification

1. `test_et_bench_report` — fnptr-struct recall must reach 100% (15/15 → 21/21)
2. `test_et_bench_fnptr_struct_example_5` — remove xfail, all 5 edges matched
3. `test_et_bench_fnptr_struct_example_9` — remove xfail, edge matched
4. `test_et_bench_fnptr_struct_full_recall` — remove xfail, asserts 100%
5. Full test suite — no regressions in any category
6. Manual trace of the two examples (documented in Problem Analysis above) — verified in design review
