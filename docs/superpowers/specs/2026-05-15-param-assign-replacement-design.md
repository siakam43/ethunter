# 架构重构 Phase 2: param_assign 完全替换设计

**日期**: 2026-05-15
**目标**: 修复 3 个 gap，彻底移除 param_assign (786行)，完成 3-Phase pipeline 全替换
**基线**: hybrid pipeline 已运行，100% 召回，FPR 30.54%，156/157 测试通过
**前提**: Phase 1 架构重构已完成（Task 1-10：新模块创建、type-aware key、hybrid pipeline）

## 背景

Phase 1 重构创建了 param_helpers、param_binding、param_dispatch、callback_reg 四个新模块，但 orchestrator 仍以 hybrid 模式运行（param_assign 保留在管线中，新模块仅作 FP 抑制增量）。原因：直接替换时发现 10 个测试失败，根因为 3 个 gap。

## 3 个 Gap 详解

### Gap 1: symbol_names 来源不兼容

**旧代码** (`param_assign.analyze():423`):
```python
symbol_names = symbol_table.all_function_names  # 跨文件，所有声明的函数
```

**新代码** (`param_binding.analyze():40`):
```python
symbol_names = set(func_params.keys())  # 仅当前 TU 中有 function_definition 的函数
```

**差异**: `func_params` 由 `param_helpers.prepare()` 通过 `_collect_func_params` 填充，但该函数只扫描 `function_definition` 节点（含函数体的定义）。跨文件场景中，头文件声明的函数在调用处没有 `function_definition`，`func_params` 中缺失。导致 `if target in symbol_names` 检查失败，参数绑定跳过，后续数据流解析全部失效。

**修复**: 
1. `param_binding.analyze()` 签名改为 `analyze(tree, filepath, symbol_table, dataflow)`，使用 `symbol_table.all_function_names` 作为 `symbol_names`
2. `orchestrator.py` 中 TARGET_RESOLVERS 列表将 `param_binding` 放在第一位，调用时传递 `symbol_table`
3. `func_params` 仅用于参数名映射（`param_names = func_params.get(call_name, [])`），不用于符号名检查

### Gap 2: Registration Gate 逻辑不同

**旧 gate**（`param_assign._collect_call_params:516`）:
```python
if _is_registration(call_name):       # 函数名匹配注册模式 (register/callback/hook/...)
    if not fp_positions or arg_idx in fp_positions:  # 二次检查
        → 产 callback_reg 边 + 写 dataflow
    else:
        → 仅写 dataflow
else:
    → 仅写 dataflow（非注册函数，不产边）
```

**新 gate**（`param_binding._collect_call_params:69-70`）:
```python
fp_params_positions = func_fp_params.get(call_name, set())
if not fp_params_positions or arg_idx in fp_params_positions:
    → 记录为 registration_site（后续 callback_reg 产边）
else:
    → 写 param_mappings
```

**差异**: `func_fp_params.get(call_name, set())` 对于以下情况返回空集 `set()`：
- 函数定义在其他 TU，`prepare()` 未能扫描其 fnptr 参数
- typedef 间接声明的 fnptr 类型未被 `_has_fnptr_declarator` 识别

空集下 `not fp_params_positions` 为 True → **所有实参都成为 registration_site**，callback_reg 会为所有已知函数名实参产边。旧代码用 `_is_registration` 作语义过滤器阻止了这一点。

**修复**: 三层 gating 决策树：

```python
fp_params_positions = func_fp_params.get(call_name, None)  # None vs set() 区分

if fp_params_positions is not None:
    # 已知 fnptr 参数信息（相同 TU 中有函数定义）
    if arg_idx in fp_params_positions:
        → registration_site  # 确定是 fnptr 位置
    else:
        → param_mapping       # 确定不是 fnptr 位置
else:
    # 无 fnptr 参数元信息（函数定义在其他 TU）
    if _is_registration(call_name):
        → registration_site  # 函数名匹配注册模式，保守记录
    else:
        → param_mapping       # 不像注册函数，仅写 dataflow
```

