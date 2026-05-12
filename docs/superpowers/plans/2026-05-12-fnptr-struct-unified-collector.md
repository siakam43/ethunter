# fnptr-struct Unified Field Assignment Collector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Achieve 100% recall in fnptr-struct category by adding a unified field-assignment collector that handles both `assignment_expression` and `designated_initializer` forms, plus suffix matching in return-value field tracking.

**Architecture:** Add `collect_field_assignments()` generator to `helpers.py` that scans AST for all field-assignment patterns. Three consumers (`_register_phase`, `_visit`, `_collect_assignments`) iterate its results instead of their own duplicate AST traversals. Add suffix-based fallback in `DataflowEngine.resolve_returned_field` to handle variable-name mismatches.

**Tech Stack:** Python 3.11, tree-sitter, pytest

---

### Task 1: Add `FieldAssignment` and `collect_field_assignments` to `helpers.py`

**Files:**
- Modify: `src/ethunter/analyzer/helpers.py`

- [ ] **Step 1: Add the `FieldAssignment` namedtuple and `collect_field_assignments` function**

Add to end of `helpers.py`:

```python
from collections import namedtuple

FieldAssignment = namedtuple('FieldAssignment', [
    'field_path',       # str: e.g. "uic.uic_cb", "handler.finalizeResultEmission"
    'value_node',       # ts.Node: the rhs node (identifier, cast_expression, or call_expression)
    'resolved_value',   # str | None: unwrapped identifier text, None for call_expression rhs
    'form',             # str: 'assign' or 'designated_init'
    'enclosing_func',   # str | None: enclosing function name, None for global scope
    'line',             # int: source line number
])


def _unwrap_identifier(node: ts.Node, unwrap_fn=None) -> str | None:
    """Extract identifier text from a node, unwrapping cast expressions.

    Uses unwrap_fn (e.g., DataflowEngine.unwrap_cast) if available for
    robust multi-level cast unwrapping. Falls back to recursive scan.
    """
    if node.type == 'identifier' and node.text:
        return node.text.decode('utf-8')
    if node.type == 'cast_expression':
        if unwrap_fn:
            result = unwrap_fn(node)
            if result:
                return result
        # Fallback: find innermost identifier in reversed children
        for c in reversed(node.children):
            result = _unwrap_identifier(c, unwrap_fn)
            if result:
                return result
    return None


def collect_field_assignments(tree: ts.Tree, unwrap_fn=None) -> list[FieldAssignment]:
    """Collect all struct-field function pointer assignments from an AST.

    Handles two forms:
    1. assignment_expression: ptr->field = rhs
    2. designated_initializer: .field = val (inside init_declarator → initializer_list)

    Args:
        tree: tree-sitter parsed AST
        unwrap_fn: optional callable for nested cast extraction (e.g., DataflowEngine.unwrap_cast)

    Returns:
        list of FieldAssignment namedtuples
    """
    results: list[FieldAssignment] = []

    def _scan(node: ts.Node) -> None:
        # Form 1: assignment_expression (e.g., handler->field = func)
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or (node.children[0] if node.children else None)
            rhs = node.child_by_field_name('right') or (
                node.children[-1] if len(node.children) >= 2 else None
            )
            if lhs and rhs and lhs.type == 'field_expression':
                field_path = extract_field_path(lhs)
                if field_path:
                    enclosing_func = find_enclosing_function(node, tree.root_node)
                    resolved = _unwrap_identifier(rhs, unwrap_fn)
                    results.append(FieldAssignment(
                        field_path=field_path,
                        value_node=rhs,
                        resolved_value=resolved,
                        form='assign',
                        enclosing_func=enclosing_func,
                        line=node.start_point[0] + 1,
                    ))

        # Form 2: designated_initializer inside init_declarator
        # (e.g., struct s uic = { .uic_cb = (cast)cb })
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            init_list = node.child_by_field_name('value')
            if not init_list:
                for c in node.children:
                    if c.type == 'initializer_list':
                        init_list = c
                        break
            if declarator and init_list and init_list.type == 'initializer_list':
                var_name = extract_identifier_from_declarator(declarator)
                if var_name:
                    enclosing_func = find_enclosing_function(node, tree.root_node)
                    for child in init_list.children:
                        if child.type == 'initializer_pair':
                            _extract_pair(child, var_name, enclosing_func)

        for child in node.children:
            _scan(child)

    def _extract_pair(pair_node: ts.Node, var_name: str, enclosing_func: str | None) -> None:
        """Extract a single designated_initializer pair: .field = value.

        initializer_pair children are always [field_designator, '=', value],
        so the last child is the value node.
        """
        field_name = None
        for c in pair_node.children:
            if c.type == 'field_designator':
                for cc in c.children:
                    if cc.type == 'field_identifier' and cc.text:
                        field_name = cc.text.decode('utf-8')
        value = pair_node.children[-1] if pair_node.children else None
        if field_name and value:
            field_path = f'{var_name}.{field_name}'
            resolved = _unwrap_identifier(value, unwrap_fn)
            results.append(FieldAssignment(
                field_path=field_path,
                value_node=value,
                resolved_value=resolved,
                form='designated_init',
                enclosing_func=enclosing_func,
                line=pair_node.start_point[0] + 1,
            ))

    _scan(tree.root_node)
    return results
```

