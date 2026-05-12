# fnptr-struct 数据流引擎增强设计

**日期**: 2026-05-11
**状态**: 待审批
**类型**: Architecture / Enhancement

## 问题陈述

ethunter 在 ET-Bench 的 fnptr-struct 类别中有 5 个失败案例，召回率 57.14%（12/21）：

| Example | 缺失边 | 根本原因 |
|---|---|---|
| example_2 | cpp_pop_definition→dump_queued_macros | 单遍 AST 遍历顺序依赖：赋值在调用之后 |
| example_5 | iterate_through_spacemap_logs_cb→count_unflushed_space_cb 等 5 条 | 跨函数参数传播断裂：参数→struct field 链路未追踪 |
| example_9 | security_callback_debug→ssl_security_default_callback | 返回值追踪缺失：RHS 是 call_expression 且返回值是另一个 struct field |
| example_12 | s_server_main→alpn_cb 等 | 注册函数参数传播断裂：set_* 函数的参数未映射到 struct field |
| example_13 | CRYPTO_gcm128_encrypt→aesni_encrypt | 嵌套 cast 解析缺失 + local variable 从 struct field 提取后通过 `(*block)()` 调用 |

## 架构设计

### 核心思路

将 `dataflow.py` 的简单 key-value 存储升级为支持 **跨函数传播** 的数据流引擎 `DataflowEngine`，同时保持与现有 `VariableState` 接口完全向后兼容。

```
┌─────────────────────────────────────────────────┐
│                Analyzer Modules                  │
│  field_call | initializer_assign | param_assign  │
└──────────────────────┬──────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────┐
│           DataflowEngine                         │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────┐ │
│  │ VariableState│ │ ParamTracker │ │ RetTracker│ │
│  │ (向后兼容)   │ │ (参数传播)   │ │ (返回值) │ │
│  └──────────────┘ └──────────────┘ └──────────┘ │
│  ┌──────────────┐                                │
│  │ CastResolver  │                                │
│  │ (嵌套cast)   │                                │
│  └──────────────┘                                │
└───────────────────────────────────────────────────┘
```

### 向后兼容保证（关键约束）

1. **DataflowEngine 完全代理 VariableState 的 assign/resolve/merge 接口**，现有 analyzer 调用 `.assign()` / `.resolve()` 的行为与原来完全一致
2. **新增方法仅在需要的地方调用**，不需要的场景不受影响
3. **不在 orchestrator 层面增加新的 AST 遍历轮次**，所有注册和推导都在现有 analyzer 模块的内部遍历中完成（各模块内部已有多遍遍历，这是现有模式）
4. **CastResolver 失败时返回 None**，调用方 fallback 到原行为（静默跳过），不引入假阳性
5. **Orchestrator 管线顺序不变**，所有改动在现有模块内部完成

## 详细设计

### 1. DataflowEngine 核心类

**文件**: `src/ethunter/analyzer/dataflow.py`

