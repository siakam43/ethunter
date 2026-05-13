# Architectural FP Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce FPs by ~126 via four architectural fixes (Fix 1: type+name key, Fix 3: behavioral registration, Fix 5: covered_callees pipeline, Fix 6: chain_context). Maintain 100% recall on 9 target scenarios.

**Architecture:** Fix 5 restructures the pipeline (foundation). Fix 3 adds param_usage classification using covered_callees. Fix 1 upgrades dataflow keys to type+name format (largest impact: ~98 field_call FPs). Fix 6 adds chain_context field (trivial). TDD throughout.

**Tech Stack:** Python 3.11, pytest, `.venv/bin/python`

---

### Task 1: Fix 5 — Pipeline Restructure + covered_callees

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py` (add Phase 1a.5, Phase 1.5, remove Fix B)
- Modify: `src/ethunter/analyzer/field_call.py` (split into `_resolve_struct_fields` + `_detect_field_calls`)
- Modify: `src/ethunter/analyzer/param_assign.py` (add covered_callees check before emit)
- Modify: `src/ethunter/analyzer/dataflow.py` (add `covered_callees` set)

- [ ] **Step 1: Write TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_fix5_covered_callees_suppress_callback():
    """callback_reg suppressed when callee is in covered_callees."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
typedef void (*fn_t)(void);
static void my_fn(void) {}

struct s { fn_t handler; };
static struct s obj = { my_fn };  /* initializer_assign writes <gstruct:obj.handler> */

/* Registration function: fn saved in struct field */
static void reg(struct s *o, fn_t f) { o->handler = f; }

void setup(void) { struct s o; reg(&o, my_fn); }
void dispatch(void) { if (obj.handler) obj.handler(); }
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
    cr_callees = {e.callee for e in graph.edges if e.indirect_kind == "callback_reg"}
    assert "my_fn" not in cr_callees, \
        f"my_fn in covered_callees, callback_reg should be suppressed: {cr_callees}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix5_covered_callees_suppress_callback -v`
Expected: FAIL — `my_fn` IS in `cr_callees`.

- [ ] **Step 3: Split field_call.analyze()**

In `src/ethunter/analyzer/field_call.py`, extract Pass 1 into a new function:

```python
def _resolve_struct_fields(tree, filepath, symbol_table, dataflow):
    """Collect field assignments and write <gstruct:> keys to dataflow. No edges."""
    symbol_names = symbol_table.all_function_names
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)
```

Extract Pass 2+ into `_detect_field_calls()`:

```python
def _detect_field_calls(tree, filepath, symbol_table, dataflow):
    """Detect field expression calls and emit field_call edges."""
    edges = []
    # Move the _visit logic from current analyze() here
    # (the main _visit tree walk, including callback-of-callback)
    ...
    return edges
```

Keep the original `analyze()` as a wrapper for backward compatibility:

```python
def analyze(tree, filepath, symbol_table, dataflow):
    _resolve_struct_fields(tree, filepath, symbol_table, dataflow)
    return _detect_field_calls(tree, filepath, symbol_table, dataflow)
```

- [ ] **Step 4: Add covered_callees to dataflow**

In `src/ethunter/analyzer/dataflow.py` DataflowEngine, add:

```python
covered_callees: set[str] = field(default_factory=set)
```

- [ ] **Step 5: Update orchestrator pipeline**

In `src/ethunter/analyzer/orchestrator.py`, restructure:

```python
# Phase 1a: Pre-scan for param->field registrations (unchanged)
for filepath, tree in trees.items():
    param_assign._register_phase(tree, filepath, symbol_table, engine)

# Phase 1a.5: field_call struct resolution (NEW)
for filepath, tree in trees.items():
    field_call._resolve_struct_fields(tree, filepath, symbol_table, engine)

# Phase 1: Target resolution (unchanged)
for filepath, tree in trees.items():
    for resolver in TARGET_RESOLVERS:
        resolver.analyze(tree=tree, filepath=filepath,
                         symbol_table=symbol_table, dataflow=engine)

# Phase 1.5: Build covered_callees (NEW)
covered_callees = set()
for key, vals in engine.targets.items():
    if key.startswith('<gstruct:'):
        covered_callees.update(vals)
engine.covered_callees = covered_callees

# Phase 1b: param_assign callback detection (unchanged)
for filepath, tree in trees.items():
    edges = param_assign.analyze(tree=tree, filepath=filepath,
                                 symbol_table=symbol_table, dataflow=engine)
    for edge in edges:
        graph.add_edge(edge)

# Phase 2: Call detection (modified: use _detect_field_calls)
for filepath, tree in trees.items():
    for detector in CALL_DETECTORS:
        if detector is field_call:
            edges = field_call._detect_field_calls(
                tree=tree, filepath=filepath,
                symbol_table=symbol_table, dataflow=engine)
        else:
            edges = detector.analyze(
                tree=tree, filepath=filepath,
                symbol_table=symbol_table, dataflow=engine)
        for edge in edges:
            graph.add_edge(edge)

# Remove Fix B section (covered_callees replaces it)
# DELETE the field_callees-based post-processing filter
```

