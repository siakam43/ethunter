# 架构级重构设计：管线解耦与 Dataflow 类型化

**日期**: 2026-05-14
**目标**: 系统性解决六大架构缺陷，在不降低召回率的前提下将误报率从 35.76% 降至 <15%
**排除场景**: fnptr-dynamic-call (dlsym 动态加载)、fnptr-virtual (C++ 虚表)
**基线**: ET-Bench 44/44 通过，召回 98.86%，误报 35.76% (339/947)

## 六大架构缺陷 (Root Causes)

1. **管线时序悖论**: param_assign 同时写 dataflow 和产边，产 callback_reg 时不知道 field_call 会覆盖哪些 callee → 依赖后处理 Fix B 亡羊补牢
2. **Dataflow 无类型全局命名空间**: 7 种 key 格式无统一设计，同名变量/字段跨函数跨类型污染
3. **Suffix Fallback 误报放大器**: field_call 的 `key.endswith('.fieldname>')` 全局 wildcard scan 是最大的单一 FP 来源
4. **param_assign God Module**: 786 行实现内部 4-Pass 管线 + 宏展开 + 注册检测 + 参数传播 + 返回值追踪
5. **累积式修复**: 6 层 Fix (A→B→A-1→P0→P2→P3→1→3) 逐层叠加 guard condition，Fix 5 因循环依赖被回退
6. **误报无测试覆盖**: FP 约束仅通过 fpr_ceilings 阈值检查，无 per-scenario FP 单元测试

## 新 3-Phase 管线架构

```
Phase 1a: CROSS-FILE PRE-SCAN
  所有文件预扫描 → 写入跨文件共享状态
  └─ param_helpers.prepare() → func_params, func_fp_params,
                                param_fields, ret_fields, param_usage

Phase 1: TARGET RESOLUTION (只写 dataflow，不产任何边)
  ├─ direct_assign       → 局部变量赋值
  ├─ initializer_assign  → 全局初始化器 (type-aware key)
  ├─ cast_assign         → cast 赋值
  └─ param_binding       → 参数→函数绑定 + registration_sites 记录

Phase 2: CALL DETECTION (读 dataflow，产边)
  ├─ direct_call_fp      → fp() 直接调用
  ├─ field_call          → obj->handler()  (type-aware 查找)
  ├─ array_call          → arr[i]() 数组调用
  └─ param_dispatch      → fnptr param 调用 (原 Pass 3 + Pass 4)

       ↓ 构建 covered_callees = {field_call 产出的所有 callee}

Phase 3: REGISTRATION DETECTION (检查 covered_callees + param_usage)
  └─ callback_reg        → 回调注册边 (三阶段判定)

Independent:
  ├─ direct_call         → 直接调用 (先于 Phase 1 运行)
  └─ dlsym_fp            → 动态加载 (最后运行)
```

**关键**: Phase 1 所有模块只写 dataflow 不产边，Phase 3 在 covered_callees 就绪后运行，不再需要 Fix B 后处理。

## 统一 Dataflow Key 系统 (4种)

| Key 格式 | 示例 | 用途 |
|----------|------|------|
| `<gstruct>:<type>.<var>.<field>` | `<gstruct>:zfs_command_t.command_table.func>` | 全局结构体字段 |
| `<garray>:<var_name>` | `<garray>:object_viewer` | 全局数组 |
| `<var>:<func>:<var_name>` | `<var>:setup:fp` | 函数作用域局部变量 (direct_assign 写入, direct_call_fp/param_binding 读取) |
| `<cs>:<caller>:<callee>:<pos>` | `<cs>:setup:register_callback:1>` | Per-call-site 调点追踪 (param_binding 写入, param_dispatch 读取) |

**废弃**: 裸变量名 `"cb"`、`"<struct:>"`、`"<chain:>"`、`"<vtable:>"`、`"{call_name}:{pname}"`

## 模块拆分: param_assign (786行) → 4 文件

