# Tiered Field Resolution — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace field_call's 15-layer fallback stack with a 4-tier resolution chain (type-aware exact → exact path → same-file suffix → cross-file suffix), enhance type tracking with local variable + cast expression sources, remove `VariableState.targets` and `param_assign.analyze()` from the orchestrator.

**Architecture:** Five tasks building on Phase A/B/D. E1 adds local var and cast expression type collection. E2 adds file-scoped index to ScopedStore. E3 implements the 4-tier `resolve_field_call()` function and replaces field_call resolution. E4 removes old fallback stack, `VariableState.targets`, and `param_assign.analyze()`. E5 updates FPR ceilings and confidence assertions.

**Tech Stack:** Python 3.11, pytest, tree-sitter-c

---

## File Map

| File | Task | Role |
|---|---|---|
| `src/ethunter/analyzer/field_call.py` | E1, E3, E4 | Collect types; use tiered resolver; remove old fallbacks |
| `src/ethunter/analyzer/field_resolver.py` | E3 | New `resolve_field_call()` with 4-tier logic |
| `src/ethunter/analyzer/scoped_store.py` | E2 | Add `struct_field_files` index |
| `src/ethunter/analyzer/dataflow.py` | E4 | Migrate internal methods to store-only |
| `src/ethunter/analyzer/initializer_assign.py` | E2, E4 | Pass filepath; remove old dataflow writes |
| `src/ethunter/analyzer/param_binding.py` | E2, E4 | Pass filepath; remove old dataflow writes |
| `src/ethunter/analyzer/orchestrator.py` | E4 | Remove `param_assign.analyze()` call |
| `src/ethunter/analyzer/direct_assign.py` | E4 | Remove old dataflow writes |
| `src/ethunter/analyzer/cast_assign.py` | E4 | Remove old dataflow writes |
| `src/ethunter/analyzer/direct_call_fp.py` | E4 | Remove old resolve fallbacks |
| `src/ethunter/analyzer/array_call.py` | E4 | Remove old resolve fallbacks |
| `src/ethunter/analyzer/param_dispatch.py` | E4 | Remove old targets iteration |
| `tests/test_et_bench.py` | E5 | FPR ceilings, remove xfail, tiered FPR test |
| `tests/test_field_resolver.py` | E3 | Tiered resolution unit tests |

---

## Task E1: Enhanced Type Tracking

### Task E1a: Local Variable Type Collection

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py` — add `_collect_local_var_types()`
- Modify: `tests/test_et_bench.py` — remove xfail from `test_type_aware_key_isolates_different_struct_types`

- [ ] **Step 1: Write unit test for local var type collection**

```python
# tests/test_et_bench.py — add new test

