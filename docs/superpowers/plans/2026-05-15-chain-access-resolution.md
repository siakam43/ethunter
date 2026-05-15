# Chain Field Access Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Recover 4 missing fnptr-struct edges (100% recall) by enabling FieldResolver to resolve chain field access like `s.method.put_cipher_by_char` through intermediate field decomposition.

**Architecture:** Two-layer fix. Layer 1 makes `collect_field_assignments` capture `&expr` RHS values by extending `_unwrap_identifier` to handle `pointer_expression` nodes. Layer 2 adds chain decomposition to `FieldResolver.resolve_field_call()` — when a 3+ segment path fails Tier 1/2, it progressively resolves intermediate fields (`s.method` → `ssl3_method`) and retries with the resolved variable's type.

**Tech Stack:** Python 3.11, tree-sitter, pytest

---

### Task 1: Fix `_unwrap_identifier` to handle `pointer_expression` RHS

**Files:**
- Modify: `src/ethunter/analyzer/helpers.py:138-152`

- [ ] **Step 1: Read the current function**

Read `_unwrap_identifier` at `src/ethunter/analyzer/helpers.py:138`:
```python
def _unwrap_identifier(node: ts.Node, unwrap_fn=None) -> str | None:
    if node.type == 'identifier' and node.text:
        return node.text.decode('utf-8')
    if node.type == 'cast_expression':
        if unwrap_fn:
            result = unwrap_fn(node)
            if result:
                return result
        for c in reversed(node.children):
            result = _unwrap_identifier(c, unwrap_fn)
            if result:
                return result
    return None
```

- [ ] **Step 2: Add pointer_expression handling**

Add the `pointer_expression` branch before the final `return None`:

```python
def _unwrap_identifier(node: ts.Node, unwrap_fn=None) -> str | None:
    """Extract identifier text from a node, unwrapping cast & pointer expressions."""
    if node.type == 'identifier' and node.text:
        return node.text.decode('utf-8')
    if node.type == 'cast_expression':
        if unwrap_fn:
            result = unwrap_fn(node)
            if result:
                return result
        for c in reversed(node.children):
            result = _unwrap_identifier(c, unwrap_fn)
            if result:
                return result
    if node.type == 'pointer_expression' and node.children:
        # Handle &func_ref, &variable
        inner = node.children[-1]
        return _unwrap_identifier(inner, unwrap_fn)
    return None
```

- [ ] **Step 3: Run regression tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: 195 passed, 1 xfailed (or 196 passed if the fix recovers edges)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/helpers.py
git commit -m "fix: handle pointer_expression in _unwrap_identifier for &expr RHS capture"
```

---

### Task 2: Add chain decomposition to `FieldResolver.resolve_field_call()`

**Files:**
- Modify: `src/ethunter/analyzer/field_resolver.py:172-218`

- [ ] **Step 1: Read the current Tier 2 block and type gate**

Read `resolve_field_call` at `src/ethunter/analyzer/field_resolver.py:193-196`:
```python
        # === Tier 2: Exact path match ===
        targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

        # === Type gate: known type + Tier 1 miss → skip Tier 3/4 suffix ===
        if struct_type:
            return set(), None, None
```

- [ ] **Step 2: Insert chain decomposition between Tier 2 and type gate**

Replace the block from Tier 2 through the type gate with:

```python
        # === Tier 2: Exact path match ===
        targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

        # === Chain decomposition ===
        # Handle s.method.put_cb where s.method resolves to a concrete struct
        parts = field_path.split('.')
        if len(parts) >= 3:
            for cut in range(2, len(parts)):
                prefix = '.'.join(parts[:cut])
                suffix = '.'.join(parts[cut:])

                resolved_vars = self._store.resolve_struct_field(f'gstruct:{prefix}')
                if not resolved_vars:
                    continue

                for var_name in resolved_vars:
                    var_type = self._symbol_table.get_var_type(var_name)
                    if var_type:
                        targets = self._store.resolve_struct_field(
                            f'gstruct:{var_type}.{suffix}')
                        if targets:
                            return targets, Confidence.HIGH, Evidence('chain_resolve', tier=1)
                    targets = self._store.resolve_struct_field(
                        f'gstruct:{var_name}.{suffix}')
                    if targets:
                        return targets, Confidence.HIGH, Evidence('chain_resolve_exact', tier=2)

        # === Type gate: known type + Tier 1 miss + no chain success → skip suffix ===
        if struct_type:
            return set(), None, None