- [ ] **Step 6: Add covered_callees check in param_assign**

In `src/ethunter/analyzer/param_assign.py`, at each callback_reg / callback_param emission point, add:

```python
# Before emitting callback_reg or callback_param edge:
if target in getattr(dataflow, 'covered_callees', set()):
    continue  # field_call will dispatch this callee via struct field
```

Apply this check in:
- Pass 1 `_is_registration` callback_reg branch (~line 377)
- Pass 3 `_detect_param_calls` emission (~line 640)
- Pass 4 `call_site_targets` emission (~line 650 area)

- [ ] **Step 7: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix5_covered_callees_suppress_callback -v`
Expected: PASS.

- [ ] **Step 8: Run full ET-Bench to verify no recall regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All recall gates 100%, FPR unchanged or slightly lower.

- [ ] **Step 9: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py src/ethunter/analyzer/field_call.py \
        src/ethunter/analyzer/param_assign.py src/ethunter/analyzer/dataflow.py \
        tests/test_et_bench.py
git commit -m "refactor: restructure pipeline for covered_callees pre-check (Fix 5)

Split field_call into _resolve_struct_fields (Phase 1a.5) and
_detect_field_calls (Phase 2). Build covered_callees at Phase 1.5
before param_assign emits callback edges. Remove Fix B post-processing.
param_assign now checks covered_callees before emitting callback_reg
and callback_param edges.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Fix 3 — Behavioral Registration Detection

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` (add param_usage in _register_phase, replace _is_registration)

- [ ] **Step 1: Write TDD test**

```python
def test_fix3_behavioral_registration():
    """Forwarder should not emit callback_reg; direct caller should."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
typedef void (*cb_t)(int);
static void my_cb(int x) { (void)x; }
static void direct_caller(cb_t cb) { cb(42); }
static void forwarder(cb_t cb) { direct_caller(cb); }
void setup(void) { forwarder(my_cb); }
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"): st.add_function(func)
    df = VariableState()
    graph = run_all_analyses({"test.c": tree}, st, df)
    cr = {e.callee for e in graph.edges if e.indirect_kind == "callback_reg"}
    # forwarder should NOT produce callback_reg for my_cb
    assert "my_cb" not in cr, f"forwarder should not emit callback_reg: {cr}"
```

- [ ] **Step 2: Run to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix3_behavioral_registration -v`
Expected: FAIL — `my_cb` IS in `cr`.

- [ ] **Step 3: Add param_usage analysis in _register_phase**

In `_register_phase`, after `_collect_func_params`, scan each function body for fnptr param usage:

```python
# In _register_phase, after func_fp_params collection:
param_usage: dict[tuple[str, int], str] = {}  # (func, idx) -> 'caller'|'forwarder'|'storage'

for func_name, fp_positions in func_fp_params.items():
    # Find the function definition and scan its body
    for pos in fp_positions:
        # Scan for storage: field = param (already tracked via param_fields)
        if (func_name, pos) in dataflow.param_fields:
            param_usage[(func_name, pos)] = 'storage'
        else:
            # Scan for caller: param(args) or (*param)(args)
            # Scan for forwarder: other_func(param)
            # (use tree-sitter AST walk)
            role = _classify_param_usage(tree.root_node, func_name, pos, func_params)
            param_usage[(func_name, pos)] = role

engine.param_usage = param_usage
```

- [ ] **Step 4: Replace _is_registration in callback_reg emission**

In `_collect_call_params`, replace the `_is_registration` check:

```python
# OLD:
if _is_registration(call_name):
    emit callback_reg

# NEW:
usage = getattr(dataflow, 'param_usage', {}).get((call_name, arg_idx), 'unknown')
if usage == 'caller':
    emit callback_reg  # direct fnptr caller
elif usage == 'unknown' and _is_registration(call_name):
    emit callback_reg  # fallback: conservative
# forwarder/storage: suppress
```

- [ ] **Step 5: Run test to verify it passes + full ET-Bench**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All recall gates 100%.

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "feat: behavioral registration detection replacing _is_registration (Fix 3)

Add param_usage classification in _register_phase (caller/forwarder/storage).
Replace _is_registration substring matching with param_usage lookup.
Forwarder and storage callbacks no longer emit callback_reg edges.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Fix 1 — Type+Name Dataflow Key

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py` (add `var_to_type` dict)
- Modify: `src/ethunter/analyzer/symbol_table.py` (add `record_var_type()`)
- Modify: `src/ethunter/analyzer/initializer_assign.py` (write `<gstruct:type.var.field>`)
- Modify: `src/ethunter/analyzer/param_assign.py` (`_propagate_call_site` use type.var.field)
- Modify: `src/ethunter/analyzer/field_call.py` (type-prefix suffix scan)

- [ ] **Step 1: Write TDD test**