```python
@dataclass
class DataflowEngine:
    """Cross-function dataflow engine for function pointer tracking.

    Wraps VariableState (backward compatible) and adds:
    - ParamTracker: parameter-to-field propagation across function calls
    - RetTracker: return value tracking for struct field function pointers
    - CastResolver: nested cast expression unwrapping
    """
    state: VariableState = field(default_factory=VariableState)

    # Parameter propagation: (func_name, param_position) -> {field_path}
    param_fields: dict[tuple[str, int], set[str]] = field(default_factory=dict)

    # Return value tracking: func_name -> set of field paths returned
    ret_fields: dict[str, set[str]] = field(default_factory=dict)

    # Alias tracking: alias_name -> real_name
    # Reserved for future use. Current alias resolution is done via dataflow keys
    # in field_call's fallback (dataflow.resolve(parts[0])).
    aliases: dict[str, str] = field(default_factory=dict)

    # === Backward compatible interface ===

    def assign(self, var_name: str, target: str) -> None:
        """Delegate to VariableState.assign."""
        self.state.assign(var_name, target)

    def resolve(self, var_name: str) -> set[str]:
        """Delegate to VariableState.resolve."""
        return self.state.resolve(var_name)

    def merge(self, src_var: str, dst_var: str) -> None:
        """Delegate to VariableState.merge."""
        self.state.merge(src_var, dst_var)

    @property
    def targets(self) -> dict[str, set[str]]:
        """Expose VariableState.targets for field_call's suffix scan."""
        return self.state.targets

    # === New: ParamTracker ===

    def register_param_mapping(
        self,
        func_name: str,
        param_idx: int,
        field_path: str,
        struct_param_idx: int = 0,
    ) -> None:
        """Register that a function stores a parameter into a struct field.

        Example: SSL_CTX_set_alpn_select_cb(ctx, cb) stores cb into ctx->ext.alpn_select_cb
        → register_param_mapping("SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb")
        """
        key = (func_name, param_idx)
        if key not in self.param_fields:
            self.param_fields[key] = set()
        self.param_fields[key].add(f"<gstruct:{field_path}>")

    def resolve_call_site_param(
        self,
        func_name: str,
        param_idx: int,
        arg_name: str,
        symbol_names: set[str] | None = None,
    ) -> set[str]:
        """Resolve what targets the call-site argument has, and propagate to field paths.

        Returns the set of function names that arg_name resolves to.
        Also writes those targets into the registered field paths in dataflow.
        """
        key = (func_name, param_idx)
        if key not in self.param_fields:
            return set()

        # Step 1: Try dataflow resolve (for variables that were assigned)
        arg_targets = self.state.resolve(arg_name)

        # Step 2: If arg_name itself is a known function name, add it directly
        if symbol_names and arg_name in symbol_names:
            arg_targets.add(arg_name)

        if not arg_targets:
            return set()

        for target in arg_targets:
            for field_key in self.param_fields[key]:
                self.state.assign(field_key, target)

        return arg_targets

    # === New: RetTracker ===

    def register_return(self, func_name: str, field_path: str) -> None:
        """Register that a function returns a struct field function pointer.

        Example: SSL_CTX_get_security_callback returns ctx->cert->sec_cb
        → register_return("SSL_CTX_get_security_callback", "cert->sec_cb")
        """
        if func_name not in self.ret_fields:
            self.ret_fields[func_name] = set()
        self.ret_fields[func_name].add(field_path)

    def resolve_returned_field(
        self,
        func_name: str,
    ) -> set[str]:
        """Resolve the targets of the field path that func_name returns."""
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            targets = self.state.resolve(f"<gstruct:{field_path}>")
            results.update(targets)
        return results

    # === New: CastResolver ===

    def unwrap_cast(self, node) -> str | None:
        """Recursively unwrap nested cast expressions.

        (T1)(T2)func  →  "func"
        (T1)(uintptr_t)cb  →  "cb"

        Returns None if the node is not a cast/pointer/paren expression,
        or if no identifier can be extracted. Callers should fallback
        to their existing behavior.
        """
        if node.type == 'identifier' and node.text:
            return node.text.decode('utf-8')

        if node.type == 'cast_expression':
            for child in reversed(node.children):
                result = self.unwrap_cast(child)
                if result:
                    return result
            return None

        if node.type == 'pointer_expression':
            operand = node.child_by_field_name('argument')
            if operand:
                return self.unwrap_cast(operand)

        if node.type == 'parenthesized_expression':
            inner = node.child_by_field_name('expression')
            if inner is None and len(node.children) >= 2:
                inner = node.children[1]
            if inner:
                return self.unwrap_cast(inner)

        return None
```

### 2. field_call.py 两遍扫描

**文件**: `src/ethunter/analyzer/field_call.py`

**改动**: 将 `analyze()` 函数的单遍遍历拆分为两遍。

**Pass 1**（`_collect_assignments`）：遍历整个文件，收集所有 `field = identifier` 赋值写入 dataflow。逻辑与原来单遍中的赋值收集**完全一致**，只是提前执行。

**Pass 2**（`_visit`）：只检测 call sites，不再修改 dataflow。call detection 逻辑完全不变。

```python
def analyze(tree, filepath, symbol_table, dataflow):
    edges = []
    symbol_names = symbol_table.all_function_names

    # Pass 1: collect all field assignments
    def _collect_assignments(node):
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left')
            rhs = node.child_by_field_name('right')
            if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                target = rhs.text.decode('utf-8')
                if target in symbol_names:
                    field_path = extract_field_path(lhs)
                    if field_path:
                        dataflow.assign(f'<gstruct:{field_path}>', target)
        for child in node.children:
            _collect_assignments(child)

    _collect_assignments(tree.root_node)

    # Pass 2: detect call sites (existing logic unchanged)
    def _visit(node):
        if node.type == 'call_expression':
            # ... existing call detection ...
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

**影响分析**：只对正向收益有影响（赋值提前写入），不会漏掉原来能检测到的边。

### 3. param_assign.py 增强

**文件**: `src/ethunter/analyzer/param_assign.py`

**现有结构**：param_assign 已有 4 遍遍历 — Pass 1（`_collect_func_params` 收集函数签名）、Pass 2（`_collect_call_params` 收集 call-site 参数映射）、Pass 3（`_visit` 解析 struct member 赋值）、Pass 4（`_detect_param_calls` / `_collect_call_args_pass4` 检测参数调用）。核心数据结构 `param_mappings: dict[str, set[str]]` 是函数内局部的。

**增强的核心目标**：将局部的 `param_mappings` 升级为可跨文件查找的全局注册机制 `DataflowEngine.param_fields`。

**改动 1 — 定义体扫描：注册参数 → struct field 映射**

在 `_visit`（Pass 3）中，当匹配到 `field_expression = identifier` 模式时，**除了**现有的 `dataflow.assign(f'<struct:{field_path}>', target)` 外，还需要：

当 RHS 是函数参数（通过比对 `func_params` dict）时：

```
function_definition
  └─ body
       └─ expression_statement
            └─ assignment_expression
                 ├─ left: field_expression (operand 是 identifier 且该 identifier 是函数参数)
                 └─ right: identifier (且该 identifier 是函数参数)
