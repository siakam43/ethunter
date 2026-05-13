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

## Fix 1: Dataflow 类型溯源（#1，影响 ~98 field_call FPs）

### 设计

将 `<gstruct:>` dataflow key 从**变量名**改为**struct 类型名**。消除 suffix 扫描，改为精确 key 查找。

```
现状: <gstruct:obj_a.handler> → {handler_a}  // 变量名空间
      <gstruct:obj_b.handler> → {handler_b}  // 同名字段无法区分 struct 类型

改为: <gstruct:type_a.handler> → {handler_a, ...}  // 类型名空间
      <gstruct:type_b.handler> → {handler_b, ...}  // 完全隔离不同类型
```

### 涉及模块

| 模块 | 变更 |
|------|------|
| `initializer_assign.py` | 写 `<gstruct:type.field>` 替代 `<gstruct:var.field>` |
| `param_assign.py` | `_propagate_call_site` / `resolve_call_site_param` 中的 `<gstruct:>` key 改用类型名 |
| `field_call.py` | 替换两处 suffix wildcard 扫描为精确 `<gstruct:type.field>` key 查找 |
| `dataflow.py` | 新增 `var_to_type` 映射字典（变量名→struct 类型名） |
| `symbol_table.py` | 新增 `record_var_type(var_name, struct_type)` 方法 |

### 变量→类型映射来源

- `initializer_assign`: 处理 `struct type_a obj_a = {...}` 时可从 AST 声明提取变量名和类型名
- `direct_assign`: 处理 `obj = &existing_obj` 时可通过指针解析传递类型
- 全局变量声明：从翻译单元的 declaration 节点提取

### field_call 精确 key 查找替代 suffix scan

```python
# OLD (~line 178-180, field_call.py):
# 扫描所有以 .fieldname> 结尾的 key
for key, vals in dataflow.targets.items():
    if key.endswith(f'.{last_part}>') and vals:
        targets.update(vals)

# NEW:
# 需要知道 field_path 对应 struct 的 type。从 dataflow.var_to_type 获取
base_var = field_path.split('.')[0]
struct_type = dataflow.var_to_type.get(base_var, base_var)
targets = dataflow.resolve(f'<gstruct:{struct_type}.{last_part}>')
```

### 召回安全性

- 同类型不同变量实例的 targets 会合并——这是**正确**行为（同类型应有相同可能的 fnptr targets）
- 不同类型同名字段完全隔离——消除 suffix collision
- 如果 var_to_type 映射缺失，降级为原 suffix 扫描（保守）

---

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

## Fix 5: 阶段间反馈 — covered_callees（#5）

### 设计

将 Fix B/A-1 的后处理抑制**内联化**——在 dataflow 中加 `covered_callees` 标记，让 Phase 1 产边前就能检查。

```python
# dataflow.py DataflowEngine 新增:
covered_callees: set[str] = field(default_factory=set)

# field_call.py Phase 2 解析后:
for target in targets:
    dataflow.covered_callees.add(target)

# param_assign.py Pass 1/3/4 产边前:
if target not in dataflow.covered_callees:
    edges.append(CallEdge(...))  # only emit if not already covered by field_call
```

### 对 orchestrator 的影响

替代当前 Fix B 区域的后处理过滤——将其移到各分析器内部的产边逻辑中。orchestrator 中的 Fix B 代码可以移除。

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
| `src/ethunter/analyzer/initializer_assign.py` | Fix 1: 写 `<gstruct:type.field>` key |
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
