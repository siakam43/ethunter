# False Positive Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce ET-Bench FPR from ~59% to <30% while maintaining 100% recall on 9 target scenarios via four fixes (P0-P3).

**Architecture:** Four fixes across two source files, ordered simplest→complexest:
P2→P3→P0 modify `param_assign.py` incrementally. P1 modifies `field_call.py` independently.
Each fix is one Task (test→implement→verify→commit). TDD throughout.

**Tech Stack:** Python 3.11, pytest, `.venv/bin/python`, `PYTHONPATH=src`

---

### Task 1: Add Recall Regression Guards & FPR Baselines

**Files:**
- Modify: `tests/test_et_bench.py` (append ~80 lines at end)

- [ ] **Step 1: Add per-category full-recall tests and FPR ceilings**

Append to `tests/test_et_bench.py`:

```python
# === Recall regression guards ===

def _category_recall(category):
    """Compute recall and FPR for a single category."""
    cat_dir = os.path.join(ET_BENCH_DIR, category)
    matched = 0
    total = 0
    extra = 0
    detected = 0
    for example in sorted(os.listdir(cat_dir)):
        if not example.startswith('example_'):
            continue
        ex_dir = os.path.join(cat_dir, example)
        expected = _load_example_ground_truth(ex_dir)
        if not expected:
            continue
        total += len(expected)
        graph = _run_analysis_on_fixture(ex_dir)
        indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']
        found_pairs = {(e.caller, e.callee) for e in indirect_edges}
        expected_pairs = {(e['caller'], e['callee']) for e in expected}
        matched += len(found_pairs & expected_pairs)
        extra += len(found_pairs - expected_pairs)
        detected += len(found_pairs)
    recall = matched / total if total > 0 else 1.0
    fpr = extra / detected if detected > 0 else 0.0
    return matched, total, recall, fpr


def test_fnptr_callback_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-callback')
    assert recall == 1.0, f"fnptr-callback recall={recall:.2%} ({matched}/{total})"

def test_fnptr_cast_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-cast')
    assert recall == 1.0, f"fnptr-cast recall={recall:.2%} ({matched}/{total})"

def test_fnptr_global_array_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-global-array')
    assert recall == 1.0, f"fnptr-global-array recall={recall:.2%} ({matched}/{total})"

def test_fnptr_global_struct_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-global-struct')
    assert recall == 1.0, f"fnptr-global-struct recall={recall:.2%} ({matched}/{total})"

def test_fnptr_global_struct_array_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-global-struct-array')
    assert recall == 1.0, f"fnptr-global-struct-array recall={recall:.2%} ({matched}/{total})"

def test_fnptr_library_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-library')
    assert recall == 1.0, f"fnptr-library recall={recall:.2%} ({matched}/{total})"

def test_fnptr_only_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-only')
    assert recall == 1.0, f"fnptr-only recall={recall:.2%} ({matched}/{total})"

def test_fnptr_struct_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-struct')
    assert recall == 1.0, f"fnptr-struct recall={recall:.2%} ({matched}/{total})"

def test_fnptr_varargs_full_recall():
    matched, total, recall, _ = _category_recall('fnptr-varargs')
    assert recall == 1.0, f"fnptr-varargs recall={recall:.2%} ({matched}/{total})"
```

Also insert these FPR ceiling assertions at the end of `test_et_bench_report` (after the print block, before the function returns):

```python
    # FPR ceilings — start at current baseline, lowered as fixes land
    fpr_ceilings = {
        'fnptr-callback': 0.80,
        'fnptr-cast': 0.63,
        'fnptr-global-array': 0.01,
        'fnptr-global-struct': 0.90,
        'fnptr-global-struct-array': 0.47,
        'fnptr-library': 0.35,
        'fnptr-only': 0.08,
        'fnptr-struct': 0.47,
        'fnptr-varargs': 0.76,
    }
    for category, ceiling in fpr_ceilings.items():
        if category in results:
            actual_fpr = results[category]['fpr']
            assert actual_fpr <= ceiling, \
                f"{category} FPR={actual_fpr:.2%} exceeds ceiling {ceiling:.2%}"
```