```

例如 `ctx->ext.alpn_select_cb = cb`（在 `SSL_CTX_set_alpn_select_cb(SSL_CTX *ctx, void (*cb)(void))` 中）：
- `ctx` 是参数（第 0 个），`cb` 是参数（第 1 个）
- **新增调用**：`engine.register_param_mapping("SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb", struct_param_idx=0)`
- 这一步将 "param cb 会存入 ctx 的 ext.alpn_select_cb field" 注册为全局映射

**改动 2 — 定义体扫描：注册返回值追踪**

新增一个轻量遍历（可与 `_collect_func_params` 合并），匹配 `return field_expression` 模式：

```
function_definition
  └─ body
       └─ return_statement
            └─ field_expression (operand 是 identifier 且该 identifier 是函数参数)
```

例如 `return ctx->cert->sec_cb`：
- 调用 `engine.register_return("SSL_CTX_get_security_callback", "ctx->cert->sec_cb")`

**简化处理**：如果返回值的 operand 不是直接参数而是局部变量（如 `cert` 是 `ctx->cert` 的副本），暂时不追溯。先只处理 `return param->field` 的最简单情况。

**改动 3 — call-site 传播（含 cast 包裹参数）**

在 `_collect_call_params`（Pass 2）中，现有代码（第 126 行）只处理 `c.type == 'identifier'` 的参数。对于 cast_expression 包裹的参数（如 `(block128_f)aesni_encrypt`）：

- **新增**：当参数节点是 `cast_expression` 时，调用 `engine.unwrap_cast(node)` 递归提取最内层标识符
- 如果提取的标识符在 `symbol_names` 中，按现有逻辑处理（注册到 param_mappings 或 emit callback edge）
- 同时调用 `engine.resolve_call_site_param(func_name, arg_idx, arg_name)`（需要传入 symbol_names 以便 resolve_call_site_param 能识别 bare function names）

**改动 4 — assignment RHS 是 call_expression**

在 `_visit`（Pass 3）中，增加对 `field_expression = call_expression` 模式的处理：

```
assignment_expression
  ├─ left: field_expression (如 sdb.old_cb)
  └─ right: call_expression (如 SSL_CTX_get_security_callback(ctx))
```

- 提取被调函数名
- **新增调用**：`engine.resolve_returned_field(func_name)` 获取返回值对应的 targets
- 将 targets 写入 `engine.assign(f'<gstruct:{field_path}>', target)`

### 4. initializer_assign.py 增强

**文件**: `src/ethunter/analyzer/initializer_assign.py`

**现有结构**：`_extract_cast_target` 目前只处理一层 cast（`node.child_by_field_name('value')` 且 value 是 identifier）。`_process_init_list` 对 designated initializer 的 value 调用 `_extract_function_from_value`，内部会调用 `_extract_cast_target`。

**改动**：在 `_extract_cast_target` 中，当 value 不是直接 identifier 而是嵌套 cast/pointer/paren 时：
1. 通过 `dataflow.unwrap_cast(node)` 递归解嵌套（`dataflow` 参数现在是 `DataflowEngine` 实例）
2. 如果返回有效标识符且在 symbol_names 中，使用它作为 target
3. 如果返回 None，保持原有的 fallback 行为（静默跳过）

**注意**：如果 `unwrap_cast` 返回的标识符不在 `symbol_names` 中（例如它是一个参数名而非已知函数名），`_extract_cast_target` 应返回 None，由 param_assign 的 call-site 参数传播处理。

这直接影响 example_5 的 `.uic_cb = (unflushed_iter_fn_t *)(uintptr_t)cb` 和 example_13 的 `(block128_f)aesni_encrypt`。

### 5. Orchestrator 集成

**文件**: `src/ethunter/analyzer/orchestrator.py`

**现有管线结构**：

```
direct_call (per-file, uses symbol_names only)
    ↓
