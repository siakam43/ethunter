# False Positive Reduction v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce ET-Bench FPR from ~56% to ~20% via two fixes (A: prefixed resolve in fallback, B: callback_reg suppression) while maintaining 100% recall on 9 target scenarios.

**Architecture:** Fix A modifies one line in `param_assign.py` Pass 1 fallback branch. Fix B adds a post-processing filter in `orchestrator.py` dedup stage. Independent changes, no interaction. TDD throughout.

**Tech Stack:** Python 3.11, pytest, `.venv/bin/python`, `PYTHONPATH=src`

---

### Task 1: Fix A TDD — Prefixed Resolve in Pass 1 Fallback

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:462`
- Create: (test code appended to `tests/test_et_bench.py`)

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_fix_a_fallback_prefixed_resolve():
    """Pass 1 fallback branch resolves via prefixed key, not polluted bare key."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void h_a(int x) { (void)x; }
static void h_b(int x) { (void)x; }

struct ctx { handler_fn handler; };

/* Leaf: stores fnptr into struct field */
static void register_legacy(struct ctx *c, handler_fn fn) {
    c->handler = fn;
}

/* Wrapper 1: forwards its own fnptr param to register_legacy */
static void wrapper_a(struct ctx *c, handler_fn fn) {
    register_legacy(c, fn);
}

/* Wrapper 2: forwards its own fnptr param to register_legacy */
static void wrapper_b(struct ctx *c, handler_fn fn) {
    register_legacy(c, fn);
}

void caller_a(void) {
    struct ctx c;
    wrapper_a(&c, h_a);
}

void caller_b(void) {
    struct ctx c;
    wrapper_b(&c, h_b);
}

void dispatch(struct ctx *c) {
    if (c->handler)
        c->handler(42);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    callback_param = [e for e in graph.edges if e.indirect_kind == 'callback_param']
    pairs = {(e.caller, e.callee) for e in callback_param}

    # wrapper_a forwards fn param to register_legacy — should only resolve to h_a
    # wrapper_b forwards fn param to register_legacy — should only resolve to h_b
    # Before fix: both resolve to {h_a, h_b} via polluted bare key "fn"

    # callback_param edges should NOT include cross-pollution:
    # wrapper_a should NOT produce (wrapper_a, h_b)
    assert ('wrapper_a', 'h_b') not in pairs, \
        f"wrapper_a should NOT connect to h_b (bare key pollution): {pairs}"

    # wrapper_b should NOT produce (wrapper_b, h_a)
    assert ('wrapper_b', 'h_a') not in pairs, \
        f"wrapper_b should NOT connect to h_a (bare key pollution): {pairs}"

    # But dispatch via field_call should still work
    field_call_edges = [e for e in graph.edges if e.indirect_kind == 'field_call']
    fc_pairs = {(e.caller, e.callee) for e in field_call_edges}
    assert ('dispatch', 'h_a') in fc_pairs, "field_call dispatch -> h_a should work"
    assert ('dispatch', 'h_b') in fc_pairs, "field_call dispatch -> h_b should work"
```

- [ ] **Step 2: Run test to verify it fails (bare key pollution)**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_a_fallback_prefixed_resolve -v`

Expected: FAIL — `('wrapper_a', 'h_b')` IS in pairs (bare key "fn" polluted with both h_a and h_b).

- [ ] **Step 3: Implement Fix A**

In `src/ethunter/analyzer/param_assign.py`, change line 462. Read the current line first to ensure exact match.

OLD:
```python
                                df_targets = dataflow.resolve(target)
```

NEW:
```python
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
```

The `caller` variable is already in scope (defined at line 404: `caller = find_enclosing_function(node, tree.root_node)`).

- [ ] **Step 4: Run Fix A test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_a_fallback_prefixed_resolve -v`

Expected: PASS — wrapper_a → h_b and wrapper_b → h_a not in pairs.

- [ ] **Step 5: Run full ET-Bench suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates 100%. FPR significantly lower for fnptr-global-struct (expected ~489 fewer callback_param FPs).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "fix: use prefixed resolve in Pass 1 fallback branch (Fix A)

