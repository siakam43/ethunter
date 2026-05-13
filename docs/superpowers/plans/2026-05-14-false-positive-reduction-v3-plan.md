# False Positive Reduction v3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce ET-Bench FPR from ~31% to ~18% via two fixes in `orchestrator.py` (A-1: extend callee-overlap to callback_param, D+E: suppress callback_reg when callee in struct field mapping). Maintain 100% recall on 9 target scenarios.

**Architecture:** Both fixes modify the same code block (Fix B area in `orchestrator.py`). One unified change replaces lines 115-125 with extended suppression logic. TDD throughout.

**Tech Stack:** Python 3.11, pytest, `.venv/bin/python`, `PYTHONPATH=src`

---

### Task 1: Fix A-1 + D+E — Extended Callee-Overlap Suppression

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py:115-125` (Fix B region)

- [ ] **Step 1: Write the TDD tests**

Append to `tests/test_et_bench.py`:

```python
def test_fix_a1_callback_param_suppress_when_field_covered():
    """callback_param edges with callee also in field_call should be suppressed."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void h_a(int x) { (void)x; }
static void h_b(int x) { (void)x; }

struct ctx { handler_fn handler; };

static void register_fn(struct ctx *c, handler_fn fn) {
    c->handler = fn;
}

static void wrapper_a(struct ctx *c, handler_fn fn) {
    register_fn(c, fn);
}

static void wrapper_b(struct ctx *c, handler_fn fn) {
    register_fn(c, fn);
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

    # field_call should still produce dispatch -> h_a, dispatch -> h_b
    fc_pairs = {(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "field_call"}
    assert ("dispatch", "h_a") in fc_pairs, "field_call dispatch -> h_a should work"
    assert ("dispatch", "h_b") in fc_pairs, "field_call dispatch -> h_b should work"

    # callback_param for h_a/h_b should be suppressed (field_call covers)
    cp_edges = [e for e in graph.edges if e.indirect_kind == "callback_param"]
    cp_callees = {e.callee for e in cp_edges}
    assert "h_a" not in cp_callees, \
        f"callback_param for h_a should be suppressed: {cp_callees}"
    assert "h_b" not in cp_callees, \
        f"callback_param for h_b should be suppressed: {cp_callees}"


def test_fix_de_callback_reg_suppress_when_struct_stored():
    """callback_reg for fnptr stored in struct field is suppressed."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef int (*secpolicy_fn)(void);

static int my_policy(void) { return 0; }

struct ops { secpolicy_fn secpolicy; };

/* Registration: stores fnptr into struct field */
static void register_ops(struct ops *o, secpolicy_fn pol) {
    o->secpolicy = pol;
}

void setup(void) {
    struct ops o;
    register_ops(&o, my_policy);
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

    # my_policy stored in o->secpolicy via register_ops
    # callback_reg should be suppressed (struct field mapping exists)
    cr_edges = [e for e in graph.edges if e.indirect_kind == "callback_reg"]
    cr_callees = {e.callee for e in cr_edges}
    assert "my_policy" not in cr_callees, \
        f"callback_reg for my_policy should be suppressed (struct stored): {cr_callees}"
```

- [ ] **Step 2: Run tests to verify they fail (FPs not yet suppressed)**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_a1_callback_param_suppress_when_field_covered tests/test_et_bench.py::test_fix_de_callback_reg_suppress_when_struct_stored -v`

Expected: 2 FAIL — `h_a` IS in callback_param callees, `my_policy` IS in callback_reg callees.

- [ ] **Step 3: Implement the fix**

Replace `src/ethunter/analyzer/orchestrator.py` lines 115-125:

OLD:
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
```

NEW:
```python
    # Fix B+A-1+D+E: suppress callback edges where callee is covered by
    # field_call or tracked via struct field mapping.
    field_callees = {e.callee for e in graph.edges
                     if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    struct_callees = set()
    for key, vals in engine.targets.items():
        if key.startswith('<gstruct:'):
            struct_callees.update(vals)

    if field_callees or struct_callees:
        filtered = []
        for edge in graph.edges:
            # A-1: callback_reg + callback_param where field_call covers callee
            if edge.indirect_kind in ('callback_reg', 'callback_param') \
                    and edge.callee in field_callees:
                continue
            # D+E: callback_reg where callee is tracked in struct field
            if edge.indirect_kind == 'callback_reg' \
                    and edge.callee in struct_callees:
                continue
            filtered.append(edge)
        graph.edges = filtered
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_a1_callback_param_suppress_when_field_covered tests/test_et_bench.py::test_fix_de_callback_reg_suppress_when_struct_stored -v`

Expected: 2 PASS.

- [ ] **Step 5: Run full ET-Bench suite to verify no recall regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates 100%. FPR lowered (fnptr-global-struct expected ~95 fewer FPs).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "fix: extend callee-overlap suppression to callback_param and struct-field callback_reg

A-1: callback_param edges suppressed when field_call covers same callee (~75 FP).
D+E: callback_reg edges suppressed when callee tracked in struct field mapping (~25 FP).
Total: ~100 FP reduction.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Final Verification & FPR Ceilings

- [ ] **Step 1: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`

Expected: All tests pass.

- [ ] **Step 2: Print final FPR report and update ceilings**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr|^OVERALL|^----"`

Expected: Overall FPR < 22%. Update `fpr_ceilings` dict in `test_et_bench_report` to actual values + 0.03 margin.

- [ ] **Step 3: Commit final ceilings**

```bash
git add tests/test_et_bench.py
git commit -m "test: finalize FPR ceilings after A-1 and D+E fixes

Expected overall FPR ~18% with 100% recall on all 9 target categories.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
