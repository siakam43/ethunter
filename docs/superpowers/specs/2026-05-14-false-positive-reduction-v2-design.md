# ET-Bench 误报率降低（第二轮）方案设计

**日期**：2026-05-13
**来源**：`docs/et_bench_false_positive_analysis_v2.md` 分析结论
**总体目标**：进一步降低误报率，**严格保证** 9 场景召回率 100%（fnptr-virtual 和 fnptr-dynamic-call 除外）

## 基线数据（P0/P2/P3 修复后）

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

## 修复设计

### Fix A：Pass 1 fallback 分支使用前缀 key 解析（影响 ~450 callback_param FPs）

**根因**：`param_assign.py` Pass 1 `_collect_call_params` 中，当 call site 的实参标识符不在 `symbol_names` 中时（是局部变量或参数名），fallback 到 `dataflow.resolve(target)`（bare key）。该 bare key 因 P3 的兼容写入（写入 `call_name:pname` 的同时也写 `pname`）被所有调用者的 targets 全局合并。在 example_4 中，`zfs_ioctl_register_pool` 体内将参数 `func` 转发给 `zfs_ioctl_register_legacy` 时，`dataflow.resolve("func")` 从 bare key 拿到全部 75 个 `zfs_ioc_*` targets（而非仅该调用点的 target）。

**修复**：fallback 分支改为前缀优先解析。

```python
# OLD (~line 462):
df_targets = dataflow.resolve(target)

# NEW:
df_targets = dataflow.resolve(f'{caller}:{target}')
if not df_targets:
    df_targets = dataflow.resolve(target)
```

`caller` 是 `find_enclosing_function(node, tree.root_node)`（当前 call expression 所在函数体）。P3 的写入 key 为 `f'{call_name}:{pname}'`。在函数体内转发参数场景下（如 `register_pool` 体内调用 `register_legacy(func)`）：
- 写入: 在 `zfs_ioctl_init` 处理中调用 `dataflow.assign("zfs_ioctl_register_pool:func", ...)`
- 解析: 在 `zfs_ioctl_register_pool` 体内处理中 `dataflow.resolve("zfs_ioctl_register_pool:func")`
- key 匹配，正确拿到该调用者特定 targets

**召回安全性**：bare key 仍然写入（P3 兼容），无前缀 key 时不命中时 fallback 到 bare key。其他模块（direct_assign 等）对参数名的 resolve 不受影响——它们使用 bare key resolve，该 key 仍被 P3 填充。

### Fix B：callback_reg struct-field 冗余后处理抑制（影响 ~100 callback_reg FPs）

**根因**：当 fnptr 实参最终存入 struct 字段时，`field_call` 已产出精确边（caller=dispatcher），`callback_reg` 同时产出粗略边（caller=注册调用者），后者全部冗余。example_4：field_call 产出 57 条正确边 `(zfsdev_ioctl_common, zfs_ioc_*)`，callback_reg 产出 77 条冗余边 `(zfs_ioctl_init, zfs_ioc_*)`。

**机制**：call chain 为 `zfs_ioctl_init → zfs_ioctl_register_pool → zfs_ioctl_register_legacy → field assignment`。`_register_phase` 的 `param_fields` 仅追踪 leaf 函数（`zfs_ioctl_register_legacy`）的 param→field 映射，中间 wrapper 不在 `param_fields` 中——因此需用后处理方式（按 callee 重叠判断）而非 emit 时的 `param_fields` 检查。

**修复**：orchestrator 去重阶段新增抑制逻辑——在所有分析完成后，对每条 `callback_reg` 边，检查是否存在同 callee 的 `field_call` 边。如果存在，表明该 fnptr 通过 struct field dispatch 被调用，`field_call` 的 caller 信息更精确，`callback_reg` 边为冗余。

```python
# orchestrator.py dedup 阶段新增:
# Collect callees that have field_call edges
field_callees = {e.callee for e in graph.edges
                 if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}

# Filter callback_reg edges: suppress those whose callee is already covered by field_call
filtered_edges = []
for edge in graph.edges:
    if edge.indirect_kind == 'callback_reg' and edge.callee in field_callees:
        continue  # field_call already provides a better edge for this callee
    filtered_edges.append(edge)
graph.edges = filtered_edges
```

**召回安全性验证**：实现前先用临时日志或 Python 脚本标记出所有由 `callback_reg` 产出、`field_call` **未**覆盖的 ground truth 边。如果存在此类边，则仅在有 `field_call` 覆盖时才抑制，不能全局删除。验证方法：

```python
# 对每个 example:
field_callees = {e.callee for e in edges if e.indirect_kind == 'field_call'}
cr_matched = {(e.caller, e.callee) for e in edges
              if e.indirect_kind == 'callback_reg' and (e.caller, e.callee) in gt_pairs}
cr_would_lose = {(c, t) for (c, t) in cr_matched if t not in field_callees}
# 如果 cr_would_lose 非空，则存在只在 callback_reg 中而不在 field_call 中的 ground truth 边
```

## 涉及文件

| 文件 | 变更类型 |
|------|---------|
| `src/ethunter/analyzer/param_assign.py` | Fix A：Pass 1 fallback 分支 ~line 462 改前缀解析 |
| `src/ethunter/analyzer/orchestrator.py` | Fix B：dedup 阶段新增 callback_reg × field_call 重叠抑制 |
| `tests/test_et_bench.py` | 新增 TDD 测试；更新 FPR ceilings |

## 测试策略（TDD）

### 回归守卫

- 现有 9 场景 100% recall gate（已存在于 test_et_bench.py）
- 现有 FPR ceiling 断言（更新为修复后值）
- 全量 `tests/` 无回归

### 针对性 TDD 测试

**test_fix_a_fallback_prefixed_resolve**：

```c
// func 参数在 register_pool 体内转发给 register_legacy
// 两个不同 caller 各传不同 target
register_pool(&c, handler_a);   // → 应只解析到 handler_a
register_pool(&c, handler_b);   // → 应只解析到 handler_b
```

断言 `register_pool` 体内的 call site 通过前缀 key 只拿到对应 target。

**test_fix_b_callback_reg_suppress_when_field_mapped**：

```c
// register_legacy 将 fnptr 存入 struct field
// register_pool 调用 register_legacy(handler)
// field_call 已产出 (dispatcher, handler)
```

断言 `callback_reg` 边不包含 `(register_pool, handler)`（被 field mapping 抑制），但 `field_call` 边正常产出。

**test_fix_b_callback_reg_still_emits_when_no_field_mapping**：

```c
// 注册函数 register_cb(cb) 直接调用 cb()，不存 field
// callback_reg 应正常产出
```

断言无 field mapping 时 callback_reg 正常产出（保证召回不降）。

### 开发流程

每个修复点：
1. 先写针对性测试，确认测试因正确原因失败
2. 实现修复代码
3. 运行全量 `tests/test_et_bench.py` 确认 9 场景召回率 100%、误报率下降
4. 运行全量 `tests/` 确认无回归