- [ ] **Step 2: Verify `helpers.py` imports are correct**

Ensure these imports exist at the top of `helpers.py`:

```python
# Already present:
from __future__ import annotations
import tree_sitter as ts

# Already present — verify:
# extract_field_path (line 58)
# find_enclosing_function (line 10)
# extract_identifier_from_declarator (line 43)
# find_child (line 35)
```

`collections.namedtuple` import is only needed in this file (no external import change).

- [ ] **Step 3: Run the existing tests to confirm no immediate breakage**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -q
```

Expected: 33 passed (collector not yet used, no functional change).

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/helpers.py
git commit -m "feat: add collect_field_assignments for unified field-assignment scanning"
```

---

### Task 2: Migrate `param_assign._register_phase` to use collector

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:16,121-158`

- [ ] **Step 0: Update top-level import**

In `param_assign.py` line 16, change:
```python
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path
```
to:
```python
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path, collect_field_assignments
```

- [ ] **Step 1: Replace `_scan_field_assigns` with collector iteration in `_register_phase`**

In `param_assign.py`, replace lines 140-158 (the `if hasattr(dataflow, 'register_param_mapping'):` block in `_register_phase`):

Old code (lines 140-158):
```python
    # Scan for field = param patterns -> register_param_mapping
    if hasattr(dataflow, 'register_param_mapping'):
        def _scan_field_assigns(node: ts.Node) -> None:
            if node.type == 'assignment_expression':
                lhs = node.child_by_field_name('left') or node.children[0]
                rhs = node.child_by_field_name('right') or node.children[1]
                if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                    param_name = rhs.text.decode('utf-8')
                    field_path = extract_field_path(lhs)
                    if field_path:
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        _try_register_param_to_field(
                            lhs, rhs, param_name, field_path,
                            enclosing_func, func_params, dataflow
                        )
            for child in node.children:
                _scan_field_assigns(child)

        _scan_field_assigns(tree.root_node)
```

New code:
```python
    # Scan for field = param patterns -> register_param_mapping
    if hasattr(dataflow, 'register_param_mapping'):
        for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
            # Only process assignments within function bodies
            if fa.enclosing_func is None or fa.enclosing_func not in func_params:
                continue
            if fa.resolved_value is None:
                continue
            params = func_params[fa.enclosing_func]
            if fa.resolved_value not in params:
                continue
            param_idx = params.index(fa.resolved_value)
            # Register — no struct-param gate (struct_param_idx was unused in resolution)
            dataflow.register_param_mapping(
                fa.enclosing_func, param_idx, fa.field_path
            )
```

- [ ] **Step 2: Remove `_try_register_param_to_field` function**

Remove lines 51-73 (the entire `_try_register_param_to_field` function definition).

- [ ] **Step 3: Verify `_extract_field_operand` stays (still used by return tracking)**

`_extract_field_operand` (lines 38-48) must NOT be removed. It is still used by return-value tracking:
  - `_register_phase._scan_returns._scan_body` (line 191): `operand = _extract_field_operand(c)`
  - `analyze._collect_returns._scan_returns._scan_body` (line 263): same pattern

Only `_try_register_param_to_field` is removed; `_extract_field_operand` stays.

- [ ] **Step 4: Run tests to verify no regression**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -q
```

Expected: 33 passed.

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py
git commit -m "refactor: migrate _register_phase field scanning to unified collector"
```

---

### Task 3: Migrate `param_assign.analyze._visit` to use collector

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py:355-393`

