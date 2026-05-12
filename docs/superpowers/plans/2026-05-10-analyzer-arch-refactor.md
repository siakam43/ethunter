# Analyzer 架构重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重构 analyzer 模块为 Target Resolution / Call Detection 两阶段架构，ET-Bench 召回率从 73.26% 提升至 95%+。

**Architecture:** 将现有 13 个耦合模块拆分为 4 个 Target Resolution 解析器（按赋值语法分类）+ 3 个 Call Detection 检测器（按调用语法分类），由 orchestrator 按顺序调度。每个新模块配套独立 fixture 和测试，TDD-first 开发。

**Tech Stack:** Python, tree-sitter, pytest, ethunter 现有 dataflow/symbol_table 基础设施

**开发原则：** 先写 fixture + 测试，再实现功能。每个模块开发完成后立即运行测试验证。

---

## 文件映射总览

| 操作 | 文件 |
|---|---|
| **新增 src** | `src/ethunter/analyzer/initializer_assign.py` |
| **新增 src** | `src/ethunter/analyzer/cast_assign.py` |
| **新增 src** | `src/ethunter/analyzer/direct_assign.py` |
| **新增 src** | `src/ethunter/analyzer/param_assign.py` |
| **新增 src** | `src/ethunter/analyzer/direct_call_fp.py` |
| **新增 src** | `src/ethunter/analyzer/field_call.py` |
| **新增 src** | `src/ethunter/analyzer/array_call.py` |
| **新增 fixtures** | `tests/fixtures/initializer_assign.c` — designated initializer |
| **新增 fixtures** | `tests/fixtures/initializer_assign_complex.c` — 多 struct 数组初始化 |
| **新增 fixtures** | `tests/fixtures/cast_assign.c` — cast 赋值 |
| **新增 fixtures** | `tests/fixtures/cast_assign_complex.c` — 多 cast 模式 |
| **新增 fixtures** | `tests/fixtures/field_call.c` — 链式 `->` 访问 |
| **新增 fixtures** | `tests/fixtures/field_call_complex.c` — 复杂链式场景 |
| **新增 fixtures** | `tests/fixtures/param_assign.c` — struct 成员参数赋值 |
| **新增 fixtures** | `tests/fixtures/param_assign_complex.c` — 多参数传递场景 |
| **修改 src** | `src/ethunter/analyzer/helpers.py` — 新增 `extract_field_path()` |
| **修改 src** | `src/ethunter/analyzer/orchestrator.py` — 新调度逻辑 |
| **重写 tests** | `tests/test_analyzers.py` — 全量重写匹配新模块 |
| **重写 tests** | `tests/test_cross_file.py` — 全量重写匹配新模块 |
| **删除 src** | `src/ethunter/analyzer/fp_assign.py`, `fp_array.py`, `vtable.py`, `callback_param.py`, `callback_reg.py`, `typedef_fp.py`, `fp_alias.py`, `fp_return.py`, `lazy_init.py`, `union_fp.py`, `macro_fp.py` |
| **删除 fixtures** | `tests/fixtures/fp_return*.c`, `typedef_fp*.c`, `fp_alias*.c`, `lazy_init*.c`, `union_fp*.c`, `macro_fp*.c`, `macro_collision.c`, `ternary_fp.c` |
| **保留 fixtures** | `direct_call*.c`, `dlsym_fp*.c`, `fp_assign*.c` → 供 direct_assign 复用 |
| **保留 fixtures** | `fp_array*.c` → 供 initializer_assign + array_call 复用 |
| **保留 fixtures** | `vtable*.c` → 供 field_call 复用 |
| **保留 fixtures** | `callback_param*.c` → 供 param_assign 复用 |
| **保留 fixtures** | `callback_reg*.c` → 供 param_assign 复用 |
| **保留 fixtures** | `long_alias_chain.c` → 供 direct_assign 复用 |

---

### Task 0: 增强 helpers.py — 新增 `extract_field_path()`

**Files:**
- Modify: `src/ethunter/analyzer/helpers.py`

- [ ] **Step 1: 添加 `extract_field_path()` 函数**

在 `helpers.py` 末尾添加：

```python
def extract_field_path(node: ts.Node) -> str | None:
    """递归提取 field_expression 的完整路径字符串。
    
    支持 . 和 -> 运算符，以及链式访问。
    例如：c->funcs->read → "c.funcs.read"
         obj.field → "obj.field"
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
        return '.'.join(parts) if parts else None
    return None
```

- [ ] **Step 2: 验证导入**

```bash
.venv/bin/python -c "from ethunter.analyzer.helpers import extract_field_path; print('OK')"
```

Expected: `OK`

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/helpers.py
git commit -m "feat: add extract_field_path helper for chain field access"
```

---

### Task 1: TDD — `initializer_assign.py`（初始化器赋值）

**Files:**
- Create: `tests/fixtures/initializer_assign.c`
- Create: `tests/fixtures/initializer_assign_complex.c`
- Create: `src/ethunter/analyzer/initializer_assign.py`

覆盖 et_bench 场景：fnptr-global-struct、fnptr-global-array、fnptr-global-struct-array、fnptr-struct

- [ ] **Step 1: 创建 fixture — designated initializer**

```c
/* Test fixture: initializer_assign — designated initializer pattern */
/* Tests init_declarator + initializer_list + pair_list: struct s = { .field = func } */

struct ops {
    int (*init)(void);
    int (*read)(char *buf);
    void (*write)(const char *buf);
};

int fs_init(void) { return 0; }
int fs_read(char *buf) { return 0; }
void fs_write(const char *buf) {}

struct ops file_ops = {
    .init = fs_init,
    .read = fs_read,
    .write = fs_write,
};

int main(void) {
    file_ops.init();
    return 0;
}
```

- [ ] **Step 2: 创建 complex fixture — 多 struct + 数组初始化**

```c
/* Test fixture: initializer_assign complex — multiple structs and array init */

struct handler {
    void (*on_start)(void);
    void (*on_stop)(void);
};

void start_a(void) {}
void stop_a(void) {}
void start_b(void) {}
void stop_b(void) {}

struct handler handlers[] = {
    { start_a, stop_a },
    { start_b, stop_b },
};

