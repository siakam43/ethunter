# ET-Bench 误报架构根因分析

**日期**: 2026-05-13
**状态**: 经 6 轮点修复后（P0/P2/P3/Fix A/B/A-1），剩余 213 FPs，9 场景 100% recall

## 现存误报分布

| indirect_kind | 数量 | 占比 |
|---|---|---|
| `callback_param` | 85 | 39.9% |
| `field_call` | 98 | 46.0% |
| `callback_reg` | 27 | 12.7% |
| `direct_assign` | 3 | 1.4% |
| **合计** | **213** | |

## 五大架构缺陷

经过 6 轮点修复的完整历程，剩余的 213 条误报指向 5 个无法通过点修复解决的根本性架构问题。

---

### 缺陷 1: 扁平全局 Dataflow — 无类型溯源

**当前设计**: `VariableState` 是 `dict[str, set[str]]`——全局 key→targets 映射，无作用域、无类型标注、无文件来源。

```python
# 当前 dataflow 条目:
<gstruct:obj_a.handler> → {handler_a}  # "obj_a" 是变量名，不是类型名
<gstruct:obj_b.handler> → {handler_b}  # "obj_b" 是变量名，不是类型名
# 两者同名字段 "handler"，但 struct 类型不同。dataflow 无类型信息。
```

**影响**: `field_call` 第 178-180 行的 suffix fallback (`key.endswith('.handler>')`) 是**唯一**能将 `obj_a.handler` 和 `obj_b.handler` 关联起来的机制——但无法区分 "同类型 struct 的不同实例" 和 "不同类型的同名 struct 字段"。这直接导致 **98 条 field_call 误报**。

核心矛盾：dataflow key 以**变量名**为命名空间 (`<gstruct:obj_a.handler>`)，但精确匹配需要以**类型名**为命名空间 (`<gstruct:type_a.handler>`)。同一类型的不同变量实例（如 `obj_a1`、`obj_a2`）理应共享 targets，不同变量名导致它们被隔离。

**结构化方案**: 将 dataflow key 从 `var.field` 改为 `struct_type.field`。需要在 `initializer_assign`/`direct_assign` 阶段追踪变量→类型的映射（通过 typedef 解析和声明信息，这些信息在 AST 中已存在）。

```
<gstruct:type_a.handler> → {handler_a, ...}  # 所有 type_a 实例的 handler 合并
<gstruct:type_b.handler> → {handler_b, ...}  # 与 type_a 完全隔离
```

**收益**: field_call 可从 suffix fallback 转为精确 key 查找，消除所有 98 条 field_call 误报。P3 的函数前缀机制（当前半手动）也可统一简化。

---

### 缺陷 2: 无调用链拓扑模型 — 无法区分 Forwarder/Dispatcher/Caller

**当前设计**: fnptr 参数追踪使用扁平映射 `param_name → {targets}`，不区分函数对 fnptr 的使用模式：
- **Caller**: 直接调用 fnptr 的函数（`cb(42)`）—— Pass 3 正确
- **Forwarder**: 将 fnptr 原样传递给另一个函数的函数（`register_fn(c, cb)`）—— 两者都不完全正确
- **Storage**: 将 fnptr 存入 struct/global 的函数（`c->handler = cb`）—— field_call 更精确

**影响**: 85 条 callback_param 误报均起源于 "使用了错误的 caller"。核心矛盾是 edge 模型需要输出一个 caller 名字，但调用链有多个候选 caller（outermost、intermediate、immediate），不同 benchmark 场景的约定不同。

具体分析各场景 call chain 形态：

| 场景 | Chain 形态 | GT 期望的 caller | cur产出的 FP caller |
|------|-----------|-----------------|-------------------|
| example_2 | `main → print_stats → print_units(fnptr) → fnptr()` | `print_units` (immediate caller) | `main`, `print_stats` (outer) |
| example_13 | `ccp_fold → gimple_fold_stmt_to_constant_1(fnptr) → fnptr()` | `ccp_fold` (outer caller) | `gimple_fold_stmt_to_constant_1` (immediate) |
| example_4 | `zfs_ioctl_init → wrapper → leaf(fnptr) → store in field → dispatch` | `zfsdev_ioctl_common` (dispatcher) | `wrapper` (intermediate) |

**方案**: 
1. 在 Pass 1 阶段为每个 fnptr 参数标记 usage 分类（Caller/Forwarder/Storage）
2. Caller: 保留 Pass 3 边（immediate caller 是正确的）
3. Forwarder: 不产边，延着 chain 向上找到第一个非-forwarder 的 caller
4. Storage: 不产 callback_param/callback_reg，交由 field_call

**收益**: 解决 call chain 相关的所有 callback_param 误报（~50 FPs）。

---

### 缺陷 3: 启发式注册判定 — 基于名称而非行为

**当前设计**:
```python
REG_PATTERNS = ['register', 'callback', 'hook', 'attach', 'subscribe', 
    'set_', 'on_', 'add_', 'once', 'submit', 'post', 'work', 'spawn',
    'scandir', 'sort', 'filter', 'notify', 'watch', 'dispatch', 'schedule']
```

