# ET-Bench 误报率降低（第三轮）方案设计

**日期**: 2026-05-13
**来源**: `docs/et_bench_false_positive_analysis_v3.md` 分析结论
**总体目标**: 进一步降低误报率，**严格保证** 9 场景召回率 100%（fnptr-virtual 和 fnptr-dynamic-call 除外）

## 基线数据（P0+P2+P3+Fix A+Fix B 修复后）

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

剩余可机械修复的误报: callback_param 75 条 + callback_reg ~25 条 = **~100 条**。

## 修复设计

两个修复均在 `orchestrator.py` 的 Fix B 抑制区域，改动相邻。

### Fix A-1: 将 callee-overlap 抑制从 callback_reg 扩展到 callback_param（~75 FPs）

**根因**: Fix B 仅抑制了 callback_reg，但 example_4 中 75 条 callback_param 边具备完全相同的 callee-overlap 特征——callee（`zfs_ioc_*`）在 field_call 中已由更精确的 dispatcher caller 覆盖，wrapper caller（`zfs_ioctl_register_pool` 等）是冗余的。

**修复**: 过滤条件从 `callback_reg` 扩展为 `callback_reg` + `callback_param`：

```python
# OLD:
if edge.indirect_kind == 'callback_reg' and edge.callee in field_callees:

# NEW:
if edge.indirect_kind in ('callback_reg', 'callback_param') and edge.callee in field_callees:
```

### Fix D+E: callback_reg 的 struct-field mapping 抑制（~25 FPs）

**根因**: 当注册函数的 fnptr 参数被存入 struct 字段时（有 param→field mapping），即使 field_call 不为该字段分发调用（如 secpolicy 字段），callback_reg 的 caller 名（`zfs_ioctl_init`）也不是正确的调用关系——真正调用方是 struct dispatch 逻辑。`<gstruct:*>` dataflow 条目记录了哪些 callee 被存入 struct 字段。

**修复**: 扫描 `engine.targets` 中所有 `<gstruct:*>` key，收集 values 为 `struct_callees` 集合。对 callee 在 `struct_callees` 中的 callback_reg 边也抑制。

orchestrator.py Fix B 区域完整代码：

```python
    # Fix B+D+E: suppress callback edges where callee is covered by
    # field_call or tracked via struct field mapping.
    field_callees = {e.callee for e in graph.edges
                     if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    struct_callees = set()
    for key, vals in engine.targets.items():
        if key.startswith('<gstruct:'):
            struct_callees.update(vals)

    if field_callees or struct_callees:
        filtered = []
        for edge in graph.edges:
            # A-1: callback_param where field_call covers same callee
            if edge.indirect_kind in ('callback_reg', 'callback_param') and edge.callee in field_callees:
                continue
            # D+E: callback_reg where callee is tracked in struct field
            if edge.indirect_kind == 'callback_reg' and edge.callee in struct_callees:
                continue
            filtered.append(edge)
        graph.edges = filtered
```

**注意**: Fix A-1 只对 callback_param 加 field_callees 检查，不加 struct_callees 检查。因为 struct_callees 范围更广，对 callback_param 可能过度抑制（callback_param 的 caller 是被调函数体，在某些场景是正确的）。

## 涉及文件

| 文件 | 变更类型 |
|------|---------|
| `src/ethunter/analyzer/orchestrator.py` | Fix B 区域扩展：A-1 加 callback_param 过滤，D+E 加 struct_callees 过滤 |
| `tests/test_et_bench.py` | 新增 TDD 测试；更新 FPR ceilings |

## 测试策略（TDD）

### 回归守卫
- 现有 9 场景 100% recall gate
- 现有 FPR ceiling 断言
- 全量 `tests/` 无回归

### 针对性 TDD 测试

**test_fix_a1_callback_param_suppress_when_field_covered**: 与 Fix B 测试相同模式但断言 callback_param 也消失。两个不同 struct field（handler + cleanup）各自 dispatch 各自 fnptr。field_call 覆盖时 callback_param 被抑制。

**test_fix_de_callback_reg_suppress_when_struct_stored**: 注册函数将 fnptr 存入 struct field（通过 dataflow 的 `<gstruct:>` 条目追踪），即使无 field_call dispatch，callback_reg 也被抑制。

### 开发流程
1. 先写测试，确认失败（FPs 未被抑制）
2. 在 orchestrator.py 改两处过滤条件
3. 全量 ET-Bench 确认 recall 100%、FPR 下降
4. 全量 tests/ 确认无回归
