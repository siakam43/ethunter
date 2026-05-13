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

### P0：Pass 4 使用按调用点解析，消除 N×M 膨胀（影响 ~500 条 callback_param 误报）

**根因**：`param_assign` Pass 1 中所有 call site 将 target 合并入全局 `param_mappings[pname]`（例如 example_13 中 7 个不同外层 caller 各自传不同的 valueize 函数，`param_mappings["valueize"]` 合并为全部 7 个 target）。Pass 4 对每个外层 caller 从 `param_mappings[pname]` 取到全部 7 个 target → 产 N×M 条边。Pass 3 同样因合并的 `param_mappings` 产生错误 caller 名（`gimple_fold_stmt_to_constant_1` 而非 ground_truth 期望的 `ccp_fold`）的多余边。

**ground truth caller 约定**：通过实际数据验证（example_2 vs example_13），ground_truth 中 caller 可能是外层调用者（Pass 4 的 caller）也可能是内层被调函数体（Pass 3 的 caller），取决于场景。因此不能简单移除任一 Pass。核心修复是消除 `param_mappings` 合并导致的 N×M 膨胀。

**修复**：
1. Pass 1 新增数据结构 `call_site_targets: dict[tuple[str, str, int], set[str]]`，key 为 `(caller, call_name, arg_idx)`，记录每个 call site 实际传入的 target（不做跨 call site 合并）。Pass 1 继续维护 `param_mappings`（合并版）供 Pass 2 field assignment 解析使用
2. Pass 4 重写为使用 `call_site_targets` 逐调用点解析：对每个 call site `(caller, call_name, arg_idx)` 仅取其实际传入的 target 发射边，避免 N×M
3. Pass 3 保留但不再使用合并后的 `param_mappings` 发射边——改为查询 `call_site_targets`：检测到 `cb(args)` 在 `callee_func` 体内时，查询所有 `(*, callee_func, arg_idx_for_cb)` 条目，对每个匹配的 target 发射 `(callee_func, target)` 边
4. orchestrator 现有的 `(caller, callee)` 去重自动消除 Pass 3 和 Pass 4 的重复边

**效果**：Pass 4 每条 call site 仅发射 1 条边（而非 M 条），Pass 3 每条 target 仅发射 1 条边（而非 N 条）。总 `callback_param` 边从 O(N×M) 降为 O(N+M)。

### P1：field_call suffix fallback 类型感知匹配（影响 ~120 条 field_call 误报）

**根因**：`field_call.py` 中有两处无差别 suffix 扫描：
- 第 134-140 行（Always merge suffix）：即使精确 `<gstruct:>` 查找已成功，仍然扫描所有以 `.fieldname>` 结尾的 key 并合并——这是无条件噪声注入
- 第 178-180 行（Last-component fallback）：当所有结构化查找都失败时，扫描所有以 `.fieldname>` 结尾的 key

两处均不对 struct 来源做任何校验，不同 struct 的同名字段（`.handler`、`.callback`、`.transform`）全部命中。

**修复**：
- 在 field_call.analyze() 入口处新增辅助函数 `_build_field_index(dataflow)` 构建索引：字段名 → 拥有该字段的基础变量名集合。
  例如 dataflow 中有 `<gstruct:my_ops.init>`、`<gstruct:my_ops.name>`、`<gstruct:other_ops.init>`，
  则索引为 `{"init": {"my_ops", "other_ops"}, "name": {"my_ops"}}`。
  只扫描 `<gstruct:>` 前缀的 key，`<struct:>`/`<chain:>` 暂不纳入（噪声较低）
- 新增辅助函数 `_suffix_resolve(dataflow, field_index, base, fieldname)` 替代所有裸 suffix 扫描：
  1. 从索引取出所有拥有 `fieldname` 的候选 bases
  2. 对每个候选 base，取它与当前 base 各自在索引中的所有字段，计算交集
  3. 交集非空 → 纳入 target（两个 struct 有共同字段名，说明是同类型 struct）
  4. 交集为空 → 排除（两个 struct 完全无共同字段名）
  5. 如果当前 base 不在索引中，降级为**同文件** suffix 匹配（只扫描 `<gstruct:>` key 中与当前分析文件路径相关的条目——实际受限于 dataflow 中同一文件赋值的 key）
- 第 134-140 行和第 178-180 行的裸 suffix 扫描均替换为 `_suffix_resolve` 调用
- 第 172-176 行的 `dataflow.resolve(last_part)` 保留（是精确 key 查找，非 suffix 扫描）
- 保留精确 `<gstruct:exact.path>` 查找优先的现有逻辑

### P2：callback_reg 加 fnptr 形参位置校验（影响 ~70 条 callback_reg 误报）

**根因**：`_is_registration` 子串匹配判定注册函数，对匹配函数的**所有** identifier 实参无条件创建 `callback_reg` 边，不区分 fnptr 与非 fnptr 实参。