20+ 子串匹配判定"注册函数"。这是一个**语义猜测机制**，不基于函数实际行为。

**影响**: 
- 误报：`zfs_ioctl_register_pool` 匹配 "register"，但其 fnptr 参数是**转发**到 leaf 函数的，不直接调用也不直接存储。`callback_reg` 边 caller 不准确。
- 遗漏：不包含这些子串的注册函数（如 `ssl_set_callback` 不含匹配词）会走 pass-through 逻辑，产生不同但同样不准确的边。

**方案**: 用行为检测替代名称匹配。在 `_register_phase` 中为每个 fnptr 参数分析其在该函数体内的使用模式：
```python
param_usage[(func_name, param_idx)] = {
    'stored_in_field': bool,     # 是否有 field = param 赋值
    'stored_in_global': bool,    # 是否有 global = param 赋值  
    'called_directly': bool,     # 是否有 param(args) 调用
    'forwarded': bool,           # 是否有 other_func(param) 传递
    'field_paths': set[str],     # 如果有 field 赋值，具体字段路径
}
```

**收益**: 消除 27 条 callback_reg 误报（基于精确行为分析而非名称猜测）。

---

### 缺陷 4: Edge 中心化输出 — 缺少 Path 语义

**当前设计**: 输出 model 是 `(caller, callee)` 二元组——一条 flat edge。ground truth 期望的语义可能是 "最外层调用者→最终 fnptr 目标" 或 "直接调用函数→fnptr 目标"，取决于场景。

**核心矛盾**: 
- `main → print_stats → print_units(fnptr) → fnptr()` 
- GT 期望: `(print_units, fnptr)` —— inline 函数体
- `ccp_fold → gimple_fold_stmt_to_constant_1(fnptr) → fnptr()`
- GT 期望: `(ccp_fold, fnptr)` —— outer 调用者
- 两者**同一模式**但 GT 期望不同 caller。无代码结构特征能区分。

**分析**: 这种差异不是 bug——它是调用图渲染的**视点选择**（viewpoint selection）问题。不同的下游消费者需要不同粒度的 caller。当前 edge 模型隐含了 "只有一个正确的 caller" 的假设，这个假设对间接调用不成立。

**方案**: 不追求 "找到唯一正确的 caller"，而是：
1. 提供完整的 call path 信息（outer→intermediate→immediate→fnptr）
2. 让 edge emission 成为一个可配置的视角（报告 immediate caller vs outermost caller vs dispatcher）
3. 当前内部可默认使用 "保守策略"（同时报告多种 callers，交由下游过滤）

这是一种**输出模型层**的架构升级，不影响分析精度但影响可用性。

---

### 缺陷 5: 严格两相分离 — 无阶段间反馈

**当前设计**: Phase 1（target resolution）→ Phase 2（call detection），单向无反馈。Phase 2 的 `field_call` 解析结果无法通知 Phase 1 的 `param_assign`："该 fnptr 已由 field dispatch 覆盖，不要产 callback_reg/callback_param 边"。

**症状**: Fix B 和 Fix A-1 都是**后处理抑制**——在所有分析完成后在 orchestrator 中扫描已产出的边，按规则删除。这是一种绕过架构限制的 hack。

**方案**: 在 dataflow 中引入 `covered_callees` 标记集合。当 `field_call` 成功解析 struct dispatch 后，将 callee 标记为 "covered"。`param_assign` 的 `_is_registration` 和 Pass 3/4 在发射边前检查此标记，如果已覆盖则跳过。

```
dataflow.covered_by_field_dispatch = {"zfs_ioc_pool_create", ...}
# param_assign 发射前检查:
if target in dataflow.covered_by_field_dispatch:
    continue  # field_call already provides a better edge
```

这比后处理抑制更干净：抑制逻辑在**边产生处**而非**边收集后**，保持了模块内聚性。

---

## 方案路线图

| 优先级 | 缺陷 | 方案 | 预计减 FP | 复杂度 | 依赖 |
|--------|------|------|----------|--------|------|
| P0 | #1 类型溯源 dataflow | key 从 `var.field` → `type.field` | ~98 (全部 field_call) | 高 | 需改 initializer_assign, field_call |
| P1 | #2 调用链分类 | fnptr param usage 分类标记 | ~50 (chain ambiguity) | 中 | 需改 param_assign, _register_phase |
| P2 | #3 行为注册检测 | 替换 `_is_registration` 为行为分析 | ~27 (全部 callback_reg) | 中 | 复用 P1 的 usage 分类 |
| P3 | #5 阶段间反馈 | `covered_callees` 标记 | 后处理→内联（无新增减 FP） | 低 | 无 |
| P4 | #4 Path 语义 | 多视角 edge 模型 | 视点问题（非 FP 问题） | 高 | 需要 output 层重构 |

**P0+P1+P2 三处架构改进，预计消除全部可修复误报，FPR → ~0%。**