struct ops {
    int (*create)(void);
    int (*destroy)(void);
};

int create_item(void) { return 0; }
int destroy_item(void) { return 0; }

struct ops item_ops = {
    .create = create_item,
    .destroy = destroy_item,
};

int main(void) {
    handlers[0].on_start();
    item_ops.create();
    return 0;
}
```

- [ ] **Step 3: 编写测试用例**

在 `tests/test_analyzers.py` 的 `_make_analyzer_env` helper 后添加：

```python
def test_initializer_assign_simple():
    """Test designated initializer: struct s = { .field = func }."""
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign.c')
    initializer_assign.analyze(tree, 'initializer_assign.c', st, df)
    # Verify dataflow was populated with gstruct keys
    assert any(k.startswith('<gstruct:') for k in df.targets), \
        f'Expected <gstruct:...> keys: {list(df.targets.keys())}'
    # Verify targets include fs_init, fs_read, fs_write
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert 'fs_init' in all_targets, f'Expected fs_init in targets: {all_targets}'
    assert 'fs_read' in all_targets, f'Expected fs_read in targets: {all_targets}'


def test_initializer_assign_complex():
    """Test multiple struct initializers + array initializer."""
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign_complex.c')
    initializer_assign.analyze(tree, 'initializer_assign_complex.c', st, df)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert len(all_targets) >= 4, f'Expected at least 4 targets: {all_targets}'
    assert 'start_a' in all_targets and 'start_b' in all_targets, \
        f'Expected handler targets: {all_targets}'
```

- [ ] **Step 4: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_initializer_assign_simple tests/test_analyzers.py::test_initializer_assign_complex -v
```

Expected: FAIL — module not found or no targets populated

- [ ] **Step 5: 实现 `initializer_assign.py`**

```python
"""Initializer-based function pointer assignment tracking.

Handles init_declarator with initializer_list patterns:
- Pure array: arr[] = { func_a, func_b } → key: <garray:arr>
- Designated initializer: s = { .field = func } → key: <gstruct:s.field>
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import extract_identifier_from_declarator


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list:
    """Track function pointer assignments via initializers."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _extract_field_name(pair_node: ts.Node) -> str | None:
        """Extract the field name from a pair node (.field = value)."""
        key = pair_node.child_by_field_name('key')
        if key:
            if key.type == 'field_designator' and key.text:
                return key.text.decode('utf-8').lstrip('.')
            if key.type == 'field_identifier' and key.text:
                return key.text.decode('utf-8')
        for c in pair_node.children:
            if c.type == 'field_designator' and c.text:
                return c.text.decode('utf-8').lstrip('.')
        return None

    def _process_init_list(init_list: ts.Node, var_name: str) -> None:
        """Process an initializer_list node."""
        if not init_list:
            return
        # Check for pair_list (designated initializer)
        for c in init_list.children:
            if c.type == 'pair_list':
                for pair in c.children:
                    if pair.type != 'pair':
                        continue
                    field_name = _extract_field_name(pair)
                    value = pair.child_by_field_name('value')
                    if not value:
                        value = pair.children[-1] if pair.children else None
                    if field_name and value and value.type == 'identifier' and value.text:
                        target = value.text.decode('utf-8')
                        if target in symbol_names:
                            dataflow.assign(f'<gstruct:{var_name}.{field_name}>', target)
                return
        # Pure identifier list: { func_a, func_b, ... }
        for c in init_list.children:
            if c.type == 'identifier' and c.text:
                name = c.text.decode('utf-8')
                if name in symbol_names:
                    dataflow.assign(f'<garray:{var_name}>', name)

    def _visit(node: ts.Node) -> None:
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            init_list = node.child_by_field_name('value')
            if not init_list:
                for c in node.children:
                    if c.type == 'initializer_list':
                        init_list = c
                        break
            if declarator and init_list:
                var_name = extract_identifier_from_declarator(declarator)
                if var_name:
                    _process_init_list(init_list, var_name)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 6: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_initializer_assign_simple tests/test_analyzers.py::test_initializer_assign_complex -v
```

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add tests/fixtures/initializer_assign.c tests/fixtures/initializer_assign_complex.c src/ethunter/analyzer/initializer_assign.py
git commit -m "feat: add initializer_assign target resolver with TDD fixtures"
```

---

### Task 2: TDD — `cast_assign.py`（类型转换赋值）

**Files:**
- Create: `tests/fixtures/cast_assign.c`
- Create: `tests/fixtures/cast_assign_complex.c`
- Create: `src/ethunter/analyzer/cast_assign.py`

覆盖 et_bench 场景：fnptr-cast

- [ ] **Step 1: 创建 fixture — 基本 cast 赋值**

```c
/* Test fixture: cast_assign — cast expression function pointer assignment */
/* Tests: fn_t *fp = (fn_t *)func_name */

typedef void (update_fn)(void *);

void update_impl(void *r) {}

update_fn *const fp_update = (update_fn *)update_impl;

int main(void) {
    int data = 42;
    fp_update(&data);
    return 0;
}
```

- [ ] **Step 2: 创建 complex fixture — 多 cast 模式**

```c
/* Test fixture: cast_assign complex — multiple cast patterns */

typedef int (*md5_init_func)(void *);
typedef int (*md5_update_func)(void *, const unsigned char *, unsigned int);

struct md5_params {
    md5_init_func init_func;
    md5_update_func update_func;
};

int my_md5_init(void *ctx) { return 0; }
int my_md5_update(void *ctx, const unsigned char *data, unsigned int len) { return 0; }

/* Cast in init_declarator */
md5_init_func g_init = (md5_init_func)my_md5_init;

/* Cast in assignment */
int main(void) {
    struct md5_params p;
    p.init_func = (md5_init_func)my_md5_init;
    p.update_func = (md5_update_func)my_md5_update;
    p.init_func(NULL);
    return 0;
}
```

- [ ] **Step 3: 编写测试用例**

```python
def test_cast_assign_simple():
    """Test cast expression assignment: fn_t *fp = (fn_t *)func."""
    from ethunter.analyzer import cast_assign
    tree, st, df = _make_analyzer_env('cast_assign.c')
    cast_assign.analyze(tree, 'cast_assign.c', st, df)
    assert 'fp_update' in df.targets, f'Expected fp_update in targets: {df.targets}'
    assert 'update_impl' in df.targets.get('fp_update', set()), \
        f'Expected update_impl as target: {df.targets.get("fp_update", set())}'


