# Local Function Pointer Tracker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Track local variables that inherit function pointer types from struct fields, enabling detection of indirect calls through those locals.

**Architecture:** New `local_fp_tracker.py` module collects `local = struct->field` and `local = struct.field` assignments, resolves dataflow keys, and returns a `{local_var: set[targets]}` mapping. `direct_call_fp.py` consumes this mapping and also gains `pointer_expression` call detection.

**Tech Stack:** Python, tree-sitter, pytest

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `tests/fixtures/local_fp_assign.c` | CREATE | Minimal fixture: `local = struct_ptr->field; local(args)` |
| `tests/fixtures/local_fp_deref_call.c` | CREATE | Minimal fixture: `local = struct.field; (*local)(args)` |
| `tests/test_analyzers.py` | MODIFY | Add unit tests for `local_fp_tracker` and updated `direct_call_fp` |
| `src/ethunter/analyzer/local_fp_tracker.py` | CREATE | New module: scan AST, return local→targets mapping |
| `src/ethunter/analyzer/direct_call_fp.py` | MODIFY | Integrate local mapping, handle `pointer_expression` calls |
| `tests/benchmark/et_bench/fnptr-struct/` | NO CHANGE | Existing ET-Bench fixtures validate end-to-end |

---

### Task 1: Test fixture — local variable from pointer field assignment

**Files:**
- Create: `tests/fixtures/local_fp_assign.c`

- [ ] **Step 1: Create the fixture file**

```c
/* Test fixture: local variable inherits function pointer from struct field */

struct ops {
    int (*compute)(int x);
};

static int double_it(int x) {
    return x * 2;
}

/* Global struct initializer — tracked by initializer_assign as <gstruct:global_ops.compute> */
static struct ops global_ops = {
    .compute = double_it,
};

void caller(void) {
    /* init_declarator with field_expression RHS */
    int (*fn)(int) = global_ops.compute;
    fn(42);
}

void caller_assign(void) {
    /* assignment_expression with field_expression RHS */
    int (*fn2)(int);
    fn2 = global_ops.compute;
    fn2(42);
}
```

This fixture creates both an `init_declarator` with a `field_expression` on the RHS (`fn`) and an `assignment_expression` with a `field_expression` RHS (`fn2`). `initializer_assign` records `<gstruct:global_ops.compute>` = `double_it`. `local_fp_tracker` resolves it and returns `{fn: {double_it}}`.

- [ ] **Step 2: Verify the file parses**

Run: `.venv/bin/python -c "from ethunter.parser.ast_builder import parse_file; print(parse_file('tests/fixtures/local_fp_assign.c'))"`

Expected: No error, tree object printed.

- [ ] **Step 3: Commit**

```bash
git add tests/fixtures/local_fp_assign.c
git commit -m "test: add fixture for local variable from struct field assignment"
```

---

### Task 2: Test fixture — pointer dereference call

**Files:**
- Create: `tests/fixtures/local_fp_deref_call.c`

- [ ] **Step 1: Create the fixture file**

```c
/* Test fixture: pointer dereference call through local variable */

struct handler {
    void (*process)(void);
};

static void default_process(void) {
    return;
}

/* Global struct initializer — tracked by initializer_assign as <gstruct:global_handler.process> */
static struct handler global_handler = {
    .process = default_process,
};

void caller(void) {
    /* init_declarator with field_expression RHS */
    void (*local)(void) = global_handler.process;
    (*local)();
}
```

This fixture creates a `call_expression` with `parenthesized_expression` → `pointer_expression` → `identifier`. `initializer_assign` records `<gstruct:global_handler.process>` = `default_process`. `local_fp_tracker` maps `local` → `{default_process}`. `direct_call_fp` unwraps `(*local)()` and resolves it.

- [ ] **Step 2: Verify the file parses**

Run: `.venv/bin/python -c "from ethunter.parser.ast_builder import parse_file; print(parse_file('tests/fixtures/local_fp_deref_call.c'))"`

Expected: No error, tree object printed.

- [ ] **Step 3: Commit**

```bash
git add tests/fixtures/local_fp_deref_call.c
git commit -m "test: add fixture for pointer dereference call through local variable"
```

---

### Task 3: Write failing unit tests

**Files:**
- Modify: `tests/test_analyzers.py`

- [ ] **Step 1: Add tests for `local_fp_tracker` module**

Append to `tests/test_analyzers.py`:

```python
# === Local Function Pointer Tracker tests ===

def test_local_fp_tracker_pointer_field_assignment():
    """Test: local = struct.field init_declarator resolves to function targets."""
    from ethunter.analyzer import initializer_assign, local_fp_tracker
    tree, st, df = _make_analyzer_env('local_fp_assign.c')
    # initializer_assign sets up <gstruct:global_ops.compute> → double_it
    initializer_assign.analyze(tree, 'local_fp_assign.c', st, df)
    # local_fp_tracker should find fn → double_it from the init_declarator
    mapping = local_fp_tracker.collect_local_fp_assignments(
        tree, df, st.all_function_names
    )
    assert 'fn' in mapping
    assert 'double_it' in mapping['fn']


def test_local_fp_tracker_deref_call():
    """Test: (*local)() call detection through direct_call_fp."""
    from ethunter.analyzer import initializer_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('local_fp_deref_call.c')
    initializer_assign.analyze(tree, 'local_fp_deref_call.c', st, df)
    edges = direct_call_fp.analyze(tree, 'local_fp_deref_call.c', st, df)
    callees = {e.callee for e in edges}
    assert 'default_process' in callees


def test_local_fp_tracker_assignment_expression():
    """Test: both init_declarator and assignment_expression patterns."""
    from ethunter.analyzer import initializer_assign, local_fp_tracker
    tree, st, df = _make_analyzer_env('local_fp_assign.c')
    initializer_assign.analyze(tree, 'local_fp_assign.c', st, df)
    mapping = local_fp_tracker.collect_local_fp_assignments(
        tree, df, st.all_function_names
    )
    # init_declarator pattern: fn = global_ops.compute
    assert 'fn' in mapping
    assert 'double_it' in mapping['fn']
    # assignment_expression pattern: fn2 = global_ops.compute
    assert 'fn2' in mapping
    assert 'double_it' in mapping['fn2']
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `.venv/bin/python -m pytest tests/test_analyzers.py::test_local_fp_tracker_pointer_field_assignment tests/test_analyzers.py::test_local_fp_tracker_deref_call -v`

Expected: FAIL — `ModuleNotFoundError: No module named 'ethunter.analyzer.local_fp_tracker'` for the first test. The second test will also fail with the same error because Task 5's `direct_call_fp.py` imports `local_fp_tracker`. The `test_direct_call_fp` existing test will also break from the import.

- [ ] **Step 3: Commit**

```bash
git add tests/test_analyzers.py
git commit -m "test: add failing tests for local_fp_tracker"
```

---

### Task 4: Implement `local_fp_tracker.py`

**Files:**
- Create: `src/ethunter/analyzer/local_fp_tracker.py`

- [ ] **Step 1: Create the module**

```python
"""Local variable function pointer tracking.

Tracks local variables that inherit function pointer types from struct field access:
- Type local = struct_ptr->field;
- Type local = struct_var.field;
- local = struct_ptr->field;
- local = struct_var.field;

Returns a mapping from local variable name to resolved function targets.
Not stored in VariableState — local variables are function-scoped.
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.helpers import extract_field_path


def collect_local_fp_assignments(
    tree: ts.Tree,
    dataflow: VariableState,
    symbol_names: set[str],
) -> dict[str, set[str]]:
    """Collect local variable assignments from struct field function pointers.

    Returns mapping from local variable name to set of resolved function targets.
    """
    mapping: dict[str, set[str]] = {}

    def _visit(node: ts.Node) -> None:
        # init_declarator: Type local = struct.field or Type local = struct_ptr->field
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            value = node.child_by_field_name('value')
            if declarator and value and value.type == 'field_expression':
                var_name = _extract_identifier(declarator)
                if var_name:
                    _resolve_and_store(var_name, value, mapping, dataflow)

        # assignment_expression: local = struct.field or local = struct_ptr->field
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left')
            rhs = node.child_by_field_name('right')
            if lhs and rhs and lhs.type == 'identifier' and rhs.type == 'field_expression':
                var_name = lhs.text.decode('utf-8')
                _resolve_and_store(var_name, rhs, mapping, dataflow)

        for child in node.children:
            _visit(child)

    def _extract_identifier(declarator: ts.Node) -> str | None:
        """Extract identifier from a declarator (handles pointer_declarator nesting)."""
        if declarator.type in ('identifier', 'field_identifier') and declarator.text:
            return declarator.text.decode('utf-8')
        if declarator.type == 'pointer_declarator':
            return _extract_identifier(declarator.children[-1])
        if declarator.type in ('parenthesized_declarator', 'function_declarator', 'array_declarator'):
            for c in declarator.children:
                if c.type not in ('(', ')'):
                    result = _extract_identifier(c)
                    if result:
                        return result
        return None

    def _resolve_and_store(
        var_name: str,
        field_expr: ts.Node,
        mapping: dict[str, set[str]],
        dataflow: VariableState,
    ) -> None:
        """Build dataflow key from field expression and resolve targets."""
        field_path = extract_field_path(field_expr)
        if not field_path:
            return
        # Try <gstruct:path> first (global struct field)
        targets = dataflow.resolve(f'<gstruct:{field_path}>')
        if not targets:
            # Try <struct:path> (from param_assign)
            targets = dataflow.resolve(f'<struct:{field_path}>')
        if not targets:
            # Try <chain:path> (complex chain)
            targets = dataflow.resolve(f'<chain:{field_path}>')
        if targets:
            if var_name not in mapping:
                mapping[var_name] = set()
            mapping[var_name].update(targets)

    _visit(tree.root_node)
    return mapping
```

- [ ] **Step 2: Run the first test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_analyzers.py::test_local_fp_tracker_pointer_field_assignment -v`

Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/local_fp_tracker.py
git commit -m "feat: add local_fp_tracker module for struct field → local variable tracking"
```

---

### Task 5: Update `direct_call_fp.py` to use local mapping and handle pointer expressions

**Files:**
- Modify: `src/ethunter/analyzer/direct_call_fp.py`

- [ ] **Step 1: Update the module**

Replace the entire `direct_call_fp.py` with:

```python
"""Direct identifier-based function pointer call detection.