- [ ] **Step 1: Replace `_visit` field-assignment handling with collector iteration**

In `param_assign.py`, replace the `_visit` function (lines 355-393):

Old code (lines 355-393):
```python
    # === Pass 2: resolve struct member assignments ===
    def _visit(node: ts.Node) -> None:
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression':
                field_path = extract_field_path(lhs)
                if field_path:
                    # === Case A: RHS is identifier (existing + registration) ===
                    if rhs.type == 'identifier' and rhs.text:
                        param_name = rhs.text.decode('utf-8')
                        # EXISTING: resolve param to actual functions
                        targets = param_mappings.get(param_name, set())
                        for t in targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                        df_targets = dataflow.resolve(param_name)
                        if not df_targets:
                            df_targets = dataflow.resolve(f'<garray:{param_name}>')
                        for t in df_targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                            field_name = field_path.split('.')[-1]
                            dataflow.assign(f'<struct:{field_name}>', t)
                        # NEW: register for cross-function propagation
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        _try_register_param_to_field(
                            lhs, rhs, param_name, field_path,
                            enclosing_func, func_params, dataflow
                        )
                    # === Case B: RHS is call_expression (return value tracking) ===
                    elif rhs.type == 'call_expression':
                        call_func = rhs.child_by_field_name('function') or rhs.children[0]
                        if call_func and call_func.type == 'identifier' and call_func.text:
                            func_name = call_func.text.decode('utf-8')
                            if hasattr(dataflow, 'resolve_returned_field'):
                                ret_targets = dataflow.resolve_returned_field(func_name)
                                for t in ret_targets:
                                    dataflow.assign(f'<gstruct:{field_path}>', t)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
```

New code:
```python
    # === Pass 2: resolve struct member assignments ===
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        # Skip global scope — handled by initializer_assign
        if fa.enclosing_func is None:
            continue
        field_path = fa.field_path
        field_name = field_path.split('.')[-1]

        # Dispatch by value_node type
        if fa.value_node.type == 'call_expression':
            # === Case B: RHS is call_expression (return value tracking) ===
            call_func = fa.value_node.child_by_field_name('function') or fa.value_node.children[0]
            if call_func and call_func.type == 'identifier' and call_func.text:
                func_name = call_func.text.decode('utf-8')
                if hasattr(dataflow, 'resolve_returned_field'):
                    ret_targets = dataflow.resolve_returned_field(func_name)
                    for t in ret_targets:
                        dataflow.assign(f'<gstruct:{field_path}>', t)
        elif fa.resolved_value is not None:
            # === Case A: RHS is identifier or cast_expression ===
            param_name = fa.resolved_value
            # Prong 1: resolve via param_mappings (call-site arg propagation)
            targets = param_mappings.get(param_name, set())
            for t in targets:
                dataflow.assign(f'<struct:{field_path}>', t)
            # Prong 2: resolve via dataflow
            df_targets = dataflow.resolve(param_name)
            if not df_targets:
                df_targets = dataflow.resolve(f'<garray:{param_name}>')
            for t in df_targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                dataflow.assign(f'<struct:{field_name}>', t)
```

- [ ] **Step 2: Verify old `_extract_field_operand` import is no longer needed**

Check if `_extract_field_operand` is still referenced anywhere in `param_assign.py`. If the call in `_register_phase._scan_returns` still uses it, keep the function. Otherwise remove.

Note: `_register_phase._scan_returns` (lines 191-192) uses `_extract_field_operand` for the operand check in return statements — it is NOT part of `_try_register_param_to_field`. So `_extract_field_operand` must stay. (Only `_try_register_param_to_field` was removed in Task 2.)

- [ ] **Step 3: Run tests to verify no regression**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -q
```

Expected: 33 passed.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py
git commit -m "refactor: migrate param_assign._visit field scanning to unified collector"
```

---

### Task 4: Migrate `field_call._collect_assignments` to use collector

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:21,84-99`

- [ ] **Step 0: Update top-level import**

In `field_call.py` line 21, change:
```python
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path
```
to:
```python
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path, collect_field_assignments
```

- [ ] **Step 1: Replace `_collect_assignments` with collector iteration**

In `field_call.py`, replace the `_collect_assignments` function and its call (lines 84-99):

Old code (lines 84-99):
```python
    # Pass 1: collect all field assignments across the entire file
    def _collect_assignments(node: ts.Node) -> None:
        """Collect field = func_name assignments (extracted from the old _visit block)."""
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                target = rhs.text.decode('utf-8')
                if target in symbol_names:
                    field_path = extract_field_path(lhs)
                    if field_path:
                        dataflow.assign(f'<gstruct:{field_path}>', target)
        for child in node.children:
            _collect_assignments(child)

    _collect_assignments(tree.root_node)