def test_cast_assign_complex():
    """Test multiple cast patterns: init + assignment."""
    from ethunter.analyzer import cast_assign
    tree, st, df = _make_analyzer_env('cast_assign_complex.c')
    cast_assign.analyze(tree, 'cast_assign_complex.c', st, df)
    assert 'g_init' in df.targets, f'Expected g_init in targets: {df.targets}'
    assert 'my_md5_init' in df.targets.get('g_init', set())
```

- [ ] **Step 4: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_cast_assign_simple tests/test_analyzers.py::test_cast_assign_complex -v
```

Expected: FAIL

- [ ] **Step 5: 实现 `cast_assign.py`**

```python
"""Cast-based function pointer assignment tracking.

Handles cast_expression patterns:
- Init: fn_t *fp = (fn_t *)func_name
- Assignment: fp = (fn_t *)func_name
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import extract_identifier_from_declarator


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list:
    """Track function pointer assignments via cast expressions."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _extract_cast_target(node: ts.Node) -> str | None:
        """Extract function name from inside a cast_expression."""
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
                if name in symbol_names:
                    return name
        return None

    def _visit(node: ts.Node) -> None:
        # init_declarator with cast: fn_t *fp = (type)func_name
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            value = node.child_by_field_name('value')
            if declarator and value:
                target = _extract_cast_target(value)
                if target:
                    var_name = extract_identifier_from_declarator(declarator)
                    if var_name:
                        dataflow.assign(var_name, target)

        # assignment_expression with cast: fp = (type)func_name
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'identifier' and lhs.text:
                target = _extract_cast_target(rhs)
                if target:
                    var_name = lhs.text.decode('utf-8')
                    dataflow.assign(var_name, target)

        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 6: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_cast_assign_simple tests/test_analyzers.py::test_cast_assign_complex -v
```

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add tests/fixtures/cast_assign.c tests/fixtures/cast_assign_complex.c src/ethunter/analyzer/cast_assign.py
git commit -m "feat: add cast_assign target resolver with TDD fixtures"
```

---

### Task 3: TDD — `direct_assign.py`（直接赋值）

**Files:**
- Create: `src/ethunter/analyzer/direct_assign.py`

复用现有 fixture：`fp_assign.c`（基本赋值）、`long_alias_chain.c`（别名链）、`fp_assign_complex.c`（条件赋值）

覆盖 et_bench 场景：fnptr-only、fnptr-varargs

- [ ] **Step 1: 编写测试用例（使用现有 fixture）**

```python
def test_direct_assign_simple():
    """Test direct assignment: void (*fp)(void) = foo; fp = bar;"""
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('fp_assign.c')
    direct_assign.analyze(tree, 'fp_assign.c', st, df)
    assert len(df.targets) > 0, 'direct_assign should populate dataflow targets'
    # fp_assign.c has: void (*fp)(void) = foo; fp = bar;
    assert 'fp' in df.targets
    assert 'foo' in df.targets['fp'] or 'bar' in df.targets['fp']


def test_direct_assign_alias_chain():
    """Test alias chain: fp2 = fp1 → fp1's targets propagate to fp2."""
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    direct_assign.analyze(tree, 'long_alias_chain.c', st, df)
    # long_alias_chain.c: fp1 = target_func; fp2 = fp1; fp3 = fp2; fp4 = fp3
    assert 'fp1' in df.targets
    assert 'target_func' in df.targets['fp1']
    # fp4 should inherit target_func through the chain
    assert 'fp4' in df.targets, f'Expected fp4 in targets: {df.targets}'
    assert 'target_func' in df.targets.get('fp4', set())
```

- [ ] **Step 2: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_direct_assign_simple tests/test_analyzers.py::test_direct_assign_alias_chain -v
```

Expected: FAIL

- [ ] **Step 3: 实现 `direct_assign.py`**

```python
"""Direct function pointer assignment tracking.

Handles simple assignment patterns:
- fp = func_name
- void (*fp)(void) = func_name
- fp2 = fp1 (alias chain)
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import extract_identifier_from_declarator


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list:
    """Track direct function pointer assignments."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # assignment_expression: fp = func_name
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if not lhs or not rhs:
                return
            if lhs.type == 'identifier' and lhs.text:
                var_name = lhs.text.decode('utf-8')
                if rhs.type == 'identifier' and rhs.text:
                    target = rhs.text.decode('utf-8')
                    if target in symbol_names:
                        dataflow.assign(var_name, target)
                    else:
                        # Alias chain: fp2 = fp1
                        targets = dataflow.resolve(target)
                        if targets:
                            for t in targets:
                                dataflow.assign(var_name, t)

        # init_declarator: void (*fp)(void) = func_name
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            value = node.child_by_field_name('value')
            if not declarator or not value:
                return
            if value.type == 'identifier' and value.text:
                target = value.text.decode('utf-8')
                var_name = extract_identifier_from_declarator(declarator)
                if var_name:
                    if target in symbol_names:
                        dataflow.assign(var_name, target)
                    else:
                        targets = dataflow.resolve(target)
                        if targets:
                            for t in targets:
                                dataflow.assign(var_name, t)

        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 4: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_direct_assign_simple tests/test_analyzers.py::test_direct_assign_alias_chain -v
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/direct_assign.py
git commit -m "feat: add direct_assign target resolver with TDD"
```

---

### Task 4: TDD — `param_assign.py`（参数传递）

**Files:**
- Create: `tests/fixtures/param_assign.c`
- Create: `tests/fixtures/param_assign_complex.c`
- Create: `src/ethunter/analyzer/param_assign.py`

覆盖 et_bench 场景：fnptr-callback、fnptr-library

- [ ] **Step 1: 创建 fixture — struct 成员参数赋值**

```c
/* Test fixture: param_assign — parameter stored in struct member */
/* Tests: handler.cb = param  where param was passed as function argument */

typedef void (*callback_t)(int);

struct handler {
    callback_t cb;
};

void my_handler(int x) {}

void setup(struct handler *h, callback_t cb) {
    h->cb = cb;
}

int main(void) {
    struct handler h;
    setup(&h, my_handler);
    h.cb(42);
    return 0;
}
```