- [ ] **Step 2: Run to confirm all pass at baseline**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: 25 original + 9 new recall + report with FPR check = 35 passed.

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add per-category recall gates and FPR baseline ceilings

9 test_<category>_full_recall enforce 100% recall.
FPR ceilings at current baseline — to be lowered after each fix.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: P2 — callback_reg fnptr Position Check

**Files:**
- Create: (test code appended to existing file)
- Modify: `src/ethunter/analyzer/param_assign.py:178-193` (_register_phase func_params call)
- Modify: `src/ethunter/analyzer/param_assign.py:286-290` (func_fp_params storage in analyze)
- Modify: `src/ethunter/analyzer/param_assign.py:363-373` (callback_reg branch in _collect_call_params)

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_p2_callback_reg_only_fnptr_positions():
    """Registration function: only fnptr-param positions emit callback_reg edges."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void my_handler(int x) { (void)x; }
static void cleanup_handler(int x) { (void)x; }
static void name_func(int x) { (void)x; }  /* fn name used as non-fnptr arg */

struct ctx { cb_fn handler; cb_fn cleanup; };

static void register_item(struct ctx *c, const char *name, int pri, cb_fn cb) {
    c->handler = cb;
}
static void register_cleanup(struct ctx *c, cb_fn cleanup) {
    c->cleanup = cleanup;
}

void setup(void) {
    struct ctx c;
    /* name_func at pos 1 (const char*, NOT fnptr) — should NOT emit callback_reg */
    register_item(&c, name_func, 10, my_handler);
    register_cleanup(&c, cleanup_handler);
}

void invoke(struct ctx *c) {
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
    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == 'callback_reg']

    # my_handler (at fnptr pos 3 in register_item) and cleanup_handler (at fnptr pos 1
    # in register_cleanup) should be in callback_reg
    callees = {e.callee for e in callback_reg_edges}
    assert 'my_handler' in callees, f"Expected my_handler in callback_reg, got: {callees}"
    assert 'cleanup_handler' in callees, \
        f"Expected cleanup_handler in callback_reg, got: {callees}"

    # name_func is at non-fnptr position 1 — should NOT be in callback_reg
    assert 'name_func' not in callees, \
        f"name_func at non-fnptr position should NOT be in callback_reg, got: {callees}"


def test_p2_callback_reg_cross_file_fallback():
    """Registration function not in func_fp_params still emits callback_reg (no regression)."""
    source = b'''
typedef void (*cb_fn)(int x);
static void my_handler(int x) { (void)x; }
struct ctx { cb_fn handler; };

/* Declaration only — no function_definition, so register_remote NOT in func_fp_params */
void register_remote(void *ctx, cb_fn cb);

void setup(void) {
    struct ctx c;
    register_remote(&c, my_handler);
}
'''
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

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
    callback_reg_edges = [e for e in graph.edges if e.indirect_kind == 'callback_reg']
    callees = {e.callee for e in callback_reg_edges}
    # register_remote has forward-declaration only → NOT in func_fp_params
    # Fallback must still emit callback_reg for my_handler
    assert 'my_handler' in callees, \
        f"Cross-file fallback failed: expected my_handler in {callees}"
```

- [ ] **Step 2: Run test to verify it fails for the right reason**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p2_callback_reg_only_fnptr_positions -v`

Expected: FAIL — `name_func` IS in callees set (current code emits callback_reg for all identifier args of registration functions).

- [ ] **Step 3: Implement the fix**

**3a. In `_register_phase`** (line ~194): change `_collect_func_params` call to also collect fnptr positions, and store on engine:

```python
# OLD (line 194):
func_params: dict[str, list[str]] = {}
_collect_func_params(tree.root_node, func_params)

# NEW:
func_params: dict[str, list[str]] = {}
func_fp_params: dict[str, set[int]] = {}
_collect_func_params(tree.root_node, func_params, func_fp_params)
# Store on engine (cross-file accumulation)
if hasattr(dataflow, 'state'):
    if not hasattr(dataflow.state, 'func_fp_params'):
        dataflow.state.func_fp_params = {}
    dataflow.state.func_fp_params.update(func_fp_params)
else:
    if not hasattr(dataflow, 'func_fp_params'):
        dataflow.func_fp_params = {}
    dataflow.func_fp_params.update(func_fp_params)
```

**3b. In `analyze()`** (lines 286-290): change func_fp_params storage from overwrite to update:

```python
# OLD:
if hasattr(dataflow, 'state'):
    dataflow.state.func_fp_params = func_fp_params
else:
    dataflow.func_fp_params = func_fp_params

# NEW:
if hasattr(dataflow, 'state'):
    if not hasattr(dataflow.state, 'func_fp_params'):
        dataflow.state.func_fp_params = {}
    dataflow.state.func_fp_params.update(func_fp_params)
else:
    if not hasattr(dataflow, 'func_fp_params'):
        dataflow.func_fp_params = {}
    dataflow.func_fp_params.update(func_fp_params)
```

**3c. In `_collect_call_params`** (lines 363-373), modify the `_is_registration` branch to check fnptr positions:

```python
# OLD (lines 363-373):
if _is_registration(call_name):
    dataflow.register_callback(target)
    edges.append(CallEdge(
        caller=caller or '<registration>',
        callee=target,
        caller_file=filepath,
        callee_file='',
        type=CallType.INDIRECT,
        indirect_kind='callback_reg',
        caller_line=node.start_point[0] + 1,
    ))
    if arg_idx < len(param_names):
        pname = param_names[arg_idx]
        dataflow.assign(pname, target)

# NEW:
if _is_registration(call_name):
    # Check if arg_idx is a known fnptr position for this registration function
    fp_params = getattr(dataflow, 'func_fp_params', None)
    if fp_params is None and hasattr(dataflow, 'state'):
        fp_params = getattr(dataflow.state, 'func_fp_params', None)
    fp_positions = fp_params.get(call_name, set()) if fp_params else set()

    if not fp_positions or arg_idx in fp_positions:
        dataflow.register_callback(target)
        edges.append(CallEdge(
            caller=caller or '<registration>',
            callee=target,
            caller_file=filepath,
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_reg',
            caller_line=node.start_point[0] + 1,
        ))
    if arg_idx < len(param_names):
        pname = param_names[arg_idx]
        dataflow.assign(pname, target)
```

The same change must also be applied to the `cast_expression` branch (lines ~409-419) and the `pointer_expression` branch (lines ~436-446) — all three `_is_registration` blocks share the same pattern. Wrap each with the same `fp_positions` check.

- [ ] **Step 4: Run P2 tests to verify they pass**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p2_callback_reg_only_fnptr_positions tests/test_et_bench.py::test_p2_callback_reg_cross_file_fallback -v`

Expected: 2 passed.

- [ ] **Step 5: Run full ET-Bench suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates still 100%, FPR lowered (especially fnptr-global-struct which had 77 callback_reg FPs).

- [ ] **Step 6: Lower FPR ceilings for improved categories**

In `test_et_bench_report`, update the FPR ceiling values based on actual post-fix numbers. Run the report test to get current values:

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr"`

Update the ceilings in the report test to the actual values + 0.02 margin.

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "fix: add fnptr position check to callback_reg edges (P2)

_register_phase now collects func_fp_params cross-file via .update().
Pass 1 callback_reg emission requires arg_idx to be a known fnptr
position (or fallback when callee not in func_fp_params).
Reduces ~70 callback_reg false positives.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: P3 — Parameter Name dataflow Key Isolation

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:376,383,392,395` (dataflow.assign calls in _collect_call_params)
- Modify: `src/ethunter/analyzer/param_assign.py:486-487` (dataflow.resolve call in Pass 2)

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_p3_param_namespace_isolation():
    """Same param name in different functions should not cross-pollute in dataflow."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void handler_a(int x) { (void)x; }