```

New code:
```python
    # Pass 1: collect all field assignments across the entire file
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)
```

- [ ] **Step 2: Run tests to verify no regression**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -q
```

Expected: 33 passed.

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "refactor: migrate field_call._collect_assignments to unified collector"
```

---

### Task 5: Add suffix matching in `DataflowEngine.resolve_returned_field`

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py:141-150`

- [ ] **Step 1: Add suffix fallback after exact-match fails**

In `dataflow.py`, replace `resolve_returned_field` (lines 141-150):

Old code:
```python
    def resolve_returned_field(self, func_name: str) -> set[str]:
        """Resolve the targets of the field path that func_name returns."""
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            targets = self.state.resolve(f"<gstruct:{field_path}>")
            results.update(targets)
        return results
```

New code:
```python
    def resolve_returned_field(self, func_name: str) -> set[str]:
        """Resolve the targets of the field path that func_name returns.

        Tries exact <gstruct:{field_path}> lookup first, then falls back to
        suffix matching to handle variable-name mismatches (e.g., the field
        was stored under "ret" but the return references "ctx").
        """
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            targets = self.state.resolve(f"<gstruct:{field_path}>")
            if targets:
                results.update(targets)
                continue

            # Suffix fallback: field_path "ctx.cert.sec_cb" →
            # try "cert.sec_cb", then "sec_cb"
            parts = field_path.split('.')
            for i in range(1, len(parts)):
                suffix = '.'.join(parts[i:])
                for key, vals in self.state.targets.items():
                    if key.endswith(f'.{suffix}>') and vals:
                        results.update(vals)
                if results:
                    break

        return results
```

- [ ] **Step 2: Run relevant tests**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -q
```

Expected: 33 passed. (The suffix change only activates on exact-match fail, so existing passing tests are unaffected.)

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py
git commit -m "feat: add suffix matching in resolve_returned_field for variable-name mismatch"
```

---

### Task 6: Remove xfail from ET-Bench tests

**Files:**
- Modify: `tests/test_et_bench.py:157,166,178`

- [ ] **Step 1: Remove `@pytest.mark.xfail` decorators**

For `test_et_bench_fnptr_struct_example_9` (line 157): Remove the decorator and its reason string.

For `test_et_bench_fnptr_struct_example_5` (line 166): Remove the decorator and its reason string.

For `test_et_bench_fnptr_struct_full_recall` (line 178): Remove the decorator and its reason string.

- [ ] **Step 2: Run ET-Bench tests to verify 100% recall**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py -v
```

Expected: ALL 8 tests PASS (no XFAIL):

```
tests/test_et_bench.py::test_et_bench_report PASSED
tests/test_et_bench.py::test_et_bench_fnptr_struct_example_2 PASSED
tests/test_et_bench.py::test_et_bench_fnptr_struct_example_13 PASSED
tests/test_et_bench.py::test_et_bench_fnptr_struct_example_12 PASSED
tests/test_et_bench.py::test_et_bench_fnptr_struct_example_9 PASSED
tests/test_et_bench.py::test_et_bench_fnptr_struct_example_5 PASSED
tests/test_et_bench.py::test_et_bench_fnptr_struct_full_recall PASSED
tests/test_et_bench.py::test_cross_file_param_registration PASSED
```

- [ ] **Step 3: Run full test suite**

```bash
.venv/bin/python -m pytest tests/ -q
```

Expected: All tests pass, fnptr-struct category at 100% recall.

- [ ] **Step 4: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: remove xfail from fnptr-struct example_5, example_9, full_recall

fnptr-struct category now at 100% recall (21/21).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

### Task 7: Final verification

- [ ] **Step 1: Run the ET-Bench report to confirm recall**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

Expected output:
```
=== ET-Bench Recall Report ===
...
fnptr-struct                                21         21    100.00%
...
```

- [ ] **Step 2: Run complete test suite one final time**

```bash
.venv/bin/python -m pytest tests/ -q
```
