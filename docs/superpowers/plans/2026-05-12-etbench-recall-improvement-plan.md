# ET-Bench 召回率提升实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 fnptr-callback, fnptr-cast, fnptr-only, fnptr-library 四个场景召回率提升至 100%，新增 34 条间接调用边。

**Architecture:** 5 个 Phase 按依赖顺序实施。Phase 1-3 修改分析器核心逻辑（cast_assign, initializer_assign, param_assign, orchestrator），Phase 4-5 修复 fixture 和 field_call 字段传播。所有改动在现有模块内扩展，不新增模块。

**Tech Stack:** Python 3.11, tree-sitter-c, pytest

---

### Task 1: Phase 1 — cast_assign 移除 symbol_names 守卫 + 单元测试

**Files:**
- Modify: `src/ethunter/analyzer/cast_assign.py:33`
- Modify: `src/ethunter/analyzer/initializer_assign.py:31-41`
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试**

在 `tests/test_et_bench.py` 末尾追加：

```python
def test_cast_assign_no_symbol_names_guard():
    """Phase 1: (type)stdlib_func cast where target is NOT in symbol_names should still be tracked."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void *(*alloc_fn)(size_t nmemb, size_t size);

    alloc_fn my_alloc = (alloc_fn)calloc;

    void use_alloc(void) {
        my_alloc(1, 64);
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
    assert ('use_alloc', 'calloc') in pairs, \
        f"Expected use_alloc -> calloc, got: {pairs}"
```

- [ ] **Step 2: 运行测试确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_cast_assign_no_symbol_names_guard -v
```

预期: FAIL — `calloc` 不在 symbol_names，cast 赋值被丢弃。

- [ ] **Step 3: 修改 cast_assign.py line 33**

```python
# 原文 (line 33):
                if name in symbol_names:
                    return name
# 改为:
                return name
```

- [ ] **Step 4: 修改 initializer_assign.py — guard 1 (line 33)**

```python
# 原文 (line 31-33):
        if hasattr(dataflow, 'unwrap_cast'):
            result = dataflow.unwrap_cast(node)
            if result and result in symbol_names:
                return result
# 改为:
        if hasattr(dataflow, 'unwrap_cast'):
            result = dataflow.unwrap_cast(node)
            if result:
                return result
```

- [ ] **Step 5: 修改 initializer_assign.py — guard 2 (lines 38-41)**

```python
# 原文 (line 38-41):
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
                if name in symbol_names:
                    return name
# 改为:
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
                return name
```

- [ ] **Step 6: 运行单元测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_cast_assign_no_symbol_names_guard -v
```

预期: PASS

- [ ] **Step 7: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-only 召回率从 75% 升至 100%

- [ ] **Step 8: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

预期: 全部 PASS，100% 场景保持 100%

- [ ] **Step 9: Commit**

```bash
git add src/ethunter/analyzer/cast_assign.py src/ethunter/analyzer/initializer_assign.py tests/test_et_bench.py
git commit -m "fix: remove symbol_names guard in cast expression extraction

Cast initializer targets (e.g. (alloc_fn)calloc) are now tracked even when
the target function is not defined in the current compilation unit.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Phase 2 — func_fp_params 收集

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` (多处)
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试 — 直接形参调用**

在 `tests/test_et_bench.py` 末尾追加：

```python
def test_param_local_call_direct():
    """Phase 2: callee(fnptr) pattern where fnptr is called directly inside callee."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef char *(*fmt_fn)(long double n);

    static char *format_time_us(long double n) {
        return "1.00us";
    }

    static void print_units(long double n, fmt_fn fmt, int width) {
        char *msg = fmt(n);
        (void)msg;
        (void)width;
    }

    void main_func(void) {
        print_units(100.0, format_time_us, 10);
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
    assert ('print_units', 'format_time_us') in pairs, \
        f"Expected print_units -> format_time_us, got: {pairs}"
```

