# 召回 Gap 修复实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 param_binding + param_dispatch 的 2 个 dataflow 传递 gap，恢复 7 条 GT 召回边。

**Architecture:** 3 TDD tasks: (1) Gap A — param_binding fallback 分支补 dataflow write，恢复 6 条 callback 边，(2) Gap B — 拆分 param_binding Pass 2 到 `_resolve_fields()` + orchestrator 重排序，恢复 1 条 struct 边，(3) 回归测试 + 召回验证。

**Tech Stack:** Python 3.11, tree-sitter-c, pytest (`.venv/bin/python`)

---

## File Structure

```
src/ethunter/analyzer/
├─ param_binding.py     MODIFY   229→235  Gap A: +4 lines; Gap B: split Pass 2 → _resolve_fields()
├─ orchestrator.py      MODIFY   180→195  Gap B: 3-phase Phase 1 + TARGET_RESOLVERS cleaned
tests/
└─ test_et_bench.py     MODIFY   +50     Gap A+B regression tests
```

---

### Task 1: Gap A — param_binding fallback 分支补 dataflow write

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py:105-117`

- [ ] **Step 1: Write TDD regression test — local variable fnptr arg**

Append to `tests/test_et_bench.py`:

```python
def test_local_var_fnptr_arg_creates_callback_param_edge():
    """Call-site passes local var as fnptr → param_dispatch should find the target."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef int (*cmp_t)(const void *, const void *);
    static int sort_asc(const void *a, const void *b) { return 1; }
    static void _pqsort(void *a, size_t n, size_t es,
        cmp_t cmp, void *l, void *r) { cmp(a, a); }
    static void pqsort(void *a, size_t n, size_t es,
        cmp_t cmp, size_t l, size_t r) { _pqsort(a, n, es, cmp, NULL, NULL); }
    void georadius(void) {
        cmp_t sort_gp_callback = sort_asc;
        pqsort(NULL, 10, 8, sort_gp_callback, 0, 9);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.param_dispatch import analyze as param_dispatch_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    edges = param_dispatch_analyze(tree, "test.c", engine)
    pairs = {(e.caller, e.callee) for e in edges}
    assert ("_pqsort", "sort_asc") in pairs, \
        f"Expected _pqsort -> sort_asc via local var, got: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_local_var_fnptr_arg_creates_callback_param_edge -v`
Expected: FAIL — `AssertionError: expected _pqsort -> sort_asc but pair is missing` (because `param_dispatch` can't find the target through dataflow)

- [ ] **Step 3: Add dataflow writes to fallback branch**

In `src/ethunter/analyzer/param_binding.py`, lines 109-117 (the fallback `else` block), add 2 dataflow writes:

```python
# CURRENT (lines 109-117):
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
                                if df_targets and arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].update(df_targets)
                                    cs_key = (caller or '<unknown>', call_name, arg_idx)
                                    if cs_key not in call_site_targets:
                                        call_site_targets[cs_key] = set()
                                    call_site_targets[cs_key].update(df_targets)

# NEW — add the 2 dataflow.assign lines after param_mappings[pname].update(df_targets):
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
                                if df_targets and arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].update(df_targets)
                                    for t in df_targets:
                                        dataflow.assign(f'{call_name}:{pname}', t)
                                        dataflow.assign(pname, t)
                                    cs_key = (caller or '<unknown>', call_name, arg_idx)
                                    if cs_key not in call_site_targets:
                                        call_site_targets[cs_key] = set()
                                    call_site_targets[cs_key].update(df_targets)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_local_var_fnptr_arg_creates_callback_param_edge -v`
Expected: PASS

- [ ] **Step 5: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: 157/157 passed, 1 xfailed (no regression)

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py tests/test_et_bench.py
git commit -m "fix: param_binding fallback branch writes dataflow for local var fnptr args (Gap A)

When call-site passes a local variable (not direct function name) as fnptr
arg, the fallback branch now writes {call_name}:{pname} and {pname} keys
to dataflow. This allows param_dispatch to find targets through its
dataflow key reconstruction. Fixes 6 missing callback recall edges.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Gap B — split param_binding Pass 2 + orchestrator reorder

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py:193-229`
- Modify: `src/ethunter/analyzer/orchestrator.py:40-95`

- [ ] **Step 1: Write TDD regression test — struct return field chain**

Append to `tests/test_et_bench.py`:

```python
def test_return_field_resolution_after_initializer_assign():
    """resolve_returned_field must find gstruct keys written by initializer_assign."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef int (*cb_t)(const void *, const void *, int, int, int, void *, void *);
    struct cert { cb_t sec_cb; };
    typedef struct { cb_t old_cb; } sdb_t;
    static int ssl_default(const void *s, const void *c, int o, int b, int n,
                           void *ex, void *e2) { return 1; }
    static cb_t get_sec_cb(const void *ctx) {
        if (!ctx) return NULL;
        return ((struct cert *)(ctx))->sec_cb;
    }
    static int debug_cb(const void *s, const void *c, int o, int b, int n,
                        void *ex, void *e2) {
        sdb_t *sdb = ex;
        return sdb->old_cb(s, c, o, b, n, ex, e2);
    }
    void setup(void) {
        struct cert *ret = (struct cert *)1;
        ret->sec_cb = ssl_default;
        static sdb_t sdb;
        sdb.old_cb = get_sec_cb(NULL);
        debug_cb(NULL, NULL, 0, 0, 0, &sdb, NULL);
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
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ("debug_cb", "ssl_default") in pairs, \
        f"Expected debug_cb -> ssl_default via return field chain, got: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_return_field_resolution_after_initializer_assign -v`
Expected: FAIL — edge missing (current pipeline: `_resolve_fields` runs before `initializer_assign`)

- [ ] **Step 3: Extract Pass 2 from param_binding.analyze() into _resolve_fields()**

In `src/ethunter/analyzer/param_binding.py`:

**3a**: Move Pass 2 code (lines 193-227) into a new `_resolve_fields()` function, with param_mappings reconstructed from dataflow:

```python
def _resolve_fields(tree: ts.Tree, filepath: str, symbol_table, dataflow) -> None:
    """Pass 2: resolve struct member assignments (field=param + return value tracking).
    Must run AFTER all other TARGET_RESOLVERS to have complete dataflow state.
    Reconstructs param_mappings from dataflow keys (consistent with param_dispatch)."""
    func_params = dataflow.func_params

    # Reconstruct param_mappings from dataflow
    param_mappings: dict[str, set[str]] = {}
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            p = key.split(':')[-1]
            if p not in param_mappings:
                param_mappings[p] = set()
            param_mappings[p].update(vals)

    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.enclosing_func is None:
            continue
        field_path = fa.field_path
        field_name = field_path.split('.')[-1]

        if fa.value_node and fa.value_node.type == 'call_expression':
            call_func = fa.value_node.child_by_field_name('function') or fa.value_node.children[0]
            if call_func and call_func.type == 'identifier' and call_func.text:
                func_name = call_func.text.decode('utf-8')
                ret_targets = dataflow.resolve_returned_field(func_name)
                for t in ret_targets:
                    dataflow.assign(f'<gstruct:{field_path}>', t)
        elif fa.resolved_value is not None:
            param_name = fa.resolved_value
            targets = param_mappings.get(param_name, set())
            for t in targets:
                dataflow.assign(f'<struct:{field_path}>', t)
            df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
            if not df_targets:
                df_targets = dataflow.resolve(param_name)
            if not df_targets:
                df_targets = dataflow.resolve(f'<garray:{param_name}>')
            for t in df_targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                dataflow.assign(f'<struct:{field_name}>', t)
            if fa.enclosing_func in func_params:
                params = func_params[fa.enclosing_func]
                if param_name in params:
                    param_idx = params.index(param_name)
                    dataflow.register_param_mapping(fa.enclosing_func, param_idx, field_path)
```

**3b**: Delete lines 193-227 from `analyze()` (the Pass 2 block). `analyze()` now ends at line 192 (`_collect_call_params(tree.root_node)`) + the `call_site_targets` store + `return []`.

The final `analyze()` should be:

```python
    _collect_call_params(tree.root_node)

    # Store call_site_targets on engine for param_dispatch (Phase 2)
    dataflow.call_site_targets.update(call_site_targets)

    return []  # Phase 1 returns NO edges
```

- [ ] **Step 4: Update orchestrator.py — 3-phase Phase 1 + clean TARGET_RESOLVERS**

In `src/ethunter/analyzer/orchestrator.py`:

**4a**: Change TARGET_RESOLVERS (line 40-46) — remove `param_binding` and `param_assign`:

```python
# OLD:
TARGET_RESOLVERS = [
    param_binding,
    param_assign,
    direct_assign,
    initializer_assign,
    cast_assign,
]

# NEW:
TARGET_RESOLVERS = [
    direct_assign,
    initializer_assign,
    cast_assign,
]
```

**4b**: Replace the Phase 1 loop + Phase 1b with the 3-part structure. Find the current Phase 1 code (~lines 83-106) and replace:

```python
# Phase 1: Target Resolution (writes to dataflow via engine)
for filepath, tree in trees.items():
    for resolver in TARGET_RESOLVERS:
        if resolver is param_binding:
            resolver.analyze(tree, filepath, symbol_table, engine)
        else:
            resolver.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=engine,
            )

# Phase 1b: param_assign callback detection
for filepath, tree in trees.items():
    edges = param_assign.analyze(
        tree=tree,
        filepath=filepath,
        symbol_table=symbol_table,
        dataflow=engine,
    )
    for edge in edges:
        graph.add_edge(edge)
```

Replace with:

```python
# Phase 1 Pass 1: param_binding call params (must run first, before direct_assign)
for filepath, tree in trees.items():
    param_binding.analyze(tree, filepath, symbol_table, engine)

# Phase 1 Pass 1b: TARGET_RESOLVERS (write dataflow, no edges)
for filepath, tree in trees.items():
    for resolver in TARGET_RESOLVERS:
        resolver.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=engine,
        )

# Phase 1 Pass 2: param_binding field resolution (after all resolvers)
for filepath, tree in trees.items():
    param_binding._resolve_fields(tree, filepath, symbol_table, engine)

# Phase 1b: param_assign callback detection [kept during hybrid state]
for filepath, tree in trees.items():
    edges = param_assign.analyze(
        tree=tree,
        filepath=filepath,
        symbol_table=symbol_table,
        dataflow=engine,
    )
    for edge in edges:
        graph.add_edge(edge)
```

- [ ] **Step 5: Run Gap B TDD test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_return_field_resolution_after_initializer_assign -v`
Expected: PASS

- [ ] **Step 6: Run all 3 new tests**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_local_var_fnptr_arg_creates_callback_param_edge tests/test_et_bench.py::test_return_field_resolution_after_initializer_assign -v`
Expected: Both PASS

- [ ] **Step 7: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests pass, no regression

- [ ] **Step 8: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "fix: split param_binding Pass 2 → _resolve_fields (Gap B)

Extract struct field resolution from analyze() into _resolve_fields(),
called after all TARGET_RESOLVERS to ensure initializer_assign gstruct
keys exist before resolve_returned_field suffix fallback runs.
TARGET_RESOLVERS cleaned: param_binding called explicitly (Pass 1+2),
param_assign kept in Phase 1b during hybrid state.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: 回归验证 — 全量 ET-Bench 召回 check

**Files:**
- (no changes, verification only)

- [ ] **Step 1: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS (157+ new tests)

- [ ] **Step 2: Run ET-Bench report — verify recall + FPR**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s 2>&1 | grep -E "^(fnptr|OVERALL)"`
Expected:
```
fnptr-callback                            33       33     xx 100.00%  xx.xx%
fnptr-struct                              21       21     xx 100.00%  xx.xx%
...
OVERALL                                  xxx      xxx    xxx   xx.xx%  30.54%
```

- Both `fnptr-callback` and `fnptr-struct` at 100% recall
- FPR ≤ 30.54% (no FP increase)
- Overall recall 98.86% (same as baseline)

- [ ] **Step 3: Commit final verification state if needed**

If no changes needed:
```bash
echo "All tests pass. Gap A + Gap B verified. 100% recall, FPR ≤ 30.54%."
```

---

## Verification Checklist

After all tasks complete:

- [ ] `.venv/bin/python -m pytest tests/ -q` — 159+ tests pass, 1 xfail
- [ ] `test_fnptr_callback_full_recall` — 33/33 (100% recall)
- [ ] `test_fnptr_struct_full_recall` — 21/21 (100% recall)
- [ ] `test_local_var_fnptr_arg_creates_callback_param_edge` — PASS
- [ ] `test_return_field_resolution_after_initializer_assign` — PASS
- [ ] `test_et_bench_report` — FPR ≤ 30.54%, overall recall ≥ 98.86%
