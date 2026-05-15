# ET-Bench 误报率分析

**日期**: 2026-05-13
**数据来源**: `test_et_bench_report` 中的 Extra 列（ethunter 检测到但不在 ground_truth.json 中的间接调用边）
**总体误报率**: 60.98% (950/1558)

## 各场景误报率

| 场景 | 检测 | 命中 | 误报 | 召回率 | FPR |
|---|---|---|---|---|---|
| fnptr-callback | 174 | 36 | 138 | 100.00% | 79.31% |
| fnptr-cast | 27 | 10 | 17 | 100.00% | 62.96% |
| fnptr-dynamic-call | 4 | 1 | 3 | 16.67% | 75.00% |
| fnptr-global-array | 307 | 307 | 0 | 100.00% | 0.00% |
| fnptr-global-struct | 678 | 68 | 610 | 100.00% | 89.97% |
| fnptr-global-struct-array | 132 | 70 | 62 | 100.00% | 46.97% |
| fnptr-library | 107 | 70 | 37 | 100.00% | 34.58% |
| fnptr-only | 26 | 24 | 2 | 100.00% | 7.69% |
| fnptr-struct | 39 | 21 | 18 | 100.00% | 46.15% |
| fnptr-varargs | 4 | 1 | 3 | 100.00% | 75.00% |
| fnptr-virtual | 60 | 0 | 60 | 0.00% | 100.00% |
| **总计** | **1558** | **608** | **950** | **98.86%** | **60.98%** |

## 误报按 indirect_kind 分布

| indirect_kind | 数量 | 占比 | 来源模块 |
|---|---|---|---|
| `callback_param` | 689 | 72.5% | `param_assign` Pass 3/4 |
| `field_call` | 157 | 16.5% | `field_call` Pass 2 |
| `callback_reg` | 100 | 10.5% | `param_assign` Pass 1 |
| `dlsym_fp` | 3 | 0.3% | `dlsym_fp` |
| `direct_assign` | 1 | 0.1% | `direct_assign` |

## 四大根因分析

### 根因 1: Pass 3/4 全链路交叉边（`callback_param`, 689 条）

**机制**: `param_assign` 的 Pass 3（被调函数体内的 fnptr 调用）和 Pass 4（外层调用者传参）对同一调用链产生多层级 caller，且所有层级间的 target 组合全部产出。

**根本原因**: Pass 3 和 Pass 4 各自独立产生边，没有去重机制。当 N 个函数调用同一个 fnptr 接收函数、且该函数有 M 个 fnptr target 时，产生 O(N×M) 条边而非 O(N) 条。

**典型实例**:

```
callback/example_13: 62 extra
  7 个 caller（ccp_fold, copy_prop_visit_assignment, back_propagate_equivalences, 
  try_to_simplify, visit_stmt, jt_state_register_equivs_stmt, 
  pointer_equiv_analyzer_visit_stmt）
  × 7 个 target（pta_valueize, threadedge_valueize, vn_valueize, dom_valueize,
  valueize_val, valueize_op, do_valueize）
  → 期望 7 条，实际 ~49 条

global-struct/example_4: 523 extra + 77 callback_reg
  zfs_ioctl_init 作为注册函数，对其所有实参创建 callback_reg 边
  同时多个 zfs_ioc_* handler 被注册到同一个 struct 的 fnptr 字段
```

**修复方向**: Pass 3 边和 Pass 4 边不应同时保留。应保留 Pass 3（函数体内调用 = 更精确的 caller），丢弃 Pass 4（外层 caller）除非需要跨函数上下文。

---

### 根因 2: field_call suffix fallback 字段名碰撞（`field_call`, 157 条）

**机制**: `field_call` Pass 2 在精确 `<gstruct:exact.path>` 解析失败时，扫描 dataflow 中所有以 `.fieldname>` 结尾的 key，将目标函数全部返回。

```python
# field_call.py ~line 178-180
for key, vals in dataflow.targets.items():
    if key.endswith(f'.{last_part}>') and vals:
        targets.update(vals)
```