| 新文件 | 行数 | Phase | 职责 |
|--------|------|-------|------|
| `param_helpers.py` | ~200 | 1a | `prepare()`: 集中收集 func_params/func_fp_params/macros/param_usage + param_field/ret_field 注册。只写 engine，不产边 |
| `param_binding.py` | ~180 | 1 | 参数绑定: 读 engine 中元数据，处理调点参数映射 + struct 赋值 → 写 dataflow + registration_sites |
| `param_dispatch.py` | ~160 | 2 | Fnptr 调用检测: Pass A (函数体内 cb()) + Pass B (调点 caller) + Pass A/B 去重 |

### param_dispatch Pass A/B 去重规则

当前 Pass 3 产 (inner_func, target)，Pass 4 产 (outer_caller, target)。当 inner_func 调用了 fnptr param 时，outer_caller→target 和 inner_func→target 都会被产出，形成 O(N×M) 膨胀。

新规则: 对同一个 `(target, arg_idx)` 对，如果 Pass A 已产 `(inner_func, target)`，则 Pass B 跳过 `(outer_caller, target)` 当 `outer_caller != inner_func` 时。这保留"直接 fnptr 调用者"视角，丢弃"间接调用者"视角（因为间接视角的信息质量更低）。
| `callback_reg.py` | ~130 | 3 | 注册检测: param_usage 行为判定 + covered_callees 覆盖判定 + _is_registration fallback |

## DataflowEngine 扩展

```python
@dataclass
class DataflowEngine:
    state: VariableState

    # === 已有 (不变) ===
    param_fields: dict[tuple[str, int], set[str]]
    ret_fields: dict[str, set[str]]
    aliases: dict[str, str]
    param_alias_map: dict[tuple[str, str], str]  # (func, local_var) → global_name (从 state 迁移)
    unwrap_cast: Callable  # CastResolver

    # === 已有 (从 state 迁移到 engine) ===
    func_fp_params: dict[str, set[int]]
    param_usage: dict[tuple[str, int], str]
    func_params: dict[str, list[str]]   # 新增: 跨文件 param 名查找

    # === 新增 ===
    registration_sites: list[dict]  # [{caller, callee, arg_idx, target, file, line}]
    covered_callees: set[str]
```

`registration_sites` 条目格式:
```python
{"caller": "setup", "callee": "register_callback", "arg_idx": 1,
 "target": "my_handler", "file": "test.c", "line": 42}
```

**不再使用 `hasattr(dataflow, 'xxx')` 的条件分支**: 所有 analyzer 统一接收 DataflowEngine。

### func_fp_params 跨模块访问变更

当前 `func_fp_params` 分别存储在 `dataflow.state.func_fp_params` 或 `dataflow.func_fp_params`（取决于调用路径），读取处需要 `hasattr` 回退链：

| 模块 | 当前访问方式 | 迁移后 |
|------|-------------|--------|
| `param_assign.py` (→ 4新文件) | `getattr(dataflow, 'func_fp_params', None)` + `getattr(dataflow.state, ...)` | `dataflow.func_fp_params` |
| `field_call.py:212-214` | `getattr(dataflow, 'func_fp_params', None)` + `getattr(dataflow.state, ...)` | `dataflow.func_fp_params` |

`field_call.py` 是 param 模块之外唯一读写 `func_fp_params` 的地方（用于 callback-of-callback 检测）。迁移后统一路径为 `dataflow.func_fp_params`，`hasattr` 回退链删除。

### Phase 1a → Phase 1 契约

**`param_helpers.prepare()` 写入 engine 的字段:**

| 字段 | 类型 | 说明 |
|------|------|------|
| `engine.func_params` | `dict[str, list[str]]` | 函数名 → 参数名列表（跨文件累积，新增） |
| `engine.func_fp_params` | `dict[str, set[int]]` | 函数名 → fnptr 参数位置集合 |
| `engine.param_usage` | `dict[tuple[str, int], str]` | (函数, 位置) → caller/forwarder/storage |
| `engine.param_fields` | `dict[tuple[str, int], set[str]]` | (函数, 位置) → struct field 路径集合 |
| `engine.ret_fields` | `dict[str, set[str]]` | 函数 → 返回的 struct field 路径集合 |
| `engine.param_alias_map` | `dict[tuple[str,str], str]` | (函数, 局部变量) → 全局结构体名（从 state 迁移） |