static void handler_b(int x) { (void)x; }

struct ctx { cb_fn h; };

/* Both use param name "cb" */
static void register_a(struct ctx *c, cb_fn cb) {
    c->h = cb;
}
static void register_b(struct ctx *c, cb_fn cb) {
    c->h = cb;
}

void setup(void) {
    struct ctx ca, cb2;
    register_a(&ca, handler_a);
    register_b(&cb2, handler_b);
}
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState, DataflowEngine
    from ethunter.analyzer import param_assign

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine(state=VariableState())

    param_assign.analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=engine)

    # "register_a:cb" should resolve to handler_a only
    targets_a = engine.resolve('register_a:cb')
    assert targets_a == {'handler_a'}, \
        f"register_a:cb should be {{handler_a}}, got: {targets_a}"

    # "register_b:cb" should resolve to handler_b only
    targets_b = engine.resolve('register_b:cb')
    assert targets_b == {'handler_b'}, \
        f"register_b:cb should be {{handler_b}}, got: {targets_b}"

    # Bare "cb" is the old key — fallback resolution (should contain both after Pass 1
    # writes with new keys, bare "cb" should be empty since we no longer write bare)
    bare = engine.resolve('cb')
    # bare may still have values from other mechanisms; the key assertion is the
    # prefixed keys above
    _ = bare
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p3_param_namespace_isolation -v`

Expected: FAIL — `engine.resolve('register_a:cb')` returns empty set (current code writes bare `"cb"` key, not `"register_a:cb"`).

- [ ] **Step 3: Implement the fix**

**3a. In `_collect_call_params`**, change all `dataflow.assign(pname, target)` calls to use `call_name` prefix. There are 3 occurrences (identifier branch ~line 383, cast_expression branch ~line 421, pointer_expression branch ~line 452). Each is in the pattern:

```python
dataflow.assign(pname, target)
```

Change each to:

```python
dataflow.assign(f'{call_name}:{pname}', target)
```

Also change the `_is_registration` branch's `dataflow.assign` (~line 376):

```python
dataflow.assign(pname, target)
```

Change to:

```python
dataflow.assign(f'{call_name}:{pname}', target)
```

**3b. In Pass 2** (~line 486), change the dataflow.resolve call from bare key to prefixed-first:

```python
# OLD (line 486):
df_targets = dataflow.resolve(param_name)

