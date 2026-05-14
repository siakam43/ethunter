# Scoped Dataflow Architecture Redesign

**Date**: 2026-05-14
**Status**: Draft — awaiting review
**Defects addressed**: #1 (unscoped dataflow), #2 (field_call fallback stack), #3 (dual module), #4 (no confidence), #6 (incomplete type info)

## Motivation

The current architecture suffers from a fundamental flaw: `VariableState.targets` is a flat global namespace (`dict[str, set[str]]`) shared across all functions and files. This forces downstream modules — especially `field_call` — to use 15+ layers of fallback resolution with suffix/prefix scans that iterate the entire dict, producing false positives from unrelated code.

The recent architectural refactor (splitting `param_assign` into `param_binding` + `param_dispatch` + `callback_reg`) reduced overall FPR from 60.98% to 31.33%, but left the underlying data model unchanged. The remaining 276 false positives (31.33% FPR) are driven primarily by cross-function dataflow pollution and indiscriminate suffix matching.

This design replaces the global flat dataflow with a function-scoped store, unifies the key convention, replaces the fallback stack with a strategy chain, completes the module migration, and adds an evidence model.

## Design Principles

1. **Explicit over implicit** — every dataflow write has a known scope; every resolve requires scope context
2. **Exact match over scan** — no iteration over the entire dataflow dict; all lookups are exact key queries
3. **Type-aware routing** — when struct type is known, use it for zero-FP matching
4. **Phase-gated migration** — each phase independently verifiable against et_bench; no regression on recall

---

## Section 1: ScopedStore — New Data Model

### 1.1 Current State

`VariableState.targets` — flat `dict[str, set[str]]`:

| Key pattern | Writer | Problem |
|---|---|---|
| `pname` (bare) | param_binding, param_assign | All functions' params merged |
| `varname` (bare) | direct_assign, helpers | All functions' locals merged |
| `callee:pname` | param_binding | Better but still unscoped |
| `<var>:<func>:<name>` | direct_assign, cast_assign | Already scoped (keep) |
| `<gstruct:<path>>` | initializer_assign, field_call, param_binding | Struct field (global OK) |
| `<struct:<path>>` | param_assign, param_binding | Redundant with gstruct |
| `<garray:<name>>` | initializer_assign | Global array (global OK) |

### 1.2 New Model

Replace `VariableState.targets` with four separate stores inside `DataflowEngine`:

```python
@dataclass
class ScopedStore:
    """Function-scoped variable → targets mapping.
    
    Keys are ALWAYS (func_name, var_name) tuples.
    Cross-function information flows through explicit bridges
    (call_site_targets, param_fields, ret_fields), not through
    shared variable names.
    """
    # (func_name, var_name) -> {target_functions}
    func_vars: dict[tuple[str, str], set[str]] = field(default_factory=dict)
    
    # Struct field targets: "gstruct:<path>" -> {target_functions}
    # Path is either "<base_var>.<field_path>" or "<struct_type>.<field_path>"
    struct_fields: dict[str, set[str]] = field(default_factory=dict)
    
    # Global array targets: "garray:<var_name>" -> {target_functions}
    global_arrays: dict[str, set[str]] = field(default_factory=dict)
    
    # Vtable entries: "vtable:<struct_type>.<field_name>" -> {target_functions}
    vtable_entries: dict[str, set[str]] = field(default_factory=dict)
```

### 1.3 Write Rules

| Producer | Store | Key | Value |
|---|---|---|---|
| `direct_assign` | `func_vars` | `(enclosing_func, var_name)` | `{targets}` |
| `cast_assign` | `func_vars` | `(enclosing_func, var_name)` | `{targets}` |
| `param_binding` | `func_vars` | `(call_name, pname)` | `{targets}` |
| `helpers.collect_field_assignments` | `func_vars` | `(enclosing_func, var_name)` | `{targets}` |
| `initializer_assign` | `struct_fields` | `gstruct:<var>.<path>` AND `gstruct:<type>.<path>` | `{targets}` |
| `field_call` Pass 1 | `struct_fields` | `gstruct:<var>.<path>` | `{targets}` |
| `param_binding._resolve_fields` | `struct_fields` | `gstruct:<var>.<path>` AND `gstruct:<type>.<path>` | `{targets}` |
| `initializer_assign` | `global_arrays` | `garray:<name>` | `{targets}` |
| New vtable module | `vtable_entries` | `vtable:<type>.<field>` | `{targets}` |