`func_params` 是新增强制写入——当前代码在 `_register_phase` 中不在跨文件存储 `func_params`，导致跨文件调点参数映射失败时退化到跳过。重构后在 `prepare()` 中通过 `engine.func_params.update()` 跨文件累积。

**`param_binding.analyze()` 从 engine 读取:**

- `engine.func_params`：跨文件参数名查找（替代当前每文件局部收集）
- `engine.func_fp_params`：判断调点位置是否为 fnptr 参数
- `engine.param_usage`：Phase 3 使用（暂不在此阶段读取）
- Macros：由 `analyze()` 内部调用 `param_helpers._collect_simple_macros(tree)` 每文件收集（宏是文件作用域，不能跨文件共享）

**`_propagate_call_site` type 信息传递:**

`resolve_call_site_param(func_name, arg_idx, arg_name)` 当前写入 `<gstruct:{field_path}>` key。type-aware 改造后：

1. `param_fields[(func_name, arg_idx)]` 存储了 field_path + struct_param_idx
2. 在调点处，struct_param_idx 位置的实参名可从 AST 获取
3. `symbol_table.get_var_type(struct_arg_name)` 查询 struct 变量的类型
4. 若类型已知，写 `<gstruct>:<type>.<struct_arg>.<field_path>`；若未知，降级写 `<gstruct>:<struct_arg>.<field_path>`（保留变量名区分）

**已知限制:**
- **跨文件参数名缺失:** 若 head file 中的函数声明只给出参数类型无参数名，`func_params` 的参数名位置信息不可用。降级行为：按 `func_fp_params` 进行位置匹配（仅验证 arg_idx in fp_positions），不产出命名 mapped key。
- **`var_to_type` 同名覆盖:** `record_var_type("name", "type")` 使用全局 key，不同文件中同名不同类型的变量后者覆盖前者。对于 `struct ssl_ctx_st *ctx` 和 `struct net_ctx_st *ctx` 并存的场景，类型查询结果取决于文件处理顺序。这是全局符号表的固有限制，不在本次重构范围内解决。

## SymbolTable 类型追踪扩展

```python
class SymbolTable:
    _var_types: dict[str, str]           # var_name → struct_type_name
    _struct_fields: dict[str, list[str]]  # struct_type → [field_names]

    def record_var_type(self, var_name: str, struct_type: str) -> None: ...
    def get_var_type(self, var_name: str) -> str | None: ...
    def record_struct_fields(self, struct_type: str, fields: list[str]) -> None: ...
    def get_struct_fields(self, struct_type: str) -> list[str]: ...
```

数据来源:
- `initializer_assign`: 扫描 `struct type var[] = {...}` 声明 → `record_var_type("var", "type")`
- `direct_assign`: 扫描 `struct type *ptr = ...` → `record_var_type("ptr", "type")`
- 各模块扫描 `struct_specifier` AST 节点 → `record_struct_fields("type", [fields])`

## field_call 查找策略: 12层 → 5层

```
Layer 1: 精确 key 查找
  "<gstruct>:<type>.<full_path>"
  → type 从 symbol_table.get_var_type(base_var) 获取

Layer 2: 类型限定 suffix scan (替代原全局 wildcard)
  keys.startswith("<gstruct>:<type>.") and keys.endswith(".<fieldname>")
  → 同 struct type 的不同实例，不跨类型扫描

Layer 3: <garray>:<base_var>
  → 数组元素直接调用

Layer 4: 指针别名解析
  → pointer_resolutions[base_var] → 重新 Layer 1

Layer D (Degradation): 当 type 未知时的安全网
  仅扫描 <gstruct>:* 前缀 keys（不含 <struct>:<garray>:<chain>: 等）
  → key.startswith("<gstruct>:") and key.endswith(".<fieldname>")
  → 限定在 struct field 域内，避免裸变量名/<struct>/<garray> 碰撞
  → 仅当 Layer 1-4 全部失败时触发
```

