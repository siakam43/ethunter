# fnptr-global-struct 场景召回率提升 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 提升 fnptr-global-struct 场景召回率从 14.7% 到 95%+，覆盖 struct 数组索引调用、运行时 struct 指针字段赋值、宏展开调用三个 gap。

**Architecture:** 在现有两阶段架构下修改 3 个文件：helpers.py 增强 field path 提取，initializer_assign.py 新增运行时赋值追踪，field_call.py 新增宏回退匹配。所有改动都是新增 fallback，不影响已有路径。

**Tech Stack:** Python 3.11, tree-sitter-c, pytest

---

### Task 1: helpers.py — extract_field_path 支持 subscript

**Files:**
- Modify: `src/ethunter/analyzer/helpers.py:58-81`（`extract_field_path` 函数）
- Test: `tests/test_analyzers.py`（新增 `test_field_call_subscript`）

当前 `extract_field_path` 已能处理 `c->funcs->read` 等链式访问，但对 `arr[i]->field` 这种 subscript+field 组合无能为力。需要增强 subscript_expression 的处理。

- [ ] **Step 1.1: 编写失败的测试**

在 `tests/test_analyzers.py` 中新增测试 `test_field_call_subscript`：

```python
def test_field_call_subscript():
    """Test arr[i]->field() pattern — extract_field_path should handle subscript."""
    from ethunter.analyzer import field_call
    from ethunter.analyzer.helpers import extract_field_path

    # Test extract_field_path with subscript
    # We'll verify via dataflow + field_call integration
    tree, st, df = _make_analyzer_env('field_call_subscript.c')
    from ethunter.analyzer import initializer_assign
    initializer_assign.analyze(tree, 'field_call_subscript.c', st, df)
    edges = field_call.analyze(tree, 'field_call_subscript.c', st, df)
    callees = {e.callee for e in edges}
    assert 'handler_a' in callees
    assert 'handler_b' in callees
```

- [ ] **Step 1.2: 创建 fixture `tests/fixtures/field_call_subscript.c`**

```c
/* Test fixture: field_call_subscript — struct array subscript field calls */
/* Tests: arr[i]->field() pattern */

typedef struct handler handler_t;
struct handler {
    void (*process)(void);
    void (*cleanup)(void);
};

void handler_a(void) {}
void handler_b(void) {}
void cleanup_a(void) {}
void cleanup_b(void) {}

handler_t handlers[] = {
    { handler_a, cleanup_a },
    { handler_b, cleanup_b },
};

void dispatch(int idx) {
    handlers[idx]->process();
    handlers[idx]->cleanup();
}
```

- [ ] **Step 1.3: 增强 `extract_field_path` 处理 subscript_expression**

在 `src/ethunter/analyzer/helpers.py` 的 `extract_field_path` 中，确保 subscript_expression 的 base 被正确提取。当前代码已经有 subscript 处理（lines 75-79），但需要增强支持 subscript+`->` 组合：

```python
def extract_field_path(node: ts.Node) -> str | None:
    """Recursively extract the full path string from a field_expression.

    Supports . and -> operators, chain access, and subscript expressions.
    Examples: c->funcs->read -> "c.funcs.read"
              obj.field -> "obj.field"
              arr[i].field -> "arr.field"
              arr[i]->field -> "arr.field"
              arr[i]->chain->field -> "arr.chain.field"
    """
    if node.type == 'field_expression':
        parts = []
        for child in node.children:
            if child.type in ('identifier', 'field_identifier') and child.text:
                parts.append(child.text.decode('utf-8'))
            elif child.type == 'field_expression':
                inner = extract_field_path(child)
                if inner:
                    parts.extend(inner.split('.'))
            elif child.type == 'subscript_expression' and child.children:
                # Handle arr[i].field or arr[i]->field -> extract arr name
                base = child.children[0]
                # base could be a simple identifier, or another field_expression
                if base.type in ('identifier', 'field_identifier') and base.text:
                    parts.append(base.text.decode('utf-8'))
                elif base.type == 'field_expression':
                    inner = extract_field_path(base)
                    if inner:
                        parts.extend(inner.split('.'))
        return '.'.join(parts) if parts else None
    return None
```

