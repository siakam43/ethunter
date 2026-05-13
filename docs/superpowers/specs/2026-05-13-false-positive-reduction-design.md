# ET-Bench 误报率降低方案设计

**日期**：2026-05-13
**来源**：`docs/et_bench_false_positive_analysis.md` 分析结论
**总体目标**：大幅降低误报率，**严格保证**召回率不降低（fnptr-virtual 和 fnptr-dynamic-call 除外）

## 基线数据

| 场景 | 检测 | 命中 | 误报 | 召回率 | FPR |
|---|---|---|---|---|---|
| fnptr-callback | 174 | 36 | 138 | 100% | 79.31% |
| fnptr-cast | 27 | 10 | 17 | 100% | 62.96% |
| fnptr-global-array | 307 | 307 | 0 | 100% | 0% |
| fnptr-global-struct | 678 | 68 | 610 | 100% | 89.97% |
| fnptr-global-struct-array | 132 | 70 | 62 | 100% | 46.97% |
| fnptr-library | 107 | 70 | 37 | 100% | 34.58% |
| fnptr-only | 26 | 24 | 2 | 100% | 7.69% |
| fnptr-struct | 39 | 21 | 18 | 100% | 46.15% |
| fnptr-varargs | 4 | 1 | 3 | 100% | 75% |
| **总计（9 场景）** | ~1494 | ~607 | ~887 | **~100%** | ~59% |

fnptr-virtual 和 fnptr-dynamic-call 不纳入本次优化范围。

## 误报根因与修复设计

### P0：Pass 4 降级为纯 dataflow 填充（影响 ~500 条 callback_param 误报）

**根因**：`param_assign.py` 的 Pass 3（被调函数体内 fnptr 调用→target）和 Pass 4（外层调用者→target）各自独立产出 `callback_param` 边。当 N 个调用者传参、M 个 target 时，产生 O(N×M) 条边。

**修复**：
- 移除 Pass 4 的边产出逻辑（第 548-617 行 `_collect_call_args_pass4` 及后续 `call_targets` 发射循环）
- Pass 3 成为 `callback_param` 边**唯一生产者**——它的 caller 是真正执行 fnptr 调用的函数，信息最精确
- Pass 1 中的 `_propagate_call_site` 数据流传播保留不动——继续为跨函数 field 解析提供 dataflow

**召回安全性验证**：实现前先用当前代码跑一轮全量 ET-Bench，标记出所有 ground_truth 边中哪些是由 Pass 4 产出、Pass 3 未产出的（通过临时日志区分 indirect_kind 来源）。如果存在此类边，分析 Pass 3 无法覆盖的原因（通常是跨文件场景：caller 和 callee 不在同一文件，Pass 3 的树遍历看不到 caller）。对此类跨文件边，保留 Pass 4 但限制为：仅当 Pass 3 的 `call_site_edges` 中没有同 (caller, target) 条目时才发射，避免 N×M 重复。

### P1：field_call suffix fallback 类型感知匹配（影响 ~120 条 field_call 误报）

**根因**：`field_call.py` 第 178-180 行 suffix fallback 对所有以 `.fieldname>` 结尾的 dataflow key 无差别匹配。不同 struct 的同名字段（`.handler`、`.callback`、`.transform`）全部命中。

**修复**：
- 在 Pass 1 收集 field assignment 时，额外构建 `field_to_bases` 索引：字段名 → 拥有该字段的基础变量名集合。
  例如 dataflow 中有 `<gstruct:my_ops.init>`、`<gstruct:my_ops.name>`、`<gstruct:other_ops.init>`，
  则索引为 `{"init": {"my_ops", "other_ops"}, "name": {"my_ops"}}`
- suffix fallback 触发时（以 `base.handler` 为例）：
  1. 从索引取出所有拥有 `.handler` 的候选 bases
  2. 对每个候选 base，检查它和当前 base 在 dataflow 中有**任意共享字段名**（不限于 fnptr，任意字段都算）：
     - 取 `{fields of 候选 base}` ∩ `{fields of 当前 base}`
     - 交集非空 → 纳入 target
     - 交集为空（两个 struct 完全无共同字段名）→ 排除
  3. 如果当前 base 本身不在索引中（即对它没有任何 field 记录），降级为**同文件内**的 suffix 匹配——只扫描与当前分析文件相同的 `<gstruct:>` key
