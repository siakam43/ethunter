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
  └─ param_binding._register_phase() → param_fields, ret_fields, param_usage

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
| `param_helpers.py` | ~120 | 1a | 共享工具: func_params 收集、fnptr typedefs、宏提取、param_usage 分类 |
| `param_binding.py` | ~200 | 1 | 参数绑定: 调点参数映射 + struct field 赋值 → 写 dataflow + registration_sites |
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
    unwrap_cast: Callable  # CastResolver

    # === 已有 (从 state 迁移到 engine) ===
    func_fp_params: dict[str, set[int]]
    param_usage: dict[tuple[str, int], str]

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

## field_call 查找策略: 12层 → 4层

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
```

**移除**: 全局 `key.endswith('.fieldname>')` wildcard scan (line 178-181) — 最大的单一 FP 来源

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
├─ param_helpers.py         NEW~120  共享工具 (从 param_assign 提取)
├─ param_binding.py         NEW~200  Phase 1: 参数绑定
├─ param_dispatch.py        NEW~160  Phase 2: fnptr 调用
├─ callback_reg.py          NEW~130  Phase 3: 注册检测
│
├─ direct_call.py            86→86   不变
├─ dlsym_fp.py               58→58   不变
├─ direct_assign.py         121→121  不变
├─ initializer_assign.py    420→450  写 type-aware key
├─ cast_assign.py            62→62   不变
├─ direct_call_fp.py         84→84   不变
├─ field_call.py            282→200  4 层类型感知查找
├─ array_call.py             58→58   不变
├─ local_fp_tracker.py       91→91   不变
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
| 5 | 改造 field_call.py (type-aware) | ET-Bench FP 下降确认 |
| 6 | 改造 initializer_assign.py + symbol_table.py (type-aware key) | ET-Bench 召回不变确认 |
| 7 | 重组 orchestrator.py (3-Phase + 移除 Fix B) | 全量 |
| 8 | 删除 param_assign.py | 全量 |

## 预期结果

| 指标 | 当前 | 目标 |
|------|------|------|
| ET-Bench 召回 | 98.86% | ≥98.86% (无回归) |
| 总误报率 | 35.76% | <15% |
| callback_param FP | 195 | <50 |
| field_call FP | 99 | <30 |
| callback_reg FP | 43 | <15 |
| 最大模块行数 | 786 (param_assign) | 450 (initializer_assign) |