注意：当前代码（lines 75-79）已经处理了 subscript_expression 的 base identifier，但缺少对 `base.type == 'field_expression'` 的递归处理（如 `obj.arr[i]->field` 的链式场景）。

- [ ] **Step 1.4: 运行测试验证**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_field_call_subscript -v
```

预期：PASS

- [ ] **Step 1.5: 提交**

```bash
git add src/ethunter/analyzer/helpers.py tests/test_analyzers.py tests/fixtures/field_call_subscript.c
git commit -m "feat: extract_field_path handles subscript+field patterns (arr[i]->field)"
```

---

### Task 2: initializer_assign.py — 追踪 struct 指针字段赋值

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py`
- Test: `tests/test_analyzers.py`（新增 `test_initializer_assign_pointer_field`）

example_4 中函数通过 `vec->zvec_legacy_func = func` 注册回调，`vec` 指向 `zfs_ioc_vec` 数组。需要在 initializer_assign 中新增 assignment_expression 处理。

- [ ] **Step 2.1: 编写失败的测试**

在 `tests/test_analyzers.py` 中新增：

```python
def test_initializer_assign_pointer_field():
    """Test vec->field = func pattern — runtime struct pointer field assignment."""
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign_pointer_field.c')
    initializer_assign.analyze(tree, 'initializer_assign_pointer_field.c', st, df)
    # Verify that <gstruct:dispatch_table.process> is registered
    assert any('dispatch_table.process' in k for k in df.targets)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert 'handler_a' in all_targets
    assert 'handler_b' in all_targets
```

- [ ] **Step 2.2: 创建 fixture `tests/fixtures/initializer_assign_pointer_field.c`**

```c
/* Test fixture: initializer_assign_pointer_field — runtime struct pointer field assignment */
/* Tests: vec->field = func where vec = &global_array[i] */

typedef struct ops ops_t;
struct ops {
    void (*process)(void);
    void (*cleanup)(void);
};

void handler_a(void) {}
void handler_b(void) {}
void cleanup_a(void) {}
void cleanup_b(void) {}

static ops_t dispatch_table[2];

static void register_ops(int idx, void (*proc)(void), void (*cln)(void))
{
    ops_t *vec = &dispatch_table[idx];
    vec->process = proc;
    vec->cleanup = cln;
}

void init(void)
{
    register_ops(0, handler_a, cleanup_a);
    register_ops(1, handler_b, cleanup_b);
}
```

- [ ] **Step 2.3: 在 initializer_assign.py 中新增 `_track_pointer_field_assignments` 函数**

在 `initializer_assign.py` 的 `analyze` 函数中，在 `_visit(tree.root_node)` 之后，新增对 assignment_expression 的遍历处理：

