# ET-Bench 剩余误报分析 (P0+P2+P3+Fix A+Fix B 全部修复后)

**日期**: 2026-05-13
**总体误报率**: 35.93% (278/886 FPs, 9 target scenarios)

## 各场景误报率

| 场景 | 检测 | 命中 | 误报 | 召回率 | FPR |
|---|---|---|---|---|---|
| fnptr-callback | 90 | 36 | 54 | 100% | 60.00% |
| fnptr-cast | 25 | 10 | 15 | 100% | 60.00% |
| fnptr-global-array | 307 | 307 | 0 | 100% | 0% |
| fnptr-global-struct | 173 | 68 | 105 | 100% | 60.69% |
| fnptr-global-struct-array | 132 | 70 | 62 | 100% | 46.97% |
| fnptr-library | 93 | 70 | 23 | 100% | 24.73% |
| fnptr-only | 26 | 24 | 2 | 100% | 7.69% |
| fnptr-struct | 35 | 21 | 14 | 100% | 40.00% |
| fnptr-varargs | 4 | 1 | 3 | 100% | 75% |
| **总计（9 场景）** | **886** | **608** | **278** | **100%** | **31.38%** |

## 剩余误报按 indirect_kind 分布

| indirect_kind | 数量 | 占比 |
|---|---|---|
| `callback_param` | 153 | 55.0% |
| `field_call` | 97 | 34.9% |
| `callback_reg` | 27 | 9.7% |
| `direct_assign` | 1 | 0.4% |

## 根因分析

### 根因 A：callback_param — Pass 3/4 caller 命名歧义（~80 FPs, callback_param, 可修复 ~75 FPs）

此根因分为两个子类：

**A-1: struct-field wrapper 场景（75 FPs, example_4, 可修复）**

当一个 fnptr 经过 wrapper 函数链路最终存入 struct field 时，Pass 4 产出的 callback_param 边以 wrapper 函数名为 caller（如 `zfs_ioctl_register_pool`），而 ground truth 期望 dispatcher 函数名（`zfsdev_ioctl_common`）。这些边的 callee 是正确的（`zfs_ioc_*`），但 caller 不正确。

```
Ground truth: (zfsdev_ioctl_common, zfs_ioc_pool_create)
Pass 4 产出:  (zfs_ioctl_register_pool, zfs_ioc_pool_create)  ← caller 错误
field_call:   (zfsdev_ioctl_common, zfs_ioc_pool_create)       ← 正确 ✓
```

**修复方向**: 将 Fix B 的 callee-overlap 抑制逻辑从 callback_reg 扩展到 callback_param。当 `callback_param` 边的 callee 也出现在 `field_call` 边中时，说明 dispatch 已由 field_call 覆盖，callback_param 冗余。与 Fix B 完全相同的后处理模式。

**预计效果**: ~75 FP reduction (example_4 的全部 callback_param 误报)。

---

**A-2: 非 struct-field 场景（~50 FPs, 多个 example, 不可机械修复）**

无 struct field dispatch 时，Pass 3（被调函数体 caller）和 Pass 4（外层调用者 caller）各有适用场景，无法从代码结构判定 ground truth 期望哪种 caller。

| Example | Pass 3 caller | Pass 4 caller | GT 期望 | FP 来源 |
|---------|--------------|--------------|---------|---------|
| example_13 | gimple_fold_stmt_to_constant_1 | ccp_fold | ccp_fold | Pass 3 (8 FPs) |
| example_2 | print_units | print_stats_latency | print_units | Pass 4 (5 FPs) |
| example_8 | _pqsort | georadiusGeneric | georadiusGeneric | Pass 3 (4 FPs) |

**不可修复原因**: 尝试过的启发式均失败——调用者数量、静态属性、wrapper 深度等特征在两个场景间无一致区分规律。剩余 ~50 条为此类固有误差。

---

