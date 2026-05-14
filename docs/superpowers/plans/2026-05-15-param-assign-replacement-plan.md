# param_assign 完全替换实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 3 个 gap，删除 param_assign.py (786行)，完成 3-Phase pipeline 全替换，删除 ~811 行代码，hasattr 回退链归零。

**Architecture:** 6 TDD tasks 按严格顺序执行: (1) param_binding 签名+三层 gating+orchestrator 调整，(2) 删除 registered_callbacks dead code，(3) 删除 param_assign.py + 最终清理，(4) func_fp_params/param_usage 从 state 迁到 engine，(5) 移除所有 hasattr 回退链，(6) 移除 Fix B + field_call 双读。关键约束：Task 3（删除 param_assign）必须在 Task 4（声明 DataflowEngine 字段）之前执行，因 param_assign 的 `getattr(dataflow, 'func_fp_params', None)` 模式在字段声明后会返回 `{}` 而非 `None`，破坏 hasattr 回退链。

**Tech Stack:** Python 3.11, tree-sitter-c, pytest (`.venv/bin/python`)

---

## File Structure

```
src/ethunter/analyzer/
├─ param_assign.py      DELETE   786→✗  (Task 3)
├─ orchestrator.py      MODIFY   152→130  Remove param_assign + Fix B; add param_binding to TARGET_RESOLVERS (Tasks 1,3,6)
├─ param_binding.py     MODIFY   207→230  Signature + 3-layer gating (Task 1) + hasattr removal (Task 5)
├─ dataflow.py          MODIFY   222→220  Delete registered_callbacks (Task 2); declare func_fp_params/param_usage (Task 4)
├─ param_helpers.py     MODIFY   210→210  prepare() writes engine fields directly (Task 4)
├─ field_call.py        MODIFY   282→240  Remove old-format dual-read (Task 6); hasattr chain replacement (Task 5)
├─ param_dispatch.py    MODIFY   140→140  hasattr chain (Task 5)
├─ callback_reg.py      MODIFY    55→55   hasattr chain (Task 5)
tests/
└─ test_et_bench.py     MODIFY     1→1    Remove xfail marker (Task 3)
```

---