def test_collect_local_var_types_records_struct_types():
    """Local struct variable declarations should record types for Tier 1 resolution."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    struct my_type { void (*cb)(void); };
    static void handler_a(void) {}
    void do_work(void) {
        struct my_type *ctx;
        ctx->cb = handler_a;
        ctx->cb();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import field_call
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())
    field_call.collect(tree, "test.c", engine, st, st.all_function_names)
    # Verify type was recorded
    t = st.get_func_var_type("do_work", "ctx")
    assert t == "my_type", f"Expected my_type, got {t}"
```

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_collect_local_var_types_records_struct_types -v`
Expected: FAIL — `get_func_var_type` returns None

- [ ] **Step 2: Implement `_collect_local_var_types()`**

Add to `src/ethunter/analyzer/field_call.py`:

```python
def _collect_local_var_types(tree, symbol_table):
    """Scan function bodies for local struct pointer declarations.
    
    struct my_type *ptr;  →  (func, "ptr") → "my_type"
    my_type *ptr;         →  (func, "ptr") → "my_type" (via typedef)
    """
    def _extract_func_name(node):
        decl = None
        for c in node.children:
            if c.type == 'function_declarator':
                decl = c; break
            if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                for cc in c.children:
                    if cc.type == 'function_declarator':
                        decl = cc; break
        if decl:
            for c in decl.children:
                if c.type == 'identifier' and c.text:
                    return c.text.decode('utf-8')
        return None

    def _scan(node, current_func):
        if node.type == 'function_definition':
            fname = _extract_func_name(node)
            if fname:
                current_func = fname
        if node.type == 'declaration' and current_func:
            # Look for: type_identifier/struct_specifier ... pointer_declarator -> identifier
            type_name = None
            var_name = None
            for c in node.children:
                if c.type == 'type_identifier' and c.text:
                    type_name = c.text.decode('utf-8')
                elif c.type == 'struct_specifier':
                    for sc in c.children:
                        if sc.type == 'type_identifier' and sc.text:
                            type_name = sc.text.decode('utf-8'); break
                elif c.type == 'pointer_declarator':
                    for pc in c.children:
                        if pc.type == 'identifier' and pc.text:
                            var_name = pc.text.decode('utf-8'); break
            if type_name and var_name:
                symbol_table.record_func_var_type(current_func, var_name, type_name)
        for child in node.children:
            _scan(child, current_func)

    _scan(tree.root_node, None)
```

- [ ] **Step 3: Call from `field_call.collect()`**

```python
# In collect(), after the existing field assignment loop:
    _collect_local_var_types(tree, symbol_table)
```

- [ ] **Step 4: Run test to verify**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_collect_local_var_types_records_struct_types -v`
Expected: PASS

- [ ] **Step 5: Remove xfail from type-aware isolation test**

```python
# tests/test_et_bench.py — remove the @pytest.mark.xfail decorator
# The test_type_aware_key_isolates_different_struct_types should now pass
# because local var types enable Tier 1 resolution
```

First check if it passes: `.venv/bin/python -m pytest tests/test_et_bench.py::test_type_aware_key_isolates_different_struct_types -v`

If PASS: remove `@pytest.mark.xfail`. If still FAIL: investigate — the test uses global vars `o1`/`o2` with type known from `initializer_assign`.record_var_type. Type-aware keys are written but field_call still uses old suffix scan which doesn't consume them. This test relies on Tier 1 being the active resolver, which happens in Task E3. **Keep xfail until E3.**

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: add local variable type collection for Tier 1 resolution"
```

### Task E1b: Cast Expression Type Collection

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py` — add `_collect_cast_types()`

- [ ] **Step 1: Write unit test**

```python
# tests/test_et_bench.py

def test_collect_cast_types_records_struct_types():
    """Cast expressions like ((struct ctx*)ptr)->handler should record ptr's type."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    struct ctx { void (*handler)(void); };
    static void h(void) {}
    void do_work(void *ptr) {
        ((struct ctx*)ptr)->handler = h;
        ((struct ctx*)ptr)->handler();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import field_call
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())
    field_call.collect(tree, "test.c", engine, st, st.all_function_names)
    t = st.get_func_var_type("do_work", "ptr")
    assert t == "ctx", f"Expected ctx, got {t}"
```

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_collect_cast_types_records_struct_types -v`
Expected: FAIL

- [ ] **Step 2: Implement `_collect_cast_types()`**