- [ ] **Step 2: 创建 complex fixture — 多参数传递 + 回调注册**

```c
/* Test fixture: param_assign complex — multiple callback parameters */

typedef void (*event_cb)(int);

void register_callback(event_cb cb) {}

void on_start(int code) {}
void on_stop(int code) {}

typedef void (*process_fn)(void *);

void execute(process_fn fn, void *data) {
    fn(data);
}

void worker(void *d) {}

int main(void) {
    register_callback(on_start);
    register_callback(on_stop);
    execute(worker, NULL);
    return 0;
}
```

- [ ] **Step 3: 编写测试用例**

```python
def test_param_assign_simple():
    """Test parameter stored in struct member: handler.cb = param."""
    from ethunter.analyzer import param_assign
    tree, st, df = _make_analyzer_env('param_assign.c')
    edges = param_assign.analyze(tree, 'param_assign.c', st, df)
    # Should track my_handler being passed to setup() and stored in h->cb
    assert len(df.targets) > 0, 'param_assign should populate dataflow'
    # Should have <struct:...> keys from struct member assignment
    assert any(k.startswith('<struct:') for k in df.targets), \
        f'Expected <struct:...> keys: {list(df.targets.keys())}'


def test_param_assign_complex():
    """Test callback registration + multiple parameter passing."""
    from ethunter.analyzer import param_assign
    tree, st, df = _make_analyzer_env('param_assign_complex.c')
    edges = param_assign.analyze(tree, 'param_assign_complex.c', st, df)
    # Should find on_start and on_stop via callback registration
    callees = {e.callee for e in edges}
    assert 'on_start' in callees or 'on_stop' in callees, \
        f'Expected registered callbacks: {callees}'
    assert len(df.targets) > 0, 'param_assign should populate dataflow'
```

- [ ] **Step 4: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_param_assign_simple tests/test_analyzers.py::test_param_assign_complex -v
```

Expected: FAIL

- [ ] **Step 5: 实现 `param_assign.py`**

```python
"""Parameter-based function pointer tracking.

Handles function pointer parameter passing:
- void fn(void (*cb)(void)) + fn(callback_func)
- Callback registration functions (register/hook/attach/subscribe)
- Parameter stored in struct field: handler.cb = param
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path

REG_PATTERNS = ['register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_']


def _is_registration(name: str) -> bool:
    lower = name.lower()
    return any(p in lower for p in REG_PATTERNS)


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Track function pointer parameters and their propagation."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # Track call expressions: fn(callback_func)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)
                    for arg in args.children:
                        if arg.type == 'identifier' and arg.text:
                            target = arg.text.decode('utf-8')
                            if target in symbol_names:
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

        # Track assignment to struct member: obj.field = param
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and rhs.type == 'identifier' and rhs.text:
                param_name = rhs.text.decode('utf-8')
                if lhs.type == 'field_expression':
                    field_path = extract_field_path(lhs)
                    if field_path:
                        if param_name in symbol_names:
                            dataflow.assign(f'<struct:{field_path}>', param_name)
                        targets = dataflow.resolve(param_name)
                        if targets:
                            for t in targets:
                                dataflow.assign(f'<struct:{field_path}>', t)

        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 6: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_param_assign_simple tests/test_analyzers.py::test_param_assign_complex -v
```

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add tests/fixtures/param_assign.c tests/fixtures/param_assign_complex.c src/ethunter/analyzer/param_assign.py
git commit -m "feat: add param_assign target resolver with TDD fixtures"
```

---

### Task 5: TDD — `direct_call_fp.py`（直接标识符调用检测）

**Files:**
- Create: `src/ethunter/analyzer/direct_call_fp.py`

复用现有 fixture：`fp_assign.c`（dataflow 由 direct_assign 填充）

- [ ] **Step 1: 编写测试用例（使用现有 fixture）**

```python
def test_direct_call_fp():
    """Test indirect call detection: fp() after direct_assign populated dataflow."""
    from ethunter.analyzer import direct_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('fp_assign.c')
    # Phase 1: populate dataflow
    direct_assign.analyze(tree, 'fp_assign.c', st, df)
    # Phase 2: detect calls
    edges = direct_call_fp.analyze(tree, 'fp_assign.c', st, df)
    callee_names = {e.callee for e in edges}
    assert callee_names, 'direct_call_fp should find indirect call targets'
    # fp_assign.c calls fp() which points to foo and bar
    assert 'foo' in callee_names, f'Expected foo in callees: {callee_names}'


def test_direct_call_fp_alias_chain():
    """Test call detection through alias chain."""
    from ethunter.analyzer import direct_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    direct_assign.analyze(tree, 'long_alias_chain.c', st, df)
    edges = direct_call_fp.analyze(tree, 'long_alias_chain.c', st, df)
    callees = {e.callee for e in edges}
    assert 'target_func' in callees, f'Expected target_func: {callees}'
```

- [ ] **Step 2: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_direct_call_fp tests/test_analyzers.py::test_direct_call_fp_alias_chain -v
```

Expected: FAIL

- [ ] **Step 3: 实现 `direct_call_fp.py`**

```python
"""Direct identifier-based function pointer call detection.

