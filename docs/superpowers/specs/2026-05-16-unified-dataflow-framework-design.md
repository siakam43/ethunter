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

    def assign(self, key: Key, targets: set[str]) -> None: ...
    def resolve(self, key: Key) -> set[str]: ...
    def merge(self, source: Key, dest: Key) -> None: ...
```

Key types:

```python
Key = Local(func: str, var: str)           # function-scoped variable
     | Global(var: str)                     # global variable
     | StructField(path: str)               # struct field
     | GlobalArray(name: str)               # global array
     | Return(func: str)                    # function return value
```

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
        for bt in base_targets or [base_name(base)]:
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

    # ... pointer_expression, cast_expression, subscript_expression, etc.
```

The key property: **operations compose**. A `return obj->handler;` automatically chains the struct-field load into the function's return value. A `dispatch(func_array[i])` automatically loads from the array and propagates to the parameter.

### Part 3: Cross-Procedure Call Bridge

```python
@dataclass
class CallBridge:
    _edges: dict[(str, str, int), set[str]] = field(default_factory=dict)
    registration_sites: list[dict] = field(default_factory=list)

    def propagate(self, caller: str, callee: str, arg_idx: int,
                  targets: set[str]) -> None:
        """Record actual→formal parameter binding."""
        key = (caller, callee, arg_idx)
        self._edges.setdefault(key, set()).update(targets)

    def resolve_args(self, caller: str, callee: str) -> dict[int, set[str]]:
        """Get all argument targets for a specific call."""
        return {idx: targets for (c, cal, idx), targets in self._edges.items()
                if c == caller and cal == callee}
```

Replaces: `_param_bindings`, `call_site_targets`, `param_fields`, `ret_fields`, `param_alias_map`.

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
- Add `Key` types: `Local`, `Global`, `StructField`, `GlobalArray`, `Return`
- Existing `ScopedStore` continues to operate unchanged
- Write comprehensive unit tests for `ValueStore`

### Phase 2: Migrate producers one at a time

- For each producer module, add writes to `ValueStore` in parallel with existing writes
- Verify identical results via integration tests
- Remove old writes once verified

### Phase 3: Implement eval() and CallBridge

- Build `eval()` with handlers for all expression types
- Build `CallBridge` replacing `_param_bindings` + `call_site_targets`
- Run against full benchmark suite, verify no regression

### Phase 4: Remove old bridges

- Delete `func_params`, `func_fp_params`, `param_fields`, `ret_fields`
- Delete `_param_bindings`, `call_site_targets`, `registration_sites`
- Delete `param_alias_map`
- Delete old resolve methods from DataflowEngine

### Phase 5: Enable composability

- With unified store and eval in place, local variable tracking becomes free
- Example_5 should pass automatically
- Add regression tests for previously-missed patterns

## Non-Goals

- Control-flow sensitivity (SSA, basic blocks) — out of scope
- Pointer alias analysis — out of scope
- Inter-procedural call graph ordering — existing orchestrator ordering is preserved

## Risks

- **Scale**: This is a 3-6 month project touching every analyzer module
- **Regression surface**: All 192 existing tests must continue passing at each phase
- **Performance**: eval() recursion may be slower than specialized bridges for simple cases

## Open Questions

1. Should `ValueStore` use immutable snapshots per function (functional style) or mutable updates (current style)?
2. Should `eval()` handle control flow (if/switch) or assume monotonic accumulation?
3. Should the migration be done in a long-lived feature branch or incrementally on main?