Detects calls through function pointers identified by simple identifiers:
- fp() where fp has been assigned via dataflow
- fp() where fp is a local variable from a struct field assignment
- (*fp)() pointer dereference calls with the same resolution
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function
from ethunter.analyzer.local_fp_tracker import collect_local_fp_assignments


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through function pointer identifiers."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names
    local_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names)

    def _get_targets(var_name: str) -> set[str]:
        """Resolve function targets for a variable name.

        Checks in order:
        1. Direct dataflow assignment (e.g., fp = func)
        2. Local variable from struct field
        """
        targets = dataflow.resolve(var_name)
        if not targets:
            targets = local_mapping.get(var_name, set()).copy()
        return targets

    def _add_edges(func_name: str, call_node: ts.Node) -> None:
        """Add call edges for resolved targets."""
        targets = _get_targets(func_name)
        if targets:
            caller = find_enclosing_function(call_node, tree.root_node)
            for target in targets:
                edges.append(CallEdge(
                    caller=caller or '<unknown>',
                    callee=target,
                    caller_file=filepath,
                    callee_file='',
                    type=CallType.INDIRECT,
                    indirect_kind='direct_assign',
                    caller_line=call_node.start_point[0] + 1,
                ))

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
                var_name = func_node.text.decode('utf-8')
                _add_edges(var_name, node)
            elif func_node and func_node.type == 'parenthesized_expression':
                # Handle (*fp)(args) pattern
                inner = _unwrap_pointer(func_node)
                if inner and inner.type == 'identifier' and inner.text:
                    var_name = inner.text.decode('utf-8')
                    _add_edges(var_name, node)
        for child in node.children:
            _visit(child)

    def _unwrap_pointer(node: ts.Node) -> ts.Node | None:
        """Unwrap parenthesized_expression → pointer_expression to get inner identifier."""
        for c in node.children:
            if c.type == 'pointer_expression':
                for cc in c.children:
                    if cc.type == 'identifier':
                        return cc
        return None

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 2: Run all new tests to verify they pass**

Run: `.venv/bin/python -m pytest tests/test_analyzers.py::test_local_fp_tracker_pointer_field_assignment tests/test_analyzers.py::test_local_fp_tracker_deref_call -v`

Expected: Both PASS

- [ ] **Step 3: Run all existing tests to verify no regression**

Run: `.venv/bin/python -m pytest tests/test_analyzers.py -v`

Expected: All PASS, including the existing `test_direct_call_fp` test (which uses `fp_assign.c` with no struct field patterns — the new code falls back to `dataflow.resolve()` first so existing behavior is preserved).

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/direct_call_fp.py
git commit -m "feat: integrate local_fp_tracker and handle (*fp)() pointer expression calls"
```

---

### Task 6: ET-Bench verification

**Files:**
- No file changes — validation only

- [ ] **Step 1: Run ET-Bench to verify fnptr-struct recall improvement**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s 2>&1 | grep -A 20 "ET-Bench"`

Expected output:
- `fnptr-struct` recall ≥ 52.38% (11/21), specifically example_6 and example_13 should now show as matched
- All other categories unchanged from baseline

- [ ] **Step 2: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`

Expected: All tests pass

- [ ] **Step 3: Commit (no files to commit, just verify)**

No commit needed — this is validation only.
