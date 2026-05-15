# 函数包装宏展开实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 支持 `#define MACRO(a,b) real_func(a,b)` 函数包装宏，使 library/example_10 `get_crl_delta → crls_http_cb` 被正确检测。

**Architecture:** 在 `param_assign.analyze()` 中收集 `preproc_function_def` 宏映射，在 `_collect_call_params` 中当 `call_name` 匹配已知宏时替换为真实函数名，复用现有参数追踪链路。改动集中在 `param_assign.py` 一个文件。

**Tech Stack:** Python 3.11, tree-sitter-c, pytest

---

### Task 1: 实现宏展开 + 单元测试

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` — 新增 `import re`，新增 `_collect_simple_macros` 函数，在 `_collect_call_params` 中插入宏替换逻辑
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试**

在 `tests/test_et_bench.py` 末尾追加：

```python
def test_macro_expansion_param_tracking():
    """Macro wrapper call: #define MACRO(a,b) real(a,b) → param tracking works."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(int x);

    static void my_handler(int x) { (void)x; }

    static void register_callback_impl(void *ctx, cb_fn cb) {
        ((struct ctx*)ctx)->handler = cb;
    }

    #define register_callback(ctx, fn) register_callback_impl((ctx), (fn))

    struct ctx {
        cb_fn handler;
    };

    void setup(void) {
        struct ctx c;
        register_callback(&c, my_handler);
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
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ('invoke', 'my_handler') in pairs, \
        f"Expected invoke -> my_handler, got: {pairs}"
```

- [ ] **Step 2: 运行测试确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_macro_expansion_param_tracking -v
```

预期: FAIL — `register_callback` 是宏名，`func_params` 无匹配 → 参数映射失败 → 无 edge

- [ ] **Step 3: 新增 `import re` 和 `_collect_simple_macros` 函数**

在 `param_assign.py` 文件头部 `import tree_sitter as ts` 之后新增：

```python
import re
```

在 `_extract_field_operand` 函数之后（line ~79），`_collect_func_params` 之前，新增：

```python
def _collect_simple_macros(tree) -> dict[str, tuple[str, list[str]]]:
    """Collect function-wrapper macros: macro_name -> (real_func_name, [param_names]).

    Only matches macros of the form: #define MACRO(a,b) real_func(a,b)
    Skips constant macros, expression macros, and multi-statement macros.
    """
    macros: dict[str, tuple[str, list[str]]] = {}

    def _scan(n) -> None:
        if n.type == 'preproc_function_def':
            name_node = None
            body_text = None
            param_idents = []
            for child in n.children:
                if child.type == 'identifier' and child.text and name_node is None:
                    name_node = child
                elif child.type == 'preproc_params':
                    for pc in child.children:
                        if pc.type == 'identifier' and pc.text:
                            param_idents.append(pc.text.decode('utf-8'))
                elif child.type == 'preproc_arg' and child.text:
                    body_text = child.text.decode('utf-8')
            if name_node and name_node.text and body_text:
                macro_name = name_node.text.decode('utf-8')
                # Extract the first identifier followed by '(' as the real function name
                func_match = re.match(r'\s*(\w+)\s*\(', body_text)
                if func_match and func_match.group(1) != macro_name:
                    macros[macro_name] = (func_match.group(1), param_idents)
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros
```

- [ ] **Step 4: 在 `analyze()` 中调用 `_collect_simple_macros`**

找到 `analyze()` 中 `_collect_func_params` 调用之后、`func_fp_params` 存储之前的位置：

```python
    _collect_func_params(tree.root_node, func_params, func_fp_params)
```

在下一行新增：

```python
    # Collect function-wrapper macros for call-site expansion
    macros = _collect_simple_macros(tree)
```

（`func_params`、`func_fp_params` 声明及 `dataflow.state.func_fp_params` 存储均为已有代码，只加这一行）

- [ ] **Step 5: 在 `_collect_call_params` 中插入宏替换逻辑**

在 `_collect_call_params` 函数中，找到 line ~303-307：

```python
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)
                    param_names = func_params.get(call_name, [])
```

在 `param_names = func_params.get(call_name, [])` **之前**插入宏替换逻辑，完整修改为：

```python
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)

                    # Macro expansion: replace macro call with real function name
                    if call_name not in func_params and call_name in macros:
                        real_func, _ = macros[call_name]
                        if real_func in func_params:
                            call_name = real_func

                    param_names = func_params.get(call_name, [])
```

- [ ] **Step 6: 运行单元测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_macro_expansion_param_tracking -v
```

预期: PASS

- [ ] **Step 7: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-library 召回率 98.57% → 100%

- [ ] **Step 8: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

预期: 全部 PASS

- [ ] **Step 9: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "feat: add function-wrapper macro expansion in param_assign

Supports #define MACRO(a,b) real_func(a,b) patterns so macro-wrapped
callback registrations are correctly resolved through the existing
param_assign parameter tracking pipeline.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