# NEW:
df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
if not df_targets:
    df_targets = dataflow.resolve(param_name)
```

- [ ] **Step 4: Run P3 test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p3_param_namespace_isolation -v`

Expected: PASS.

- [ ] **Step 5: Run full ET-Bench suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates 100%.

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "fix: add function-scoped prefix to param dataflow keys (P3)

Pass 1 now writes f'{call_name}:{pname}' instead of bare pname.
Pass 2 resolves with fa.enclosing_func prefix first, bare as fallback.
Prevents cross-function param name pollution in global dataflow.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: P0 — Per-Call-Site Resolution for callback_param Edges

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:334-460` (Pass 1 — add call_site_targets)
- Modify: `src/ethunter/analyzer/param_assign.py:505-546` (Pass 3 — use call_site_targets)
- Modify: `src/ethunter/analyzer/param_assign.py:548-617` (Pass 4 — rewrite to use call_site_targets)

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_p0_param_callback_no_nx_m_edges():
    """N callers × M targets should produce O(N+M) edges, not O(N×M)."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*cb_fn)(int x);

static void h1(int x) { (void)x; }
static void h2(int x) { (void)x; }
static void h3(int x) { (void)x; }

/* Non-registration function: receives fnptr and calls it */
static void dispatch(cb_fn cb) {
    cb(42);
}

void caller1(void) { dispatch(h1); }
void caller2(void) { dispatch(h2); }
void caller3(void) { dispatch(h3); }
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

    # Expected: Pass 3 produces (dispatch, h1), (dispatch, h2), (dispatch, h3)
    # Pass 4 produces (caller1, h1), (caller2, h2), (caller3, h3)
    # Total at most 6 edges. Current code produces 3×3=9 (N×M).
    pairs = {(e.caller, e.callee) for e in callback_param}

    # Pass 3 edges (callee body = dispatch)
    assert ('dispatch', 'h1') in pairs
    assert ('dispatch', 'h2') in pairs
    assert ('dispatch', 'h3') in pairs

    # Pass 4 edges (outer callers)
    assert ('caller1', 'h1') in pairs
    assert ('caller2', 'h2') in pairs
    assert ('caller3', 'h3') in pairs

    # No N×M cross edges: caller1 should NOT be connected to h2 or h3
    assert ('caller1', 'h2') not in pairs, \
        f"N×M cross edge (caller1, h2) should not exist"
    assert ('caller1', 'h3') not in pairs, \
        f"N×M cross edge (caller1, h3) should not exist"
    assert ('caller2', 'h1') not in pairs, \
        f"N×M cross edge (caller2, h1) should not exist"

    # Total callback_param edges: at most 6 (3 from Pass 3 + 3 from Pass 4)
    assert len(callback_param) <= 6, \
        f"Expected <=6 callback_param edges, got {len(callback_param)}: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails (N×M explosion)**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p0_param_callback_no_nx_m_edges -v`

