# ET-Bench 剩余误报分析 (P0/P2/P3 修复后)

**日期**: 2026-05-13
**数据来源**: P0/P2/P3 修复后 `test_et_bench_report` 输出
**总体误报率**: 57.84% (771/1379 in 9 target scenarios)

## 各场景误报率

| 场景 | 检测 | 命中 | 误报 | 召回率 | FPR |
|---|---|---|---|---|---|
| fnptr-callback | 96 | 36 | 60 | 100% | 62.50% |
| fnptr-cast | 27 | 10 | 17 | 100% | 62.96% |
| fnptr-global-array | 307 | 307 | 0 | 100% | 0% |
| fnptr-global-struct | 644 | 68 | 576 | 100% | 89.44% |
| fnptr-global-struct-array | 132 | 70 | 62 | 100% | 46.97% |
| fnptr-library | 103 | 70 | 33 | 100% | 32.04% |
| fnptr-only | 26 | 24 | 2 | 100% | 7.69% |
| fnptr-struct | 39 | 21 | 18 | 100% | 46.15% |
| fnptr-varargs | 4 | 1 | 3 | 100% | 75% |
| **总计（9 场景）** | **1379** | **608** | **771** | **100%** | **55.91%** |

比较修复前: 误报 887 → 771 (减少 116), FPR ~59% → ~56%

## 剩余误报按 indirect_kind 分布

| indirect_kind | 数量 | 占比 |
|---|---|---|
| `callback_param` | 573 | 74.3% |
| `callback_reg` | 100 | 13.0% |
| `field_call` | 97 | 12.6% |
| `direct_assign` | 1 | 0.1% |

## 根因分析

### 根因 A: Pass 1 fallback 分支使用污染的 bare key 解析（~489 FPs, callback_param）

**机制**: fnptr 参数被转发到另一个注册函数时（如 `func` → `zfs_ioctl_register_legacy(func)`），Pass 1 fallback 分支使用 `dataflow.resolve(target)`（bare key）解析，该 key 被所有调用者的 targets 全局合并污染。

**数据流追踪** (以 `fnptr-global-struct/example_4` 为例):

```
1. zfs_ioctl_init() 调用 zfs_ioctl_register_pool(ioc, zfs_ioc_pool_create, ...)
   → Pass 1: _is_registration("zfs_ioctl_register_pool") = True
   → dataflow.assign("zfs_ioctl_register_pool:func", "zfs_ioc_pool_create")  // prefixed
   → dataflow.assign("func", "zfs_ioc_pool_create")  // bare key (P3 保留兼容)

2. zfs_ioctl_init() 调用 zfs_ioctl_register_dataset_read(ioc, zfs_ioc_dataset_list_next, ...)
   → dataflow.assign("func", "zfs_ioc_dataset_list_next")  // bare key 合并!
   
   → dataflow.resolve("func") → {ALL 75 zfs_ioc_* functions}  // 全部污染！

3. zfs_ioctl_register_pool() 体内部调用 zfs_ioctl_register_legacy(ioc, func, ...)
   → func 不在 symbol_names → fallback 分支
   → df_targets = dataflow.resolve("func")  // ← 拿到全部 75 个 targets!
   → call_site_targets[("zfs_ioctl_register_pool", "zfs_ioctl_register_legacy", 1)] = {all 75}

4. Pass 4: 产边 (zfs_ioctl_register_pool, each_zfs_ioc_func)
   → 每个 wrapper 函数 × 75 targets = 7 × 75 = 525 cb_param edges
   → 但用户只有 489 callback_param FPs（去重后减少）
```

**根本原因**: P3 修复了 Pass 2 的 resolve（用 `fa.enclosing_func:param_name` 前缀），但 Pass 1 fallback 分支的 `dataflow.resolve(target)` 仍使用 bare key。

**修复方向**: fallback 分支改为优先 `dataflow.resolve(f'{caller}:{target}')`，命中直接用，不命中再 fallback 到 bare key。

**预计效果**: ~450 FP reduction

---

### 根因 B: Pass 3 vs Pass 4 caller 命名歧义（~80 FPs, callback_param）

**机制**: 两个 Pass 使用不同的 caller 语义——Pass 3 用"执行 fnptr 调用的被调函数体"，Pass 4 用"外层传 fnptr 的调用者"。ground truth 的 caller 约定因场景而异，无法通过单一策略覆盖。

