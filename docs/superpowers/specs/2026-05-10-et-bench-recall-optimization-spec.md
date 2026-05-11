# ET-Bench Recall Optimization Spec

## Current State (After Optimizations)

```
Category                               Matched   Expected     Recall
-------------------------------------------------------------------
fnptr-callback                              24         36     66.67%
fnptr-cast                                   1         10     10.00%
fnptr-dynamic-call                           0          6      0.00%
fnptr-global-array                         307        307    100.00%
fnptr-global-struct                          5         68      7.35%
fnptr-global-struct-array                   51         72     70.83%
fnptr-library                               33         70     47.14%
fnptr-only                                  15         24     62.50%
fnptr-struct                                 5         21     23.81%
fnptr-varargs                                1          1    100.00%
fnptr-virtual                                0          2      0.00%
-------------------------------------------------------------------
OVERALL                                    442        617     71.64%
```

**Benchmark tests**: cJSON 85.71% indirect recall, libuv 100% indirect recall.
**All 74 tests passing**.

## Optimizations Applied

### 1. field_call.py: garray fallback
- Added fallback to check `<garray:{base_name}>` when `<gstruct:{field_path}>` fails
- Handles cases like `global_hooks.deallocate()` where `global_hooks` was initialized as an array
- File: `src/ethunter/analyzer/field_call.py`

### 2. field_call.py: suffix match on struct keys
- Added progressive suffix matching (e.g., `input_buffer.hooks.allocate` → try `<struct:hooks.allocate>`, then `<struct:hooks>`)
- Added middle-component matching (e.g., `hooks` alone as `<struct:hooks>`)
- File: `src/ethunter/analyzer/field_call.py`

### 3. param_assign.py: garray fallback for struct resolution
- When resolving `p.hooks = global_hooks`, also checks `<garray:global_hooks>`
- Also stores `<struct:hooks>` (field name alone) for alias variables
- File: `src/ethunter/analyzer/param_assign.py`

### 4. param_assign.py: expanded REG_PATTERNS
- Added: `once`, `submit`, `post`, `work`, `spawn`, `scandir`, `sort`, `filter`, `notify`, `watch`, `dispatch`, `schedule`
- Enables detection of `uv_once()`, `uv__work_submit()`, `scandir()` as callback registrations
- File: `src/ethunter/analyzer/param_assign.py`

### 5. param_assign.py: recursive _extract_param_name
- Rewrote to recursively search declarator tree for function pointer parameter names
- Handles `void (*convert_to_ascii)(unsigned char *, unsigned char *)` patterns
- File: `src/ethunter/analyzer/param_assign.py`

### 6. param_assign.py: Pass 4 — call-site edge emission
- When a call-site passes function pointers as args, emits edges from the call-site's enclosing function to the actual targets
- Handles the curl `Curl_auth_create_digest_http_message → auth_digest_md5_to_ascii` pattern
- File: `src/ethunter/analyzer/param_assign.py`

### 7. orchestrator.py: param_assign callback edges
- Added Phase 1b to capture callback_reg edges from param_assign (previously discarded)
- File: `src/ethunter/analyzer/orchestrator.py`

### 8. helpers.py: extract_field_path for subscript+field chains
- Added handling for `arr[i].field()` patterns (subscript_expression inside field_expression)
- `auxFieldHandlers[j].setter()` now extracts path as `auxFieldHandlers.setter`
- File: `src/ethunter/analyzer/helpers.py`

### 9. helpers.py: pointer_expression in init_declarator
- `handle_init_declarator` now handles `*var = &target` (pointer_expression values)
- File: `src/ethunter/analyzer/helpers.py`

### 10. direct_assign.py: pointer_expression support
- Handles `*Curl_ssl = &Curl_ssl_openssl` pattern for struct pointer aliases
- Always stores the alias even if target is not a function name
- File: `src/ethunter/analyzer/direct_assign.py`

### 11. field_call.py: struct alias resolution
- When `Curl_ssl->sha256sum` fails exact match, resolves `Curl_ssl` → `Curl_ssl_openssl`
- Then looks up `<gstruct:Curl_ssl_openssl.sha256sum>`
- File: `src/ethunter/analyzer/field_call.py`

### 12. initializer_assign.py: numeric struct keys
- For pure array initializers `{ func_a, func_b }`, also stores `<gstruct:var.0>`, `<gstruct:var.1>`, etc.
- Enables field_call to match numeric index-based lookups
- File: `src/ethunter/analyzer/initializer_assign.py`

## Remaining Gaps (Target 95%+)

### fnptr-global-struct (4.41%) and fnptr-global-struct-array (0.00%) — 140 edges
These likely involve struct initializers and vtable patterns not yet detected by `initializer_assign`. Need to investigate specific fixtures.

### fnptr-cast (10.00%) — 9 edges
`cast_assign` handles `(cast_type)func` patterns but may miss complex cast expressions or typedef'd function pointer casts.

### fnptr-global-struct (4.41%) — 65 edges
Global struct field assignments that initializer_assign or field_call don't detect.

### fnptr-struct (23.81%) — 16 edges
Struct-based function pointer patterns not yet covered.

### fnptr-library (47.14%) — 37 edges
Library-level patterns involving cross-file function pointer flow.

### fnptr-dynamic-call (0.00%) and fnptr-virtual (0.00%) — 8 edges
Excluded per original requirements (dlsym and virtual dispatch patterns).

## Architecture Summary

The two-phase analyzer architecture:

**Phase 1: Target Resolution** (writes to dataflow)
- `direct_assign`: `fp = func`, alias chains
- `initializer_assign`: `{ .field = func }`, `{ func1, func2 }`
- `cast_assign`: `(cast_type)func`
- `param_assign`: callback parameters, registration patterns, struct field propagation

**Phase 1b: Callback Edges**
- `param_assign` returns callback_reg edges for registration patterns

**Phase 2: Call Detection** (reads from dataflow)
- `direct_call_fp`: `fp()` calls where fp was assigned
- `field_call`: `obj.field()`, `ptr->field()` calls
- `array_call`: `arr[i]()` calls

**Independent**
- `dlsym_fp`: `dlsym()` dynamic loading