`func_fp_params.get()` 默认值从 `set()` 改为 `None`：空集表示"已知无 fnptr 参数"，None 表示"无信息"。

### Gap 3: registered_callbacks Dead Code

`VariableState.registered_callbacks` 在 `param_assign` callback_reg 产边时被写入，但搜索全部 analyzer 模块发现：**没有任何模块读取它**。删除范围：
- `VariableState`: 删除 `registered_callbacks` 字段 + `register_callback()` 方法
- `DataflowEngine`: 删除 `register_callback()` delegate + `registered_callbacks` property
- `param_assign.py`: 删除 3 处 `dataflow.register_callback(target)` 调用（随 param_assign 删除自然消除）

## 3 个 Gap 修复清单

| Gap | 变更 | 文件 |
|-----|------|------|
| 1 | `param_binding.analyze()` 接收 `symbol_table`，`symbol_names = symbol_table.all_function_names` | param_binding.py, orchestrator.py |
| 2 | 三层 gating：`func_fp_params.get(call_name, None)` + `_is_registration` fallback | param_binding.py |
| 3 | 删除 `registered_callbacks` 字段和 `register_callback` 方法（dead code） | dataflow.py |

## Cleanup: 移除 param_assign 后的整理工作

### 4.1 删除 param_assign.py

移除 `src/ethunter/analyzer/param_assign.py` (786行)。orchestrator 中的 `import param_assign` 删除，所有 `param_assign.analyze()` 和 `param_assign._register_phase()` 调用删除。

`param_helpers.prepare()` 功能完全覆盖 `_register_phase()`：两者均收集 func_params + func_fp_params、注册 param→field mapping、注册 return→field、分类 param_usage。`prepare()` 额外存储 `func_params` 到 engine（_register_phase 不存），这正是 `param_binding` 需要的。`_register_phase()` 删除后不会丢失功能。

### 4.2 移除 Fix B 后处理

当前 orchestrator 末尾有 Fix B 过滤器：

```python
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

替换后不需要：`callback_reg` Phase 3 的 Stage 2 (covered_callees) 在产边前已做此检查。

### 4.3 移除 field_call 双读兼容

Phase 1 重构中 field_call 的 Layer 1 有旧格式双读：

```python
# 保留的旧格式回退（仅当新格式未命中时）
targets = dataflow.resolve(f'<gstruct:{field_path}>')    # 移除
targets = dataflow.resolve(f'<struct:{field_path}>')     # 移除
```

param_assign 删除后旧格式不再有写入者，双读代码删除。

### 4.4 func_fp_params / param_usage 从 state 迁到 engine

Phase 1 重构中 `func_fp_params` 和 `param_usage` 因 `hasattr` 回退链问题留在 `dataflow.state` 上。旧代码移除后，这两个字段需显式声明到 `DataflowEngine`（之前故意未声明以避免 getattr fallback 链冲突），然后直接写入。

**Step A**: 在 `DataflowEngine` 上显式声明 `func_fp_params: dict[str, set[int]]` 和 `param_usage: dict[tuple[str, int], str]`（之前因 hasattr 回退链问题未声明，param_assign 删除后不再有冲突）。

**Step B**: `param_helpers.prepare()` 改为直接写 `dataflow.func_fp_params.update(...)` 和 `dataflow.param_usage.update(...)`，替代当前 `dataflow.state.func_fp_params`。`hasattr(dataflow.state, ...)` guard 删除。

所有读取处改为 `dataflow.func_fp_params` 和 `dataflow.param_usage`，移除 `getattr(dataflow.state, 'xxx', {})` 模式。

### 4.5 移除 hasattr 回退链

当前代码中仍有多个位置使用 `getattr(dataflow, 'xxx', None)` + `hasattr(dataflow, 'state')` 回退模式。全部替换为直接访问 `dataflow.xxx` 或 `dataflow.state.xxx`（取决于字段位置）。

## 最终 3-Phase Pipeline

```
Phase 1a: param_helpers.prepare()
  写入 engine: func_params, func_fp_params, param_usage, param_fields, ret_fields
  (单次 AST 扫描，不产边)