Phase 1: TARGET_RESOLVERS (per-file, writes dataflow)
    direct_assign → initializer_assign → cast_assign → param_assign
    ↓
Phase 1b: param_assign.analyze() (per-file, returns edges)
    ↓
Phase 2: CALL_DETECTORS (per-file, reads dataflow)
    direct_call_fp → field_call → array_call
    ↓
dlsym_fp (independent, per-file)
    ↓
Deduplication
```

**关键约束**：Phase 1 是按文件遍历的——file A 的 param_assign 结果写入 dataflow 后，file B 的 param_assign 可以读到（因为 dataflow 是全局的）。但 file A 的 param_assign 在 Phase 1 注册的全局映射（`engine.param_fields`），要在 Phase 1b 的 call-site 传播中被使用，需要确保 `param_fields` 在 Phase 1 遍历所有文件后已完全填充。

**改动方案**：

由于 `DataflowEngine` 的 `assign/resolve/merge/targets` 接口与 `VariableState` 完全一致，只需在 `run_all_analyses()` 入口处包装：

```python
def run_all_analyses(trees, symbol_table, dataflow):
    from ethunter.analyzer.dataflow import DataflowEngine
    engine = DataflowEngine(state=dataflow)
    # Pass engine to all analyzers — backward compatible for those that only use assign/resolve
    # ... rest of pipeline unchanged ...