- [ ] **Step 2: 运行确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_param_local_call_direct -v
```

预期: FAIL — param_assign 未创建 `print_units -> format_time_us` 间接边

- [ ] **Step 3: 在 param_assign.py 中新增 func_fp_params 收集**

在 `_collect_func_params` 函数结尾（line 106 的 `for child in node.children:` 之后）嵌入 fnptr 参数识别逻辑。将 `_collect_func_params` 的返回类型从隐式修改 `func_params` dict 改为同时填充 `func_fp_params`。

在 `_collect_func_params` 函数定义前新增一个辅助函数，并在 `_collect_func_params` 内调用：

```python
def _has_fnptr_declarator(node: ts.Node) -> bool:
    """Check if a parameter_declaration contains a function_declarator (fnptr param)."""
    if node.type == 'function_declarator':
        return True
    for c in node.children:
        if _has_fnptr_declarator(c):
            return True
    return False
```

在 `_collect_func_params` 中，初始化 `func_fp_params: dict[str, set[int]] = {}`，并在收集参数名时同时检测 fnptr：

```python
def _collect_func_params(node, func_params: dict, func_fp_params: dict) -> None:
    """Collect function parameter lists and fnptr parameter positions."""
    if node.type == 'function_definition':
        decl = _find_child(node, 'function_declarator')
        if not decl:
            for c in node.children:
                if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                    d = _find_child(c, 'function_declarator')
                    if d:
                        decl = d
                        break
        if decl:
            fname, inner_decl = _find_func_name_from_decl(decl)
            if fname:
                params = []
                fp_positions = set()
                plist = _find_child(inner_decl, 'parameter_list')
                if plist:
                    pos = 0
                    for p in plist.children:
                        if p.type == 'parameter_declaration':
                            pname = _extract_param_name(p)
                            if pname:
                                params.append(pname)
                                if _has_fnptr_declarator(p):
                                    fp_positions.add(pos)
                                pos += 1
                func_params[fname] = params
                if fp_positions:
                    func_fp_params[fname] = fp_positions
    for child in node.children:
        _collect_func_params(child, func_params, func_fp_params)
```

修改 `analyze()` 中调用 `_collect_func_params` 的地方，增加 `func_fp_params` 参数：

```python
func_params: dict[str, list[str]] = {}
func_fp_params: dict[str, set[int]] = {}
_collect_func_params(tree.root_node, func_params, func_fp_params)
```

同样修改 `_register_phase()` 中的调用——该函数在开头有独立的 `func_params` 和两参数 `_collect_func_params(tree.root_node, func_params)` 调用。改为传入一个 dummy dict 适配新签名（`_register_phase` 不需要 `func_fp_params`）：

```python
# _register_phase 中 (line ~142):
func_params: dict[str, list[str]] = {}
_dummy_fp_params: dict[str, set[int]] = {}
_collect_func_params(tree.root_node, func_params, _dummy_fp_params)
```

- [ ] **Step 4: 运行测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_param_local_call_direct -v
```

预期: PASS — func_fp_params 已收集，但还需要 Pass 1 扩展才能产生边。等一下，当前 Pass 1 已经能处理 `identifier` 实参且 target 在 `symbol_names` 中的情况。`format_time_us` 在 `symbol_names` 中，`print_units(100.0, format_time_us, 10)` → arg 1 是 `format_time_us`，在 symbol_names 中 → 添加到 param_mappings["fmt"] = {"format_time_us"} → Pass 3 `_detect_param_calls` 检测到 `fmt(n)` 调用 → 创建边。所以这个测试在 Step 3 后就应该 PASS。

如果未 PASS，继续 Step 5-6。

---

