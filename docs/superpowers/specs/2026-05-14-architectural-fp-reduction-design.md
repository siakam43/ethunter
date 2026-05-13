# 架构级误报降低方案设计

**日期**: 2026-05-14
**目标**: 解决设计缺陷 #1（类型溯源）、#3（启发式注册）、#5（阶段间反馈）、#6（输出模型），降低 ~130 FPs

## 当前基线

| 场景 | 召回 | FPR | 主要 FP 来源 |
|------|------|-----|------------|
| fnptr-callback | 100% (33/33) | 64.52% | callback_param 57 |
| fnptr-global-struct-array | 100% | 46.97% | field_call 62 |
| fnptr-global-struct | 100% | 43.33% | callback_param 24 + callback_reg 20 + field_call 8 |
| fnptr-struct | 100% | 38.24% | field_call 7 |
| fnptr-library | 100% | 17.65% | field_call 13 |
| fnptr-cast | 100% | 60.00% | field_call 6 + callback_reg 4 |
| fnptr-varargs | 100% | 50.00% | field_call 1 |
| fnptr-only | 100% | 7.69% | callback_reg 1 |

剩余可机械修复的误报: field_call ~98 + callback_reg ~28 = **~126 FPs**。

---

## Fix 1: Dataflow Type+Name Key（#1，98 field_call FPs）

### 设计

将 `<gstruct:>` dataflow key 从 `<gstruct:var.field>` 改为 `<gstruct:type.var.field>`——type+name 复合 key。

```
现状: <gstruct:obj_a.handler> -> {handler_a}
      <gstruct:obj_b.handler> -> {handler_b}
      // 不同类型同名字段无法区分

改为: <gstruct:type_a.obj_a.handler> -> {handler_a}
      <gstruct:type_b.obj_b.handler> -> {handler_b}
      // type 前缀区分不同类型，var 后缀区分同类型实例
```

**suffix 扫描改进**：从全局 `key.endswith()` 改为 type 前缀限定：

```python
# OLD: 无差别全局扫描
for key, vals in dataflow.targets.items():
    if key.endswith(f'.{fieldname}>') and vals:
        targets.update(vals)

# NEW: type 前缀限定扫描
type_prefix = f'<gstruct:{struct_type}.'
for key, vals in dataflow.targets.items():
    if key.startswith(type_prefix) and key.endswith(f'.{fieldname}>') and vals:
        targets.update(vals)
```

**example_6 效果**：

```
现状: <gstruct:command_table.func> -> 36 zfs_do_*
      <gstruct:command_table.help> -> 36 HELP_*     <- 整数常量误存
      <garray:command_table>       -> 72（合并）    <- 引入 HELP_* FPs

改后: <gstruct:zfs_command_t.command_table.func> -> 36 zfs_do_*
      suffix scan 限定 startswith('<gstruct:zfs_command_t.')
      HELP_* 不会混入。不再需要 <garray:> fallback。
```

### 涉及模块

| 模块 | 变更 |
|------|------|
| `initializer_assign.py` | 写 `<gstruct:type.var.field>` 替代 `<gstruct:var.field>` |
| `param_assign.py` | `_propagate_call_site` key 改用 type.var.field |
| `field_call.py` | suffix scan 加 type 前缀限定 |
| `dataflow.py` | 新增 `var_to_type` dict |
| `symbol_table.py` | 新增 `record_var_type(var_name, struct_type)` |

### 变量->类型映射

- `initializer_assign`: 从 AST 声明提取 `struct type_a obj_a = {...}`
- `direct_assign`: 指针解析传递类型
- 全局变量: translation_unit 的 declaration 节点

### 召回安全性

- 同类型不同实例通过 type 前缀关联，不跨类型污染
- 不同实例由 var 后缀独立，不错误合并
- var_to_type 缺失时降级为原 suffix 扫描

## Fix 3: 行为注册检测替代启发式匹配（#3，影响 ~28 callback_reg FPs）

### 设计

用 func_fp_params 的行为分类替代 `_is_registration` 子串匹配。已经在 `_register_phase` 中通过 `param_fields` 追踪了 field assignment 模式——扩展为 fnptr param usage 分类。