```python
# In field_call.py

def _collect_cast_types(tree, symbol_table):
    """Scan for cast expressions that reveal struct pointer types."""
    def _extract_current_func(node):
        """Find the enclosing function definition."""
        if node.type == 'function_definition':
            for c in node.children:
                if c.type == 'function_declarator':
                    for cc in c.children:
                        if cc.type == 'identifier' and cc.text:
                            return cc.text.decode('utf-8')
                if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                    for cc in c.children:
                        if cc.type == 'function_declarator':
                            for ccc in cc.children:
                                if ccc.type == 'identifier' and ccc.text:
                                    return ccc.text.decode('utf-8')
        return None

    def _extract_cast_struct_type(cast_node):
        """Extract struct type name from cast_expression."""
        for c in cast_node.children:
            if c.type == 'type_identifier':
                return c.text.decode('utf-8') if c.text else None
            if c.type == 'struct_specifier':
                for sc in c.children:
                    if sc.type == 'type_identifier' and sc.text:
                        return sc.text.decode('utf-8')
        return None

    def _scan(node, current_func):
        if node.type == 'function_definition':
            fname = _extract_current_func(node)
            if fname:
                current_func = fname
        if node.type == 'field_expression' and current_func:
            # Check if base is a parenthesized cast: (struct type *)var
            base = node.children[0] if node.children else None
            if base and base.type == 'parenthesized_expression':
                inner = base.children[1] if len(base.children) > 1 else None
                if inner and inner.type == 'cast_expression':
                    type_name = _extract_cast_struct_type(inner)
                    operand = None
                    for cc in inner.children:
                        if cc.type == 'pointer_expression':
                            for pcc in cc.children:
                                if pcc.type == 'identifier' and pcc.text:
                                    operand = pcc
                    if not operand:
                        for cc in reversed(inner.children):
                            if cc.type == 'identifier' and cc.text:
                                operand = cc; break
                    if type_name and operand:
                        var_name = operand.text.decode('utf-8')
                        symbol_table.record_func_var_type(current_func, var_name, type_name)
        for child in node.children:
            _scan(child, current_func)

    _scan(tree.root_node, None)
```

- [ ] **Step 3: Call from `field_call.collect()`**

```python
# After _collect_local_var_types:
    _collect_cast_types(tree, symbol_table)
```

- [ ] **Step 4: Run test to verify**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_collect_cast_types_records_struct_types -v`
Expected: PASS

- [ ] **Step 5: Run full suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: 184 passed (182 + 2 new tests)

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: add cast expression type collection for Tier 1 resolution"
```

---

## Task E2: File-Scoped Index

**Files:**
- Modify: `src/ethunter/analyzer/scoped_store.py` — add `struct_field_files`, update `assign_struct_field`
- Modify: `src/ethunter/analyzer/field_call.py` — pass `filepath` to `assign_struct_field` in `collect()`
- Modify: `src/ethunter/analyzer/initializer_assign.py` — pass `filepath` to `assign_struct_field` in `_assign_gstruct`
- Modify: `src/ethunter/analyzer/param_binding.py` — pass `filepath` to `assign_struct_field` in `_resolve_fields`

- [ ] **Step 1: Add `struct_field_files` to ScopedStore + update `assign_struct_field`**

```python
# scoped_store.py — add to ScopedStore dataclass

    # Per-file index: key -> set of source filepaths
    struct_field_files: dict[str, set[str]] = field(default_factory=dict)

    # Update assign_struct_field to accept optional filepath
    def assign_struct_field(self, key: str, target: str, filepath: str = '') -> None:
        if key not in self.struct_fields:
            self.struct_fields[key] = set()
        self.struct_fields[key].add(target)
        if filepath:
            if key not in self.struct_field_files:
                self.struct_field_files[key] = set()
            self.struct_field_files[key].add(filepath)
```

- [ ] **Step 2: Write unit test for file index**

```python
# tests/test_scoped_store.py — add to existing test file

class TestStructFieldFiles:
    def test_records_filepath(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "func_a", "fixture.c")
        assert "fixture.c" in store.struct_field_files["gstruct:handler.cb"]

    def test_multiple_files(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:h.cb", "func_a", "callee.c")
        store.assign_struct_field("gstruct:h.cb", "func_b", "callee.c")
        assert store.struct_field_files["gstruct:h.cb"] == {"callee.c"}

    def test_no_filepath(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:h.cb", "func_a")
        assert "gstruct:h.cb" not in store.struct_field_files
```

Run: `.venv/bin/python -m pytest tests/test_scoped_store.py::TestStructFieldFiles -v`
Expected: PASS

- [ ] **Step 3: Update `field_call.collect()` to pass filepath**