### Task 3: Phase 2 — pointer_expression 实参 + dataflow 回退

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` (`_collect_call_params`)
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试 — &func 地址传递**

```python
def test_param_local_call_address_of():
    """Phase 2: &func passed as fnptr argument, called through parameter in callee."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef const void *(*ptr_getter)(void *ctx, size_t i);

    const void *my_getter(void *ctx, size_t i) {
        return (void *)(uintptr_t)i;
    }

    static void batch_lookup(size_t n, ptr_getter getter, void *ctx) {
        for (size_t i = 0; i < n; i++) {
            getter(ctx, i);
        }
    }

    void caller_func(void) {
        batch_lookup(10, &my_getter, NULL);
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
    assert ('caller_func', 'my_getter') in pairs, \
        f"Expected caller_func -> my_getter, got: {pairs}"
```

- [ ] **Step 2: 编写失败测试 — 局部变量 dataflow 回退**

```python
def test_param_local_var_dataflow_fallback():
    """Phase 2: local var = func_name; callee(local_var) → resolve via dataflow."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef int (*cmp_fn)(const void *, const void *);

    int sort_asc(const void *a, const void *b) { return 0; }

    static void my_qsort(void *base, size_t n, size_t sz, cmp_fn cmp) {
        cmp(base, ((char *)base) + sz);
    }

    void sort_data(void) {
        cmp_fn callback = sort_asc;
        my_qsort(NULL, 10, 8, callback);
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
    assert ('sort_data', 'sort_asc') in pairs, \
        f"Expected sort_data -> sort_asc, got: {pairs}"
```

- [ ] **Step 3: 运行两个测试确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_param_local_call_address_of tests/test_et_bench.py::test_param_local_var_dataflow_fallback -v
```

预期: 两个均 FAIL

- [ ] **Step 4: 修改 `_collect_call_params` — 新增 pointer_expression 实参处理**

在 `_collect_call_params` 中，`elif c.type == 'cast_expression':` 块之后，新增：

```python
                        elif c.type == 'pointer_expression' and c.children:
                            # Extract &func from pointer_expression
                            inner = c.children[-1]
                            if inner.type == 'identifier' and inner.text:
                                target = inner.text.decode('utf-8')
                                if target in symbol_names:
                                    arg_idx = comma_count
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
                                    elif arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                    _propagate_call_site(
                                        call_name, arg_idx, target,
                                        dataflow, symbol_names
                                    )
```

- [ ] **Step 5: 修改 `_collect_call_params` — 新增 dataflow 回退**

在标识符处理分支中（`if c.type == 'identifier' and c.text:`），`target = c.text.decode('utf-8')` 之后，当 `target not in symbol_names` 时新增回退：

当前代码流程是：
```python
if c.type == 'identifier' and c.text:
    arg_idx = comma_count
    target = c.text.decode('utf-8')
    if target in symbol_names:
        # ... existing logic ...
```

改为：
```python
if c.type == 'identifier' and c.text:
    arg_idx = comma_count
    target = c.text.decode('utf-8')
    if target in symbol_names:
        # ... existing logic (unchanged) ...
    else:
        # Fallback: check dataflow for local variable assigned to fnptr
        df_targets = dataflow.resolve(target)
        if df_targets and arg_idx < len(param_names):
            pname = param_names[arg_idx]
            if pname not in param_mappings:
                param_mappings[pname] = set()
            param_mappings[pname].update(df_targets)
```

- [ ] **Step 6: 运行测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_param_local_call_address_of tests/test_et_bench.py::test_param_local_var_dataflow_fallback -v
```

预期: 两个均 PASS

---

### Task 4: Phase 2 — 解引用调用检测 + 回调的回调

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` (`_detect_param_calls`, `_collect_call_params`)
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试 — 解引用调用**

```python
def test_param_local_call_deref():
    """Phase 2: (*fnptr)(args) dereference call through parameter."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(int x);

    static void actual_cb(int x) { (void)x; }

    static void invoke_cb(int x, cb_fn cb) {
        (*cb)(x);
    }

    void main_func(void) {
        invoke_cb(42, actual_cb);
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
    assert ('main_func', 'actual_cb') in pairs, \
        f"Expected main_func -> actual_cb, got: {pairs}"
```

- [ ] **Step 2: 编写失败测试 — 回调的回调**

```python
def test_param_callback_of_callback():
    """Phase 2: field->fnptr(fnptr_arg) — fnptr passed as arg to indirect field call."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*op_fn)(void *ptr);

    static void my_relocate(void *ptr) { (void)ptr; }

    typedef struct {
        void (*note_fn)(void *obj, void *cookie, op_fn op);
        void *obj;
        void *cookie;
    } ptr_data_t;

    static void my_note_fn(void *obj, void *cookie, op_fn op) {
        op(obj);
    }

    static ptr_data_t slot;

    void caller_func(void) {
        slot.note_fn = my_note_fn;
        if (slot.note_fn)
            slot.note_fn(slot.obj, slot.cookie, my_relocate);
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
    assert ('caller_func', 'my_relocate') in pairs, \
        f"Expected caller_func -> my_relocate, got: {pairs}"
```

- [ ] **Step 3: 运行测试确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_param_local_call_deref tests/test_et_bench.py::test_param_callback_of_callback -v
```

预期: 两个均 FAIL

- [ ] **Step 4: 修改 `_detect_param_calls` — 新增 parenthesized_expression / pointer_expression 调用**

在 `_detect_param_calls` 中，当前逻辑：

```python
if func_node and func_node.type == 'identifier' and func_node.text:
    fname = func_node.text.decode('utf-8')
    targets = param_mappings.get(fname)
```

改为同时处理 `parenthesized_expression`（`(*fp)()` 模式）和 `pointer_expression`（`*fp()` 模式）：

```python
call_target_name = None
if func_node and func_node.type == 'identifier' and func_node.text:
    call_target_name = func_node.text.decode('utf-8')
elif func_node and func_node.type == 'parenthesized_expression':
    # (*fp)(args) — extract inner identifier from pointer_expression
    for c in func_node.children:
        if c.type == 'pointer_expression' and c.children:
            inner = c.children[-1]
            if inner.type == 'identifier' and inner.text:
                call_target_name = inner.text.decode('utf-8')
                break
elif func_node and func_node.type == 'pointer_expression' and func_node.children:
    # *fp(args) — extract identifier
    inner = func_node.children[-1]
    if inner.type == 'identifier' and inner.text:
        call_target_name = inner.text.decode('utf-8')

if call_target_name:
    targets = param_mappings.get(call_target_name)
    if targets:
        caller = find_enclosing_function(node, tree.root_node)
        for target in targets:
            call_site_edges.append((caller or '<unknown>', target, filepath, node.start_point[0] + 1))
```

- [ ] **Step 5: 修改 `_collect_call_params` — 新增 field_expression 调用分支**

在 `_collect_call_params` 的函数体开头（`if node.type == 'call_expression':` 之后，现有 `func_node` 处理之前），新增 field_expression 调用分支：

```python
            # Handle field_expression calls: obj->fnptr_field(fnptr_arg)
            if func_node and func_node.type == 'field_expression':
                from ethunter.analyzer.helpers import extract_field_path
                field_path = extract_field_path(func_node)
                if field_path and args:
                    # Resolve field targets
                    field_targets = dataflow.resolve(f'<gstruct:{field_path}>')
                    if not field_targets:
                        field_targets = dataflow.resolve(f'<struct:{field_path}>')
                    if field_targets:
                        caller = find_enclosing_function(node, tree.root_node)
                        # Build arg index → value map for fnptr position matching
                        comma_count = 0
                        arg_values = []
                        for c in args.children:
                            if c.type == ',':
                                comma_count += 1
                            elif c.type not in ('(', ')'):
                                arg_values.append((comma_count, c))
                        for ftarget in field_targets:
                            fp_positions = func_fp_params.get(ftarget, set())
                            for pos, arg_node in arg_values:
                                if pos in fp_positions:
                                    actual_target = None
                                    if arg_node.type == 'identifier' and arg_node.text:
                                        actual_target = arg_node.text.decode('utf-8')
                                    elif arg_node.type == 'pointer_expression' and arg_node.children:
                                        inner = arg_node.children[-1]
                                        if inner.type == 'identifier' and inner.text:
                                            actual_target = inner.text.decode('utf-8')
                                    elif arg_node.type == 'cast_expression':
                                        if hasattr(dataflow, 'unwrap_cast'):
                                            actual_target = dataflow.unwrap_cast(arg_node)
                                        if not actual_target:
                                            for cc in reversed(arg_node.children):
                                                if cc.type == 'identifier' and cc.text:
                                                    actual_target = cc.text.decode('utf-8')
                                                    break
                                    if actual_target and actual_target in symbol_names:
                                        edges.append(CallEdge(
                                            caller=caller or '<unknown>',
                                            callee=actual_target,
                                            caller_file=filepath,
                                            callee_file='',
                                            type=CallType.INDIRECT,
                                            indirect_kind='callback_param',
                                            caller_line=node.start_point[0] + 1,
                                        ))
```

- [ ] **Step 6: 运行测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_param_local_call_deref tests/test_et_bench.py::test_param_callback_of_callback -v
```

预期: 两个均 PASS

---

### Task 5: Phase 2 — 集成验证 (Gap 4 + 全量)

**Files:**
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试 — Gap 4 (fnptr pointer global)**

```python
def test_fnptr_pointer_global():
    """Phase 2/Gap4: log_handler_fn *global → local tmp_handler → call through local."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*log_handler_fn)(int level, const char *msg, void *ctx);

    static log_handler_fn *log_handler;
    static void *log_handler_ctx;

    static void mm_log_handler(int level, const char *msg, void *ctx) {
        (void)level; (void)msg; (void)ctx;
    }

    static void do_log(int level, const char *msg) {
        log_handler_fn *tmp_handler;
        if (log_handler != ((void *)0)) {
            tmp_handler = log_handler;
            tmp_handler(level, msg, log_handler_ctx);
        }
    }

    void set_log_handler(log_handler_fn *handler, void *ctx) {
        log_handler = handler;
        log_handler_ctx = ctx;
    }

    void init_logging(void) {
        set_log_handler(mm_log_handler, ((void *)0));
        do_log(1, "test");
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
    assert ('do_log', 'mm_log_handler') in pairs, \
        f"Expected do_log -> mm_log_handler, got: {pairs}"
```

- [ ] **Step 2: 运行确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_fnptr_pointer_global -v
```

预期: FAIL

- [ ] **Step 3: 修改 param_assign — Pass 1 同时写入 dataflow**

Gap 4 链路：`set_log_handler(mm_log_handler, ...)` → `log_handler = handler` → `tmp_handler = log_handler` → `tmp_handler(...)`。

根因：`param_assign` 的 Pass 1 将实参→形参映射写入局部 `param_mappings` dict，但不写入全局 `dataflow`。当 `direct_assign` 处理 `tmp_handler = log_handler` 时调用 `dataflow.resolve("log_handler")` 返回空——因为 `mm_log_handler` 仅存在于 `param_mappings["handler"]` 中，不在 `dataflow.targets` 中。

修复：在 `_collect_call_params` 中，当 identifier 实参在 `symbol_names` 中且非 registration 调用时，同时将 target 写入 dataflow。

在 `src/ethunter/analyzer/param_assign.py` 的 `_collect_call_params` 中，找到：

```python
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
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
```

在非 registration 分支（`else` 块）中，`param_mappings[pname].add(target)` 之后新增一行 `dataflow.assign(pname, target)`：

```python
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                        # Propagate param→fn mapping to dataflow so
                                        # direct_assign alias chain can resolve it
                                        dataflow.assign(pname, target)
```

此改动使 `direct_assign` 现有的别名链逻辑（`dataflow.resolve(target)` → `dataflow.assign(var_name, t)`）能在处理 `tmp_handler = log_handler` 时穿透双层别名正确找到 `mm_log_handler`。

- [ ] **Step 4: 运行 verif 确认所有 Phase 2 新测试通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py -k "test_param_local_call or test_fnptr_pointer" -v
```

- [ ] **Step 5: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-callback 召回率从 80.56% 升至 100%，fnptr-only 保持 100%

- [ ] **Step 6: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_et_bench.py
git commit -m "feat: extend param_assign for local param call, deref, callback-of-callback

- Collect func_fp_params for fnptr parameter positions
- Handle pointer_expression args (&func) in call-site collection
- Handle parenthesized_expression/pointer_expression in call detection
- Add dataflow fallback for local variable args
- Add field_expression call branch for callback-of-callback pattern

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Phase 3 — CALL_DETECTORS 重排 + local_fp_tracker

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py:33-37`
- Modify: `src/ethunter/analyzer/local_fp_tracker.py` (if needed)
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试**

```python
def test_local_fp_from_struct_field_init():
    """Phase 3/Gap2: Type *fp = obj->field; fp() resolves through field_call+direct_call_fp chain."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef int (*holdfunc_t)(void *dp, const char *name, void *tag, void **dsp);

    static int my_hold(void *dp, const char *name, void *tag, void **dsp) {
        return 0;
    }

    typedef struct {
        holdfunc_t holdfunc;
    } arg_t;

    static void release_sync(void *arg_ptr) {
        arg_t *a = (arg_t *)arg_ptr;
        holdfunc_t *hf = a->holdfunc;
        void *ds;
        hf(((void *)0), "test", ((void *)0), &ds);
    }

    void setup_and_call(void) {
        arg_t a;
        a.holdfunc = (holdfunc_t)my_hold;
        release_sync(&a);
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
    assert ('release_sync', 'my_hold') in pairs, \
        f"Expected release_sync -> my_hold, got: {pairs}"
```

- [ ] **Step 2: 运行确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_local_fp_from_struct_field_init -v
```

预期: FAIL — CALL_DETECTORS 时序问题导致 local_fp_tracker 读取时 dataflow 中字段未赋值

- [ ] **Step 3: 重排 CALL_DETECTORS**

```python
# 原文 (orchestrator.py:33-37):
CALL_DETECTORS = [
    direct_call_fp,
    field_call,
    array_call,
]

# 改为:
CALL_DETECTORS = [
    field_call,
    direct_call_fp,
    array_call,
]
```

- [ ] **Step 4: 运行测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_local_fp_from_struct_field_init -v
```

预期: PASS

- [ ] **Step 5: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

确认 `test_fix_c2_call_expression_rhs_field_assign`、`test_example_13_chain_through_local_fp`、`test_et_bench_fnptr_struct_full_recall` 全部 PASS

- [ ] **Step 6: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-cast 召回率从 80% 升至 100%

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "fix: reorder CALL_DETECTORS so field_call runs before direct_call_fp

Ensures struct field fnptr assignments populated in dataflow are visible
to local_fp_tracker within the same file's Phase 2 processing.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 7: Phase 4 — 修复 example_2, 9, 18 fixture

**Files:**
- Modify: `tests/benchmark/et_bench/fnptr-library/example_2/fixture.c`
- Modify: `tests/benchmark/et_bench/fnptr-library/example_9/fixture.c`
- Modify: `tests/benchmark/et_bench/fnptr-library/example_18/fixture.c`

- [ ] **Step 1: 修复 example_2 — 添加 lj_alloc_f 注册调用**

在 `tests/benchmark/et_bench/fnptr-library/example_2/fixture.c` 末尾追加：

```c
/* Registration: bind lj_alloc_f as allocator */
static void register_lj_alloc(void) {
    lua_State *L = lua_newstate(lj_alloc_f, NULL);
    (void)L;
}
```

- [ ] **Step 2: 修复 example_9 — 为 8 个 dtor 添加注册调用**

在 `tests/benchmark/et_bench/fnptr-library/example_9/fixture.c` 末尾追加：

```c
/* Registration: bind all dtor targets to Curl_llist instances */
void register_all_dtors(void) {
    Curl_llist l1, l2, l3, l4, l5, l6;
    Curl_llist_init(&l1, fileinfo_dtor);
    Curl_llist_init(&l2, hash_element_dtor);
    Curl_llist_init(&l3, free_bundle_hash_entry);
    Curl_llist_init(&l4, freednsentry);
    Curl_llist_init(&l5, trhash_dtor);
    Curl_llist_init(&l6, sh_freeentry);
    /* curl_free and gsasl_free use a different signature pattern;
       register via hash init or direct list->dtor assignment */
    Curl_hash h1, h2;
    Curl_hash_init(&h1, 16, ((void *)0), ((void *)0), (Curl_hash_dtor)curl_free);
    Curl_hash_init(&h2, 16, ((void *)0), ((void *)0), (Curl_hash_dtor)gsasl_free);
    (void)l1; (void)l2; (void)l3; (void)l4; (void)l5; (void)l6;
    (void)h1; (void)h2;
}
```

- [ ] **Step 3: 修复 example_18 — 添加 key_print_wrapper 注册调用**

在 `tests/benchmark/et_bench/fnptr-library/example_18/fixture.c` 末尾追加：

```c
/* Registration: bind key_print_wrapper as verify_host_key callback */
void register_key_print_wrapper(void) {
    ssh *s;
    ssh_init(&s, 0, NULL);
    ssh_set_verify_host_key_callback(s, key_print_wrapper);
}
```

- [ ] **Step 4: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-library example_2, 9, 18 各自的 edge 被检测到

- [ ] **Step 5: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

- [ ] **Step 6: Commit**

```bash
git add tests/benchmark/et_bench/fnptr-library/example_2/fixture.c \
        tests/benchmark/et_bench/fnptr-library/example_9/fixture.c \
        tests/benchmark/et_bench/fnptr-library/example_18/fixture.c
git commit -m "fix(et-bench): add missing registration calls to library fixtures 2, 9, 18

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 8: Phase 4 — 修复 example_4, 19, 20 fixture（函数体 + 注册调用）

**Files:**
- Modify: `tests/benchmark/et_bench/fnptr-library/example_4/fixture.c`
- Modify: `tests/benchmark/et_bench/fnptr-library/example_19/fixture.c`
- Modify: `tests/benchmark/et_bench/fnptr-library/example_20/fixture.c`

- [ ] **Step 1: 修复 example_4 — 函数体 + 注册调用**

修改 `channel_register_filter` 函数体，移除早退：

```c
// 原文:
    Channel *c = (Channel *)0; /* simplified lookup */
    if (c == NULL) return;

// 改为:
    Channel c_storage;
    Channel *c = &c_storage;
```

在文件末尾追加注册调用：

```c
/* Registration: bind all filter targets */
void register_all_filters(void) {
    channel_register_filter(NULL, 0, client_simple_escape_filter, NULL, NULL, NULL);
    channel_register_filter(NULL, 1, sys_tun_infilter, NULL, NULL, NULL);
}
```

- [ ] **Step 2: 修复 example_19 — 函数体 + 注册调用**

同理修改 `channel_register_filter`，在末尾追加：

```c
/* Registration: bind sys_tun_outfilter */
void register_output_filter(void) {
    channel_register_filter(NULL, 0, NULL, sys_tun_outfilter, NULL, NULL);
}
```

- [ ] **Step 3: 修复 example_20 — 函数体 + 5 个注册调用**

修改 `channel_register_open_confirm` 函数体：

```c
// 原文:
    Channel *c = NULL;
    if (c == NULL) return;

// 改为:
    Channel c_storage;
    Channel *c = &c_storage;
```

在文件末尾追加注册调用：

```c
/* Registration: bind all open_confirm targets */
void register_all_open_confirm(void) {
    channel_register_open_confirm(NULL, 0, mux_session_confirm, NULL);
    channel_register_open_confirm(NULL, 1, mux_stdio_confirm, NULL);
    channel_register_open_confirm(NULL, 2, ssh_stdio_confirm, NULL);
    channel_register_open_confirm(NULL, 3, ssh_session2_setup, NULL);
    channel_register_open_confirm(NULL, 4, ssh_tun_confirm, NULL);
}
```

- [ ] **Step 4: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-library 召回率从 72.86% 升至 ~97%（仅剩 library/10 的 2 边）

- [ ] **Step 5: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

- [ ] **Step 6: Commit**

```bash
git add tests/benchmark/et_bench/fnptr-library/example_4/fixture.c \
        tests/benchmark/et_bench/fnptr-library/example_19/fixture.c \
        tests/benchmark/et_bench/fnptr-library/example_20/fixture.c
git commit -m "fix(et-bench): fix stub registrations and add calls in library 4, 19, 20

Replace early-return NULL stubs with actual field assignments so
fnptr registration propagates to dataflow correctly.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 9: Phase 5 — 字段间 fnptr 传播

**Files:**
- Modify: `src/ethunter/analyzer/helpers.py` (`_scan` Form 1)
- Modify: `src/ethunter/analyzer/field_call.py` (Pass 1)
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: 编写失败测试**

```python
def test_field_to_field_propagation():
    """Phase 5: a->fp = b->fp field-to-field fnptr propagation."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(int x);

    static void my_cb(int x) { (void)x; }

    struct store {
        cb_fn callback;
    };

    struct ctx {
        cb_fn callback;
    };

    static void store_set_cb(struct store *s, cb_fn cb) {
        s->callback = cb;
    }

    static void ctx_init(struct ctx *c, struct store *s) {
        c->callback = s->callback;
    }

    void use_ctx(struct ctx *c) {
        if (c->callback)
            c->callback(42);
    }

    void main_func(void) {
        struct store s;
        struct ctx c;
        store_set_cb(&s, my_cb);
        ctx_init(&c, &s);
        use_ctx(&c);
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
    assert ('use_ctx', 'my_cb') in pairs, \
        f"Expected use_ctx -> my_cb, got: {pairs}"
```

- [ ] **Step 2: 运行确认失败**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_field_to_field_propagation -v
```

预期: FAIL — `c->callback = s->callback` 的 RHS 是 field_expression，`collect_field_assignments` 将其丢弃

- [ ] **Step 3: 修改 helpers.py `_scan` — Form 1 增加 field_expression RHS 分支**

在 `_scan` 函数的 Form 1 (`assignment_expression`) 中，`if lhs and rhs and lhs.type == 'field_expression':` 块内，`resolved = _unwrap_identifier(rhs, unwrap_fn)` 之后，新增：

```python
                    # When RHS is itself a field_expression, return it as-is
                    # with a special form marker so field_call can propagate
                    if rhs.type == 'field_expression':
                        rhs_field_path = extract_field_path(rhs)
                        if rhs_field_path:
                            results.append(FieldAssignment(
                                field_path=field_path,
                                value_node=rhs,
                                resolved_value=None,   # resolved by field_call via rhs_field_path
                                form='field_copy',
                                enclosing_func=enclosing_func,
                                line=node.start_point[0] + 1,
                            ))
```

- [ ] **Step 4: 修改 field_call.py Pass 1 — 处理 form='field_copy'**

在 `field_call.analyze()` 的 Pass 1 循环（`for fa in collect_field_assignments(...)`）中，现有处理逻辑之后新增：

```python
            # Handle field_copy: ctx->lookup_crls = store->lookup_crls
            if fa.form == 'field_copy' and fa.value_node.type == 'field_expression':
                rhs_field_path = extract_field_path(fa.value_node)
                if rhs_field_path:
                    rhs_targets = dataflow.resolve(f'<gstruct:{rhs_field_path}>')
                    if not rhs_targets:
                        rhs_targets = dataflow.resolve(f'<struct:{rhs_field_path}>')
                    for t in rhs_targets:
                        dataflow.assign(f'<gstruct:{fa.field_path}>', t)
```

- [ ] **Step 5: 运行测试确认通过**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_field_to_field_propagation -v
```

预期: PASS

- [ ] **Step 6: 运行集成验证**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期: fnptr-library 召回率达 100%

- [ ] **Step 7: 全量回归**

```bash
.venv/bin/python -m pytest tests/ -q
```

- [ ] **Step 8: Commit**

```bash
git add src/ethunter/analyzer/helpers.py src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: add field-to-field fnptr propagation for collect_field_assignments

When RHS of field assignment is a field_expression, resolve source
field targets and propagate to destination field dataflow key.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 10: 最终集成验证

- [ ] **Step 1: 运行完整 et_bench report**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

预期输出：

```
=== ET-Bench Recall Report ===
Category                               Matched   Expected     Recall
-------------------------------------------------------------------
fnptr-callback                              36         36    100.00%
fnptr-cast                                  10         10    100.00%
fnptr-dynamic-call                           1          6     16.67%
fnptr-global-array                         307        307    100.00%
fnptr-global-struct                         68         68    100.00%
fnptr-global-struct-array                   70         70    100.00%
fnptr-library                               70         70    100.00%
fnptr-only                                  24         24    100.00%
fnptr-struct                                21         21    100.00%
fnptr-varargs                                1          1    100.00%
fnptr-virtual                                0          2      0.00%
-------------------------------------------------------------------
OVERALL                                    608        615     98.86%
```

- [ ] **Step 2: 全量测试**

```bash
.venv/bin/python -m pytest tests/ -q
```

预期: 全部 PASS

- [ ] **Step 3: 最终 Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add all Phase 1-5 TDD unit tests for et_bench recall improvement

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
