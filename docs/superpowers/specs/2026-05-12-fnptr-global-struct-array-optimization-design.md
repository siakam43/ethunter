# fnptr-global-struct-array 100% Recall Optimization

## Goal

Improve ethunter's `fnptr-global-struct-array` scenario recall from 77.78% (56/72 edges) to 100%.

## Current State

11 categories in ET-Bench, 4 with missing edges:

| Example | Missing Edges | Root Cause |
|---------|------|------|
| example_2 | `curl_version_info -> https_proxy_present` | Local pointer `p = &features_table[i]` aliases global array element; `p->present()` unresolved |
| example_3 | 4 edges: `ourWriteOutJSON -> writeLong/writeOffset/writeString/writeTime` | Array `variables` passed as parameter `mappings[]`; `mappings[i].writefunc()` unresolved |
| example_4 | 5 edges: `zstream_do_recompress -> lzjb/gzip/zle/lz4/zstd decompress` | Local pointer `dinfo = &zio_compress_table[3]` aliases global array element; `dinfo->ci_decompress()` unresolved |
| example_7 | 4 edges: `sha256_update -> sha256_generic/sha512_generic/tf_sha512_transform_x64/tf_sha256_transform_x64` | Multi-hop: `sha256_get_ops()` returns from global pointer array; result stored to `ctx->ops`; `ops->transform()` called |

## Design

Three independent fixes, each with TDD anchor.

### Fix A: Local Pointer Alias Resolution

**Pattern**: `p = &global_array[i]; p->field()` — `field_call` sees `p.field` but cannot resolve `p` to the global array.

**Mechanism**:
1. Extract `_collect_pointer_resolutions()` from `initializer_assign.py` into `helpers.py` as public function `collect_pointer_resolutions(tree) -> dict[str, str]`
2. In `field_call.py` Pass 1, call `collect_pointer_resolutions()` to build local-var → global-array-name mapping
3. In Pass 2 lookup sequence, when resolving `p.field` and all existing lookups fail, use the mapping to substitute `p` → `features_table`, then retry `<gstruct:features_table.field>`

**Files changed**: `helpers.py`, `initializer_assign.py`, `field_call.py`

### Fix B: Parameter-to-Global-Array Binding

**Pattern**: `caller_func(stream, variables)` with function signature `callee_func(FILE *s, struct_type mappings[])`. Inside `callee_func`, `mappings[i].field()` — `field_call` sees `mappings.field` but `mappings` is a parameter, not a global.

**Mechanism**:
1. In `initializer_assign.py` Phase 1, detect call expressions where an argument is a known global array name (dataflow has `<gstruct:arg.*>` entries). Register `param_global_map[(callee_func, param_idx)] = global_name`.
2. In `field_call.py` Pass 2 lookup sequence, when resolving `mappings.field` fails: find enclosing function name, check if any caller passed a global array to this parameter position, substitute root name, retry `<gstruct:variables.field>`.

**Files changed**: `initializer_assign.py`, `field_call.py`

### Fix C: Multi-hop Return Value Tracking

**Pattern**: `obj->field = get_ops()` where `get_ops()` returns from a global pointer array `impls[] = { &impl_a, &impl_b }`. Then `ops->transform()` is called.

**Mechanism**:
1. In `local_fp_tracker.py`, extend `collect_local_fp_assignments` to handle `assignment_expression` where RHS is a `call_expression`
2. For `obj->field = func_call()`: scan the called function's body, collect return values (identifiers/field expressions/subscript expressions), resolve them to struct instances from global arrays
3. Propagate resolved targets to `<gstruct:obj.field>` in dataflow

**Files changed**: `local_fp_tracker.py`

### Shared Principle

- TDD: write failing test anchored to each example's `ground_truth.json`, then minimal implementation
- No regression on currently passing examples
- Each fix independently verifiable

## Success Criteria

- `fnptr-global-struct-array` category recall = 100%
- All existing ET-Bench tests continue to pass
- No false positives introduced in other categories (verified via ET-Bench report)