**移除**: 全局无限制 `key.endswith('.fieldname>')` wildcard scan — 替换为 Layer D 的 `<gstruct>:` 前缀限定扫描

## callback_reg 三阶段判定

```
Stage 1: 行为判定 (param_usage)
  'forwarder' | 'storage' → 跳过
  'caller' → 继续

Stage 2: 覆盖判定 (covered_callees)
  target in covered_callees → 跳过 (field_call 已覆盖)

Stage 3: 启发式 fallback
  usage == 'unknown' and _is_registration(callee) → 产边
```

`_is_registration` 和 `REG_PATTERNS` 从主要判定逻辑降级为 unknown 情况的保守 fallback。

## 模块清单

```
src/ethunter/analyzer/
├─ orchestrator.py          150→180  3-Phase 管线
├─ dataflow.py              208→260  新增 registration_sites/covered_callees/var_to_type
├─ symbol_table.py          141→170  新增 record_var_type/get_var_type/get_struct_fields
├─ helpers.py               293→293  不变
│
├─ param_helpers.py         NEW~200  共享工具 + prepare() 入口
├─ param_binding.py         NEW~200  Phase 1: 参数绑定
├─ param_dispatch.py        NEW~160  Phase 2: fnptr 调用
├─ callback_reg.py          NEW~130  Phase 3: 注册检测
│
├─ direct_call.py            86→86   不变
├─ dlsym_fp.py               58→58   不变
├─ direct_assign.py         121→130  写 <var>:<enclosing_func>:<var_name> 替代裸变量名
├─ initializer_assign.py    420→450  写 type-aware key
├─ cast_assign.py            62→70   写 <var>:<enclosing_func>:<var_name> 替代裸变量名
├─ direct_call_fp.py         84→90   读 <var>:<enclosing_func>:<var_name> 替代裸变量名
├─ field_call.py            282→200  4 层类型感知查找
├─ array_call.py             58→58   不变
├─ local_fp_tracker.py       91→100  查询时使用 type-aware key + 接收 symbol_table 参数
│
└─ (删除) param_assign.py   786→✗
   总行数: ~2840 → ~2770
```

## 迁移策略 (8 Step, 每步独立可回退)

| Step | 变更 | 测试 |
|------|------|------|
| 1 | 创建 param_helpers.py (纯提取) | 全量 |
| 2 | 创建 param_binding.py (只写不产边) | 单独 |
| 3 | 创建 param_dispatch.py (Pass 3+4) | 单独 |
| 4 | 创建 callback_reg.py (Phase 3) | 单独 |
| 5 | 改造 initializer_assign.py + symbol_table.py (type-aware key 写入) | ET-Bench 召回不变确认 |
| 6 | 改造 field_call.py (type-aware 查找 + 新旧格式双读) | ET-Bench FP 下降确认 |
| 7 | 重组 orchestrator.py (3-Phase + 移除 Fix B) | 全量 |
| 8 | 删除 param_assign.py + 移除双读兼容代码 | 全量 |

**Step 5-6 合并说明**: old key 格式 `<gstruct:var.field>` 与 new key 格式 `<gstruct>:<type>.<var>.<field>` 不兼容。Step 6（field_call）实现时需同时读取新旧两种格式——优先查新格式，miss 时回退查旧格式。Step 8 删除 param_assign 后旧格式不再写入，双读代码随之删除。

## 预期结果

| 指标 | 当前 | 目标 |
|------|------|------|
| ET-Bench 召回 | 98.86% | ≥98.86% (无回归) |
| 总误报率 | 35.76% | <15% |
| callback_param FP | 195 | <50 |
| field_call FP | 99 | <30 |
| callback_reg FP | 43 | <15 |
| 最大模块行数 | 786 (param_assign) | 450 (initializer_assign) |
