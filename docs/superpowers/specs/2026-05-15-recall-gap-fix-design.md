# 召回 Gap 修复设计

**日期**: 2026-05-15
**目标**: 修复 param_binding + param_dispatch 相对于 param_assign 的 7 个召回缺失
**基线**: hybrid pipeline, 100% 召回, FPR 30.54%, 157/157 测试通过

## 背景

Phase 2 替换计划执行时（删除 param_assign）发现 7 条 GT 边丢失：
- fnptr-callback: 6 条 (27/33 → 目标 33/33)
- fnptr-struct: 1 条 (20/21 → 目标 21/21)

经逐边 trace 确认 2 个根因。

## Gap A: param_binding fallback 分支不写 dataflow

### 缺失边

| 场景 | 缺失边 |
|------|--------|
| fnptr-callback/example_6 | `tcache_bin_flush_edatas_lookup -> tcache_bin_flush_ptr_getter` |
| fnptr-callback/example_7 | `defragStream -> defragStreamConsumerGroup`, `defragStreamConsumer -> defragStreamConsumerPendingEntry`, `defragStreamConsumerGroup -> defragStreamConsumer` |
| fnptr-callback/example_8 | `_pqsort -> sort_gp_asc`, `_pqsort -> sort_gp_desc` |

### 机制

Call-site 传递 fnptr 时使用**局部变量**而非直接函数名：

```c
int (*sort_gp_callback)(...) = sort_gp_asc;  // direct_assign 写入 dataflow
pqsort(..., sort_gp_callback, ...);           // ← 局部变量!
```

`param_binding._collect_call_params` 处理流程：
```
target = "sort_gp_callback"  →  not in symbol_names
  → fallback else 分支: dataflow.resolve("sort_gp_callback") → {sort_gp_asc} ✓
  → 更新 LOCAL param_mappings["cmp"] = {sort_gp_asc}
  → 更新 LOCAL call_site_targets[...]
  → 不写 dataflow! ← ROOT CAUSE
```

`param_dispatch` 是**独立模块**，无法访问 `param_binding` 的局部 dict。它从 dataflow 重建 `param_mappings`，但 fallback 分支不写入，导致目标丢失。

```python
# param_dispatch 重建逻辑:
for key, vals in dataflow.targets.items():
    if ':' in key and not key.startswith('<'):
        param_name = key.split(':')[-1]
        param_mappings[param_name].update(vals)
# fallback 分支没写 dataflow → param_mappings["cmp"] 为空 → targets = ∅
```

### 修复

`param_binding._collect_call_params` 的 fallback `else` 分支，补加 dataflow 写入（新增 4 行）：

```python
# 当前代码 (param_binding.py ~line 96-108)
else:
    df_targets = dataflow.resolve(f'{caller}:{target}')
    if not df_targets:
        df_targets = dataflow.resolve(target)
    if df_targets and arg_idx < len(param_names):
        pname = param_names[arg_idx]
        if pname not in param_mappings:
            param_mappings[pname] = set()
        param_mappings[pname].update(df_targets)
        # NEW: write to dataflow so param_dispatch can find these targets
        for t in df_targets:
            dataflow.assign(f'{call_name}:{pname}', t)
            dataflow.assign(pname, t)
        cs_key = (caller or '<unknown>', call_name, arg_idx)
        if cs_key not in call_site_targets:
            call_site_targets[cs_key] = set()
        call_site_targets[cs_key].update(df_targets)
```

### 影响

+6 recall edges（callback scenario 回到 33/33）。

## Gap B: resolve_returned_field 在 initializer_assign 之前运行

### 缺失边

| 场景 | 缺失边 |
|------|--------|
| fnptr-struct/example_9 | `security_callback_debug -> ssl_security_default_callback` |

### 机制