Detects calls through function pointers identified by simple identifiers:
- fp() where fp has been assigned via dataflow
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through function pointer identifiers."""
    edges: list[CallEdge] = []

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
                var_name = func_node.text.decode('utf-8')
                targets = dataflow.resolve(var_name)
                if targets:
                    caller = find_enclosing_function(node, tree.root_node)
                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='direct_assign',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 4: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_direct_call_fp tests/test_analyzers.py::test_direct_call_fp_alias_chain -v
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/direct_call_fp.py
git commit -m "feat: add direct_call_fp call detector with TDD"
```

---

### Task 6: TDD — `field_call.py`（字段访问调用检测）

**Files:**
- Create: `tests/fixtures/field_call.c`
- Create: `tests/fixtures/field_call_complex.c`
- Create: `src/ethunter/analyzer/field_call.py`

覆盖 et_bench 场景：fnptr-global-struct、fnptr-struct、fnptr-library

- [ ] **Step 1: 创建 fixture — 基本 field_expression 调用**

```c
/* Test fixture: field_call — struct field expression calls */
/* Tests: obj.field() and ptr->field() */

struct driver {
    int (*init)(void);
    int (*read)(char *buf);
};

int fs_init(void) { return 0; }
int fs_read(char *buf) { return 0; }

int main(void) {
    struct driver d;
    d.init = fs_init;
    d.read = fs_read;
    d.init();
    d.read(NULL);
    return 0;
}
```

- [ ] **Step 2: 创建 complex fixture — 链式 `->` 访问**

```c
/* Test fixture: field_call complex — chain pointer access */
/* Tests: c->funcs->read()  (multi-level chain) */

struct funcs {
    void (*read)(void *c, char *buf, int len);
    void (*write)(void *c, const char *buf, int len);
};

struct context {
    struct funcs *funcs;
};

void net_read(void *c, char *buf, int len) {}
void net_write(void *c, const char *buf, int len) {}

struct funcs default_funcs = {
    .read = net_read,
    .write = net_write,
};

int main(void) {
    struct context ctx;
    ctx.funcs = &default_funcs;
    ctx.funcs->read(&ctx, NULL, 0);
    ctx.funcs->write(&ctx, NULL, 0);
    return 0;
}
```

- [ ] **Step 3: 编写测试用例**

```python
def test_field_call_simple():
    """Test field expression call: obj.field() after initializer_assign."""
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call.c')
    initializer_assign.analyze(tree, 'field_call.c', st, df)
    edges = field_call.analyze(tree, 'field_call.c', st, df)
    callees = {e.callee for e in edges}
    assert 'fs_init' in callees, f'Expected fs_init: {callees}'
    assert 'fs_read' in callees, f'Expected fs_read: {callees}'


def test_field_call_chain():
    """Test chain pointer access: c->funcs->field() after initializer_assign."""
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call_complex.c')
    initializer_assign.analyze(tree, 'field_call_complex.c', st, df)
    edges = field_call.analyze(tree, 'field_call_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'net_read' in callees, f'Expected net_read: {callees}'
    assert 'net_write' in callees, f'Expected net_write: {callees}'
```

- [ ] **Step 4: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_field_call_simple tests/test_analyzers.py::test_field_call_chain -v
```

Expected: FAIL

- [ ] **Step 5: 实现 `field_call.py`**

```python
"""Field-expression-based function pointer call detection.

Detects calls through struct field access:
- obj.field()
- ptr->field()
- ptr->chain->field()  (chain access)
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through struct field expressions."""
    edges: list[CallEdge] = []

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'field_expression':
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(func_node)
                if field_path:
                    # Try <gstruct:path> first (from initializer_assign)
                    targets = dataflow.resolve(f'<gstruct:{field_path}>')
                    # Try <struct:path> (from param_assign struct member)
                    if not targets:
                        targets = dataflow.resolve(f'<struct:{field_path}>')
                    # Try <chain:path> for complex chain
                    if not targets:
                        targets = dataflow.resolve(f'<chain:{field_path}>')
                    # Fallback: try last component alone
                    if not targets:
                        last_part = field_path.split('.')[-1]
                        targets = dataflow.resolve(last_part)

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='field_call',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 6: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_field_call_simple tests/test_analyzers.py::test_field_call_chain -v
```

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add tests/fixtures/field_call.c tests/fixtures/field_call_complex.c src/ethunter/analyzer/field_call.py
git commit -m "feat: add field_call call detector with TDD fixtures"
```

---

### Task 7: TDD — `array_call.py`（数组下标调用检测）

**Files:**
- Create: `src/ethunter/analyzer/array_call.py`

复用现有 fixture：`fp_array.c`

- [ ] **Step 1: 编写测试用例（使用现有 fixture）**

```python
def test_array_call():
    """Test array subscript call: arr[i]() after initializer_assign."""
    from ethunter.analyzer import initializer_assign, array_call
    tree, st, df = _make_analyzer_env('fp_array.c')
    initializer_assign.analyze(tree, 'fp_array.c', st, df)
    edges = array_call.analyze(tree, 'fp_array.c', st, df)
    callees = {e.callee for e in edges}
    assert callees, 'array_call should find indirect targets'
    # fp_array.c: dispatch[] = { cmd_help, cmd_quit, cmd_list }; dispatch[0]()
    assert 'cmd_help' in callees, f'Expected cmd_help: {callees}'


def test_array_call_complex():
    """Test multiple dispatch table calls."""
    from ethunter.analyzer import initializer_assign, array_call
    tree, st, df = _make_analyzer_env('fp_array_complex.c')
    initializer_assign.analyze(tree, 'fp_array_complex.c', st, df)
    edges = array_call.analyze(tree, 'fp_array_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert any('cmd' in c.lower() for c in callees), f'Expected cmd targets: {callees}'
```

- [ ] **Step 2: 运行测试验证失败（TDD）**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_array_call tests/test_analyzers.py::test_array_call_complex -v
```

Expected: FAIL

- [ ] **Step 3: 实现 `array_call.py`**

```python
"""Subscript-expression-based function pointer call detection.

Detects calls through array indexing:
- arr[i]()
- structs[i].field()
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through array subscript expressions."""
    edges: list[CallEdge] = []

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'subscript_expression':
                caller = find_enclosing_function(node, tree.root_node)
                arr_node = func_node.children[0] if func_node.children else None
                if arr_node and arr_node.text:
                    arr_name = arr_node.text.decode('utf-8')
                    # Try <garray:name> first
                    targets = dataflow.resolve(f'<garray:{arr_name}>')
                    # Fallback: bare array name
                    if not targets:
                        targets = dataflow.resolve(arr_name)
                    # Backward compat
                    if not targets:
                        targets = dataflow.resolve('<initializer>')

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='array_call',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 4: 运行测试验证通过**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py::test_array_call tests/test_analyzers.py::test_array_call_complex -v
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/array_call.py
git commit -m "feat: add array_call call detector with TDD"
```

---

### Task 8: 整合 — orchestrator.py + 删除旧模块 + 全量测试

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py`
- Modify: `tests/test_analyzers.py`（整合为完整文件）
- Modify: `tests/test_cross_file.py`
- Delete: 11 个旧 src 文件
- Delete: 8 个旧 fixture 文件

- [ ] **Step 1: 重写 orchestrator.py**

```python
"""Orchestrator: runs all analyzer modules and merges results into a single CallGraph."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallGraph, CallType, CallEdge
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer import (
    direct_call,
    dlsym_fp,
)
from ethunter.analyzer import (
    direct_assign,
    initializer_assign,
    cast_assign,
    param_assign,
)
from ethunter.analyzer import (
    direct_call_fp,
    field_call,
    array_call,
)