```
param_usage[(func_name, param_idx)] = {
    'stored_in_field': bool,    # field = param 赋值
    'called_directly': bool,    # param(args) 或 (*param)(args)
    'forwarded': bool,          # other_func(param) 传递
}
```

### 在 Pass 1 callback_reg 判定中使用

```python
# OLD: 子串匹配
if _is_registration(call_name):
    # emit callback_reg edge

# NEW: 行为检测
usage = dataflow.param_usage.get((call_name, arg_idx), {})
if usage.get('called_directly'):
    # fnptr is directly called — emit callback_reg with caller as immediate context
    edges.append(CallEdge(indirect_kind='callback_reg', ...))
elif usage.get('stored_in_field') or usage.get('forwarded'):
    # fnptr stored or forwarded — suppress callback_reg
    # (field_call or chain resolution handles it with better caller info)
    pass
else:
    # Unknown behavior — fallback to existing _is_registration (conservative)
    if _is_registration(call_name):
        edges.append(CallEdge(indirect_kind='callback_reg', ...))
```

### 分析位置

`_register_phase`（Phase 1a）中——已有 `collect_field_assignments` 和 `_collect_func_params`，可直接在此基础上扩展。

**Forwarder 检测**: 扫描函数体内 call_expression，检查实参列表中是否有 fnptr param 名。

**Storage 检测**: 已有——`param_fields` 记录。

**Caller 检测**: 扫描函数体内 call_expression，检查被调用函数是否是 fnptr param（`cb(args)` / `(*cb)(args)`）。

---

## Fix 5: 前置 covered_callees 替代后处理 suppression（#5）

### 设计

当前 Fix B/A-1 的问题是后处理：param_assign 产边时不知道 field_call 会覆盖相同 callee，边产出后才被 orchestrator 删除。

**核心思路**：将 `covered_callees` 的构建前置到 param_assign 产边之前，让产边时就能判断"这个 callee 会被 field dispatch 覆盖，不需要我产边"。

### 流水线调整

```
Phase 1a:  _register_phase（param_fields 注册）
Phase 1a.5: field_call._resolve_struct_fields（写 <gstruct:> key，不产边）
Phase 1:   TARGET_RESOLVERS（initializer_assign 写 <gstruct:> key；
          param_assign 第一次调用写 <gstruct:> key 和 dataflow，边丢弃）
Phase 1.5: 从 engine.targets 的所有 <gstruct:>* key 构建 covered_callees
Phase 1b:  param_assign 第二次调用（产 callback 边 ← 检查 covered_callees）
Phase 2:   CALL_DETECTORS（field_call._detect_field_calls 读 <gstruct:> key 产边）
```

### field_call 拆分为两个函数

```python
# field_call.py

def _resolve_struct_fields(tree, filepath, symbol_table, dataflow):
    """Phase 1a.5: 扫描 struct field assignments，写 <gstruct:> key。不产边。"""
    symbol_names = symbol_table.all_function_names
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)

def _detect_field_calls(tree, filepath, symbol_table, dataflow):
    """Phase 2: 检测 struct field expression calls，产 field_call 边。"""
    # 原 analyze() 的 Pass 2 逻辑（_visit 遍历 + callback-of-callback）
    ...
```

### orchestrator 流水线

```python
# Phase 1a
for filepath, tree in trees.items():
    param_assign._register_phase(tree, filepath, symbol_table, engine)

# Phase 1a.5: field_call struct resolution (only writes dataflow, no edges)
for filepath, tree in trees.items():
    field_call._resolve_struct_fields(tree, filepath, symbol_table, engine)

# Phase 1: TARGET_RESOLVERS
# initializer_assign, direct_assign, cast_assign, param_assign all write
# <gstruct:> keys to engine.targets. param_assign edges are discarded here.
for filepath, tree in trees.items():
    for resolver in TARGET_RESOLVERS:
        resolver.analyze(tree=tree, filepath=filepath,
                         symbol_table=symbol_table, dataflow=engine)

# Phase 1.5: Build covered_callees from all <gstruct:> keys written above
covered_callees = set()
for key, vals in engine.targets.items():
    if key.startswith('<gstruct:'):
        covered_callees.update(vals)
engine.covered_callees = covered_callees

# Phase 1b: param_assign callback detection (checks covered_callees before emit)
for filepath, tree in trees.items():
    edges = param_assign.analyze(tree=tree, filepath=filepath,
                                 symbol_table=symbol_table, dataflow=engine)
    for edge in edges:
        graph.add_edge(edge)

# Phase 2: CALL_DETECTORS
for filepath, tree in trees.items():
    for detector in CALL_DETECTORS:
        edges = detector.analyze(tree=tree, filepath=filepath,
                                 symbol_table=symbol_table, dataflow=engine)
        for edge in edges:
            graph.add_edge(edge)
```