```python
def _collect_pointer_resolutions(tree: ts.Tree) -> dict[str, str]:
    """Scan function bodies for ptr = &global_name[...] patterns.
    Returns mapping: local_var_name -> global_name
    """
    resolutions: dict[str, str] = {}

    def _visit(n: ts.Node) -> None:
        if n.type == 'assignment_expression':
            lhs = n.child_by_field_name('left') or (n.children[0] if n.children else None)
            rhs = n.child_by_field_name('right') or (n.children[-1] if n.children else None)
            if lhs and rhs and lhs.type == 'identifier' and lhs.text:
                var_name = lhs.text.decode('utf-8')
                # rhs: pointer_expression -> identifier (possibly subscript)
                if rhs.type == 'pointer_expression' and rhs.children:
                    inner = rhs.children[-1]
                    if inner.type == 'identifier' and inner.text:
                        global_name = inner.text.decode('utf-8')
                        resolutions[var_name] = global_name
                    elif inner.type == 'subscript_expression' and inner.children:
                        base = inner.children[0]
                        if base.type == 'identifier' and base.text:
                            global_name = base.text.decode('utf-8')
                            resolutions[var_name] = global_name
        for child in n.children:
            _visit(child)

    _visit(tree.root_node)
    return resolutions


def _track_pointer_field_assignments(
    tree: ts.Tree,
    filepath: str,
    dataflow: VariableState,
    symbol_names: set[str],
) -> None:
    """Track vec->field = func assignments, resolving vec to global array name."""
    resolutions = _collect_pointer_resolutions(tree)

    def _visit(n: ts.Node) -> None:
        if n.type == 'assignment_expression':
            lhs = n.child_by_field_name('left') or (n.children[0] if n.children else None)
            rhs = n.child_by_field_name('right') or (n.children[-1] if n.children else None)
            if lhs and lhs.type == 'field_expression' and rhs:
                # Extract variable name and field name from lhs
                parts = []
                for child in lhs.children:
                    if child.type in ('identifier', 'field_identifier') and child.text:
                        parts.append(child.text.decode('utf-8'))

                if len(parts) == 1:
                    var_name = parts[0]
                    # Extract function name from rhs
                    if rhs.type == 'identifier' and rhs.text:
                        target = rhs.text.decode('utf-8')
                        if target in symbol_names:
                            # Resolve local var to global name
                            resolved = resolutions.get(var_name, var_name)
                            field_name = None
                            # Find the field name from the field_expression
                            for child in lhs.children:
                                if child.type == 'field_identifier' and child.text:
                                    field_name = child.text.decode('utf-8')
                                    break
                            if field_name:
                                dataflow.assign(f'<gstruct:{resolved}.{field_name}>', target)
        for child in n.children:
            _visit(child)

    _visit(tree.root_node)
```

然后在 `analyze` 函数末尾调用 `_track_pointer_field_assignments(tree, filepath, dataflow, symbol_names)`。

- [ ] **Step 2.4: 运行测试验证**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_initializer_assign_pointer_field -v
```

预期：PASS

- [ ] **Step 2.5: 提交**

```bash
git add src/ethunter/analyzer/initializer_assign.py tests/test_analyzers.py tests/fixtures/initializer_assign_pointer_field.c
git commit -m "feat: track runtime struct pointer field assignments (vec->field = func)"
```

---

### Task 3: field_call.py — 宏展开调用回退

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py`
- Test: `tests/test_analyzers.py`（新增 `test_field_call_macro`）

example_9 中 `stream_read_tree(...)` 是宏，展开后等价于 `streamer_hooks.read_tree(...)`。需要在 field_call 中识别宏调用。

- [ ] **Step 3.1: 编写失败的测试**

```python
def test_field_call_macro():
    """Test macro-expanded field call — #define MACRO(...) obj.field(...)."""
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call_macro.c')
    initializer_assign.analyze(tree, 'field_call_macro.c', st, df)
    edges = field_call.analyze(tree, 'field_call_macro.c', st, df)
    callees = {e.callee for e in edges}
    assert 'target_handler' in callees
```

- [ ] **Step 3.2: 创建 fixture `tests/fixtures/field_call_macro.c`**

```c
/* Test fixture: field_call_macro — macro-expanded field call */
/* Tests: #define MACRO(...) obj.field(...) pattern */

struct hooks {
    void (*read)(int);
    void (*write)(int);
};

void target_handler(int fd) {}
void write_handler(int fd) {}

struct hooks my_hooks = {
    .read = target_handler,
    .write = write_handler,
};

#define hooks_read(fd) my_hooks.read(fd)

void process_input(int fd) {
    hooks_read(fd);
}
```

- [ ] **Step 3.3: 在 field_call.py 中新增宏回退逻辑**

在 `field_call.py` 的 `analyze` 函数中，新增宏收集和回退匹配：

在 `analyze` 函数内、`_visit` 定义之前，添加：