Phase 1: TARGET_RESOLVERS（只写 dataflow，不产任何边）
  执行顺序: param_binding 必须最先运行（direct_assign 需要其 param 映射）
  ├─ param_binding       → <cs>:<caller>:<callee>:<pos>
                            + engine.registration_sites
                            + engine.call_site_targets
  ├─ direct_assign       → <var>:<func>:<name>
  ├─ initializer_assign  → <gstruct>:<type>.<var>.<field>
  └─ cast_assign         → <var>:<func>:<name>

Phase 2: CALL_DETECTORS（读 dataflow，产边）
  ├─ direct_call_fp      → indirect_kind=direct_assign
  ├─ field_call          → indirect_kind=field_call
  ├─ array_call          → indirect_kind=array_call
  └─ param_dispatch      → indirect_kind=callback_param
     └─ Pass A: 函数体内 fnptr 调用 + Pass B: 调点 caller + dedup

        ↓ engine.covered_callees = {field_call 产出的 callee}

Phase 3: callback_reg（三阶段判定）
  产 callback_reg 边
  Stage 1: param_usage 行为检查 (forwarder/storage → skip)
  Stage 2: covered_callees 覆盖检查 (field_call 已覆盖 → skip)
  Stage 3: _is_registration 启发式 fallback (usage==unknown → 子串匹配)

Independent:
  ├─ direct_call         直接调用（先于 Phase 1）
  └─ dlsym_fp            动态加载（最后）
```

## 涉及文件清单

| 文件 | 变更 | 行数变化 |
|------|------|---------|
| `src/ethunter/analyzer/param_assign.py` | **删除** | 786 → ✗ |
| `src/ethunter/analyzer/orchestrator.py` | 移除 param_assign + _register_phase + Fix B；添加 param_binding 到 TARGET_RESOLVERS(首位)；添加 func_fp_params/param_usage 字段声明 | 152 → 130 |
| `src/ethunter/analyzer/param_binding.py` | 签名改为 analyze(tree, filepath, symbol_table, dataflow) + 三层 gating | 207 → 230 |
| `src/ethunter/analyzer/dataflow.py` | 删除 registered_callbacks + register_callback；显式声明 func_fp_params/param_usage 字段 | 222 → 220 |
| `src/ethunter/analyzer/param_helpers.py` | prepare() 写 engine 字段（不再写 state） | ~210 → ~210 |
| `src/ethunter/analyzer/field_call.py` | 移除旧格式双读 + hasattr 回退链（func_fp_params 从 engine 读） | 282 → 240 |
| `src/ethunter/analyzer/param_dispatch.py` | hasattr 回退链替换（func_fp_params → dataflow.func_fp_params） | ~140 → ~140 |
| `src/ethunter/analyzer/callback_reg.py` | hasattr 回退链替换（param_usage → dataflow.param_usage） | ~55 → ~55 |
| `tests/test_et_bench.py` | 移除 xfail marker（type-aware 测试应通过） | +1/-1 |

## 预期结果

| 指标 | 当前 (hybrid) | 目标 (完全替换) |
|------|-------------|---------------|
| ET-Bench 召回 | 100% (8/9 场景) | ≥100% (无回归) |
| FPR | 30.54% | ≤30.54% (不回升) |
| 最大模块行数 | 786 (param_assign) | 450 (initializer_assign) |
| hasattr 回退链 | 22 处 | 0 处 |
| 总模块行数 | ~2770 | ~2480 |
| 删除代码 | - | param_assign 786 + Fix B 15 + 双读 10 = ~811 行 |

## 风险

| 风险 | 缓解 |
|------|------|
| 三层 gating 中的 `_is_registration` fallback 与旧 `param_assign` 行为不同 | TDD 先行，对 `test_p2_callback_reg_only_fnptr_positions` 等已有测试逐例验证 |
| `func_params` 迁移后跨文件参数名查找不可用 | 降级：无 `param_names` 时仅做位置检查（`arg_idx in fp_positions`），不写命名 key |
| 删除 param_assign 后 edge 总数变化 | ET-Bench 全量测试 guard：recall ≥ 100% 且 FPR ≤ 30.54% 才能通过 |
