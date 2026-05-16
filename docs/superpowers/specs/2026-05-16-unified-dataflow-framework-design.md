# Unified Cross-Procedural Dataflow Framework Design

Date: 2026-05-16
Status: Design (not yet planned for implementation)

## Motivation

The current system uses 8 specialized bridges for cross-procedural dataflow:

| Bridge | Data Structure | Producer | Consumer |
|---|---|---|---|
| func_params + func_fp_params | dict[str, list] + dict[str, set] | param_helpers | param_binding |
| param_fields + ret_fields | dict[(str,int), set] + dict[str,set] | param_helpers, param_binding | param_binding |
| _param_bindings | dict[(str,str), set] | param_binding | param_dispatch, param_binding |
| registration_sites | list[dict] | param_binding | callback_reg |
| call_site_targets | dict[(str,str,int), set] | param_binding | param_dispatch |
| param_alias_map | dict[(str,str), str] | initializer_assign | FieldResolver |
| covered_callees | set[str] | orchestrator | callback_reg |
| func_vars + struct_fields + global_arrays | 3 dicts | multiple | resolve_* |

Each bridge is non-composable. `example_5` (fnptr-global-struct) fails because it requires chaining `load_from_global_array → assign_to_local → return → call_site_argument`, and no bridge covers the first two steps.

## Core Design

Replace all 8 bridges with three primitives:

1. **Unified Value Store** — single key-value store for all variable/field/array targets
2. **Composable eval()** — recursive expression evaluator
3. **Cross-procedure Call Bridge** — single structure for all inter-procedural propagation

### Part 1: Unified Value Store

```python
@dataclass
class ValueStore:
    _data: dict[Key, set[str]] = field(default_factory=dict)

    def assign(self, key: Key, targets: set[str]) -> None:
        """Add targets to key. UNION semantics — multiple calls to the same
        key accumulate targets (matching current ret_fields + func_vars behavior)."""
        ...

    def resolve(self, key: Key) -> set[str]:
        """Return a COPY of the target set for key, or empty set."""
        ...

    def set(self, key: Key, targets: set[str]) -> None:
        """REPLACE targets for key. Used when a definitive assignment overwrites
        previous values (e.g., struct field reassignment)."""
        ...

    def merge(self, source: Key, dest: Key) -> None:
        """Copy all targets from source key to dest key (union at dest)."""
        ...
```

Key types:

```python
Key = Local(func: str, var: str)           # function-scoped variable
     | Global(var: str)                     # global variable
     | StructField(path: str)               # struct field (path format: "prefix.field_tail")
     | GlobalArray(name: str)               # global array element
     | Return(func: str)                    # function return value
     | VTable(struct_type: str, field: str) # vtable entry (reserved, mirrors current vtable_entries)
     | Alias(var: str)                      # struct variable -> struct type alias
```

`StructField` path uses the canonical format established in 3.5: `"{prefix}.{field_tail}"`
where `field_tail = compute_field_tail(full_field_path)`. The store does not enforce this —
it is a convention documented here.

`Alias` stores var_name → struct_type mappings (currently in `ScopedStore.aliases`).
These are consumed by FieldResolver, not by eval() directly.

Single API for all writes (replaces `assign_func_var`, `assign_struct_field`, `assign_global_array`, `add_param_binding`, `register_return`, `register_param_mapping`):

```python
store.assign(Local("my_func", "fp"), {"target_func"})
store.assign(StructField("ctx.handler"), {"handler_func"})
store.assign(GlobalArray("ops_table"), {"op_init", "op_read"})
store.assign(Return("get_handler"), {"handler_func"})
```

Single API for all reads (replaces `resolve_variable`, `resolve_struct_field`, `resolve_global_array`, `resolve_returned_field`, `resolve_call_site_param`):

```python
store.resolve(Local("my_func", "fp"))
store.resolve(StructField("ctx.handler"))
store.resolve(GlobalArray("ops_table"))
store.resolve(Return("get_handler"))
```

### Part 2: Composable eval()