### Task 1: Fix Gap 1 + Gap 2 — param_binding 签名 + 三层 gating + orchestrator 调整

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py`
- Modify: `src/ethunter/analyzer/orchestrator.py`

- [ ] **Step 1: Write TDD test — Gap 1 + Gap 2 regression guard**

Append to `tests/test_et_bench.py`:

```python
def test_param_binding_suppresses_non_fnptr_args_as_registration():
    """Non-fnptr args to known-function should NOT be recorded as registration_sites."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*cb_fn)(int x);
    static void my_handler(int x) { (void)x; }
    static void name_func(int x) { (void)x; }
    static void register_item(struct ctx *c, const char *name, int pri, cb_fn cb) {
        c->handler = cb;
    }
    void setup(void) {
        struct ctx c;
        register_item(&c, name_func, 10, my_handler);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", st, engine)
    # name_func at arg_idx=1 (const char* — NOT fnptr) should NOT be a registration_site
    for site in engine.registration_sites:
        assert site["target"] != "name_func", \
            f"name_func at non-fnptr position should not be registration_site: {site}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_binding_suppresses_non_fnptr_args_as_registration -v`
Expected: FAIL — `name_func` appears in registration_sites (current bug: empty `fp_params_positions` makes all args registration_sites)

- [ ] **Step 3: Update param_binding.py — Gap 1 + Gap 2 fixes**

Replace `param_binding.py` `analyze()` signature and first 45 lines:

```python
def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table,
    dataflow,
) -> list:
    """Phase 1: Bind call-site arguments to function targets. Writes dataflow
    and registration_sites. Returns empty list (no edges).

    Reads: engine.func_params, engine.state.func_fp_params (from prepare)
    Writes: dataflow.targets, engine.registration_sites, engine.call_site_targets
    """
    func_params = dataflow.func_params
    func_fp_params = getattr(dataflow.state, 'func_fp_params', {})
    symbol_names = symbol_table.all_function_names
    macros = _collect_simple_macros(tree)
```

In `_collect_call_params`, replace the registration gate in all 3 argument-type branches (identifier, cast_expression, pointer_expression):

```python
# OLD:
                                fp_params_positions = func_fp_params.get(call_name, set())
                                if not fp_params_positions or arg_idx in fp_params_positions:
                                    dataflow.registration_sites.append({...})

# NEW — three-layer gating:
                                fp_params_positions = func_fp_params.get(call_name, None)
                                is_reg = False
                                if fp_params_positions is not None:
                                    if arg_idx in fp_params_positions:
                                        is_reg = True
                                else:
                                    if _is_registration(call_name):
                                        is_reg = True
                                if is_reg:
                                    dataflow.registration_sites.append({...})
```

- [ ] **Step 4: Update orchestrator.py — add param_binding as first TARGET_RESOLVER**

In `src/ethunter/analyzer/orchestrator.py`:

Change TARGET_RESOLVERS:
```python
# OLD:
TARGET_RESOLVERS = [
    param_assign,
    direct_assign,
    initializer_assign,
    cast_assign,
]

# NEW — param_binding must be first:
TARGET_RESOLVERS = [
    param_binding,
    direct_assign,
    initializer_assign,
    cast_assign,
]
```

Update the Phase 1 loop to handle param_binding's different signature:
```python
# Phase 1: Target Resolution (write dataflow only, no edges)
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
```

Add `param_binding` to imports:
```python
from ethunter.analyzer import (
    param_binding,
    direct_assign,
    initializer_assign,
    cast_assign,
)
```

Keep `param_assign` import for now (still needed for Phase 1b callback detection and _register_phase until Task 3).

- [ ] **Step 5: Run TDD test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_binding_suppresses_non_fnptr_args_as_registration -v`
Expected: PASS

- [ ] **Step 6: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All existing tests PASS (param_assign still in pipeline)

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "fix: param_binding symbol_table + 3-layer gating (Gap 1+2)

Gap 1: param_binding receives symbol_table, uses all_function_names as
symbol_names (not func_params.keys()). Gap 2: 3-layer registration gating
replaces flat func_fp_params check — known fnptr positions use exact match,
unknown functions use _is_registration fallback, known-no-fnptr skips.
param_binding added as first TARGET_RESOLVER in orchestrator.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Fix Gap 3 — 删除 registered_callbacks dead code

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`

- [ ] **Step 1: Delete from VariableState**

In `src/ethunter/analyzer/dataflow.py`, remove from `VariableState`:

```python
# REMOVE this field:
    registered_callbacks: set[str] = field(default_factory=set)

# REMOVE this method:
    def register_callback(self, func_name: str) -> None:
        self.registered_callbacks.add(func_name)
```

- [ ] **Step 2: Delete from DataflowEngine**

Remove the delegate method and property:

```python
# REMOVE this method:
    def register_callback(self, func_name: str) -> None:
        self.state.register_callback(func_name)

# REMOVE this property:
    @property
    def registered_callbacks(self) -> set[str]:
        return self.state.registered_callbacks
```

- [ ] **Step 3: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS (registered_callbacks is dead code, no impact)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "refactor: delete registered_callbacks dead code (Gap 3)

VariableState.registered_callbacks was written by param_assign but never
read by any analyzer module. Removed field + register_callback() from
both VariableState and DataflowEngine.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Delete param_assign.py + final orchestrator cleanup (Spec 4.1)

> **关键约束**: 此 Task 必须在 Task 4（声明 DataflowEngine 字段）之前执行。

**Files:**
- Delete: `src/ethunter/analyzer/param_assign.py`
- Modify: `src/ethunter/analyzer/orchestrator.py`
- Modify: `tests/test_et_bench.py`

- [ ] **Step 1: Delete param_assign.py**

Run: `rm src/ethunter/analyzer/param_assign.py`

- [ ] **Step 2: Clean orchestrator.py**

Remove the `import param_assign` block:
```python
# REMOVE:
from ethunter.analyzer import (
    param_assign,
    direct_assign,
    initializer_assign,
    cast_assign,
)
# REPLACE with:
from ethunter.analyzer import (
    direct_assign,
    initializer_assign,
    cast_assign,
)
```

Remove the `param_assign._register_phase()` call (search for `_register_phase`):
```python
# REMOVE:
    # Phase 1a (cont'd): param_assign pre-scan for cross-file state
    for filepath, tree in trees.items():
        param_assign._register_phase(tree, filepath, symbol_table, engine)
```

Remove the `param_assign.analyze()` Phase 1b call (search for `Phase 1b: param_assign`):
```python
# REMOVE:
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

Update the comment at the top to reflect the final pipeline.

- [ ] **Step 3: Remove xfail marker from type-aware test**

In `tests/test_et_bench.py`:
```python
# OLD:
@pytest.mark.xfail(reason="Requires full type-aware field_call migration (Task 11 cleanup)")
def test_type_aware_key_isolates_different_struct_types():

# NEW:
def test_type_aware_key_isolates_different_struct_types():
```

- [ ] **Step 4: Run full test suite — verify recall + FPR**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS (including previously-xfailed test). 157/157.

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s 2>&1 | grep -E "^(fnptr|OVERALL)"`
Expected: 8/9 scenarios at 100% recall, FPR ≤ 30.54%.

- [ ] **Step 5: Commit final**

```bash
git rm src/ethunter/analyzer/param_assign.py
git add src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "refactor: delete param_assign.py — full 3-phase pipeline replacement

Removed param_assign (786 lines). _register_phase replaced by
param_helpers.prepare(). callback detection replaced by callback_reg
Phase 3. Final pipeline: Phase 1a (prepare) → Phase 1 (TARGET_RESOLVERS
with param_binding first) → Phase 2 (CALL_DETECTORS + param_dispatch)
→ Phase 3 (callback_reg with covered_callees). xfail marker removed.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Promote func_fp_params / param_usage from state to engine (Spec 4.4)

> **前置条件**: Task 3 已完成（param_assign 已删除，其 `getattr(dataflow, 'func_fp_params', None)` 模式不再存在）。

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`
- Modify: `src/ethunter/analyzer/param_helpers.py`

- [ ] **Step 1: Declare fields on DataflowEngine**

In `src/ethunter/analyzer/dataflow.py`, add after the `func_params` field:

```python
    # Fnptr parameter metadata (promoted from state to engine after param_assign removal)
    func_fp_params: dict[str, set[int]] = field(default_factory=dict)
    param_usage: dict[tuple[str, int], str] = field(default_factory=dict)
```

Remove the `NOTE` comment about them remaining on state.

- [ ] **Step 2: Update param_helpers.prepare() — write to engine directly**

In `src/ethunter/analyzer/param_helpers.py`, change `prepare()`:

```python
# OLD:
    # Store func_fp_params on state (migration: still on state to keep hasattr fallback working)
    if not hasattr(dataflow.state, 'func_fp_params'):
        dataflow.state.func_fp_params = {}
    dataflow.state.func_fp_params.update(func_fp_params)

# NEW:
    dataflow.func_fp_params.update(func_fp_params)
```

```python
# OLD:
        if not hasattr(dataflow.state, 'param_usage'):
            dataflow.state.param_usage = {}
        dataflow.state.param_usage.update(param_usage)

# NEW:
        dataflow.param_usage.update(param_usage)
```

- [ ] **Step 3: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS (readers still use dataflow.state until Task 5 migration)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py src/ethunter/analyzer/param_helpers.py
git commit -m "refactor: promote func_fp_params/param_usage from state to engine

Declare func_fp_params and param_usage on DataflowEngine (previously
deferred due to hasattr fallback chain conflict with param_assign).
prepare() now writes directly to engine fields.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Remove hasattr fallback chains (Spec 4.5)

> **前置条件**: Task 4 已完成（字段已声明在 DataflowEngine 上）。

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py`
- Modify: `src/ethunter/analyzer/param_dispatch.py`
- Modify: `src/ethunter/analyzer/callback_reg.py`
- Modify: `src/ethunter/analyzer/field_call.py`

- [ ] **Step 1: Update param_binding.py**

```python
# OLD (line 39):
    func_fp_params = getattr(dataflow.state, 'func_fp_params', {})

# NEW:
    func_fp_params = dataflow.func_fp_params
```

- [ ] **Step 2: Update param_dispatch.py**

```python
# OLD (line 25):
    func_fp_params = getattr(dataflow.state, 'func_fp_params', {})

# NEW:
    func_fp_params = dataflow.func_fp_params
```

- [ ] **Step 3: Update callback_reg.py**

```python
# OLD (line 26):
    param_usage = getattr(dataflow.state, 'param_usage', {})

# NEW:
    param_usage = dataflow.param_usage
```

- [ ] **Step 4: Update field_call.py**

Replace the callback-of-callback func_fp_params access (line 221):
```python
# OLD:
    func_fp_params = getattr(dataflow.state, 'func_fp_params', None) if hasattr(dataflow, 'state') else None

# NEW:
    func_fp_params = dataflow.func_fp_params
```

- [ ] **Step 5: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py src/ethunter/analyzer/param_dispatch.py src/ethunter/analyzer/callback_reg.py src/ethunter/analyzer/field_call.py
git commit -m "refactor: remove hasattr fallback chains for func_fp_params/param_usage

All readers now use dataflow.func_fp_params and dataflow.param_usage directly
(instead of getattr(dataflow.state, 'xxx', {}) pattern). Possible now that
param_assign is deleted (Task 3) and fields are declared on DataflowEngine (Task 4).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Remove Fix B + field_call dual-read (Spec 4.2 + 4.3)

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py`
- Modify: `src/ethunter/analyzer/field_call.py`

- [ ] **Step 1: Remove Fix B from orchestrator.py**

Delete the Fix B post-processing filter:
```python
# REMOVE this entire block:
    # Fix B: suppress callback edges where callee is covered by field_call
    field_callees = {e.callee for e in graph.edges
                     if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    if field_callees:
        filtered = []
        for edge in graph.edges:
            if edge.indirect_kind in ('callback_reg', 'callback_param') \
                    and edge.callee in field_callees:
                continue
            filtered.append(edge)
        graph.edges = filtered
```

- [ ] **Step 2: Remove old-format dual-read from field_call.py**

In `field_call.py`, in `_visit()` inside the `if field_path:` block, remove the old-format fallback that follows the type-aware Layer 0 check:

```python
# REMOVE:
                    if not targets:
                        targets = dataflow.resolve(f'<gstruct:{field_path}>')
                    # Try <struct:path> (from param_assign)
                    if not targets:
                        targets = dataflow.resolve(f'<struct:{field_path}>')
```

Keep `<chain:path>` and all subsequent fallback layers (suffix match, garray, etc.) — those are independent of param_assign.

- [ ] **Step 3: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS (callback_reg Phase 3 Stage 2 replaces Fix B; old-format keys no longer written after param_assign deletion)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py src/ethunter/analyzer/field_call.py
git commit -m "refactor: remove Fix B post-processing + field_call old-format dual-read

Fix B replaced by callback_reg Phase 3 Stage 2 (covered_callees pre-check).
Old-format <gstruct:path> and <struct:path> keys are no longer written
(param_assign already deleted in Task 3), so dual-read code removed.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Verification Checklist

After all tasks complete:

- [ ] `.venv/bin/python -m pytest tests/ -q` — 157/157 passed
- [ ] `tests/test_et_bench.py::test_et_bench_report` — recall ≥ 98.86%, FPR ≤ 30.54%
- [ ] `tests/test_et_bench.py::test_fnptr_callback_full_recall` — PASS (100% recall)
- [ ] `tests/test_et_bench.py::test_fnptr_global_struct_full_recall` — PASS (100% recall)
- [ ] `tests/test_et_bench.py::test_fnptr_struct_full_recall` — PASS (100% recall)
- [ ] `tests/test_et_bench.py::test_fnptr_only_full_recall` — PASS (100% recall)
- [ ] `tests/test_et_bench.py::test_fnptr_cast_full_recall` — PASS (100% recall)
- [ ] `tests/test_et_bench.py::test_type_aware_key_isolates_different_struct_types` — PASS (no longer xfail)
- [ ] `tests/test_et_bench.py::test_param_binding_suppresses_non_fnptr_args_as_registration` — PASS
- [ ] `grep -rn 'getattr.*func_fp_params\|getattr.*param_usage\|hasattr.*state.*func_fp_params\|hasattr.*state.*param_usage' src/ethunter/analyzer/` — zero matches
- [ ] `ls src/ethunter/analyzer/param_assign.py` — file not found
- [ ] `grep -n 'param_assign' src/ethunter/analyzer/orchestrator.py` — zero matches
