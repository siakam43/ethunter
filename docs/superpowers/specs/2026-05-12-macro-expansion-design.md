# 函数包装宏展开支持设计

**日期**: 2026-05-12
**目标**: 解决 library/example_10 `get_crl_delta → crls_http_cb` 缺失边
**原因**: 注册调用使用 `#define` 包装宏，tree-sitter 不展开，`param_assign` 无法解析参数映射
**开发模式**: TDD

## 背景

library/example_10 fixture 中：

```c
#define X509_STORE_set_lookup_crls_cb(ctx, func) \
    X509_STORE_set_lookup_crls((ctx), (func))

void store_setup_crl_download(X509_STORE *st) {
    X509_STORE_set_lookup_crls_cb(st, crls_http_cb);  // ← 宏调用
}
```

tree-sitter AST 中 `X509_STORE_set_lookup_crls_cb(st, crls_http_cb)` 是普通的 `call_expression`，函数名为 `X509_STORE_set_lookup_crls_cb`（宏名）。`param_assign` 查找 `func_params` 时找不到该宏名的参数列表 → 参数映射失败 → `crls_http_cb` 未被追踪。

## 范围

**仅支持函数包装宏**：`#define MACRO(a, b) real_func(a, b)` 或 `#define MACRO(a, b) real_func((a), (b))` — 宏体为单个函数调用表达式，无 `##` 拼接、`#` 字符串化、无多语句。

## 设计

### 改动文件

- `src/ethunter/analyzer/param_assign.py` — `analyze()` 和 `_collect_call_params` 两处
  - 新增 `import re` 在文件头部

### 第 1 步：在 `analyze()` 开头收集宏

在 Pass 1 之前，扫描树中所有 `preproc_function_def` 节点，提取宏 → (真实函数名, 宏形参列表) 的映射。

```python
import re

def _collect_simple_macros(tree) -> dict[str, tuple[str, list[str]]]:
    """Collect function-wrapper macros: macro_name -> (real_func_name, [param_names])."""
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
                # Extract the first identifier as the real function name
                func_match = re.match(r'\s*(\w+)\s*\(', body_text)
                if func_match and func_match.group(1) != macro_name:
                    macros[macro_name] = (func_match.group(1), param_idents)
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros
```

### 第 2 步：在 `_collect_call_params` 中内联替换宏调用

插入点在 line 307，`param_names = func_params.get(call_name, [])` **之前**：

```python
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)

                    # --- Macro expansion: replace macro call with real function ---
                    if call_name not in func_params and call_name in macros:
                        real_func, _macro_params = macros[call_name]
                        if real_func in func_params:
                            call_name = real_func  # substitute for the rest of this block

                    param_names = func_params.get(call_name, [])
                    # ... rest of existing logic unchanged ...
```

**原理**：当 `call_name` 匹配已知宏时，将 `call_name` 替换为宏体中的真实函数名。后续的 `param_names` 查找、`_is_registration` 判断、`_propagate_call_site` 调用全部基于真实函数名执行，零代码重复。

**为什么安全**：宏形参名仅用于 `_collect_simple_macros` 中的提取元信息，运行时不需要用到。A → B 的参数顺序由 C 预处理器保证一致（函数包装宏按定义保持参数位置）。

**示例走查** (library/example_10)：

```
call_name = "X509_STORE_set_lookup_crls_cb"  ← 宏名
→ not in func_params, in macros ✓
→ real_func = "X509_STORE_set_lookup_crls"
→ call_name = "X509_STORE_set_lookup_crls"   ← 替换为真实函数

param_names = func_params["X509_STORE_set_lookup_crls"]
  = ["store", "lookup_crls"]

arg[0] = "st" → arg_idx=0, target not in symbol_names ✓ (identifier st)
arg[1] = "crls_http_cb" → arg_idx=1, target in symbol_names ✓
  → _is_registration("X509_STORE_set_lookup_crls")? "set_" matches → True
  → dataflow.register_callback("crls_http_cb")
  → _propagate_call_site("X509_STORE_set_lookup_crls", 1, "crls_http_cb", ...)
  → dataflow.assign("lookup_crls", "crls_http_cb")
```

### 新增测试

在 `tests/test_et_bench.py` 中新增：

| 测试函数 | 验证内容 |
|---|---|
| `test_macro_expansion_param_tracking` | `#define WRAPPER(a,b) real_func(a,b)` 宏调用时 `param_assign` 将实参映射到 `real_func` 的参数 |

测试源码：

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

### 验证

```bash
# 单元测试
.venv/bin/python -m pytest tests/test_et_bench.py::test_macro_expansion_param_tracking -v

# 集成验证
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
# 预期: fnptr-library 召回率 98.57% → 100%
```

## 不在范围内

- 多语句宏体（`#define MACRO(a) { stmt1; stmt2; }`）
- `##` 拼接和 `#` 字符串化
- 嵌套宏（宏体引用另一个宏）
- 非函数调用的宏体（常量、表达式等）
- 宏展开与 `field_call._collect_macros` 的整合（独立实现，不共享）