```python
# In collect(), update the assign_struct_field call:
    dataflow.store.assign_struct_field(
        f'gstruct:{base_var}.{field_tail}', fa.resolved_value, filepath)
    if struct_type:
        dataflow.store.assign_struct_field(
            f'gstruct:{struct_type}.{field_tail}', fa.resolved_value, filepath)
```

- [ ] **Step 4: Update `initializer_assign._assign_gstruct()` to pass filepath**

```python
# initializer_assign.py — _assign_gstruct already has access to filepath
# via closure over analyze() parameter.
# Update the hasattr(dataflow, 'store') branch:
    if hasattr(dataflow, 'store'):
        dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', target, filepath)
        if struct_type:
            dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}', target, filepath)
```

- [ ] **Step 5: Update `param_binding._resolve_fields()` to pass filepath**

```python
# param_binding.py — _resolve_fields receives filepath parameter.
# Update store writes:
    if hasattr(dataflow, 'store'):
        dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', t, filepath)
```

- [ ] **Step 6: Verify no regression**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: 184 passed (no new tests fail, file index is write-only — no reader yet)

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/scoped_store.py src/ethunter/analyzer/field_call.py src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/param_binding.py tests/test_scoped_store.py
git commit -m "feat: add struct_field_files index for file-scoped suffix (Tier 3)"
```

---

## Task E3: Tiered FieldResolver

**Files:**
- Modify: `src/ethunter/analyzer/field_resolver.py` — add `resolve_field_call()` with 4-tier logic
- Modify: `src/ethunter/analyzer/field_call.py` — replace `_visit()` resolution with tiered resolver
- Modify: `tests/test_field_resolver.py` — add tiered resolution tests

- [ ] **Step 1: Write unit tests for tiered resolution**

```python
# tests/test_field_resolver.py — add new test class

class FakeSymbolTableForTiers:
    def __init__(self, func_types=None, global_types=None):
        self._func_types = func_types or {}
        self._global_types = global_types or {}
    def get_func_var_type(self, func, var):
        return self._func_types.get((func, var))
    def get_var_type(self, var):
        return self._global_types.get(var)
    def resolve_typedef(self, name):
        return None

class FakeStoreForTiers:
    def __init__(self, struct_fields=None, file_index=None):
        self.struct_fields = struct_fields or {}
        self.struct_field_files = file_index or {}
    def compute_field_tail(self, field_path):
        return field_path.split('.', 1)[1] if '.' in field_path else field_path
    def resolve_struct_field(self, key):
        return self.struct_fields.get(key, set()).copy()