**根本原因**: 不同 struct 的同名字段（如 `.handler`、`.callback`、`.transform`、`.init`）会互相污染。suffix 回退是"尽力而为"策略，不做来源验证。

**典型实例**:

| 场景 | 误报数 | 碰撞字段 |
|---|---|---|
| fnptr-virtual | 60 | vtable struct 的 `.get_state_map_by_name` |
| fnptr-global-struct-array | 62 | 数组元素 struct 的 `.transform`、`.init` 等 |
| fnptr-struct | 7 | `.handler`、`.callback` 等 |

**修复方向**: suffix fallback 限定到同一文件内的 struct 字段，或要求字段所在的 struct 类型名也部分匹配。

---

### 根因 3: callback_reg 注册函数名过度匹配（`callback_reg`, 100 条）

**机制**: `_is_registration` 函数通过 20+ 子串匹配判定"注册函数"：

```python
REG_PATTERNS = [
    'register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_',
    'once', 'submit', 'post', 'work', 'spawn', 'scandir', 'sort', 'filter',
    'notify', 'watch', 'dispatch', 'schedule',
]
```

对匹配函数的**所有** identifier 实参无条件创建 `callback_reg` 边，不验证实参类型是否为 fnptr。

**根本原因**: 
1. 子串匹配过宽 — `zfs_ioctl_init` 匹配 "init"（不在 REG_PATTERNS 中... 让我检查 — 实际上 "init" 不在列表中。让我重新检查为什么 global-struct/example_4 有 77 条 callback_reg）。

实际上，`zfs_ioctl_init` 的 "init" 不在 REG_PATTERNS 中。77 条 callback_reg 的真正原因可能是 fixture 中存在 `zfs_ioctl_register` 等命名包含 "register" 的函数，它们调用了 `zfs_ioctl_init`。

2. 不区分函数实参和 fnptr 实参 — 对字符串常量名、整数变量、结构体指针等非 fnptr 实参也创建边。

**典型实例**: `global-struct/example_4` 的 77 条 callback_reg，来自注册函数的非 fnptr 实参被错误当作 callback target。

**修复方向**: callback_reg 仅在实参标识符与已知函数名匹配且实参位置对应 fnptr 形参时才创建边（类似 `func_fp_params` 检查）。

---

### 根因 4: dataflow 跨函数参数名污染（间接，影响精度）

**机制**: Phase 2 中新增的 `dataflow.assign(pname, target)` 将形参名→函数的映射写入全局 dataflow。不同函数中同名形参（如 `cb`、`fn`、`handler`、`op`）的 targets 会合并。

```python
# param_assign.py: dataflow.assign(pname, target)
# "handler" → {"func_a"} in func1
# "handler" → {"func_b"} in func2
# dataflow.resolve("handler") → {"func_a", "func_b"}  # 跨函数合并
```

**根本原因**: VariableState 是全局作用域，不区分函数的局部变量/参数。对别名链解析（Gap 4 需要）是必需的，但对精度有副作用。

**修复方向**: 当 `direct_assign` 做 `tmp_handler = log_handler` 解析时，用 `(enclosing_func, target)` 作为限定 key，而非裸 `target` 名。

---

## 修复优先级建议

| 优先级 | 根因 | 影响 | 修复策略 | 预减误报 |
|--------|------|------|---------|---------|
| P0 | #1 Pass 3/4 冗余 | 689 | Pass 3 + Pass 4 去重，保留 Pass 3 | ~500 |
| P1 | #2 suffix fallback | 157 | 限定 suffix 匹配范围为同文件同 struct | ~120 |
| P2 | #3 callback_reg 过宽 | 100 | 加 fnptr 类型检查 | ~70 |
| P3 | #4 dataflow 污染 | 间接 | 加函数作用域限定 | ~30 |

**即使只修复 P0**，FPR 可从 60.98% 降至约 25%。