```python
def eval(node: ASTNode, func_context: str, store: ValueStore,
         bridge: CallBridge, symbol_names: set[str]) -> set[str]:
    """Evaluate an expression node to a set of function pointer targets."""

    if node.type == 'identifier':
        name = node.text
        if name in symbol_names:
            return {name}
        return store.resolve(Local(func_context, name)) \
               | store.resolve(Global(name))

    if node.type == 'field_expression':
        base, field = extract_field_components(node)
        base_targets = eval(base, func_context, store, bridge, symbol_names)
        results = set()
        # Fallback: if base doesn't resolve to any target, use the
        # base variable name directly as a struct field key prefix
        resolved_bases = base_targets or {extract_base_var_name(base)}
        for bt in resolved_bases:
            results |= store.resolve(StructField(f"{bt}.{field}"))
        return results

    if node.type == 'call_expression':
        callee = extract_callee_name(node)
        args = extract_arguments(node)
        for i, arg in enumerate(args):
            arg_targets = eval(arg, func_context, store, bridge, symbol_names)
            bridge.propagate(func_context, callee, i, arg_targets)
        # Also resolve return value if callee returns a struct/pointer
        return store.resolve(Return(callee))

    if node.type == 'return_statement':
        value = extract_return_value(node)
        result = eval(value, func_context, store, bridge, symbol_names)
        store.assign(Return(func_context), result)
        return result

    if node.type == 'assignment_expression':
        lhs = extract_lhs_identifier(node)  # variable name or field_expression
        rhs = extract_rhs(node)
        targets = eval(rhs, func_context, store, bridge, symbol_names)
        if lhs.type == 'identifier':
            store.assign(Local(func_context, lhs.text), targets)
        elif lhs.type == 'field_expression':
            base, field = extract_field_components(lhs)
            for bt in eval(base, func_context, store, bridge, symbol_names) or {base.text}:
                store.assign(StructField(f"{bt}.{field}"), targets)
        return targets

    # ... pointer_expression, cast_expression, subscript_expression, etc.
```

The key property: **operations compose**. A `return obj->handler;` automatically chains the struct-field load into the function's return value. A `dispatch(func_array[i])` automatically loads from the array and propagates to the parameter.

**Relationship to FieldResolver**: eval() and FieldResolver serve different roles.
eval() evaluates expressions within a function body and writes results into ValueStore
and CallBridge. FieldResolver reads from ValueStore to resolve struct-field dispatch
targets (with its 4-tier resolution chain and confidence/evidence annotation).
They are complementary: eval() populates the store; FieldResolver queries it.

**Limitation — alias chains**: eval() operates on a single tree traversal. When
an assignment references a variable not yet resolved (e.g., `fp2 = fp1` where `fp1`
is assigned later in the source), eval() returns an empty set for `fp1`. The current
system handles this via a second pass in `direct_assign.py`. The unified framework
can adopt the same two-pass strategy: first pass evaluates all assignments in order,
second pass re-evaluates unresolved references. This is a known constraint, not a
regression from current behavior.

**Constraint — complete expression type coverage**: eval() must handle ALL expression
types that can appear in function pointer contexts. The pseudocode shows 4 types;
real C code requires at minimum: `identifier`, `field_expression`, `call_expression`,
`return_statement`, `pointer_expression` (&var, *ptr), `cast_expression`, `subscript_expression`
(arr[i]), `parenthesized_expression`, `conditional_expression` (ternary), `assignment_expression`,
`comma_expression`. Unhandled types must return a sentinel value (not empty set) to
avoid silently breaking composability chains.

**Constraint — value ambiguity**: ValueStore values are `Set[str]` where each string
may be a function name OR a struct/variable name (used for chain resolution, e.g.,
Tier 3 in FieldResolver). This ambiguity is INHERITED from the current system
(`ScopedStore.struct_fields` values already mix function names and struct references).
The unified store does not introduce this problem, but it also does not solve it.

### Part 3: Cross-Procedure Call Bridge

```python
@dataclass
class CallBridge:
    _edges: dict[(str, str, int), set[str]] = field(default_factory=dict)

    def propagate(self, caller: str, callee: str, arg_idx: int,
                  targets: set[str]) -> None:
        """Record actual→formal parameter binding."""
        key = (caller, callee, arg_idx)
        if key not in self._edges:
            self._edges[key] = set()
        self._edges[key].update(targets)

    def resolve_args(self, caller: str, callee: str) -> dict[int, set[str]]:
        """Get all argument targets for a specific call site."""
        return {idx: targets for (c, cal, idx), targets in self._edges.items()
                if c == caller and cal == callee}

    def resolve_all_callers(self, callee: str) -> dict[tuple[str, int], set[str]]:
        """Get all call sites targeting callee. Returns {(caller, arg_idx): targets}.
        Used by param_dispatch to find all fnptr argument targets for a function."""
        return {(c, idx): targets for (c, cal, idx), targets in self._edges.items()
                if cal == callee}
```

Replaces: `_param_bindings`, `call_site_targets`, `param_fields`, `ret_fields`, `param_alias_map`.

**Registration classification remains separate**: `param_usage` classification (caller/
forwarder/storage/unknown) and `_is_registration()` name heuristics are classification
concerns, not transport concerns. They stay in callback_reg or a dedicated classifier.
CallBridge handles the transport (recording actual→formal bindings); the classifier
decides whether a binding represents a callback registration.