```python
def test_fix1_type_aware_field_lookup():
    """Two different struct types with same field name: targets must not mix."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
typedef void (*fn_t)(void);
static void h_a(void) {}
static void h_b(void) {}
struct type_a { const char *n; fn_t handler; };
struct type_b { int id; fn_t handler; };
static struct type_a o1 = {"a", h_a};
static struct type_b o2 = {42, h_b};
void use_a(void) { if (o1.handler) o1.handler(); }
void use_b(void) { if (o2.handler) o2.handler(); }
'''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"): st.add_function(func)
    df = VariableState()
    graph = run_all_analyses({"test.c": tree}, st, df)
    fc = {(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "field_call"}
    assert ("use_a", "h_b") not in fc, f"type_a.handler should not resolve to h_b: {fc}"
    assert ("use_b", "h_a") not in fc, f"type_b.handler should not resolve to h_a: {fc}"
```

- [ ] **Step 2: Run to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix1_type_aware_field_lookup -v`
Expected: FAIL — `("use_a", "h_b")` IS in `fc`.

- [ ] **Step 3: Add var_to_type to dataflow and symbol_table**

`dataflow.py` DataflowEngine:
```python
var_to_type: dict[str, str] = field(default_factory=dict)
```

`symbol_table.py`:
```python
def record_var_type(self, var_name: str, struct_type: str) -> None:
    """Record that var_name is of struct_type."""
    self._var_types[var_name] = struct_type
```

- [ ] **Step 4: Update initializer_assign to write type.var.field key**

In `initializer_assign.py`, when writing `<gstruct:>` key, use `type.var.field` format:

```python
# OLD:
dataflow.assign(f'<gstruct:{field_path}>', target)

# NEW:
struct_type = resolve_struct_type(var_name)  # from AST declaration
if struct_type:
    key = f'<gstruct:{struct_type}.{field_path}>'
else:
    key = f'<gstruct:{field_path}>'  # fallback
dataflow.assign(key, target)
# Also write to var_to_type
if struct_type:
    dataflow.var_to_type[field_path.split('.')[0]] = struct_type
```

- [ ] **Step 5: Update param_assign _propagate_call_site**

In `resolve_call_site_param`, use type.var.field key:

```python
# Add struct_type prefix if var_to_type is known
base_var = field_key.split('.')[0]
struct_type = getattr(dataflow, 'var_to_type', {}).get(base_var, '')
if struct_type:
    field_key = f'<gstruct:{struct_type}.{field_key[9:]}'  # prepend type
```

- [ ] **Step 6: Update field_call suffix scan with type prefix**

In `_detect_field_calls`, replace suffix scan:

```python
# OLD:
for key, vals in dataflow.targets.items():
    if key.endswith(f'.{fieldname}>') and vals:
        targets.update(vals)

# NEW:
base_var = field_path.split('.')[0]
struct_type = getattr(dataflow, 'var_to_type', {}).get(base_var, '')
if struct_type:
    type_prefix = f'<gstruct:{struct_type}.'
    for key, vals in dataflow.targets.items():
        if key.startswith(type_prefix) and key.endswith(f'.{fieldname}>') and vals:
            targets.update(vals)
else:
    # Fallback: original wildcard scan
    for key, vals in dataflow.targets.items():
        if key.endswith(f'.{fieldname}>') and vals:
            targets.update(vals)
```

- [ ] **Step 7: Run test + full ET-Bench**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: All recall gates 100%. FPR significantly lower (field_call FPs reduced).

- [ ] **Step 8: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py src/ethunter/analyzer/symbol_table.py \
        src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/param_assign.py \
        src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: type+name composite dataflow key for field_call (Fix 1)

Dataflow <gstruct:> keys upgraded from var.field to type.var.field.
Suffix scan scoped by struct type prefix. Eliminates cross-type
field name collision. Reduces ~98 field_call false positives.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Fix 6 — chain_context Field

**Files:**
- Modify: `src/ethunter/graph/model.py` (add `chain_context` to CallEdge)

- [ ] **Step 1: Add chain_context to CallEdge**

In `model.py`:

```python
@dataclass
class CallEdge:
    caller: str
    callee: str
    caller_file: str
    callee_file: str
    type: CallType
    indirect_kind: str
    caller_line: int
    chain_context: str = ''  # NEW: 'immediate' | 'outer' | 'field_dispatch'
```

- [ ] **Step 2: Set chain_context in edge emission**

In `param_assign.py` Pass 3: set `chain_context='immediate'`
In `param_assign.py` Pass 4: set `chain_context='outer'`
In `field_call.py`: set `chain_context='field_dispatch'`

- [ ] **Step 3: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All pass.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/graph/model.py src/ethunter/analyzer/param_assign.py \
        src/ethunter/analyzer/field_call.py
git commit -m "feat: add chain_context field to CallEdge for viewpoint selection (Fix 6)

chain_context marks edge source: 'immediate' (Pass 3), 'outer' (Pass 4),
'field_dispatch' (field_call). Backward compatible (default empty string).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Final Verification & FPR Ceilings

- [ ] **Step 1: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`

- [ ] **Step 2: Update FPR ceilings**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -vs 2>&1 | grep -E "^fnptr"`

Update `fpr_ceilings` dict to match new values + 0.03 margin.

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: finalize FPR ceilings after architectural fixes

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