**No bare-name writes.** Every write is scoped to a function or a qualified struct/array key.

Note: `helpers.py` currently writes bare variable names via `collect_field_assignments` (`helpers.py:104,115`). In Phase A, these are updated to accept a `store` parameter and write to `func_vars[(enclosing_func, var_name)]` instead.

### 1.4 Read Rules

| Consumer | Store | Key | Notes |
|---|---|---|---|
| `direct_call_fp` | `func_vars` | `(caller_func, var_name)` | Exact lookup only |
| `param_dispatch` Pass A | `func_vars` | `(enclosing_func, param_name)` | Exact lookup |
| `param_dispatch` Pass A | `call_site_targets` | `(caller, callee, arg_idx)` | Per-call-site bridge |
| `field_call` | `struct_fields` | `gstruct:<type>.<path>` first, then `gstruct:<var>.<path>` | Strategy chain |
| `array_call` | `global_arrays` | `garray:<name>` | Exact lookup |

**No bare-name reads. No suffix scans. No iteration over all keys.**

### 1.5 Cross-Function Bridges (Explicit)

Data that must cross function boundaries flows through these explicit structures (already exist on `DataflowEngine`, unchanged):

- `param_fields: dict[(func, param_idx), set[field_path]]` — param→field mappings
- `ret_fields: dict[func, set[field_path]]` — return value→field mappings
- `call_site_targets: dict[(caller, callee, arg_idx), set[target]]` — per-call-site target tracking
- `param_alias_map: dict[(func, local_var), global_name]` — parameter aliases
- `registration_sites: list[dict]` — registration sites for Phase 3

### 1.6 Migration (Phase A)

Dual-write during migration:
- All producers write to BOTH old `VariableState.targets` AND new `ScopedStore`
- Readers switch one at a time: direct_call_fp first (simplest), field_call last (most complex)
- Verify via `test_et_bench_report` after each reader switch
- Remove old `targets` dict after all readers migrated

---

## Section 2: Unified Key Convention

### 2.1 Current State

12+ distinct key formats across modules:

```
<var>:<func>:<name>        # scoped (good)
<gstruct:<path>>           # from initializer_assign (good)
<gstruct>:<type>.<path>    # type-aware (good)
<struct:<path>>            # from param_assign (redundant)
<struct:<name>>            # from param_binding (redundant)
<garray:<name>>            # global array (good)
<chain:<path>>             # unused by any writer, only resolved
<vtable:<path>>            # unused, resolved by field_call
<vtable_init>              # unused
<call_name>:<pname>       # param_binding (absorbed into func_vars)
bare <pname>               # problematic (absorbed into func_vars)
bare <varname>             # problematic (absorbed into func_vars)
```

### 2.2 Target State

Four key formats, each in its own store:

| Format | Store | Example |
|---|---|---|
| `(func, var)` tuple | `func_vars` | `("register_callback", "cb")` |
| `gstruct:<path>` | `struct_fields` | `gstruct:ssl_ctx.ext.alpn_select_cb` |
| `gstruct:<type>.<path>` | `struct_fields` | `gstruct:SSL_CTX.ext.alpn_select_cb` |
| `garray:<name>` | `global_arrays` | `garray:global_hooks` |
| `vtable:<type>.<field>` | `vtable_entries` | `vtable:region_model_context_vtable.get_state_map_by_name` |

### 2.3 Dual-Write Convention

When struct type IS known, producers write BOTH:
- `gstruct:<var>.<path>` → `{targets}`  (exact var name match)
- `gstruct:<type>.<path>` → `{targets}`  (type-aware, cross-var match)

The resolver tries type-aware first. If the same struct type appears under different variable names in different functions, the type-aware key bridges them without suffix scanning.