```python
def _collect_macros(tree: ts.Tree) -> dict[str, str]:
    """Collect preproc_def/preproc_function_def macros and their bodies.
    Returns mapping: macro_name -> macro_body_text
    """
    macros: dict[str, str] = {}

    def _scan(n: ts.Node) -> None:
        if n.type in ('preproc_def', 'preproc_function_def'):
            name_node = None
            body_parts = []
            for child in n.children:
                if child.type == 'identifier' and child.text and not name_node:
                    name_node = child
                elif child.type not in ('#', 'define', '(', ')'):
                    if child.text:
                        body_parts.append(child.text.decode('utf-8'))
            if name_node and name_node.text:
                macro_name = name_node.text.decode('utf-8')
                macros[macro_name] = ' '.join(body_parts)
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros


def _extract_field_path_from_macro_body(body: str) -> str | None:
    """Extract struct_var.field pattern from macro body text.
    e.g., 'my_hooks.read(fd)' -> 'my_hooks.read'
    """
    import re
    # Match identifier.identifier (with . or ->)
    match = re.search(r'(\w+)\s*(?:\.|->)\s*(\w+)', body)
    if match:
        return f'{match.group(1)}.{match.group(2)}'
    return None
```

然后在 `_visit` 函数中，在 `call_expression` 处理逻辑的最后（所有现有 fallback 之后，`for target in targets` 之前），新增宏回退：

```python
# Fallback: macro-expanded field call
if not targets and field_path is None:
    # This is a plain identifier call (not a field_expression)
    func_node = node.child_by_field_name('function') or node.children[0]
    if func_node.type == 'identifier' and func_node.text:
        call_name = func_node.text.decode('utf-8')
        if call_name in macro_map:
            macro_body = macro_map[call_name]
            resolved_path = _extract_field_path_from_macro_body(macro_body)
            if resolved_path:
                targets = dataflow.resolve(f'<gstruct:{resolved_path}>')
                if targets:
                    field_path = resolved_path
```

需要在 `analyze` 函数开头收集宏映射：`macro_map = _collect_macros(tree)`，并在 `_visit` 中使用它（通过 `nonlocal macro_map` 或将其作为闭包变量）。

- [ ] **Step 3.4: 运行测试验证**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_field_call_macro -v
```

预期：PASS

- [ ] **Step 3.5: 提交**

```bash
git add src/ethunter/analyzer/field_call.py tests/test_analyzers.py tests/fixtures/field_call_macro.c
git commit -m "feat: handle macro-expanded field calls in field_call"
```

---

### Task 4: 集成验证 — 全量测试 + et_bench

**Files:**
- No source changes

- [ ] **Step 4.1: 运行全量测试**

```bash
.venv/bin/python -m pytest tests/ -q
```

预期：全部 PASS（不能有任何回归）

- [ ] **Step 4.2: 运行 et_bench 验证 fnptr-global-struct 召回率**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py -v -s 2>&1 | grep "fnptr-global-struct"
```

预期：`fnptr-global-struct` recall >= 95%

- [ ] **Step 4.3: 逐例验证 example_4 和 example_9**

运行以下脚本确认：

```bash
PYTHONPATH=src .venv/bin/python -c "
import json, os
from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.orchestrator import run_all_analyses

for ex in ['example_4', 'example_9']:
    d = f'tests/benchmark/et_bench/fnptr-global-struct/{ex}'
    gt = json.load(open(os.path.join(d, 'ground_truth.json')))
    expected = gt['examples']
    trees, st, df = {}, SymbolTable(), VariableState()
    for r, _, fs in os.walk(d):
        for f in fs:
            if f.endswith(('.c', '.h')):
                p = os.path.join(r, f)
                t = parse_file(p); trees[p] = t
                for func in extract_functions(t, p): st.add_function(func)
    g = run_all_analyses(trees, st, df)
    ie = [e for e in g.edges if e.type.value == 'indirect']
    found = {(e.caller, e.callee) for e in ie}
    exp = {(e['caller'], e['callee']) for e in expected}
    m = found & exp
    print(f'{ex}: {len(m)}/{len(exp)} matched')
    missing = exp - found
    if missing:
        for c in sorted(missing):
            print(f'  MISSING: {c[0]} -> {c[1]}')
"
```

- [ ] **Step 4.4: 提交**

```bash
git commit --allow-empty -m "chore: verify fnptr-global-struct recall >= 95%"
```