Expected: FAIL — `('caller1', 'h2')` IS in pairs (current N×M = 9 edges).

- [ ] **Step 3: Implement the fix — Part A: Add call_site_targets in Pass 1**

In `analyze()`, before `_collect_call_params(tree.root_node)` (line ~460), add the call_site_targets dict:

```python
# Add before _collect_call_params
call_site_targets: dict[tuple[str, str, int], set[str]] = {}
```

Inside `_collect_call_params`, in each arg-handling branch (identifier, cast_expression, pointer_expression), after identifying `target`, `caller`, `call_name`, `arg_idx`, add:

```python
# In the identifier branch (~line 360), after computing arg_idx and target,
# add to call_site_targets:
key = (caller or '<unknown>', call_name, arg_idx)
if key not in call_site_targets:
    call_site_targets[key] = set()
call_site_targets[key].add(target)
```

Do the same in the `cast_expression` branch (~line 406) and `pointer_expression` branch (~line 433), in the non-registration sub-branches (the `else` clauses).

In the `_is_registration` branches, do NOT add to call_site_targets — those edges are handled by callback_reg, not callback_param.

Apply the same addition in the dataflow fallback branch (~line 389-395) where `df_targets` is used:

```python
# In the dataflow fallback branch:
for t in df_targets:
    key = (caller or '<unknown>', call_name, arg_idx)
    if key not in call_site_targets:
        call_site_targets[key] = set()
    call_site_targets[key].add(t)
```

- [ ] **Step 4: Implement the fix — Part B: Rewrite Pass 4**

Replace the entire Pass 4 section (lines 548-617, from `call_targets` dict to the final emission loop) with:

```python
    # === Pass 4: emit edges from call-site to actual targets ===
    # Uses per-call-site resolution via call_site_targets (no N×M merge)
    seen_pass4: set[tuple[str, str]] = set()
    for (caller, call_name, arg_idx), targets in call_site_targets.items():
        for target in targets:
            key = (caller, target)
            if key not in seen_pass4:
                seen_pass4.add(key)
                edges.append(CallEdge(
                    caller=caller,
                    callee=target,
                    caller_file=filepath,
                    callee_file='',
                    type=CallType.INDIRECT,
                    indirect_kind='callback_param',
                    caller_line=0,
                ))
```

- [ ] **Step 5: Implement the fix — Part C: Rewrite Pass 3**

Replace the `_detect_param_calls` function (lines 506-533) with a version that queries call_site_targets instead of merged param_mappings:

```python
    # === Pass 3: detect calls through function pointer parameters ===
    def _detect_param_calls(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            call_target_name = None
            if func_node and func_node.type == 'identifier' and func_node.text:
                call_target_name = func_node.text.decode('utf-8')
            elif func_node and func_node.type == 'parenthesized_expression':
                for c in func_node.children:
                    if c.type == 'pointer_expression' and c.children:
                        inner = c.children[-1]
                        if inner.type == 'identifier' and inner.text:
                            call_target_name = inner.text.decode('utf-8')
                            break
            elif func_node and func_node.type == 'pointer_expression' and func_node.children:
                inner = func_node.children[-1]
                if inner.type == 'identifier' and inner.text:
                    call_target_name = inner.text.decode('utf-8')

            if call_target_name:
                enclosing_func = find_enclosing_function(node, tree.root_node)
                targets = set()

                # Try per-call-site resolution first
                if enclosing_func and enclosing_func in func_params:
                    params = func_params[enclosing_func]
                    if call_target_name in params:
                        arg_idx = params.index(call_target_name)
                        for (clr, cn, ai), tgs in call_site_targets.items():
                            if cn == enclosing_func and ai == arg_idx:
                                targets.update(tgs)

                # Fallback to merged param_mappings (cross-file or missing func_params)
                if not targets:
                    targets = param_mappings.get(call_target_name, set())

                if targets:
                    caller = enclosing_func
                    for target in targets:
                        call_site_edges.append(
                            (caller or '<unknown>', target, filepath,
                             node.start_point[0] + 1))

        for child in node.children:
            _detect_param_calls(child)

    _detect_param_calls(tree.root_node)
```