class TestResolveFieldCall:
    def test_tier1_type_aware_match(self):
        store = FakeStoreForTiers({
            "gstruct:my_type.cb": {"handler_a"}
        }, {"gstruct:my_type.cb": {"fixture.c"}})
        sym = FakeSymbolTableForTiers({("caller", "obj"): "my_type"})
        from ethunter.analyzer.field_resolver import resolve_field_call
        targets, conf, ev = resolve_field_call(
            "obj.cb", "obj", "caller", "fixture.c", store, sym)
        assert targets == {"handler_a"}
        assert conf == 'high'
        assert 'type-aware' in ev

    def test_tier1_fallback_to_tier2_when_no_type(self):
        store = FakeStoreForTiers({
            "gstruct:handler.cb": {"handler_a"}
        }, {"gstruct:handler.cb": {"fixture.c"}})
        sym = FakeSymbolTableForTiers()  # no type info
        from ethunter.analyzer.field_resolver import resolve_field_call
        targets, conf, ev = resolve_field_call(
            "handler.cb", "handler", "caller", "fixture.c", store, sym)
        assert targets == {"handler_a"}
        assert conf == 'high'  # Tier 2 hit
        assert 'exact path' in ev

    def test_tier3_same_file_suffix(self):
        store = FakeStoreForTiers({
            "gstruct:handler.cb": {"handler_a"},
            "gstruct:other_type.cb": {"handler_b"},
        }, {
            "gstruct:handler.cb": {"fixture.c"},
            "gstruct:other_type.cb": {"other.c"},  # different file
        })
        sym = FakeSymbolTableForTiers()  # no type, different var name
        from ethunter.analyzer.field_resolver import resolve_field_call
        targets, conf, ev = resolve_field_call(
            "obj.cb", "obj", "caller", "fixture.c", store, sym)
        assert targets == {"handler_a"}  # only same-file match
        assert "handler_b" not in targets  # other.c filtered out
        assert conf == 'medium'
        assert 'same-file' in ev

    def test_tier4_cross_file_suffix(self):
        store = FakeStoreForTiers({
            "gstruct:handler.cb": {"handler_a"},
        }, {
            "gstruct:handler.cb": {"other.c"},
        })
        sym = FakeSymbolTableForTiers()
        from ethunter.analyzer.field_resolver import resolve_field_call
        targets, conf, ev = resolve_field_call(
            "obj.cb", "obj", "caller", "fixture.c", store, sym)
        assert targets == {"handler_a"}
        assert conf == 'low'
        assert 'cross-file' in ev

    def test_returns_empty_when_no_match(self):
        store = FakeStoreForTiers()
        sym = FakeSymbolTableForTiers()
        from ethunter.analyzer.field_resolver import resolve_field_call
        targets, conf, ev = resolve_field_call(
            "x.y", "x", "func", "f.c", store, sym)
        assert targets == set()
        assert conf == 'none'