TARGET_RESOLVERS = [
    direct_assign,
    initializer_assign,
    cast_assign,
    param_assign,
]

CALL_DETECTORS = [
    direct_call_fp,
    field_call,
    array_call,
]


def run_all_analyses(
    trees: dict[str, ts.Tree],
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> CallGraph:
    """Run all analyzer modules on the parsed trees and build the CallGraph."""
    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    # Add all functions to the graph
    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # Phase 1: Target resolution (writes to dataflow)
    for filepath, tree in trees.items():
        for resolver in TARGET_RESOLVERS:
            resolver.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=dataflow,
            )

    # Phase 2: Call detection (reads from dataflow)
    for filepath, tree in trees.items():
        for detector in CALL_DETECTORS:
            edges = detector.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=dataflow,
            )
            for edge in edges:
                graph.add_edge(edge)

    # dlsym_fp (independent)
    for filepath, tree in trees.items():
        edges = dlsym_fp.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=dataflow,
        )
        for edge in edges:
            graph.add_edge(edge)

    # Deduplicate
    edge_map: dict[tuple[str, str], dict] = {}
    for edge in graph.edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge.to_dict()
        else:
            existing = edge_map[key]
            if existing.get('type') == 'indirect' and edge.type == CallType.DIRECT:
                edge_map[key] = edge.to_dict()

    graph.edges = [CallEdge(
        caller=d['caller'],
        callee=d['callee'],
        caller_file=d.get('caller_file', ''),
        callee_file=d.get('callee_file', ''),
        type=CallType(d.get('type', 'direct')),
        indirect_kind=d.get('indirect_kind', ''),
        caller_line=d.get('caller_line', 0),
    ) for d in edge_map.values()]

    return graph
```

- [ ] **Step 2: 验证 orchestrator 导入**

```bash
.venv/bin/python -c "from ethunter.analyzer.orchestrator import run_all_analyses; print('OK')"
```

Expected: `OK`

- [ ] **Step 3: 整合完整 test_analyzers.py**

```python
"""Tests for all analyzer modules (new architecture)."""

import os
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.direct_call import analyze as direct_analyze
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState

FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures')


def _make_analyzer_env(fixture_name):
    """Create symbol_table + dataflow for a fixture file."""
    path = os.path.join(FIXTURES, fixture_name)
    tree = parse_file(path)
    st = SymbolTable()
    for func in extract_functions(tree, fixture_name):
        st.add_function(func)
    df = VariableState()
    return tree, st, df


# === Core tests ===

def test_direct_call_simple():
    tree, st, _ = _make_analyzer_env('direct_call.c')
    edges = direct_analyze(tree, 'direct_call.c', st.all_function_names)
    edge_pairs = {(e.caller, e.callee) for e in edges}
    assert ('worker', 'helper') in edge_pairs
    assert ('main', 'worker') in edge_pairs
    assert ('main', 'helper') in edge_pairs


def test_direct_call_complex():
    tree, st, _ = _make_analyzer_env('direct_call_complex.c')
    edges = direct_analyze(tree, 'direct_call_complex.c', st.all_function_names)
    callees_of_top = {e.callee for e in edges if e.caller == 'top'}
    assert callees_of_top >= {'middle_two', 'leaf_a', 'leaf_b'}
    callees_of_middle_one = {e.callee for e in edges if e.caller == 'middle_one'}
    assert callees_of_middle_one >= {'leaf_a', 'leaf_b'}


# === Target Resolution tests ===

def test_direct_assign_simple():
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('fp_assign.c')
    direct_assign.analyze(tree, 'fp_assign.c', st, df)
    assert len(df.targets) > 0
    assert 'fp' in df.targets


def test_direct_assign_alias_chain():
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    direct_assign.analyze(tree, 'long_alias_chain.c', st, df)
    assert 'fp1' in df.targets
    assert 'target_func' in df.targets['fp1']
    assert 'fp4' in df.targets
    assert 'target_func' in df.targets.get('fp4', set())


def test_direct_assign_complex():
    from ethunter.analyzer import direct_assign
    tree, st, df = _make_analyzer_env('fp_assign_complex.c')
    direct_assign.analyze(tree, 'fp_assign_complex.c', st, df)
    assert len(df.targets) >= 2


def test_initializer_assign_simple():
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign.c')
    initializer_assign.analyze(tree, 'initializer_assign.c', st, df)
    assert any(k.startswith('<gstruct:') for k in df.targets)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert 'fs_init' in all_targets
    assert 'fs_read' in all_targets


def test_initializer_assign_complex():
    from ethunter.analyzer import initializer_assign
    tree, st, df = _make_analyzer_env('initializer_assign_complex.c')
    initializer_assign.analyze(tree, 'initializer_assign_complex.c', st, df)
    all_targets = set()
    for targets in df.targets.values():
        all_targets.update(targets)
    assert len(all_targets) >= 4
    assert 'start_a' in all_targets and 'start_b' in all_targets


def test_cast_assign_simple():
    from ethunter.analyzer import cast_assign
    tree, st, df = _make_analyzer_env('cast_assign.c')
    cast_assign.analyze(tree, 'cast_assign.c', st, df)
    assert 'fp_update' in df.targets
    assert 'update_impl' in df.targets.get('fp_update', set())


def test_cast_assign_complex():
    from ethunter.analyzer import cast_assign
    tree, st, df = _make_analyzer_env('cast_assign_complex.c')
    cast_assign.analyze(tree, 'cast_assign_complex.c', st, df)
    assert 'g_init' in df.targets
    assert 'my_md5_init' in df.targets.get('g_init', set())