### 2.4 Eliminated Formats

| Old Format | Reason | Replacement |
|---|---|---|
| `<struct:<path>>` | Historical, same semantics as gstruct | `gstruct:<path>` |
| `<struct:<name>>` | Short form, imprecise | `gstruct:<var>.<path>` |
| `<chain:<path>>` | Never written by any producer | Removed |
| `<vtable:<path>>` | Never written | `vtable:<type>.<field>` |
| `<vtable_init>` | Dead code | Removed |
| `callee:pname` | Absorbed into func_vars | `func_vars[(callee, pname)]` |
| bare `pname` | Cross-function pollution | `func_vars[(func, pname)]` |
| bare `varname` | Cross-function pollution | `func_vars[(func, varname)]` |

---

## Section 3: field_call Strategy Chain

### 3.1 Current State

`field_call.py:108-218` — one large function with 15+ fallback layers, 3 of which iterate `dataflow.targets` by suffix/prefix.

### 3.2 New Design: `FieldResolver`

```python
class FieldResolver:
    """Chain of resolution strategies for struct field function pointer calls.
    
    Each strategy does exact key lookups only. No suffix scans, no
    iteration over all dataflow entries. Returns empty set if unresolvable.
    """
    
    def __init__(self, store: ScopedStore, dataflow: DataflowEngine,
                 symbol_table: SymbolTable, local_fp_mapping: dict,
                 pointer_resolutions: dict):
        # `store` is the ScopedStore (struct_fields, global_arrays, vtable_entries)
        # `dataflow` is the full DataflowEngine for cross-function bridges
        #   (param_alias_map, call_site_targets)
        self.strategies: list[ResolutionStrategy] = [
            TypeAwareStructLookup(store, symbol_table),
            ExactPathStructLookup(store),
            TypeAwareVtableLookup(store, symbol_table),
            GlobalArrayLookup(store),
            StructAliasLookup(store),
            ParamAliasLookup(dataflow),
            LocalFpLookup(local_fp_mapping),
            PointerAliasLookup(pointer_resolutions),
        ]
    
    def resolve(self, field_path: str, base_var: str,
                caller_func: str | None = None) -> set[str]:
        """Resolve targets for a field expression call.
        
        Args:
            field_path: e.g., "ctx.ext.alpn_select_cb"
            base_var: e.g., "ctx" (the base variable in the expression)
            caller_func: enclosing function name (for param alias lookup)
        
        Returns:
            Set of target function names, or empty set.
        """
        context = ResolutionContext(
            field_path=field_path,
            base_var=base_var,
            caller_func=caller_func,
        )
        for strategy in self.strategies:
            targets = strategy.resolve(context)
            if targets:
                return targets
        return set()
```

### 3.3 Individual Strategies

Each strategy is a separate class with a single `resolve(context) -> set[str]` method:

**TypeAwareStructLookup**
```
Query: struct_fields["gstruct:<type>.<path>"]
Where type = symbol_table.get_var_type(base_var)
FP risk: zero (type + field path are exact)
```

**ExactPathStructLookup**
```
Query: struct_fields["gstruct:<base_var>.<field_path>"]
FP risk: zero (variable name + field path are exact)
Limit: only matches when the SAME variable name was used in the assignment
```

**TypeAwareVtableLookup**
```
Query: vtable_entries["vtable:<type>.<field_name>"]
Where type = symbol_table.get_var_type(base_var)
FP risk: zero
```

**GlobalArrayLookup**
```
Query: global_arrays["garray:<base_var>"]
FP risk: zero
```

**StructAliasLookup**
```
Query: resolve base_var alias, then gstruct:<resolved>.<suffix>
Example: Curl_ssl -> Curl_ssl_openssl
FP risk: near-zero (requires alias to exist)
```

**ParamAliasLookup**
```
Query: dataflow.param_alias_map[(caller_func, base_var)] -> global_name
Then: struct_fields["gstruct:<global_name>.<suffix>"]
FP risk: near-zero (requires explicit param alias registration)
```