Also update the `call_site_edges` type annotation at line 504:

```python
call_site_edges: list[tuple[str, str, str, int]] = []
```

- [ ] **Step 6: Run P0 test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p0_param_callback_no_nx_m_edges -v`

Expected: PASS — exactly 6 callback_param edges, no N×M cross edges.

- [ ] **Step 7: Run full ET-Bench suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates 100%. FPR significantly lower (especially fnptr-callback and fnptr-global-struct).

- [ ] **Step 8: Lower FPR ceilings**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr"`

Update the FPR ceiling values in `test_et_bench_report` to actual + 0.03 margin. Expected FPR drops significantly for fnptr-callback (from ~0.79 toward ~0.35) and fnptr-global-struct (from ~0.90 toward ~0.70).

- [ ] **Step 9: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "fix: per-call-site resolution for callback_param edges (P0)

Add call_site_targets dict indexed by (caller, call_name, arg_idx).
Pass 4 now emits one edge per call site (no N×M merge).
Pass 3 queries call_site_targets per callee function.
Eliminates ~500 callback_param false positives.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: P1 — field_call Type-Aware Suffix Matching

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:62-69` (add _build_field_index and _suffix_resolve)
- Modify: `src/ethunter/analyzer/field_call.py:133-140` (replace suffix merge)
- Modify: `src/ethunter/analyzer/field_call.py:178-180` (replace last-component fallback)

- [ ] **Step 1: Write the TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_p1_field_call_suffix_same_struct_only():
    """Two unrelated structs with same field name: suffix matching must not mix them."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*handler_fn)(int x);

static void handler_a(int x) { (void)x; }
static void handler_b(int x) { (void)x; }

struct type_a {
    const char *name;
    int version;
    handler_fn handler;
};

struct type_b {
    int id;
    handler_fn handler;
};

static struct type_a obj_a = { "alpha", 1, handler_a };
static struct type_b obj_b = { 42, handler_b };

/* Only obj_a.handler should resolve to handler_a */
void use_obj_a(void) {
    if (obj_a.handler)
        obj_a.handler(42);
}

void use_obj_b(void) {
    if (obj_b.handler)
        obj_b.handler(42);
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
    field_call_edges = [e for e in graph.edges if e.indirect_kind == 'field_call']

    # obj_a.handler → handler_a should exist
    assert ('use_obj_a', 'handler_a') in {(e.caller, e.callee) for e in field_call_edges}, \
        "Missing use_obj_a -> handler_a"

    # obj_b.handler → handler_b should exist
    assert ('use_obj_b', 'handler_b') in {(e.caller, e.callee) for e in field_call_edges}, \
        "Missing use_obj_b -> handler_b"

    # CRITICAL: obj_a.handler must NOT resolve to handler_b (different struct type)
    extra_pairs = {(e.caller, e.callee) for e in field_call_edges}
    assert ('use_obj_a', 'handler_b') not in extra_pairs, \
        f"type_a.handler should not match type_b's handler_b target"
    assert ('use_obj_b', 'handler_a') not in extra_pairs, \
        f"type_b.handler should not match type_a's handler_a target"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p1_field_call_suffix_same_struct_only -v`

Expected: FAIL — `('use_obj_a', 'handler_b')` IS in extra_pairs (current suffix scan matches all `.handler>` keys regardless of struct type).

- [ ] **Step 3: Implement the fix — Part A: Add helper functions**

Add these two functions near the top of `field_call.py`, after the existing imports and `_collect_macros`:

```python
def _build_field_index(dataflow) -> dict[str, set[str]]:
    """Build field_name -> {base_var_names} from <gstruct:*> keys."""
    index: dict[str, set[str]] = {}
    for key in dataflow.targets:
        if key.startswith('<gstruct:') and key.endswith('>'):
            inner = key[9:-1]
            if '.' in inner:
                base, field = inner.split('.', 1)
                if field not in index:
                    index[field] = set()
                index[field].add(base)
    return index


def _suffix_resolve(dataflow, field_index, base, fieldname):
    """Type-aware suffix resolution for base.fieldname.

    Only matches candidates whose structs share at least one field name
    with the current base — indicating same struct type.
    """
    targets = set()
    candidates = field_index.get(fieldname, set())
    if not candidates:
        return targets

    current_fields = {f for f, bases in field_index.items() if base in bases}
    if not current_fields:
        return targets

    for cand in candidates:
        cand_fields = {f for f, bases in field_index.items() if cand in bases}
        # Must share at least one field OTHER than the queried fieldname.
        # If two structs share only the fnptr field itself, they are unrelated.
        if (current_fields & cand_fields) - {fieldname}:
            tgs = dataflow.resolve(f'<gstruct:{cand}.{fieldname}>')
            targets.update(tgs)

    return targets
```

- [ ] **Step 4: Implement the fix — Part B: Build index and replace suffix scans**

At the start of `analyze()` (after `macro_map = _collect_macros(tree)` at line 71), add:

```python
    field_index = _build_field_index(dataflow)
```

Replace the "Always merge suffix" block (lines 133-140):

```python
                    # OLD (lines 133-140):
                    # Always merge suffix-matched targets (even when <gstruct:> had partial hits)
                    if '.' in field_path:
                        parts = field_path.split('.')
                        for i in range(1, len(parts)):
                            suffix = '.'.join(parts[i:])
                            for key, vals in dataflow.targets.items():
                                if key.endswith(f'.{suffix}>') and vals:
                                    targets.update(vals)

                    # NEW:
                    if '.' in field_path and targets:
                        base = field_path.split('.')[0]
                        fieldname = field_path.split('.')[-1]
                        targets.update(_suffix_resolve(dataflow, field_index, base, fieldname))
```

Replace the "last component fallback" (lines 178-180):

```python
                    # OLD (lines 178-180):
                            # Scan for keys ending with .{last_part}> in dataflow
                            for key, vals in dataflow.targets.items():
                                if key.endswith(f'.{last_part}>') and vals:
                                    targets.update(vals)

                    # NEW:
                            base = field_path.split('.')[0]
                            targets.update(_suffix_resolve(dataflow, field_index, base, last_part))
```

- [ ] **Step 5: Run P1 test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_p1_field_call_suffix_same_struct_only -v`

Expected: PASS — type_a.handler and type_b.handler are correctly isolated.

- [ ] **Step 6: Run full ET-Bench suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All recall gates 100%. FPR lowered for fnptr-struct, fnptr-global-struct-array (affected by field_call FPs).

- [ ] **Step 7: Lower FPR ceilings**

Update FPR ceilings in `test_et_bench_report` based on actual values. Run full suite and check:

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr"`

Set each ceiling to actual + 0.03 margin. Expected overall FPR < 30%.

- [ ] **Step 8: Commit**

```bash
git add src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "fix: type-aware suffix matching for field_call (P1)

Replace wildcard .fieldname> suffix scans with _suffix_resolve that
validates struct type similarity via shared field names. Two suffix
paths replaced: the 'always merge' at line 134-140 and the
'last-component fallback' at line 178-180.
Eliminates ~120 field_call false positives.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Final Verification

- [ ] **Step 1: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -v`

Expected: All tests pass (benchmarks + et_bench + unit tests + cross-file).

- [ ] **Step 2: Print final FPR report**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1`

Verify:
- All 9 target categories have 100% recall
- Overall FPR is below 30% (down from ~59%)
- FPR ceilings are set appropriately

- [ ] **Step 3: Commit final FPR ceiling values**

```bash
git add tests/test_et_bench.py
git commit -m "test: finalize FPR ceilings after P0-P3 fixes

Expected overall FPR <30% with 100% recall on all 9 target categories.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