```

Run: `.venv/bin/python -m pytest tests/test_field_resolver.py::TestResolveFieldCall -v`
Expected: FAIL — `resolve_field_call` not defined

- [ ] **Step 2: Implement `resolve_field_call()`**

Add to `src/ethunter/analyzer/field_resolver.py`:

```python
def resolve_field_call(field_path: str, base_var: str,
                       caller_func: str | None, filepath: str,
                       store, symbol_table) -> tuple[set[str], str, str]:
    """Resolve a struct field function pointer call via 4-tier chain.
    
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
    suffix = f'.{field_tail}'
    for key, vals in store.struct_fields.items():
        if not key.endswith(suffix):
            continue
        if filepath not in store.struct_field_files.get(key, set()):
            continue
        targets.update(vals)
    if targets:
        return targets, 'medium', f'same-file suffix: {suffix}'
    
    # === Tier 4: Cross-file suffix (last resort) ===
    for key, vals in store.struct_fields.items():
        if key.endswith(suffix):
            targets.update(vals)
    if targets:
        return targets, 'low', f'cross-file suffix: {suffix}'
    
    return set(), 'none', ''
```

- [ ] **Step 3: Run unit tests**

Run: `.venv/bin/python -m pytest tests/test_field_resolver.py::TestResolveFieldCall -v`
Expected: 5 passed

- [ ] **Step 4: Replace `field_call._visit()` resolution with tiered resolver**

In `field_call.analyze()._visit()`, replace the entire resolution block (from `targets = set()` through the 15 fallback layers) with:

```python
                if field_path:
                    caller = find_enclosing_function(node, tree.root_node)
                    # Tiered resolution
                    if resolver is not None:
                        targets, confidence, evidence = resolver.resolve_field_call(
                            field_path, base_var, caller, filepath)
                    else:
                        # Fallback for bare VariableState (test compat)
                        targets = dataflow.resolve(f'<gstruct:{field_path}>')
                        if not targets:
                            targets = dataflow.resolve(f'<struct:{field_path}>')
                        confidence, evidence = 'medium', 'legacy resolution'

                    # Callback-of-callback
                    func_fp_params = getattr(dataflow.state, 'func_fp_params', None) if hasattr(dataflow, 'state') else None
                    if func_fp_params and targets:
                        _resolve_callback_of_callback(
                            targets, node, func_fp_params, symbol_names,
                            edges, filepath)
                    
                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='field_call',
                            caller_line=node.start_point[0] + 1,
                            confidence=confidence,
                            evidence=evidence,
                        ))
```

Note: Keep the old fallback stack commented out during this step — used for side-by-side comparison.

- [ ] **Step 5: Side-by-side verification with old fallback stack**

Add debug comparison in `_visit()` BEFORE removing the old code:

```python
# DEBUG: compare tiered resolver vs old suffix scan
if '.' in field_path:
    # Old suffix scan reading from store (NOT dataflow.targets)
    old_targets = set()
    parts = field_path.split('.')
    for i in range(1, len(parts)):
        old_suffix = '.'.join(parts[i:])
        for key, vals in dataflow.store.struct_fields.items():
            if key.endswith(f'.{old_suffix}'):
                old_targets.update(vals)
    missed = old_targets - targets
    if missed:
        import sys
        print(f"TIERED MISS: {field_path} in {filepath}:{caller} missed {missed}", file=sys.stderr)
```

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: No "TIERED MISS" output; 56 passed. If misses appear, analyze before proceeding.

- [ ] **Step 6: Run full suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests pass; recall ≥ 98.86%

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/field_resolver.py src/ethunter/analyzer/field_call.py tests/test_field_resolver.py
git commit -m "feat: implement 4-tier resolve_field_call replacing 15-layer fallback (Task E3)"
```

---

## Task E4: Remove Old Code

### Task E4a: Remove Old Fallback Stack from field_call

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py` — delete old resolution layers and duplicate Pass 1

- [ ] **Step 1: Delete old fallback stack from `_visit()`**

Remove all code between the tiered resolver call and the callback-of-callback block:
- Delete the 15 old resolution layers (~130 lines)
- Delete the debug `_resolve_with_old_suffix()` function (if present)
- Delete the duplicate Pass 1 inside `analyze()` (lines collecting field assignments — already handled by `collect()`)

The `_visit()` function now has a clean structure:
```python
    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            field_expr = _extract_field_expression(func_node)
            if field_expr:
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(field_expr)
                if field_path:
                    base_var = field_path.split('.')[0]
                    # Tiered resolution (only path)
                    targets, confidence, evidence = resolver.resolve_field_call(
                        field_path, base_var, caller, filepath)
                    # ... callback-of-callback + edge emission ...
            # ... macro fallback ...
        for child in node.children:
            _visit(child)
```

- [ ] **Step 2: Remove `analyze()` internal duplicate Pass 1**

Remove the duplicate field assignment collection inside `analyze()` (the loop calling `collect_field_assignments` and writing to dataflow). `collect()` already handles this.

- [ ] **Step 3: Run et_bench**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q`
Expected: 56 passed; recall unchanged

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "refactor: delete 15-layer fallback stack, use tiered resolver exclusively"
```

### Task E4b: Remove VariableState.targets and param_assign.analyze()

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py` — remove `VariableState.targets/assign/resolve`, `DataflowEngine` backward compat
- Modify: `src/ethunter/analyzer/orchestrator.py` — remove `param_assign.analyze()` call
- Modify: `src/ethunter/analyzer/direct_assign.py` — remove old dataflow writes
- Modify: `src/ethunter/analyzer/cast_assign.py` — remove old dataflow writes
- Modify: `src/ethunter/analyzer/initializer_assign.py` — remove old dataflow writes
- Modify: `src/ethunter/analyzer/param_binding.py` — remove old dataflow writes
- Modify: `src/ethunter/analyzer/direct_call_fp.py` — remove old resolve fallbacks
- Modify: `src/ethunter/analyzer/array_call.py` — remove old resolve fallbacks
- Modify: `src/ethunter/analyzer/param_dispatch.py` — remove old targets iteration

- [ ] **Step 1: Migrate `resolve_call_site_param` to store-only**

```python
# dataflow.py — in DataflowEngine.resolve_call_site_param
# Replace self.state.resolve(arg_name) with store lookup:
        arg_targets = self.store.resolve_func_var('<callee>', arg_name)
        if not arg_targets:
            arg_targets = self.store.resolve_func_var('<global>', arg_name)
        # Also check if arg_name is a known function
        if symbol_names and arg_name in symbol_names:
            arg_targets = set(arg_targets)
            arg_targets.add(arg_name)
        # ... write via store ...
```

Note: `resolve_call_site_param` is called from `_propagate_call_site` which has `call_name` (the callee). This needs to be threaded through. Add a `callee_name` parameter with default `'<global>'`.

Update the signature and call site:
```python
def resolve_call_site_param(self, func_name, param_idx, arg_name,
                             symbol_names=None, callee_name='<global>'):
    ...
    arg_targets = self.store.resolve_func_var(callee_name, arg_name)
    if not arg_targets:
        arg_targets = self.store.resolve_func_var('<global>', arg_name)
```

- [ ] **Step 2: Migrate `resolve_returned_field` to store-only**

```python
# dataflow.py — in DataflowEngine.resolve_returned_field
# Replace self.state.resolve / self.state.targets with store:
        for field_path in self.ret_fields[func_name]:
            targets = self.store.resolve_struct_field(f'gstruct:{field_path}')
            results.update(targets)
            # Suffix fallback via store
            parts = field_path.split('.')
            for i in range(1, len(parts)):
                suffix = '.'.join(parts[i:])
                for key, vals in self.store.struct_fields.items():
                    if key.endswith(f'.{suffix}'):
                        results.update(vals)
```

- [ ] **Step 3: Remove `VariableState.targets`, `assign()`, `resolve()`**

```python
# dataflow.py — VariableState class
# Remove: targets dict, assign(), resolve() methods
# Keep: var_types dict
```

Remove `DataflowEngine` backward compat:
```python
# Remove: assign(), resolve(), targets property
```

- [ ] **Step 4: Remove old dataflow writes from all producers**

In each producer, remove the `dataflow.assign(...)` lines, keeping only `dataflow.store.assign_*()`:

- `direct_assign.py:_assign()`: remove `dataflow.assign(f'<var>:...')` and `dataflow.assign(var_name, target)`
- `cast_assign.py:_assign()`: same
- `initializer_assign.py:_assign_gstruct()`: remove `dataflow.assign(f'<gstruct:...>')` and `dataflow.assign(f'<gstruct>:...')`; remove `dataflow.assign(f'<garray:...>')`
- `param_binding.py`: remove all `dataflow.assign(pname, target)` and `dataflow.assign(f'{call_name}:{pname}', target)` and struct field assigns
- `field_call.py:collect()`: already cleaned up in E4a Step 2

- [ ] **Step 5: Remove old dataflow reads from all readers**

- `direct_call_fp.py:_get_targets()`: remove `dataflow.resolve(f'<var>:...')` and `dataflow.resolve(var_name)` fallbacks
- `array_call.py`: remove `dataflow.resolve(f'<garray:...>')`, `dataflow.resolve(arr_name)`, `dataflow.resolve('<initializer>')`
- `param_dispatch.py`: remove `dataflow.targets.items()` iteration for param_mappings reconstruction

- [ ] **Step 6: Remove `param_assign.analyze()` from orchestrator**

```python
# orchestrator.py — delete Phase 1c block (lines ~104-112)
# The deprecated param_assign.analyze() call.
```

- [ ] **Step 7: Run full suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests pass; recall ≥ 98.86%

If any `callback_param` edges are missing (from removed param_assign.analyze()):
- Check `test_et_bench_report` for categories with recall drop
- Fix `param_dispatch` Pass A to cover missing patterns
- Re-run until recall restored

- [ ] **Step 8: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py src/ethunter/analyzer/orchestrator.py src/ethunter/analyzer/direct_assign.py src/ethunter/analyzer/cast_assign.py src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/direct_call_fp.py src/ethunter/analyzer/array_call.py src/ethunter/analyzer/param_dispatch.py
git commit -m "refactor: remove VariableState.targets and param_assign.analyze() from orchestrator"
```

---

## Task E5: Confidence Update + FPR Ceilings

**Files:**
- Modify: `tests/test_et_bench.py` — update FPR ceilings, update high-confidence FPR assertion, remove xfail

- [ ] **Step 1: Remove xfail from type-aware isolation test**

The test `test_type_aware_key_isolates_different_struct_types` should now pass with Tier 1 type-aware resolution.

```python
# Remove @pytest.mark.xfail decorator
```

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_type_aware_key_isolates_different_struct_types -v`
Expected: PASS

- [ ] **Step 2: Update FPR ceilings**

Run `test_et_bench_report` to get actual FPR values, then update `fpr_ceilings`:

```python
    fpr_ceilings = {
        'fnptr-callback': 0.55,
        'fnptr-cast': 0.55,
        'fnptr-global-array': 0.03,
        'fnptr-global-struct': 0.25,
        'fnptr-global-struct-array': 0.30,
        'fnptr-library': 0.12,
        'fnptr-only': 0.06,
        'fnptr-struct': 0.25,
        'fnptr-varargs': 0.53,
    }
```

- [ ] **Step 3: Update high-confidence FPR assertion**

```python
def test_et_bench_high_confidence_fpr():
    """High-confidence edge subset should have FPR < 5%."""
    # ... same logic ...
    assert fpr < 0.05, f"High-confidence FPR={fpr:.2%} exceeds 5% ceiling"
```

- [ ] **Step 4: Add tiered FPR breakdown test**

```python
def test_et_bench_tiered_fpr():
    """Tier 1-2 (high confidence) should have FPR < 5%."""
    categories = _get_categories()
    total_extra = 0
    total_detected = 0
    for category in categories:
        for example in _get_examples(category):
            example_dir = os.path.join(ET_BENCH_DIR, category, example)
            example_edges = _load_example_ground_truth(example_dir)
            if not example_edges:
                continue
            graph = _run_analysis_on_fixture(example_dir)
            high_edges = [e for e in graph.edges
                         if e.type.value == 'indirect'
                         and getattr(e, 'confidence', 'medium') == 'high']
            found_pairs = {(e.caller, e.callee) for e in high_edges}
            expected_pairs = {(e['caller'], e['callee']) for e in example_edges}
            extra = found_pairs - expected_pairs
            total_extra += len(extra)
            total_detected += len(found_pairs)
    fpr = total_extra / total_detected if total_detected > 0 else 0.0
    print(f'\nHigh-confidence FPR: {fpr:.2%} ({total_extra}/{total_detected})')
    assert fpr < 0.05, f"High-confidence FPR={fpr:.2%} exceeds 5%"
```

- [ ] **Step 5: Run full suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests pass; overall FPR < 20%; high-confidence FPR < 5%

- [ ] **Step 6: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: update FPR ceilings, high-confidence assertion to <5%, remove xfail"
```

---

## Completion Checklist

- [ ] `test_type_aware_key_isolates_different_struct_types` — PASS (no xfail)
- [ ] `test_et_bench_report` — recall ≥ 98.86%; FPR < 20%
- [ ] `test_et_bench_high_confidence_fpr` — high-confidence FPR < 5%
- [ ] `field_call.py` no longer contains old 15-layer fallback stack or `dataflow.targets` scans
- [ ] `field_call.py` no longer has duplicate Pass 1 inside `analyze()`
- [ ] `VariableState.targets` no longer exists (removed from dataflow.py)
- [ ] `param_assign.analyze()` not called from orchestrator
- [ ] All producers write only to `ScopedStore` (no `dataflow.assign()` calls)
- [ ] All readers read only from `ScopedStore` (no `dataflow.resolve()` calls)
- [ ] `struct_field_files` index populated by all struct field writers
