# fnptr-global-struct-array 100% Recall Optimization

## Goal

Improve ethunter's `fnptr-global-struct-array` scenario recall from 77.78% (56/72 edges) to 100%.

## Current State

4 examples with missing edges:

| Example | Missing Edges | Root Cause |
|---------|------|------|
| example_2 | `curl_version_info -> https_proxy_present` | Local pointer `p = &features_table[i]` aliases global array element; `p->present()` unresolved |
| example_3 | 4 edges: `ourWriteOutJSON -> writeLong/writeOffset/writeString/writeTime` | Array `variables` passed as parameter `mappings[]` + positional index bug in struct field mapping |
| example_4 | 5 edges: `zstream_do_recompress -> lzjb/gzip/zle/lz4/zstd decompress` | Local pointer `dinfo = &zio_compress_table[3]` aliases global array element; `dinfo->ci_decompress()` unresolved |
| example_7 | 4 edges: `sha256_update -> sha256_generic/sha512_generic/tf_sha512_transform_x64/tf_sha256_transform_x64` | Multi-hop chain: missing pointer_expression in array init + call_expression RHS handling + field_call local var integration + positional index bug |

## Design

### Bug 0 (Prerequisite): Fix positional index tracking in `_process_init_list`

**Symptom**: Struct fields mapped to wrong function targets. e.g., `<gstruct:sha256_generic_impl.transform>` = `{sha2_is_supported}` instead of `{sha256_generic}`.

**Root cause**: `_process_init_list` iterates `init_list.children` but only increments the positional index for `identifier`, `cast_expression`, and `call_expression` node types. `string_literal`, `number_literal`, and `null` nodes are skipped without incrementing the counter, causing all subsequent values to be mapped to wrong field indices.

Example: `{"generic", sha256_generic, sha2_is_supported}` — `"generic"` (string_literal) is skipped without index increment. `sha256_generic` gets index 0 (should be 1). `sha2_is_supported` gets index 1 (should be 2).

**Fix**: In the `_process_init_list` positional loop, increment `index` for EVERY child that is a value node (not `{`, `}`, or `,`). Specifically, increment for: `identifier`, `cast_expression`, `call_expression`, `string_literal`, `number_literal`, `null`, `pointer_expression`, `field_expression`, `parenthesized_expression`, `char_literal`, `concatenated_string`, `sizeof_expression`, `conditional_expression`, `binary_expression`, `unary_expression`, `subscript_expression`.

**Mechanism**: Replace `if c.type in ('identifier', 'cast_expression', 'call_expression'):` with a broader check that increments index for all non-syntax node types, while still only storing function targets for identifier/cast/call nodes.

**Files changed**: `initializer_assign.py`

**Test anchor**: example_7 should show `<gstruct:sha256_generic_impl.transform> = {sha256_generic}` after Phase 1. example_3 should show `<gstruct:variables.writefunc>` with correct function targets.

---

### Fix A: Local Pointer Alias Resolution

**Pattern**: `p = &global_array[i]; p->field()` — `field_call` sees `p.field` but cannot resolve `p` to the global array.

**Mechanism**:
1. Extract `_collect_pointer_resolutions()` from `initializer_assign.py` into `helpers.py` as public function `collect_pointer_resolutions(tree) -> dict[str, str]`
2. Extend it to also handle `&field_expression` (e.g., `p = &obj->field` → maps `p` to the field path `obj.field`). Currently only handles `&identifier` and `&identifier[expr]`.
3. In `field_call.py` Pass 1, call `collect_pointer_resolutions()` to build local-var → target-name mapping
4. In Pass 2 lookup sequence, when resolving `p.field` and all existing lookups fail:
   - Look up `p` in pointer resolutions → get target name `features_table` (or field path like `ctx.sha256`)
   - Construct new key `<gstruct:{target}.{field}>` and retry dataflow lookup
   - If target is a field path (e.g., `ctx.sha256`), construct `<gstruct:ctx.sha256.field>`

**Files changed**: `helpers.py`, `initializer_assign.py`, `field_call.py`

---

### Fix B: Parameter-to-Global-Array Binding

**Pattern**: `caller_func(stream, variables)` where `variables` is a known global array. Inside `callee_func`, `mappings[i].field()` — `field_call` sees `mappings.field` but `mappings` is a parameter.