```

- [ ] **Step 3: Run regression tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: 195 passed, 1 xfailed (verify fnptr-struct recall is 100%)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/field_resolver.py
git commit -m "feat: add chain decomposition to FieldResolver for s.method.put_cb access"
```

---

### Task 3: Add chain resolution tests

**Files:**
- Modify: `tests/test_et_bench.py` (append new tests at end of file)

- [ ] **Step 1: Write test for pointer_expression unwrapping**

Append to `tests/test_et_bench.py`:

```python
def test_unwrap_pointer_expression():
    """_unwrap_identifier should extract identifier from &expr (pointer_expression)."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    from ethunter.analyzer.helpers import _unwrap_identifier as _unwrap_id

    source = b'void setup(void) { ctx->handler = &my_handler; }'
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    def _visit(n):
        if n.type == 'pointer_expression':
            result = _unwrap_id(n)
            assert result == 'my_handler', f"Expected my_handler, got {result}"
        for child in n.children:
            _visit(child)
    _visit(tree.root_node)
```

- [ ] **Step 2: Write test for chain decomposition**

```python
def test_chain_resolve_s_method_put_cb():
    """Chain access s->method->put_cipher_by_char must resolve through s.method → ssl3_method."""
    import tempfile, os
    from ethunter.parser.ast_builder import parse_file
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    code = b"""
    typedef struct SSL_METHOD { int (*put_cb)(void); } SSL_METHOD;
    typedef struct SSL { SSL_METHOD *method; } SSL;

    int ssl3_put_cb(void) { return 1; }
    static const SSL_METHOD ssl3_method = { .put_cb = ssl3_put_cb, };

    int ssl_cipher_list_to_bytes(SSL *s) {
        s->method = (SSL_METHOD *)&ssl3_method;
        return s->method->put_cb();
    }
    """
    with tempfile.NamedTemporaryFile(suffix='.c', delete=False) as f:
        f.write(code)
        tmp = f.name
    try:
        tree = parse_file(tmp)
        trees = {tmp: tree}
        st = SymbolTable()
        for func in extract_functions(tree, tmp):
            st.add_function(func)
        df = VariableState()
        graph = run_all_analyses(trees, st, df)

        indirects = {(e.caller, e.callee)
                    for e in graph.edges if e.type.value == 'indirect'}
        assert ('ssl_cipher_list_to_bytes', 'ssl3_put_cb') in indirects, \
            f"Chain access not resolved. Got: {indirects}"
    finally:
        os.unlink(tmp)
```

- [ ] **Step 3: Run new tests only**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_unwrap_pointer_expression tests/test_et_bench.py::test_chain_resolve_s_method_put_cb -v`
Expected: 2 PASS

- [ ] **Step 4: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add chain resolution and pointer_expression unwrap tests"
```

---

### Task 4: Verify fnptr-struct recall and FPR

- [ ] **Step 1: Run full test suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS

- [ ] **Step 2: Verify fnptr-struct recall is 100%**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_fnptr_struct_full_recall -v`
Expected: PASS (recall 100%)

- [ ] **Step 3: Check FPR report**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s`
Expected: FPR ≤ 31.33% (no increase), fnptr-struct FPR may decrease

- [ ] **Step 4: Check all 4 previously-missing examples**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_fnptr_struct_example_8 tests/test_et_bench.py::test_et_bench_fnptr_struct_example_10 tests/test_et_bench.py::test_et_bench_fnptr_struct_example_11 tests/test_et_bench.py::test_et_bench_fnptr_struct_example_12 -v` 2>&1
Note: test names may differ; verify the 4 examples that were missing edges

- [ ] **Step 5: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: chain access resolution complete — fnptr-struct recall verified"
```

---

## Summary

| Task | File | Changes | Est. Time |
|------|------|---------|-----------|
| 1 | `helpers.py:138` | `_unwrap_identifier` + pointer_expression branch | 10 min |
| 2 | `field_resolver.py:193` | Chain decomposition in `resolve_field_call` | 15 min |
| 3 | `tests/test_et_bench.py` | 2 new tests | 10 min |
| 4 | (verification) | Regression + FPR/recall checks | 10 min |
| **Total** | **3 files** | **~65 LoC** | **~45 min** |