**修复**：
- `_register_phase`（Phase 1a）中已有的 `_collect_func_params(tree.root_node, func_params)` 调用改为 `_collect_func_params(tree.root_node, func_params, func_fp_params)`，同时收集 fnptr 形参位置并累积到 `engine.func_fp_params`（使用 `.update()` 合并跨文件结果，不可覆盖赋值）
- 同时修改 Phase 1 `analyze()` 中的 `func_fp_params` 存储：从覆盖赋值（`=`）改为 `.update()` 合并，防止 Phase 1a 收集的跨文件数据被覆盖
- Pass 1 `_collect_call_params` 中 `_is_registration(call_name)` 分支：
  - 从 `dataflow.func_fp_params`（或 `dataflow.state.func_fp_params`）获取 fnptr 形参位置
  - 如果 `call_name` 在 `func_fp_params` 中有记录，只对落在 fnptr 形参位置上的 identifier 实参创建 `callback_reg` 边
  - 如果 `call_name` 不在 `func_fp_params` 中（跨文件调用且被调函数定义不可见），保留现有无条件行为避免漏报

### P3：参数名 dataflow key 加函数作用域前缀（影响约 30 条间接误报）

**根因**：`param_assign` Pass 1 直接以裸形参名作为 dataflow key，不同函数同名形参的 targets 在全局 dataflow 中合并污染。

**修复**：
- 写入侧（`param_assign.py` Pass 1 `_collect_call_params`）：`dataflow.assign(pname, target)` → `dataflow.assign(f'{call_name}:{pname}', target)`。
  注意使用 `call_name`（被调函数名——形参所属函数）而非 `caller`（外层调用函数），因为污染根源在于不同被调函数中同名形参的 targets 合并
- 读取侧（`param_assign.py` Pass 2）：`dataflow.resolve(param_name)` 前加一步 `dataflow.resolve(f'{fa.enclosing_func}:{param_name}')`，命中则用，未命中 fallback 到裸 key（兼容其他模块写入的历史条目）
- `VariableState` 本身不修改——作用域语义由 key 命名约定保证
- `direct_assign` 等其他模块不受影响（它们使用变量名和 `<gstruct:>`/`<garray:>` 等结构化 key）

## 涉及文件

| 文件 | 变更类型 |
|------|---------|
| `src/ethunter/analyzer/param_assign.py` | P0：新增 `call_site_targets` 数据结构，Pass 3/Pass 4 改用按调用点解析；P2：`_register_phase` 收集 `func_fp_params`，Pass 1 加 fnptr 形参位置校验；P3：dataflow key 加 `call_name` 前缀 |
| `src/ethunter/analyzer/field_call.py` | P1：新增 `_build_field_index` 和 `_suffix_resolve`，替换两处裸 suffix 扫描 |
| `src/ethunter/analyzer/orchestrator.py` | 无变更（P0/P2/P3 变更均在各模块内部；Phase 1a 调用 `_register_phase` 的接口不变） |
| `tests/test_et_bench.py` | 新增 9 个场景 recall gate + FPR 上限断言；新增 4 个针对性 TDD 测试 |

## 测试策略（TDD）

### 回归守卫

- 为 9 个目标场景各加 `test_<category>_full_recall` 断言，100% recall 硬 gate
- 每个场景加 FPR 上限断言（修复后数值低于基线）

### 针对性 TDD 测试

**test_p0_param_callback_no_nx_m_edges**：非注册函数 `dispatch(cb)` 体内调用 `cb()`。3 个不同外层 caller 各传不同 target（`dispatch(h1)`、`dispatch(h2)`、`dispatch(h3)`）。断言：
- Pass 4（外层 caller）：恰好 3 条 `callback_param` 边（每 caller 仅其 target），无 N×M
- Pass 3（callee 体内）：恰好 3 条边 `(dispatch, h1)`、`(dispatch, h2)`、`(dispatch, h3)`
- 总计最多 6 条 `callback_param` 边（Pass 3 和 Pass 4 各自有不同 caller 名），无 N×M 交叉乘积

**test_p1_field_call_suffix_same_struct_only**：两个不相关 struct（`A`、`B`）各有 `.handler` 字段。断言 `a->handler()` 只解析到 `A.handler` 的赋值，不匹配 `B.handler`。

**test_p2_callback_reg_only_fnptr_positions**：注册函数 `register_item(name, priority, handler)` 第三个参数 `handler` 是 fnptr。传入 `register_item("test", 10, my_handler)`。断言仅 `my_handler` 产出 `callback_reg` 边，`"test"` 和 `10` 不产（即使它们的值恰好也是函数名）。额外验证 `(type)func` 和 `&func` 形式的 fnptr 实参也被正确处理。

**test_p2_callback_reg_cross_file_fallback**：注册函数 `register_unknown` 定义在另一个文件（`func_fp_params` 中无记录）。断言其所有 identifier 实参仍产出 `callback_reg` 边（保证跨文件不降召回）。

**test_p3_param_namespace_isolation**：函数 `f1(cb)` 接收 `func_a`，`f2(cb)` 接收 `func_b`。断言 resolve `f1:cb` = `{func_a}`，resolve `f2:cb` = `{func_b}`。

### 开发流程

每个修复点：
1. 先写针对性测试，确认测试**因正确原因失败**（当前代码产生额外误报边）
2. 实现修复代码
3. 运行全量 `tests/test_et_bench.py` 确认 9 场景召回率 100%、误报率下降
4. 运行全量 `tests/` 确认无回归