```

**但有一个问题**：TARGET_RESOLVERS 按文件遍历（file A 跑完所有 resolver 才开始 file B），且 param_assign 的 Pass 2（call-site 收集）在 Pass 3（注册）之前执行。这意味着：

- **单文件内**：注册和传播在同一个 analyze() 调用中完成，无问题
- **跨文件时**：file B 的 Pass 2 执行时，file A 的 Pass 3 还没运行（file B 开始于 file A 完成后），所以 file A 的注册对 file B 的 Pass 2 不可见

**解决方案（两阶段）**：

1. **Phase 1a（新增预扫描）**：在所有文件上运行 param_assign 的"注册逻辑"（Pass 3 的 `field = param` 匹配），只填充 `engine.param_fields`，不做传播
2. **Phase 1b（正式遍历）**：按现有管线运行 TARGET_RESOLVERS 和 CALL_DETECTORS，param_assign 的 call-site 传播可以使用已填充的 `engine.param_fields`

或者更简单的**渐进方案**：先只处理单文件内的注册和传播（大多数情况注册函数和调用方在同一文件），跨文件能力作为后续扩展。spec 保留两阶段方案作为设计目标，实施时可以评估复杂度决定是否只做单文件。

**analyzer 兼容性矩阵**：

| Analyzer | 使用接口 | 是否需要改动 |
|---|---|---|
| direct_call | `symbol_names`（SymbolTable） | 否 |
| direct_assign | `dataflow.assign()`, `dataflow.resolve()` | 否（接口兼容） |
| initializer_assign | `dataflow.assign()`, `dataflow.resolve()` + **新增 unwrap_cast** | 是（新增调用 unwrap_cast） |
| cast_assign | `dataflow.assign()`, `dataflow.resolve()` | 否（接口兼容） |
| param_assign | `dataflow.assign()`, `dataflow.resolve()` + **新增 register_param_mapping / resolve_call_site_param / register_return / resolve_returned_field** + **新增 call-site 参数提取时调用 unwrap_cast** | 是（4 处新增调用） |
| local_fp_tracker | `dataflow.resolve()` | 否（接口兼容，但依赖 dataflow 被 param_assign 填充） |
| field_call | `dataflow.assign()`, `dataflow.resolve()`, `dataflow.targets` + **新增两遍扫描** | 是（内部重构为两遍） |
| array_call | `dataflow.assign()`, `dataflow.resolve()` | 否（接口兼容） |
| dlsym_fp | `dataflow.assign()`, `dataflow.resolve()` | 否（接口兼容） |
| direct_call_fp | `dataflow.resolve()`, `local_fp_tracker.collect_local_fp_assignments()` | 否（间接受益于 param_assign 填充 dataflow） |

只有需要新能力的 analyzer 才调用新方法，其余模块无需改动。

### 6. 现有测试保护

**约束**：所有现有测试（`test_analyzers.py`、`test_cross_file.py`、`test_et_bench.py` 中原本通过的例子）必须全部通过。新增能力只增加正向收益，不能改变已有行为。

### 7. TDD 测试用例设计

**新增单元测试**（文件 `tests/test_dataflow_engine.py`）：

| # | 测试名 | 验证什么 | fixture 要求 |
|---|---|---|---|
| 1 | `test_field_call_two_pass_order` | field_call Pass 1 先收集赋值，Pass 2 检测调用，赋值在调用之后时仍能检测到边 | 单 C 文件：先定义 `fn_caller()` 调 `obj.cb()`，后定义 `fn_init()` 写 `obj.cb = handler` |
| 2 | `test_unwrap_cast_nested` | `DataflowEngine.unwrap_cast()` 递归解嵌套 cast | 纯 Python 测试，构造 mock tree-sitter node |
| 3 | `test_register_param_mapping` | `register_param_mapping` 注册 + `resolve_call_site_param` 传播 | 纯 Python 测试 |
| 4 | `test_resolve_returned_field` | `register_return` 注册 + `resolve_returned_field` 查找 | 纯 Python 测试 |
| 5 | `test_resolve_call_site_bare_function` | `resolve_call_site_param` 能识别 bare function name（非 dataflow tracked 变量） | 纯 Python 测试 |

**新增集成测试**（在 `tests/test_et_bench.py` 中验证 ET-Bench 结果）：

| # | 场景 | 验证 |
|---|---|---|
| 6 | `test_et_bench_fnptr_struct_example_2` | cpp_pop_definition→dump_queued_macros |
| 7 | `test_et_bench_fnptr_struct_example_13` | CRYPTO_gcm128_encrypt→aesni_encrypt（端到端链路） |
| 8 | `test_et_bench_fnptr_struct_example_12` | s_server_main→alpn_cb |
| 9 | `test_et_bench_fnptr_struct_full_recall` | fnptr-struct 类别整体召回率达到 100% |

**回归保护**：

- 所有现有测试（`test_analyzers.py`、`test_cross_file.py`、`test_et_bench.py` 中原本通过的例子）必须全部通过
- 每个测试先写、确认失败，再实现对应能力、确认通过

### 8. 实施顺序

各改动之间存在依赖关系，必须按以下顺序实施：

| 顺序 | 改动 | 依赖 |
|---|---|---|
| 1 | `DataflowEngine` 核心类（dataflow.py） | 无 |
| 2 | `field_call.py` 两遍扫描 | DataflowEngine |
| 3 | `unwrap_cast`（DataflowEngine 方法） | DataflowEngine |
| 4 | `initializer_assign.py` 调用 unwrap_cast | unwrap_cast |
| 5 | `param_assign` 改动 1（register_param_mapping） | DataflowEngine |
| 6 | `param_assign` 改动 2（register_return） | DataflowEngine |
| 7 | `param_assign` 改动 3（call-site unwrap_cast + resolve_call_site_param） | 改动 5 + unwrap_cast |
| 8 | `param_assign` 改动 4（resolve_returned_field） | 改动 6 |
| 9 | `orchestrator.py` 包装 DataflowEngine | 以上所有 |

**实施策略**：每完成一步，运行测试确认不引入回归。建议按 1→2→3→4→5→6→7→8→9 顺序，或者将 1+2 作为第一批（解决 example_2），3+4 作为第二批（解决 cast 提取），5~8 作为第三批（解决跨函数传播），9 作为集成。

### 9. 类型安全与降级策略

param_assign 和 initializer_assign 中调用新方法时（如 `dataflow.register_param_mapping(...)`），必须使用 `hasattr` 检查：

```python
if hasattr(dataflow, 'register_param_mapping'):
    dataflow.register_param_mapping(...)
```

这样如果直接传入 `VariableState` 实例（例如单元测试或外部调用），新方法不会被调用，不会抛出 `AttributeError`。这也意味着 `VariableState` 本身不会被修改。

## 影响范围总结

| 改动文件 | 改动性质 | 风险 |
|---|---|---|
| dataflow.py | 新增 DataflowEngine 类，VariableState 保持不变 | 低（新增类，不修改原有逻辑） |
| field_call.py | 两遍扫描，逻辑拆分 | 低（正向收益，赋值提前写入不会漏边） |
| param_assign.py | 新增 4 个方法调用：register_param_mapping / resolve_call_site_param / register_return / resolve_returned_field | 低（仅在匹配的 AST 模式触发，无模式时静默跳过） |
| initializer_assign.py | unwrap_cast 调用 | 低（返回 None 时保持原有 fallback） |
| orchestrator.py | 包装 DataflowEngine，传入各 analyzer | 低（接口完全兼容） |