**Mechanism**:
1. In `initializer_assign.py` Phase 1, detect call expressions where an argument name is a known global array (dataflow has `<gstruct:arg.*>` or `<garray:arg>` entries). Register `param_alias_map[(callee_func, param_name)] = global_name` using the callee's **parameter name** (not index), avoiding the need for field_call to compute parameter positions.
2. In `field_call.py` Pass 2 lookup sequence, when resolving `mappings.field` fails: find enclosing function name + the root of field_path (`mappings`), look up `param_alias_map[(enclosing_func, root)]` → get global array name, construct new path `<gstruct:global_name.field>`, retry lookup.

**Note**: After Bug 0 fix, suffix matching on `*.writefunc` in `field_call` may already resolve `mappings.writefunc` → `variables.writefunc`. Fix B is still needed for cases where suffix matching is ambiguous (multiple arrays with same field name) or where the global array name differs substantially from the parameter name.

**Files changed**: `initializer_assign.py`, `field_call.py`

---

### Fix C: Multi-hop Return Value Tracking + Field Call Local Var Integration [Precision Enhancement]

**Note**: After Bug 0 fix, example_7 may already pass via `field_call`'s suffix matching (matching `*.transform` against `<gstruct:sha256_generic_impl.transform>`, etc.). Fix C provides a more precise resolution path that does not depend on broad suffix matching, reducing false-positive risk when multiple struct types share the same field name.

**Pattern**: Three-hop chain in example_7:
```
sha256_get_ops() returns from generic_supp_impls[idx]
  → ctx256->ops = sha256_get_ops()       [field assign with call_expression RHS]
  → ops = ctx->ops                       [local var from field expression]
  → ops->transform()                     [field call through local var]
```

This requires three sub-fixes:

**C1: Handle `pointer_expression` in array initializers**

`sha256_impls[] = { &sha256_generic_impl, &sha256_x64_impl }` — `_process_init_list` skips `pointer_expression` nodes. After Bug 0 (which adds pointer_expression to the index increment list), also add target extraction: unwrap `pointer_expression` → `identifier` and resolve via `symbol_names`, storing the struct name as a target. For `&struct_name`, the target is the struct itself (used later for field resolution).

**C2: Handle `call_expression` RHS in field assignments**

`ctx256->ops = sha256_get_ops()` — `_track_pointer_field_assignments` only handles `rhs.type == 'identifier'`. Extend to handle `call_expression` with depth-limited inter-procedural scanning (max 2 levels: callee body + one hop of global array write tracing):

a. Find the called function's AST (same-file lookup)
b. Scan its body for return statements; collect return expressions
c. For each return expression, resolve within a 2-level depth limit:
   - Level 0: `return &global_struct` → extract struct name directly
   - Level 0: `return global_array[idx]` → if `<garray:global_array>` exists, resolve all elements
   - Level 1 (when Level 0 finds empty array): scan all function bodies for `global_array[...] = source_expr` patterns where `source_expr` references another global array. If found, use the source array's `<garray:>` elements
d. For each resolved element (struct name), look up `<gstruct:element.field_name>` in dataflow for the target field
e. Merge all found targets into `<gstruct:obj.field>` in dataflow

For example_7: `sha256_get_ops()` returns from `generic_supp_impls[idx]` (Level 0). `generic_supp_impls` is empty in dataflow (runtime-populated via `generic_impl_init()`). Level 1 traces `generic_supp_impls[c++] = sha256_impls[i]` in `generic_impl_init()` → discovers source array `sha256_impls[]`. After C1, `<garray:sha256_impls>` contains `{sha256_generic_impl, sha256_x64_impl}`. Look up `<gstruct:sha256_generic_impl.transform>` etc. → get all 4 transform functions.

**C3: `field_call` integration with `local_fp_tracker`**

When `field_call` resolves `ops.transform` and all existing lookups fail:
- Check if `ops` is a local variable in the `local_fp_tracker` mapping
- If so, `ops` was assigned from a struct field (e.g., `ops = ctx->ops`), and the local mapping contains the resolved targets
- Use those targets directly

Alternative simpler approach for C3: In `field_call` Pass 2, when resolving `local_var.field`:
- Call `collect_local_fp_assignments(tree, dataflow, symbol_names)` to get local var mappings
- If `local_var` is in the mapping, it was assigned from a struct field expression
- The local mapping contains the RESOLVED targets (not field paths), so use them directly

**Files changed**: `initializer_assign.py` (C1, C2), `field_call.py` (C3)

---

### Shared Principle

- TDD: write failing test anchored to each example's `ground_truth.json`, then minimal implementation
- No regression on currently passing examples
- Each fix independently verifiable
- Implement in order: Bug 0 → Fix A → Fix B → Fix C

## Success Criteria

- `fnptr-global-struct-array` category recall = 100%
- All existing ET-Bench tests continue to pass
- No false positives introduced in other categories (verified via ET-Bench report)