**example_13**: ground truth 期望外层 caller（`ccp_fold`）
```
Pass 3 → (gimple_fold_stmt_to_constant_1, valueize_op) ← 7 FPs, caller 错误
Pass 4 → (ccp_fold, valueize_op) ← 8 matched ✓
```

**example_2**: ground truth 期望被调函数体（`print_units`）
```
Pass 3 → (print_units, format_time_us) ← 2 matched ✓
Pass 4 → (print_stats_latency, format_time_us) ← 3 FPs, caller 错误
```

**根本原因**: 语义依赖——无法从代码结构机械判定 ground truth 期望哪种 caller。Pass 3 对简单 fnptr 调用正确，Pass 4 对通过 wrapper 函数的调用正确。

**修复方向**: 
- A. 对于字段赋值（含 param→field 映射）场景，field_call 已产出精确 caller，callback_param/callback_reg 边应被抑制
- B. 对非字段赋值场景，保持当前双 Pass 行为（剩余的 ~80 条远少于之前的 N×M）

**预计效果**: ~80 FP reduction（方案 A）

---

### 根因 C: callback_reg 对 struct-field 注册模式冗余（100 FPs, callback_reg）

**机制**: 当 fnptr 实参最终被存入 struct 字段时，`field_call` 已产出精确边（caller=dispatcher），`callback_reg` 同时产出粗略边（caller=注册调用者或 `<registration>`），后者均为冗余。

**example_4 案例**:
```
callback_reg: zfs_ioctl_init → zfs_ioc_* (77 FPs)
field_call:   zfsdev_ioctl_common → zfs_ioc_* (57 matched ✓)
```
callback_reg 的 caller 名不准确（`zfs_ioctl_init` vs `zfsdev_ioctl_common`），所有 77 条均为冗余。

**修复方向**: `_register_phase` 已追踪 param→field 映射。当 call site 的 callee+arg_idx 在 param_fields 中有注册记录时（即 fnptr 被写入 struct field），抑制 callback_reg 边。这也适用于其他类别中的 23 条 callback_reg 误报（如 fnptr-cast/6，fnptr-struct/6，fnptr-library/10）。

**预计效果**: ~100 FP reduction

---

### 根因 D: field_call suffix fallback 字段名碰撞（97 FPs, field_call）

**机制**: `field_call.py` 第 178-180 行无差别扫描所有以 `.fieldname>` 结尾的 dataflow key。不同 struct 的同名字段全部命中。

**分布**:
- fnptr-global-struct-array: 62 FPs（`.transform`、`.init` 等字段名碰撞）
- fnptr-library: 12 FPs
- fnptr-struct: 7 FPs（`.handler`、`.callback` 碰撞）
- fnptr-cast: 6 FPs
- fnptr-global-struct: 8 FPs

**根本原因**: dataflow 中 `<gstruct:var.field>` key 只记录了 struct 实例的 fnptr 字段（非 fnptr 字段不产生 key），缺少 struct 类型和字段拓扑信息。没有足够信息做安全的类型感知筛选。

**修复方向**: 
- 短期: 需要在 `initializer_assign` 等 Phase 1 模块中为非 fnptr 字段也创建索引条目（空 targets），丰富 field_index
- 长期: 在 dataflow key 中嵌入 struct type 名（如 `<gstruct:type_a.handler>` 而非 `<gstruct:obj_a.handler>`），实现精确类型隔离

**预计效果**: ~60 FP reduction（早期实现）

---

## 修复优先级建议

| 优先级 | 根因 | 位置 | 影响 FPs | 难度 | 预计减 FP |
|--------|------|------|---------|------|----------|
| P0（新） | A: fallback 污染 | `param_assign.py` Pass 1 fallback 分支 | 489 | 低 | ~450 |
| P1（新） | C: callback_reg 冗余 | `param_assign.py` Pass 1 + `_register_phase` | 100 | 中 | ~100 |
| P2（新） | B: Pass3/4 歧义 | `param_assign.py` Pass 3/4 抑制逻辑 | ~80 | 中 | ~80 |
| P3（延续） | D: field_call suffix | `field_call.py` + `initializer_assign.py` | 97 | 高 | ~60 |

**A + C 仅修复两处**即可将 FPR 从 ~56% 降至 ~20%。