- 保留精确 `<gstruct:exact.path>` 查找优先的现有逻辑

### P2：callback_reg 加 fnptr 形参位置校验（影响 ~70 条 callback_reg 误报）

**根因**：`_is_registration` 子串匹配判定注册函数，对匹配函数的**所有** identifier 实参无条件创建 `callback_reg` 边，不区分 fnptr 与非 fnptr 实参。

**修复**：
- `_register_phase`（Phase 1a）中已有的 `_collect_func_params(tree.root_node, func_params)` 调用改为 `_collect_func_params(tree.root_node, func_params, func_fp_params)`，同时收集 fnptr 形参位置到 `engine.func_fp_params`
- Pass 1 `_collect_call_params` 中 `_is_registration(call_name)` 分支：
  - 从 `dataflow.func_fp_params`（或 `dataflow.state.func_fp_params`）获取 fnptr 形参位置
  - 如果 `call_name` 在 `func_fp_params` 中有记录，只对落在 fnptr 形参位置上的 identifier 实参创建 `callback_reg` 边
  - 如果 `call_name` 不在 `func_fp_params` 中（跨文件调用且被调函数定义不可见），保留现有无条件行为避免漏报

### P3：参数名 dataflow key 加函数作用域前缀（影响约 30 条间接误报）

**根因**：`param_assign` Pass 1 直接以裸形参名作为 dataflow key，不同函数同名形参的 targets 在全局 dataflow 中合并污染。

**修复**：
- 写入侧（`param_assign.py` Pass 1）：`dataflow.assign(pname, target)` → `dataflow.assign(f'{enclosing_func}:{pname}', target)`
- 读取侧（`param_assign.py` Pass 2）：先 resolve 带前缀 key，命中则用；未命中 fallback 到裸 key（兼容其他模块写入的历史条目）
- `VariableState` 本身不修改——作用域语义由 key 命名约定保证
- `direct_assign` 等其他模块不受影响（它们使用变量名和 `<gstruct:>`/`<garray:>` 等结构化 key）

## 涉及文件

| 文件 | 变更类型 |
|------|---------|
| `src/ethunter/analyzer/param_assign.py` | P0：移除 Pass 4 边产出；P2：在 `_register_phase` 和 Pass 1 加 fnptr 形参校验；P3：key 加函数前缀 |
| `src/ethunter/analyzer/field_call.py` | P1：suffix fallback 改为类型感知匹配（新增 field→bases 索引 + 交集校验） |
| `src/ethunter/analyzer/orchestrator.py` | P2：Phase 1a 调用 `_register_phase` 时确保 `func_fp_params` 被收集到 engine 上 |
| `tests/test_et_bench.py` | 新增各场景 recall gate + FPR assertion；新增 4 个针对性 TDD 测试 |

## 测试策略（TDD）

### 回归守卫

- 为 9 个目标场景各加 `test_<category>_full_recall` 断言，100% recall 硬 gate
- 每个场景加 FPR 上限断言（修复后数值低于基线）

### 针对性 TDD 测试

**test_p0_param_callback_no_nx_m_edges**：3 个不同 caller 各传自己的 callback 给同一个注册函数。断言 `callback_param` 边恰好每 caller→对应 target，无 N×M 乘积极。

**test_p1_field_call_suffix_same_struct_only**：两个不相关 struct（`A`、`B`）各有 `.handler` 字段。断言 `a->handler()` 只解析到 `A.handler` 的赋值，不匹配 `B.handler`。

**test_p2_callback_reg_only_fnptr_positions**：注册函数 `register_item(name, priority, handler)` 第三个参数是 fnptr。断言只对 handler 位置的实参产 `callback_reg`，name/priority 位置不产。

**test_p3_param_namespace_isolation**：函数 `f1(cb)` 接收 `func_a`，`f2(cb)` 接收 `func_b`。断言 resolve `f1:cb` = `{func_a}`，resolve `f2:cb` = `{func_b}`。

### 开发流程

每个修复点：
1. 先写针对性测试，确认测试**因正确原因失败**（当前代码产生额外误报边）
2. 实现修复代码
3. 运行全量 `tests/test_et_bench.py` 确认 9 场景召回率 100%、误报率下降
4. 运行全量 `tests/` 确认无回归