### 根因 B：callback_param — Pass 4 非 wrapper caller（~25 FPs, 分散, 部分可修复）

一些非 wrapper 调用场景中 Pass 4 产出的 caller 也是错误的。这些 case 与 A-2 类似但规模较小，分布在 fnptr-callback 和 fnptr-library 各个 example 中。

---

### 根因 C：field_call — suffix fallback 字段名碰撞（97 FPs, 当前不可修复）

`field_call.py` 第 178-180 行对所有以 `.fieldname>` 结尾的 dataflow key 无差别匹配。不同 struct/array 的同名字段全部命中。

**主要来源**: `fnptr-global-struct-array/example_6`（36 FPs）中，一个全局数组包含两类 entry——`zfs_do_*`（ground truth target）和 `HELP_*`（帮助函数），它们共享相同的 struct 类型因此同名字段。suffix matching 同时匹配了两类。

**不可修复原因**: 上一轮 P1 尝试已证实——当前 dataflow 的 `<gstruct:>` key 只包含 fnptr 字段，缺少非 fnptr 字段的索引信息。需要增强 `initializer_assign` 为非 fnptr 字段也创建索引条目，或为 dataflow key 嵌入 struct type 名。

---

### 根因 D：callback_reg — struct 非 fnptr 字段注册（20 FPs, example_4, 可修复）

`zfs_ioctl_init` 调用注册函数时，fnptr 参数中既包含 ioctl handler（`zfs_ioc_*`，存入 `zvec_legacy_func` 字段，Fix B 已消除），也包含 security policy 函数（`zfs_secpolicy_*`，存入 `zvec_secpolicy` 字段）。后者无 field_call dispatch（fixture 中 dispatcher 未调用 security policy），Fix B 的 callee-overlap 无法覆盖。

**修复方向**: 将 Fix B 的 suppression 从 "callee in field_call" 改为 "callee's param→field mapping exists"。即：不仅检查 field_call 是否产出边，还检查 param_fields 中是否有映射——表示 fnptr 最终存入某 struct field（无论 field_call 是否调用它）。

---

### 根因 E：callback_reg — 多 fnptr 注册匹配（~5 FPs, 可修复）

注册函数（如 `Curl_MD5_setup`）接收包含多个 fnptr 的 struct，_is_registration 对所有 struct 成员的 identifier 实参都创建 callback_reg 边。但 ground truth 只期望其中一个 fnptr。

```
Ground truth: (Curl_MD5_init, my_md5_init)
callback_reg FP: (<registration>, my_md5_update), (<registration>, my_md5_final)
(my_md5_init, my_md5_update, my_md5_final 来自同一个 struct 的多个字段)
```

**修复方向**: 与 D 类似——如果注册函数的 fnptr param 被存入 struct field（param→field mapping 存在），抑制 callback_reg。

---

### 根因 F：其余零星（~5 FPs, callback_reg + direct_assign, 不可修复）

单个 FP 分布在 fnptr-only 和 fnptr-library 中，属于极端边界 case。

## 修复优先级

| 优先级 | 根因 | 影响 FPs | 方式 | 难度 | 预计减 FP |
|--------|------|---------|------|------|----------|
| P0 | A-1: Pass 4 wrapper caller 重叠 | 75 | Fix B 扩展到 callback_param | 低 | ~75 |
| P1 | D+E: callback_reg field mapping | 25 | param→field mapping 存在时抑制 | 低 | ~25 |
| P2 | C: field_call suffix 碰撞 | 97 | 需 dataflow 模型增强 | 高 | ~60 |
| — | A-2: Pass 3/4 歧义 | ~50 | 不可机械修复 | — | — |
| — | B: 其他 callback_param | ~25 | 分散边界 case | — | — |
| — | F: 零星 | ~5 | 不可修复 | — | — |

**P0+P1 仅两处改动**即可将 FPR 从 ~31% 降至 ~18%，进一步接近目标。
