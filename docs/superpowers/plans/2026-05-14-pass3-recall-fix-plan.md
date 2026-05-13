# Pass 3 Recall Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 3 recall gaps, restore fnptr-callback to 100% recall via two fixes (Fix 1: fallback param_mappings check, Fix 2: field_call callback-of-callback caller correction).

**Architecture:** Fix 1 adds one line in `param_assign.py` Pass 1 fallback branch. Fix 2 changes one argument in `field_call.py` callback-of-callback edge emission. Independent changes, no interaction.

**Tech Stack:** Python 3.11, pytest, `.venv/bin/python`

---

### Task 1: Fix 1 — Fallback branch param_mappings check

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:462-464`

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_fix1_multi_level_forwarding():
    """Two-level fnptr forwarding: inner function should be the caller."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void h_a(int x) { (void)x; }
static void h_b(int x) { (void)x; }

/* Level 2: actually calls the fnptr */
static void inner_dispatch(cb_fn cb) {
    cb(42);
}

/* Level 1: forwards fnptr to level 2 */
static void mid_forward(cb_fn cb) {
    inner_dispatch(cb);
}

void outer_a(void) { mid_forward(h_a); }
void outer_b(void) { mid_forward(h_b); }
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
    cp_edges = [(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "callback_param"]
    pairs = set(cp_edges)

    # inner_dispatch should be the caller for both h_a and h_b
    assert ("inner_dispatch", "h_a") in pairs, \
        f"inner_dispatch -> h_a missing: {pairs}"
    assert ("inner_dispatch", "h_b") in pairs, \
        f"inner_dispatch -> h_b missing: {pairs}"

    # mid_forward should NOT be the caller (it just forwards)
    assert ("mid_forward", "h_a") not in pairs, \
        f"mid_forward should not be caller: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix1_multi_level_forwarding -v`

Expected: FAIL — `("inner_dispatch", "h_a")` NOT in pairs (inner caller not detected through two-level forwarding).

- [ ] **Step 3: Implement Fix 1**

In `src/ethunter/analyzer/param_assign.py`, after the existing `dataflow.resolve` fallback, add `param_mappings` merge:

```python
# OLD (lines 462-464):
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)

# NEW:
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
                                # Merge param_mappings for multi-level forwarding chains
                                pm_targets = param_mappings.get(target, set())
                                if pm_targets:
                                    df_targets = df_targets | pm_targets
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix1_multi_level_forwarding -v`

Expected: PASS.

- [ ] **Step 5: Run full ET-Bench to verify recall improvement**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: example_8's two missing edges now matched (recall 32/33 or better).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "fix: check param_mappings in Pass 1 fallback for multi-level forwarding

When fnptr is forwarded through two levels (outer->mid->inner),
the first level populates param_mappings but not dataflow. The
second level's fallback now merges param_mappings to find targets.
Fixes example_8: (_pqsort, sort_gp_asc/desc).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Fix 2 — field_call callback-of-callback caller correction

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:239`

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_fix2_field_call_callback_of_callback_caller():
    """field_call callback-of-callback caller should be ftarget, not enclosing func."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*note_fn)(void *, void *, void *);

static void relocate(void *a, void *b, void *c) { (void)a; (void)b; (void)c; }

struct ptr_data {
    void *obj;
    note_fn note_ptr_fn;
    void *cookie;
};

static void target_func(void *obj, void *x, note_fn op, void *cookie) {
    op(obj, x, cookie);
}

static struct ptr_data slot;

void dispatcher(void) {
    slot.note_ptr_fn = target_func;
    if (slot.note_ptr_fn)
        slot.note_ptr_fn(slot.obj, slot.cookie, relocate);
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

    # The callback_param edge from field_call's callback-of-callback should have
    # caller = target_func (the field target), not dispatcher (the enclosing func)
    cp_edges = [(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "callback_param"]
    pairs = set(cp_edges)

    # target_func is the resolved field target (ftarget) — should be caller
    assert ("target_func", "relocate") in pairs, \
        f"target_func -> relocate missing: {pairs}"

    # dispatcher should NOT be the caller for callback-of-callback edge
    # (it dispatches through the field, but target_func calls the fnptr)
    assert ("dispatcher", "relocate") not in pairs, \
        f"dispatcher should not be caller for relocate: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix2_field_call_callback_of_callback_caller -v`

Expected: FAIL — `("target_func", "relocate")` NOT in pairs (current code uses dispatcher as caller).

- [ ] **Step 3: Implement Fix 2**

In `src/ethunter/analyzer/field_call.py` line 239, change the callback-of-callback edge's caller from `caller` to `ftarget`:

```python
# OLD (line 239):
                                        edges.append(CallEdge(
                                            caller=caller or '<unknown>',
                                            callee=actual,

# NEW:
                                        edges.append(CallEdge(
                                            caller=ftarget,
                                            callee=actual,
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix2_field_call_callback_of_callback_caller -v`

Expected: PASS.

- [ ] **Step 5: Run full ET-Bench to verify recall improvement**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: example_14's missing edge now matched. fnptr-callback recall should be 33/33 (100%).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "fix: use field target as caller in callback-of-callback edges

When field_call detects fnptr arg passing through a resolved field
target, use ftarget (the field target function) as the edge caller
instead of the enclosing function. Fixes example_14:
(gt_pch_p_14lang_tree_node, relocate_ptrs).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Final Verification

- [ ] **Step 1: Restore fnptr-callback recall gate to 100%**

In `tests/test_et_bench.py`, change line ~1058:

```python
# OLD:
    assert recall >= 30/33, f"fnptr-callback recall={recall:.2%} ({matched}/{total})"

# NEW:
    assert recall == 1.0, f"fnptr-callback recall={recall:.2%} ({matched}/{total})"
```

- [ ] **Step 2: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`

Expected: All tests pass, fnptr-callback 100% recall.

- [ ] **Step 3: Update FPR ceilings and commit**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr"`

Update `fpr_ceilings` dict to match new values + 0.03 margin.

```bash
git add tests/test_et_bench.py
git commit -m "test: restore fnptr-callback recall gate to 100%, update FPR ceilings

All 3 recall gaps fixed. 9 target scenarios at 100% recall.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
