# ET-Bench 工具设计缺陷综合分析

**日期**: 2026-05-13
**基线**: GT 修正后，9 场景 207 FPs + 3 召回缺陷

## 一、当前状态

### 1.1 各场景概览

| 场景 | 命中/期望 | 召回 | 误报 | FPR |
|---|---|---|---|---|
| fnptr-callback | 30/33 | 90.91% | 59 | 66.29% |
| fnptr-cast | 10/10 | 100% | 15 | 60.00% |
| fnptr-global-array | 307/307 | 100% | 0 | 0% |
| fnptr-global-struct | 68/68 | 100% | 52 | 43.33% |
| fnptr-global-struct-array | 70/70 | 100% | 62 | 46.97% |
| fnptr-library | 70/70 | 100% | 15 | 17.65% |
| fnptr-only | 24/24 | 100% | 2 | 7.69% |
| fnptr-struct | 21/21 | 100% | 13 | 38.24% |
| fnptr-varargs | 1/1 | 100% | 1 | 50.00% |
| **9 场景合计** | **602/609** | **98.85%** | **219** | **26.67%** |

### 1.2 误报按类型分布

| indirect_kind | 数量 | 占比 | 主要影响场景 |
|---|---|---|---|
| `field_call` | 98 | 44.7% | fnptr-global-struct-array (62), fnptr-library (13) |
| `callback_param` | 90 | 41.1% | fnptr-callback (57), fnptr-global-struct (24) |
| `callback_reg` | 28 | 12.8% | fnptr-global-struct (20) |
| `direct_assign` | 3 | 1.4% | 零星 |
| **合计** | **219** | | |

### 1.3 召回缺陷（3 条 GT 无法匹配）

| Example | GT 期望 | ethunter 产出 | 根因 |
|---------|--------|-------------|------|
| fnptr-callback/example_8 | `(_pqsort, sort_gp_asc)` | `(georadiusGeneric, sort_gp_asc)` | Pass 3 未检测 inner caller |
| fnptr-callback/example_8 | `(_pqsort, sort_gp_desc)` | `(georadiusGeneric, sort_gp_desc)` | 同上 |
| fnptr-callback/example_14 | `(gt_pch_p_14lang_tree_node, relocate_ptrs)` | `(gt_pch_save, relocate_ptrs)` | Pass 3 未检测 inner fnptr call |

---

## 二、设计缺陷全景

六个设计层面的问题，按影响排序：

### 缺陷 1: Dataflow 无类型溯源 → 98 field_call FPs

`VariableState` 是 `dict[str, set[str]]`，key 以**变量名**为命名空间 (`<gstruct:obj_a.handler>`)，而非 struct 类型名。导致 `field_call` 的 suffix fallback 只能做 wildcard 扫描（`key.endswith('.handler>')`），无法区分不同 struct 类型的同名字段。

**根因**: `initializer_assign` 等模块处理 struct 初始化时，已解析了 typedef 和字段名映射，但仅写入 `var.field` 形式的 key。类型信息（`obj_a` 属于 `struct type_a`）在写入 dataflow 时丢失。

**典型 case**: `fnptr-global-struct-array/example_6`——全局数组包含 36 个 `HELP_*` 函数（帮助函数）和 36 个 `zfs_do_*` 函数（命令处理函数），它们同属一个 struct 类型但语义完全不同。suffix 扫描无法区分。

### 缺陷 2: 无调用链拓扑模型 → 50+ callback_param FPs

ethunter 追踪 fnptr 参数使用 `param_mappings[pname] → {targets}` 的扁平映射，不区分函数的**角色**：
- **Caller**: 体内直接调用 fnptr → Pass 3 正确
- **Forwarder**: 仅将 fnptr 传给另一个函数 → 两者 caller 都不对
- **Storage**: 将 fnptr 存入 struct/global → field_call 更精确

**根因**: `_collect_call_params` 处理所有 call site 时不分析被调函数体对 fnptr 参数的使用模式（调用 vs 转发 vs 存储）。结果 Pass 3 和 Pass 4 各自使用不同的 caller 语义，每个场景都有部分边 caller 是错的。

**典型 case**: `fnptr-callback/example_13`——8 个外层函数传不同 valueize 到 `gimple_fold_stmt_to_constant_1`，后者才是实际调用 fnptr 的函数。Pass 3 用 inner caller，Pass 4 用 outer caller，GT 已统一到 inner caller。

### 缺陷 3: 启发式注册判定 → 28 callback_reg FPs

`_is_registration` 用 20+ 子串匹配猜测函数行为，而非分析函数体实际对 fnptr 参数做了什么。