**LocalFpLookup**
```
Query: local_fp_mapping[base_var] -> targets
Uses existing local_fp_tracker module (already function-scoped)
FP risk: zero (local_fp_tracker is function-scoped by design)
```

**PointerAliasLookup**
```
Query: pointer_resolutions[base_var] -> resolved_base
Then: struct_fields["gstruct:<resolved_base>.<suffix>"]
FP risk: near-zero
```

### 3.4 What Is NOT in the Chain

- No suffix scan of `struct_fields` keys
- No bare-parameter-name lookup
- No `endswith` matching
- No "try last component alone" fallback
- No `<vtable_init>` generic lookup

### 3.5 Callback-of-Callback Handling

The existing callback-of-callback logic (`field_call.py:220-253`) moves to a separate method that runs AFTER target resolution:

```python
def _resolve_callback_of_callback(targets, call_node, func_fp_params, symbol_names):
    """For each resolved target, check if it has fnptr params
    that receive known functions as arguments."""
    # Same logic as current, but only runs when explicit targets exist
```

---

## Section 4: Pipeline Simplification

### 4.1 Current State

```
Phase 0:   direct_call
Phase 1a:  param_helpers.prepare + param_assign._register_phase
Phase 1:   param_binding + TARGET_RESOLVERS (4 modules)
Phase 1b:  param_binding._resolve_fields
Phase 1b*: param_assign.analyze()           ← OLD, produces edges
Phase 2:   CALL_DETECTORS (3 modules)
Phase 2*:  param_dispatch                    ← NEW, produces callback_param edges
Phase 3:   callback_reg + dlsym_fp
Post:     field_callees filter + dedup       ← cleans up dual-module overlap
```

### 4.2 Target State

```
Phase 0:   direct_call
Phase 1a:  param_helpers.prepare + register_phase (metadata, no edges)
Phase 1:   param_binding + TARGET_RESOLVERS (write ScopedStore, no edges)
Phase 1b:  param_binding._resolve_fields (field resolution, no edges)
Phase 2:   CALL_DETECTORS + param_dispatch (read ScopedStore, produce edges)
Phase 3:   callback_reg (produces edges with suppression)
Final:    dlsym_fp + dedup_by_confidence
```

### 4.3 Removed Code (Phased)

- `param_assign.analyze()` — entire function (~400 lines, Pass 1-4)
- `param_assign.REG_PATTERNS` — moved to `param_helpers`
- `param_assign._is_registration()` — already in `param_helpers`
- `param_assign._propagate_call_site()` — absorbed into `param_binding`
- Orchestrator post-hoc `field_callees` filter (lines 152-161):
  - **Kept in Phases A-C** as safety net for callback_param suppression by field_call
  - **Removed in Phase D** — replaced by confidence-based dedup (Section 5.3)
  - `callback_reg`'s own Stage 2 coverage check (using `covered_callees`) remains throughout

### 4.4 Retained from param_assign.py

- `_register_phase()` — renamed to `register_phase()` at module level
- `_collect_func_params()`, `_collect_fnptr_typedefs()`, `_has_fnptr_declarator()` — moved to `param_helpers`
- `_collect_simple_macros()` — already in `param_helpers`
- Utility functions: `_find_child()`, `_extract_param_name()`, `_find_func_name_from_decl()` — moved to `param_helpers`

---

## Section 5: Confidence Model

### 5.1 CallEdge Extension

```python
@dataclass
class CallEdge:
    caller: str
    callee: str
    caller_file: str
    callee_file: str
    type: CallType
    indirect_kind: str
    caller_line: int
    confidence: str = 'medium'   # NEW: 'high' | 'medium' | 'low'
    evidence: str = ''           # NEW: human-readable evidence description
```

### 5.2 Confidence Assignment