Pass 1 fallback dataflow.resolve(target) now tries f'{caller}:{target}'
first, falling back to bare key. Eliminates cross-function parameter
name pollution in forwarded-param scenarios. Reduces ~450 FPs.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Fix B TDD — callback_reg Field-Call Suppression

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py:115` (before dedup)
- Create: (test code appended to `tests/test_et_bench.py`)

- [ ] **Step 1: Write the TDD tests**

Append to `tests/test_et_bench.py`:

```python
def test_fix_b_callback_reg_suppress_when_field_covered():
    """callback_reg edges with callee also in field_call should be suppressed."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void my_handler(int x) { (void)x; }

struct ctx { handler_fn handler; };

static void register_handler(struct ctx *c, handler_fn fn) {
    c->handler = fn;
}

/* wrapper: forwards fnptr param to register_handler */
static void register_wrapper(struct ctx *c, handler_fn fn) {
    register_handler(c, fn);
}

void setup(void) {
    struct ctx c;
    register_wrapper(&c, my_handler);
}

void dispatch(struct ctx *c) {
    if (c->handler)
        c->handler(42);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    # field_call should produce (dispatch, my_handler)
    field_call_edges = [e for e in graph.edges if e.indirect_kind == 'field_call']
    assert ('dispatch', 'my_handler') in {(e.caller, e.callee) for e in field_call_edges}, \
        "field_call should produce dispatch -> my_handler"

    # callback_reg for my_handler should be suppressed (field_call covers it)
    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == 'callback_reg']
    cr_callees = {e.callee for e in callback_reg_edges}
    assert 'my_handler' not in cr_callees, \
        f"callback_reg for my_handler should be suppressed: {cr_callees}"


def test_fix_b_callback_reg_kept_when_no_field_cover():
    """callback_reg edges without field_call coverage should be retained."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void my_cb(int x) { (void)x; }

/* Registration function that directly calls the fnptr (no struct field) */
static void register_cb(cb_fn cb) {
    cb(42);
}

void setup(void) {
    register_cb(my_cb);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)

    # my_cb should still have callback_reg edge (no field_call covers it)
    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == 'callback_reg']
    cr_callees = {e.callee for e in callback_reg_edges}
    assert 'my_cb' in cr_callees, \
        f"callback_reg for my_cb should be retained (no field_call): {cr_callees}"
```

- [ ] **Step 2: Run tests to verify they fail for the right reason**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_b_callback_reg_suppress_when_field_covered -v`

Expected: FAIL — `my_handler` IS in callback_reg callees (not yet suppressed).

- [ ] **Step 3: Implement Fix B**

In `src/ethunter/analyzer/orchestrator.py`, insert the suppression logic before the existing dedup code (before line 115 `# Deduplicate`):

```python
    # Fix B: suppress callback_reg edges where the callee is already covered
    # by a field_call edge (which has a more precise caller name).
    field_callees = {e.callee for e in graph.edges
                     if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    if field_callees:
        filtered = []
        for edge in graph.edges:
            if edge.indirect_kind == 'callback_reg' and edge.callee in field_callees:
                continue
            filtered.append(edge)
        graph.edges = filtered

    # Deduplicate: same caller+callee = one edge, prefer direct over indirect
```

- [ ] **Step 4: Run Fix B tests to verify they pass**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_b_callback_reg_suppress_when_field_covered tests/test_et_bench.py::test_fix_b_callback_reg_kept_when_no_field_cover -v`

Expected: 2 PASS.

- [ ] **Step 5: Run full ET-Bench suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates 100%. FPR lowered for fnptr-global-struct (expected ~77 fewer callback_reg FPs).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "fix: suppress callback_reg edges when field_call covers same callee (Fix B)

Post-processing filter in orchestrator dedup: callback_reg edges
whose callee also appears in field_call are removed (field_call
provides more precise caller name via struct dispatch).
Reduces ~100 callback_reg false positives.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Final Verification & FPR Ceilings

- [ ] **Step 1: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`

Expected: All tests pass (benchmarks + et_bench + unit tests + cross-file).

- [ ] **Step 2: Print final FPR report**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr|^OVERALL|^----"`

Verify:
- All 9 target categories have 100% recall
- Overall FPR is below 30% (expected ~20%)
- fnptr-global-struct FPR should be significantly lower

- [ ] **Step 3: Lower FPR ceilings to match new values**

Read the current FPR ceilings in `test_et_bench_report` and update them to the actual post-fix values, adding a 0.03 margin. Run:

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr"`

Then update the `fpr_ceilings` dict in `test_et_bench_report` accordingly.

- [ ] **Step 4: Commit final ceilings**

```bash
git add tests/test_et_bench.py
git commit -m "test: finalize FPR ceilings after Fix A and Fix B

Expected overall FPR ~20% with 100% recall on all 9 target categories.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