```python
REG_PATTERNS = ['register', 'callback', 'hook', 'attach', 'subscribe',
    'set_', 'on_', 'add_', 'once', 'submit', 'post', 'work', 'spawn',
    'scandir', 'sort', 'filter', 'notify', 'watch', 'dispatch', 'schedule']
```

**根因**: 子串匹配无法判断函数对 fnptr 参数的实际行为。`zfs_ioctl_register_pool` 含 "register" 但只是 forwarder，其 `zfs_secpolicy_*` 参数存入 struct 后从未 dispatch。这些 callback_reg 边的 caller 名不准确且无 field_call 覆盖。

### 缺陷 4: Pass 3 fnptr 检测不完整 → 3 召回缺口

两个缺失原因：

**4a. typedef fnptr 参数未被识别**（example_8）：

`_pqsort` 的参数声明为 `int (*cmp)(const void *, const void *)`——使用**内联 function_declarator** 语法。`_has_fnptr_declarator` 可以检测。但 `sort_gp_asc`/`sort_gp_desc` 到达 `_pqsort` 时，`cmp` 的 targets 在 dataflow 中的解析存在问题——`dataflow.resolve("cmp")` 被多个调用者的 targets 污染。Fix A 已通过前缀 key 修复部分 case，但 typedef 场景的 `_pqsort:cmp` 前缀解析可能失败——`_pqsort` 的调用者使用不同函数名（`pqsort`），前缀 mismatch。

**4b. 间接调用链中 inner caller 未被发现**（example_14）：

`gt_pch_save` 调用 `gt_pch_p_14lang_tree_node` 传入 `relocate_ptrs`。`gt_pch_p_14lang_tree_node` 是 inner caller，但其函数体内对 fnptr 的调用可能使用非标准 AST 模式（如宏展开、类型转换包裹），Pass 3 的 call_expression 遍历未匹配到。

### 缺陷 5: 严格两相分离无反馈 → 后处理补丁

Phase 1（target resolution）→ Phase 2（call detection），单向无反馈。Phase 2 的 `field_call` 解析结果无法通知 Phase 1 的 `param_assign` "该 fnptr 已由 field dispatch 覆盖"。

**症状**: Fix B 和 Fix A-1 都是 orchestrator 后处理抑制——在所有边产出后扫描删除。这是绕过架构限制的 hack，而非架构内的解决方案。

### 缺陷 6: Edge 中心化输出模型

输出 model 是 `(caller, callee)` 二元组，隐含了 "只有一个正确 caller" 的假设。对于间接调用，call chain 可能有多个合法的 caller 视角（outermost caller、immediate fnptr-dispatcher、field dispatcher），不同下游消费者需要不同粒度。当前 flat edge 模型将视点选择强制内化为分析器的责任。

---

## 三、缺陷与症状的映射

| 设计缺陷 | 症状 | 影响 |
|---------|------|------|
| #1 无类型溯源 dataflow | field_call suffix 碰撞 | 98 FPs |
| #2 无调用链拓扑 | callback_param caller 歧义 + wrapper 误报 | ~50 FPs |
| #3 启发式注册判定 | callback_reg 注册函数误报 | 28 FPs |
| #4 Pass 3 检测不完整 | typedef fnptr + inner caller 漏检 | 3 recall gaps |
| #5 两相无反馈 | Fix B/A-1 的后处理 hack | 架构质量 |
| #6 Edge 中心化输出 | caller 视点不可配置 | 可用性 |

---

## 四、修复路线图（按架构改进深度）

### 短期（局部侵入）——解决 #4 召回缺口

- 修复 Pass 3 的 `_has_fnptr_declarator` 对 typedef fnptr 的检测
- 增强 fallback 分支的前缀解析，确保 `inner_func:param` 匹配 `outer_func:param`

**预计**: 3 recall gaps → 0，fnptr-callback 100% recall

### 中期（模块级重构）——解决 #2 和 #3 误报

- 在 `_register_phase` 中为每个 fnptr 参数分析使用模式（Caller/Forwarder/Storage），存入 dataflow
- 用行为检测替换 `_is_registration` 子串匹配
- Pass 3/4 根据使用模式决定是否产边：
  - Caller → Pass 3 边
  - Forwarder → 不产边（追链到第一个非-forwarder）
  - Storage → 不产 callback_*，交由 field_call

**预计**: ~80 FPs reduction

### 长期（架构级重构）——解决 #1 误报

- dataflow key 从 `var.field` 改为 `struct_type.field`
- 在 `initializer_assign`/`direct_assign` 阶段追踪变量→类型映射
- `field_call` 从 suffix 扫描改为精确 key 查找

**预计**: ~98 FPs reduction（全部 field_call）+ 召回率提升

### 不变量（#5 #6）

- #5 可随中期重构逐步内联（后处理→产边前检查）
- #6 是输出模型层升级，不影响分析精度