def test_param_assign_simple():
    from ethunter.analyzer import param_assign
    tree, st, df = _make_analyzer_env('param_assign.c')
    edges = param_assign.analyze(tree, 'param_assign.c', st, df)
    assert len(df.targets) > 0
    assert any(k.startswith('<struct:') for k in df.targets)


def test_param_assign_complex():
    from ethunter.analyzer import param_assign
    tree, st, df = _make_analyzer_env('param_assign_complex.c')
    edges = param_assign.analyze(tree, 'param_assign_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'on_start' in callees or 'on_stop' in callees


# === Call Detection tests ===

def test_direct_call_fp():
    from ethunter.analyzer import direct_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('fp_assign.c')
    direct_assign.analyze(tree, 'fp_assign.c', st, df)
    edges = direct_call_fp.analyze(tree, 'fp_assign.c', st, df)
    callees = {e.callee for e in edges}
    assert 'foo' in callees


def test_direct_call_fp_alias_chain():
    from ethunter.analyzer import direct_assign, direct_call_fp
    tree, st, df = _make_analyzer_env('long_alias_chain.c')
    direct_assign.analyze(tree, 'long_alias_chain.c', st, df)
    edges = direct_call_fp.analyze(tree, 'long_alias_chain.c', st, df)
    callees = {e.callee for e in edges}
    assert 'target_func' in callees


def test_array_call():
    from ethunter.analyzer import initializer_assign, array_call
    tree, st, df = _make_analyzer_env('fp_array.c')
    initializer_assign.analyze(tree, 'fp_array.c', st, df)
    edges = array_call.analyze(tree, 'fp_array.c', st, df)
    callees = {e.callee for e in edges}
    assert 'cmd_help' in callees


def test_array_call_complex():
    from ethunter.analyzer import initializer_assign, array_call
    tree, st, df = _make_analyzer_env('fp_array_complex.c')
    initializer_assign.analyze(tree, 'fp_array_complex.c', st, df)
    edges = array_call.analyze(tree, 'fp_array_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert any('cmd' in c.lower() for c in callees)


def test_field_call_simple():
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call.c')
    initializer_assign.analyze(tree, 'field_call.c', st, df)
    edges = field_call.analyze(tree, 'field_call.c', st, df)
    callees = {e.callee for e in edges}
    assert 'fs_init' in callees
    assert 'fs_read' in callees


def test_field_call_chain():
    from ethunter.analyzer import initializer_assign, field_call
    tree, st, df = _make_analyzer_env('field_call_complex.c')
    initializer_assign.analyze(tree, 'field_call_complex.c', st, df)
    edges = field_call.analyze(tree, 'field_call_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'net_read' in callees
    assert 'net_write' in callees


def test_dlsym_fp():
    from ethunter.analyzer import dlsym_fp
    tree, st, df = _make_analyzer_env('dlsym_fp.c')
    edges = dlsym_fp.analyze(tree, 'dlsym_fp.c', st, df)
    callees = {e.callee for e in edges}
    assert 'plugin_init' in callees


def test_dlsym_fp_complex():
    from ethunter.analyzer import dlsym_fp
    tree, st, df = _make_analyzer_env('dlsym_fp_complex.c')
    edges = dlsym_fp.analyze(tree, 'dlsym_fp_complex.c', st, df)
    callees = {e.callee for e in edges}
    assert 'plugin_start' in callees


# === Integration tests ===

def test_symbol_table_extraction():
    tree, st, _ = _make_analyzer_env('direct_call.c')
    names = st.all_function_names
    assert 'main' in names
    assert 'worker' in names
    assert 'helper' in names


def test_dataflow_assign_merge():
    df = VariableState()
    df.assign('fp', 'foo')
    assert df.resolve('fp') == {'foo'}
    df.merge('fp', 'fp2')
    assert df.resolve('fp2') == {'foo'}


def test_call_graph_dedup():
    from ethunter.graph.model import CallGraph, CallEdge, CallType
    from ethunter.analyzer.orchestrator import run_all_analyses
    files = ['direct_call.c', 'fp_assign.c']
    trees = {}
    st = SymbolTable()
    df = VariableState()
    for f in files:
        path = os.path.join(FIXTURES, f)
        tree = parse_file(path)
        trees[path] = tree
        for func in extract_functions(tree, f):
            st.add_function(func)
    graph = run_all_analyses(trees, st, df)
    graph.source_files = [os.path.join(FIXTURES, f) for f in files]
    pairs = [(e.caller, e.callee) for e in graph.edges]
    assert len(pairs) == len(set(pairs)), f'Duplicate edges: {pairs}'
```

- [ ] **Step 4: 重写 test_cross_file.py**

```python
"""Cross-file tests for all analyzer modules."""

import os
import pytest

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.direct_call import analyze as direct_analyze
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState


FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures', 'cross_file')


def _make_cross_file_env(dir_name, files):
    """Create symbol_table + dataflow for cross-file fixture directory."""
    base = os.path.join(FIXTURES, dir_name)
    trees = {}
    st = SymbolTable()
    df = VariableState()
    for f in files:
        path = os.path.join(base, f)
        tree = parse_file(path)
        trees[path] = tree
        for func in extract_functions(tree, f):
            st.add_function(func)
    return trees, st, df


def test_cross_file_direct_call():
    trees, st, df = _make_cross_file_env('direct_call', ['caller.c', 'callee.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(direct_analyze(tree, path, st.all_function_names))
    caller_edges = {(e.caller, e.callee) for e in edges if e.caller == 'main_func'}
    assert ('main_func', 'helper') in caller_edges or ('main_func', 'worker') in caller_edges


def test_cross_file_direct_assign():
    from ethunter.analyzer import direct_assign, direct_call_fp
    trees, st, df = _make_cross_file_env('fp_assign', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        direct_assign.analyze(tree, path, st, df)
        edges.extend(direct_call_fp.analyze(tree, path, st, df))
    assert any(e.callee == 'actual_handler' for e in edges)


def test_cross_file_param_assign():
    from ethunter.analyzer import param_assign
    trees, st, df = _make_cross_file_env('callback_param', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        edges.extend(param_assign.analyze(tree, path, st, df))
    assert any(e.callee == 'local_handler' or e.callee == 'my_callback' for e in edges)


def test_cross_file_initializer_assign():
    from ethunter.analyzer import initializer_assign, array_call
    trees, st, df = _make_cross_file_env('fp_array', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        initializer_assign.analyze(tree, path, st, df)
        edges.extend(array_call.analyze(tree, path, st, df))
    assert any('cmd' in e.callee.lower() for e in edges)


def test_cross_file_field_call():
    from ethunter.analyzer import initializer_assign, field_call
    trees, st, df = _make_cross_file_env('vtable', ['callee.c', 'caller.c'])
    edges = []
    for path, tree in trees.items():
        initializer_assign.analyze(tree, path, st, df)
        edges.extend(field_call.analyze(tree, path, st, df))
    assert any('init' in e.callee.lower() or 'read' in e.callee.lower() for e in edges)


def test_cross_file_dlsym_fp():
    from ethunter.analyzer import dlsym_fp
    trees, st, df = _make_cross_file_env('dlsym_fp', ['caller.c', 'callee.h'])
    edges = []
    for path, tree in trees.items():
        edges.extend(dlsym_fp.analyze(tree, path, st, df))
    callees = {e.callee for e in edges}
    assert 'plugin_func_a' in callees or 'plugin_func_b' in callees
```

- [ ] **Step 5: 运行全量测试**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -v
```

Expected: ALL PASS

- [ ] **Step 6: 删除旧模块和旧 fixture**

```bash
# Delete old analyzer modules
rm src/ethunter/analyzer/fp_assign.py \
   src/ethunter/analyzer/fp_array.py \
   src/ethunter/analyzer/vtable.py \
   src/ethunter/analyzer/callback_param.py \
   src/ethunter/analyzer/callback_reg.py \
   src/ethunter/analyzer/typedef_fp.py \
   src/ethunter/analyzer/fp_alias.py \
   src/ethunter/analyzer/fp_return.py \
   src/ethunter/analyzer/lazy_init.py \
   src/ethunter/analyzer/union_fp.py \
   src/ethunter/analyzer/macro_fp.py

# Delete old fixtures that only tested removed modules
rm tests/fixtures/fp_return.c tests/fixtures/fp_return_complex.c \
   tests/fixtures/typedef_fp.c tests/fixtures/typedef_fp_complex.c \
   tests/fixtures/fp_alias.c tests/fixtures/fp_alias_complex.c \
   tests/fixtures/lazy_init.c tests/fixtures/lazy_init_complex.c \
   tests/fixtures/union_fp.c tests/fixtures/union_fp_complex.c \
   tests/fixtures/macro_fp.c tests/fixtures/macro_fp_complex.c \
   tests/fixtures/macro_collision.c tests/fixtures/ternary_fp.c
```

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py tests/test_analyzers.py tests/test_cross_file.py
git rm src/ethunter/analyzer/fp_assign.py src/ethunter/analyzer/fp_array.py src/ethunter/analyzer/vtable.py src/ethunter/analyzer/callback_param.py src/ethunter/analyzer/callback_reg.py src/ethunter/analyzer/typedef_fp.py src/ethunter/analyzer/fp_alias.py src/ethunter/analyzer/fp_return.py src/ethunter/analyzer/lazy_init.py src/ethunter/analyzer/union_fp.py src/ethunter/analyzer/macro_fp.py
git rm tests/fixtures/fp_return.c tests/fixtures/fp_return_complex.c tests/fixtures/typedef_fp.c tests/fixtures/typedef_fp_complex.c tests/fixtures/fp_alias.c tests/fixtures/fp_alias_complex.c tests/fixtures/lazy_init.c tests/fixtures/lazy_init_complex.c tests/fixtures/union_fp.c tests/fixtures/union_fp_complex.c tests/fixtures/macro_fp.c tests/fixtures/macro_fp_complex.c tests/fixtures/macro_collision.c tests/fixtures/ternary_fp.c
git commit -m "refactor: integrate new architecture, remove old modules and fixtures"
```

---

### Task 9: ET-Bench 验收

**Files:**
- 无需新增

- [ ] **Step 1: 运行 et_bench 测试**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py -v -s
```

Expected output:
- fnptr-global-struct: 95%+
- fnptr-cast: 95%+
- fnptr-struct: 80%+
- fnptr-library: 80%+
- fnptr-callback: 90%+
- fnptr-global-array: 100%
- fnptr-global-struct-array: 97%+
- fnptr-only: 90%+
- fnptr-varargs: 100%
- OVERALL: 95%+

- [ ] **Step 2: 运行全量测试套件**

```bash
.venv/bin/python -m pytest tests/ -v
```

Expected: ALL PASS

- [ ] **Step 3: Commit（如测试通过）**

```bash
git add -A
git commit -m "test: verify et_bench recall targets met"
```

---

## 任务执行顺序

```
Task 0: helpers.py (extract_field_path)
  ↓
Task 1: TDD initializer_assign (新建 2 fixture + 2 test + 实现)
  ↓
Task 2: TDD cast_assign (新建 2 fixture + 2 test + 实现)
  ↓
Task 3: TDD direct_assign (复用 fixture + 2 test + 实现)
  ↓
Task 4: TDD param_assign (新建 2 fixture + 2 test + 实现)
  ↓
Task 5: TDD direct_call_fp (复用 fixture + 2 test + 实现)
  ↓
Task 6: TDD field_call (新建 2 fixture + 2 test + 实现)
  ↓
Task 7: TDD array_call (复用 fixture + 2 test + 实现)
  ↓
Task 8: 整合 orchestrator + 全量测试 + 清理旧模块
  ↓
Task 9: ET-Bench 验收
```

## 验收标准

| 指标 | 当前 | 目标 |
|---|---|---|
| 总召回率 | 73.26% | 95%+ |
| fnptr-global-struct | 0% | 95%+ |
| fnptr-cast | 10% | 95%+ |
| fnptr-struct | 19% | 80%+ |
| fnptr-library | 41% | 80%+ |
| fnptr-callback | 69% | 90%+ |
| fnptr-global-array | 100% | 100% |
| fnptr-global-struct-array | 97% | 97%+ |
| fnptr-only | 62% | 90%+ |
| fnptr-varargs | 100% | 100% |
| test_analyzers.py | 全通过 | 全通过 |
| test_cross_file.py | 全通过 | 全通过 |