### How This Fixes example_5

Current architecture trace (fails at step 2 — no bridge for local-variable assignment from global array):

```
1. connTypes[type] → ct_tcp          [load from global array to local]
2. ct_tcp = connTypes[type]          [assign local variable]        ← NO BRIDGE
3. return ct_tcp                     [return local]                 ← NO BRIDGE
4. connTypeRead = connectionTypeTcp() [call-site binding]           ← param_binding
5. connTypeRead->read(conn, ...)     [field dispatch]               ← field_call
```

Unified framework trace (all steps covered by generic primitives):

```
1. eval(GlobalArray("connTypes") + subscript) → {connTLSRead}
2. store.assign(Local("connectionTypeTcp", "ct_tcp"), {connTLSRead})
3. store.assign(Return("connectionTypeTcp"), store.resolve(Local("connectionTypeTcp", "ct_tcp")))
4. bridge.propagate("connUnixRead", "connectionTypeTcp", 0,
     store.resolve(Return("connectionTypeTcp")))
5. field_call resolves connTypeRead->read → connTLSRead ✓
```

## Migration Strategy

### Phase 1: Add ValueStore alongside existing stores (non-breaking)

- Implement `ValueStore` as a new class in `dataflow.py`
- Add `Key` types: `Local`, `Global`, `StructField`, `GlobalArray`, `Return`, `VTable`, `Alias`
- Existing `ScopedStore` continues to operate unchanged
- Write comprehensive unit tests for `ValueStore`

### Phase 2: Migrate producer+consumer pairs one module at a time

Each sub-phase targets one data category (struct fields, func vars, etc.):

1. Add writes to `ValueStore` in the producer module, alongside existing ScopedStore writes
2. Update the consumer module to read from `ValueStore` (with ScopedStore fallback)
3. Run integration tests — verify identical edge output
4. Remove old ScopedStore writes and old consumer code paths

**Migration order** (least-risk first):
- **2a**: `struct_fields` — producer: field_call.collect + initializer_assign; consumer: FieldResolver
- **2b**: `func_vars` — producer: direct_assign + cast_assign; consumer: resolve_variable
- **2c**: `global_arrays` — producer: initializer_assign; consumer: resolve_global_array + array_call
- **2d**: param bindings — producer: param_binding; consumer: param_dispatch + _resolve_fields

### Phase 3: Implement eval() and CallBridge

- Build `eval()` with handlers for all expression types
- Build `CallBridge` replacing `_param_bindings` + `call_site_targets`
- eval() writes to ValueStore and CallBridge; existing specialized bridges still operate
- Run against full benchmark suite, verify no regression

### Phase 4: Remove old bridges

- Delete `func_params`, `func_fp_params`, `param_fields`, `ret_fields`
- Delete `_param_bindings`, `call_site_targets`, `registration_sites`
- Delete `param_alias_map`
- Delete old resolve methods from DataflowEngine
- Remove ScopedStore (replaced entirely by ValueStore)

### Phase 5: Enable composability

- With unified store and eval in place, local variable tracking becomes free
- Example_5 should pass automatically
- Add regression tests for previously-missed patterns

## Non-Goals

- Control-flow sensitivity (SSA, basic blocks) — out of scope
- Pointer alias analysis — out of scope
- Inter-procedural call graph ordering — existing orchestrator ordering is preserved

## Design Decisions Not Yet Made

1. **eval() traversal direction**: Top-down (walk from root, maintain func_context as state)
   or bottom-up (evaluate individual expression nodes, look up enclosing function via
   `find_enclosing_function()`). Top-down is simpler for func_context bookkeeping;
   bottom-up is more flexible for targeted re-evaluation. This decision affects the
   signature of eval() — if top-down, eval() doesn't need a func_context parameter
   (it's maintained during traversal); if bottom-up, it does.

2. **Re-evaluation vs caching**: eval() re-evaluates expressions each time they are
   encountered. For large projects, repeated evaluation of identical sub-expressions
   could add overhead. A cache keyed by (node_id, func_context) could eliminate
   redundant computation but adds complexity. Decision deferred to implementation.

## Risks

- **Scale**: This is a 3-6 month project touching every analyzer module
- **Regression surface**: All 192 existing tests must continue passing at each phase
- **Performance**: eval() recursion may be slower than specialized bridges for simple
  cases. For typical C projects (<10k LOC), the overhead is negligible (AST depth
  is bounded by tree-sitter parsing, typically <50 levels per expression).

## Open Questions

1. Should `ValueStore` use immutable snapshots per function (functional style) or mutable updates (current style)?
2. Should `eval()` handle control flow (if/switch) or assume monotonic accumulation?
3. Should the migration be done in a long-lived feature branch or incrementally on main?