### param_assign 产边前检查

在 `_is_registration` 和 Pass 3/4 产 callback_reg / callback_param 边前：

```python
# 如果 callee 在 covered_callees 中，field dispatch 会覆盖它
if target in getattr(dataflow, 'covered_callees', set()):
    continue  # field_call will dispatch this callee via struct field
```

### 对 orchestrator 的影响

替代当前 Fix B 的后处理过滤器——将其从 orchestrator 中移除，改为 Phase 1a.6 的 `covered_callees` 构建。Fix B 代码删除。

### 召回安全性

与 Fix B 的 recall safety check 结果相同：0 GT 边丢失。`covered_callees` 仅抑制已被 struct field tracking 覆盖的 callback 边。

---

## Fix 6: 多视角 Edge 输出模型（#6）

### 设计

在 `CallEdge` 中新增可选字段 `chain_context`，记录完整调用链信息：

```python
@dataclass
class CallEdge:
    caller: str
    callee: str
    ...
    indirect_kind: str
    chain_context: str = ''  # NEW: 'immediate' | 'outer' | 'field_dispatch'
```

- `immediate`: 边来自直接 fnptr 调用（Pass 3）
- `outer`: 边来自 call site 参数传递（Pass 4）
- `field_dispatch`: 边来自 struct field dispatch（field_call）

下游消费者可根据 `chain_context` 选择需要的视角，而非当前硬编码 "唯一正确 caller"。

### 实现

各分析器产边时设置 `chain_context` 字段。JSON 序列化/反序列化支持该字段。现有 edge 模型无变更（新字段默认为空字符串，向后兼容）。

---

## 涉及文件

| 文件 | 变更 |
|------|------|
| `src/ethunter/analyzer/dataflow.py` | Fix 1: `var_to_type` dict + `covered_callees` set；Fix 5: covered_callees |
| `src/ethunter/analyzer/symbol_table.py` | Fix 1: `record_var_type()` |
| `src/ethunter/analyzer/initializer_assign.py` | Fix 1: 写 `<gstruct:type.var.field>` key |
| `src/ethunter/analyzer/param_assign.py` | Fix 1: `<gstruct:>` key 改用类型名；Fix 3: 行为检测替代 `_is_registration`；Fix 5: 产边前检查 covered_callees |
| `src/ethunter/analyzer/field_call.py` | Fix 1: 精确 key 查找替代 suffix scan；Fix 5: 写入 covered_callees |
| `src/ethunter/analyzer/orchestrator.py` | Fix 5: 移除 Fix B 后处理（已被内联替代） |
| `src/ethunter/graph/model.py` | Fix 6: CallEdge 新增 `chain_context` 字段 |
| `tests/test_et_bench.py` | 新增 TDD 测试；更新 FPR ceilings |

## 测试策略（TDD）

### Fix 1 测试

**test_fix1_type_aware_field_lookup**: 两个不同 struct type 同名 handler 字段。断言 `<gstruct:type_a.handler>` 和 `<gstruct:type_b.handler>` 分别只解析到各自的 targets。

### Fix 3 测试

**test_fix3_behavioral_registration**: 注册函数 `register_fn(fnptr)` 仅转发（不直接调用不存储）。断言不产 callback_reg。注册函数 `dispatch_fn(fnptr)` 直接调用 fnptr。断言产 callback_reg。

### Fix 5 测试

**test_fix5_covered_callees**: 同 Fix B 测试场景——field_call 覆盖的 callee 不再由 callback_reg 产出。

### 回归守卫

- 9 场景 100% recall gate
- 全量 tests/ 无回归。