| Producer | indirect_kind | Match Type | Confidence | Evidence |
|---|---|---|---|---|
| `direct_call` | (DIRECT) | AST name match | `high` | `"direct call expression"` |
| `direct_call_fp` | `direct_assign` | `<var>:<func>:<name>` scoped | `high` | `"scoped variable resolution"` |
| `direct_call_fp` | `direct_assign` | local_fp_mapping | `medium` | `"local fp from struct field"` |
| `field_call` | `field_call` | type-aware key | `high` | `"type-aware gstruct match: <type>.<path>"` |
| `field_call` | `field_call` | exact path key | `high` | `"exact gstruct match: <var>.<path>"` |
| `field_call` | `field_call` | vtable key | `high` | `"vtable match: <type>.<field>"` |
| `field_call` | `field_call` | alias/fallback | `medium` | `"struct alias resolution: <alias> -> <target>"` |
| `param_dispatch` Pass A | `callback_param` | call_site_targets | `high` | `"fnptr call in callee body"` |
| `param_dispatch` Pass B | `callback_param` | call_site_targets | `medium` | `"call-site caller -> target"` |
| `callback_reg` Stage 1 | `callback_reg` | param_usage='caller' | `medium` | `"behavioral: fnptr called in callee"` |
| `callback_reg` Stage 3 | `callback_reg` | heuristic | `low` | `"heuristic: registration name match"` |
| `dlsym_fp` | `dlsym_fp` | string literal | `low` | `"dlsym string literal match"` |

### 5.3 Deduplication with Confidence

When duplicate (caller, callee) pairs exist, keep the edge with HIGHER confidence:

```python
def dedup_with_confidence(edges: list[CallEdge]) -> list[CallEdge]:
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
```

### 5.4 Confidence Assertions in Tests

Add optional filter to benchmark tests:
```python
def compute_recall(found_edges, expected_edges, min_confidence=None):
    if min_confidence:
        found_edges = [e for e in found_edges 
                       if getattr(e, 'confidence', 'medium') >= min_confidence]
    # ... existing logic
```

---

## Section 6: Type Tracking Completion

### 6.1 Current State

`SymbolTable._var_types: dict[str, str]` — tracks variable name → struct type, but:
- Only populated for variables in global struct initializers (`initializer_assign`)
- Not populated for local variables, function parameters, or struct field assignments
- Not function-scoped

### 6.2 Target State

```python
class SymbolTable:
    # Existing
    _typedefs: dict[str, str]
    _struct_fields: dict[str, list[str]]
    
    # Enhanced
    _var_types: dict[str, str]           # global vars (unchanged)
    _func_var_types: dict[tuple[str, str], str]  # NEW: (func, var) -> struct_type
    
    def record_func_var_type(self, func: str, var: str, struct_type: str):
        """Record a function-scoped variable's struct type."""
        self._func_var_types[(func, var)] = struct_type
    
    def get_func_var_type(self, func: str | None, var: str) -> str | None:
        """Get struct type for a variable, checking func-scoped first."""
        if func:
            result = self._func_var_types.get((func, var))
            if result:
                return result
        # Fallback to global var types
        return self._var_types.get(var)
```

### 6.3 Type Information Sources

| Source | What It Tracks | Module |
|---|---|---|
| Global struct init | `struct type_a obj = {...}` | `initializer_assign` (existing) |
| Local struct var | `struct type_a *ptr;` | `field_call` Pass 1 (new) |
| Function params | `void fn(struct type_a *ptr)` | `param_helpers.prepare()` (new) |
| Typedef resolution | `typedef struct foo bar_t` | `SymbolTable` (existing) |
| Struct field types | `struct a { fn_t handler; }` | `SymbolTable` (existing) |

### 6.4 Type Collection in param_helpers.prepare()

```python
def prepare(tree, filepath, engine, symbol_table):
    # Existing: collect func_params, func_fp_params
    
    # NEW: collect parameter types (cross-file accumulation on symbol_table)
    _collect_param_types(tree.root_node, filepath, symbol_table)
```

`_collect_param_types()` scans function definitions and records `(func_name, param_name)` → struct type for all parameters with struct pointer types. Since `symbol_table` is shared across all files in the analysis, types from one file are visible to `field_call` in another file.

### 6.5 Type Collection in field_call Pass 1