`initializer_assign._track_pointer_field_assignments` 处理 `ptr->field = func` 模式，写入 `<gstruct:*>` dataflow keys。`param_binding` Pass 2 的 `resolve_returned_field` 依赖这些 keys 的 suffix fallback 来解析返回的 struct field 函数指针。

但 `param_binding` 当前在 TARGET_RESOLVERS 中排在**第一位**，Pass 2 在 `initializer_assign` 运行之前执行。时序冲突：

```
Phase 1 execution:
  param_binding Pass 2: resolve_returned_field(...) → suffix scan → ∅ (keys not yet written)
  imedizer_assign._track_pointer_field_assignments → writes <gstruct:ret.sec_cb> (too late)
```

### 修复

将 `param_binding` 的 Pass 2（struct field resolution）独立为一个函数，延迟到所有 TARGET_RESOLVERS 之后执行。

**param_binding.py**:

```python
def analyze(tree, filepath, symbol_table, dataflow) -> list:
    """Pass 1 only: collect call-site params, write dataflow + registration_sites."""
    # 原 Pass 1 的 _collect_call_params + func_params/macros 初始化
    # returns [] (no edges)

def _resolve_fields(tree, filepath, symbol_table, dataflow) -> None:
    """Pass 2: resolve struct member assignments (field=param + return value tracking).
    Must run AFTER all other TARGET_RESOLVERS to have complete dataflow state."""
    # 原 Pass 2 的 collect_field_assignments 处理 + resolve_returned_field
```

**orchestrator.py**:

```python
# Phase 1a: param_helpers.prepare()
for filepath, tree in trees.items():
    param_helpers.prepare(tree, filepath, engine)

# Phase 1a (cont'd): param_assign._register_phase()  [kept during hybrid state]
for filepath, tree in trees.items():
    param_assign._register_phase(tree, filepath, symbol_table, engine)

# Phase 1: Pass 1 — param_binding call params (must be first)
for filepath, tree in trees.items():
    param_binding.analyze(tree, filepath, symbol_table, engine)

# Phase 1: TARGET_RESOLVERS (write dataflow, no edges)
for filepath, tree in trees.items():
    for resolver in [direct_assign, initializer_assign, cast_assign]:
        resolver.analyze(tree=tree, filepath=filepath, symbol_table=symbol_table, dataflow=engine)

# Phase 1: Pass 2 — param_binding field resolution (after all resolvers)
for filepath, tree in trees.items():
    param_binding._resolve_fields(tree, filepath, symbol_table, engine)

# Phase 1b: param_assign callback detection  [kept during hybrid state]
for filepath, tree in trees.items():
    edges = param_assign.analyze(...)

# Phase 2: ...
```

**关键**: `param_assign.analyze()` 仍然调用完整的 Pass 1 + Pass 2（内部已包含 field resolution），但 `param_binding` 的 field resolution 也额外执行，两者不冲突（都写入相同的 dataflow keys）。

### 影响

+1 recall edge（fnptr-struct/example_9 回到 21/21）。

## 涉及文件

| 文件 | 变更 |
|------|------|
| `src/ethunter/analyzer/param_binding.py` | Gap A: fallback 分支加 4 行 dataflow write；Gap B: 拆出 `_resolve_fields()` |
| `src/ethunter/analyzer/orchestrator.py` | Gap B: Phase 1 拆分为 Pass 1 (param_binding) + TARGET_RESOLVERS + Pass 2 (`_resolve_fields`) |
| `tests/test_et_bench.py` | 新增 regression 测试覆盖局部变量传递 fnptr 场景 |

## 预期结果

| 指标 | 当前 | 目标 |
|------|------|------|
| fnptr-callback 召回 | 33/33 (100%) | 33/33 (100%) 无回归 |
| fnptr-struct 召回 | 21/21 (100%) | 21/21 (100%) 无回归 |
| 全场景召回 | 100% (8/9) | 100% (8/9) 无回归 |
| FPR | 30.54% | ≤30.54% 不回升 |