```python
# In field_call Pass 1, when processing a field assignment:
# handler.cb = func_a
# "handler" is a local variable. Extract its declared type from
# the enclosing function's declaration list.
base_var = field_path.split('.')[0]
struct_type = resolve_struct_type(base_var, enclosing_func, tree)
if struct_type:
    symbol_table.record_func_var_type(enclosing_func, base_var, struct_type)
    # Write type-aware key
    dataflow.store.struct_fields[f'gstruct:{struct_type}.{field_path}'] = targets
```

---

## Section 7: Migration Plan

### Phase A: ScopedStore (estimated: 2-3 TDD tasks)

1. Add `ScopedStore` to `DataflowEngine` (no behavior change)
2. Dual-write in all producers: direct_assign, cast_assign, initializer_assign, param_binding, field_call Pass 1, helpers
3. Switch readers one at a time: direct_call_fp → array_call → param_dispatch → field_call
4. Verify `test_et_bench_report` recall unchanged after each switch
5. Remove `VariableState.targets` and backward-compat write paths

**Acceptance**: All 56 et_bench tests pass; recall ≥ 98.86%; FPR unchanged (dual-write = identical output)

### Phase B: Unified Keys (estimated: 2 TDD tasks)

1. Remove `<struct:>` writes; producers use `gstruct:` exclusively
2. Remove `<struct:>` reads from field_call strategies
3. Standardize dual-write convention (type-aware key always written alongside exact key)
4. Add type collection in `param_helpers.prepare()` and `field_call` Pass 1 (defect #6)

**Acceptance**: All 56 et_bench tests pass; recall ≥ 98.86%; FPR unchanged or slightly decreased

### Phase C: field_call Rewrite + Remove param_assign.analyze() (estimated: 3-4 TDD tasks)

1. Implement `FieldResolver` with strategy chain (Section 3)
2. Replace `field_call._visit()` resolution logic with `FieldResolver`
3. Remove suffix scans and endswith matching
4. Remove `param_assign.analyze()` from orchestrator
5. Run et_bench: FPR should decrease significantly; recall must not drop
6. If recall drops on any category: identify which strategy needs enhancement BEFORE proceeding

**Acceptance**: All 56 et_bench tests pass; recall ≥ 98.86%; FPR < 20%

### Phase D: Confidence Model (estimated: 2 TDD tasks)

1. Add `confidence` and `evidence` fields to `CallEdge`
2. Annotate all edge producers with confidence assignment (Section 5.2)
3. Replace orchestrator dedup with confidence-based dedup
4. Verify output is a strict subset (edges only removed, never added)

**Acceptance**: All 56 et_bench tests pass; recall unchanged from Phase C; FPR unchanged from Phase C; high-confidence subset has near-zero FPR

---

## Section 8: Risk Analysis

| Risk | Likelihood | Mitigation |
|---|---|---|
| Recall regression from removing suffix scans | Medium | Phase C gated on et_bench; type-aware keys must close the gap first |
| Performance regression from strategy chain | Low | Exact key lookups are O(1); suffix scans were O(n) — net improvement |
| Missing edges from removed param_assign.analyze() | Medium | New modules already produce same edges; Phase C verifies |
| ScopedStore memory overhead | Low | Tuple keys are minor overhead vs string keys |
| Incomplete type info causing field_call misses | Medium | Exact path match (Layer 1) is the safety net; type-aware (Layer 0) is the optimization |

---

## Section 9: Success Criteria

1. **Recall**: all 11 et_bench categories maintain current recall (no regression)
   - fnptr-dynamic-call and fnptr-virtual excluded (structural gap, defect #5)
2. **FPR**: overall FPR reduced from 31.33% to < 20%
3. **Code quality**: `field_call._visit()` no longer contains suffix scans or `dataflow.targets` iteration
4. **Module hygiene**: `param_assign.analyze()` removed; `param_assign.py` reduced to registration phase + shared utilities
5. **Test coverage**: 1 new xfailed test (`test_type_aware_key_isolates_different_struct_types`) passes
6. **Confidence**: high-confidence edge subset has FPR < 5%
