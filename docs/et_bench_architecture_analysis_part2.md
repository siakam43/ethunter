# ethunter 架构深度分析与改进设计（P0/P2/P3）

**日期**: 2026-05-15
**分析范围**: 缺陷 P0（Suffix 扫描精度）、P2（双轨迁移 + 类型系统扩展）、P3（Registration 启发式替换 + 置信度形式化）
**基于**: et_bench 当前数据（召回率 98.86%，FPR 31.33%，高置信度 FPR 13.44%）

---

## P0：Suffix 扫描精度缺陷

### 当前状态

`FieldResolver.resolve_field_call()` 使用 4-tier 策略链：

| Tier | 方法 | 置信度 | 当前误报来源 |
|------|------|--------|-------------|
| 1 | `gstruct:{type}.{field_tail}` 精确匹配 | high | 覆盖率不足 |
| 2 | `gstruct:{base_var}.{field_tail}` 精确匹配 | high | — |
| 3 | 同文件 suffix 扫描 `.field_tail` | medium | 同文件内不同 struct 同名字段碰撞 |
| 4 | 跨文件 suffix 扫描 `.field_tail` | low | 全局任意 struct 同名字段碰撞 |

Tier 3/4 产生的主要误报：
- `fnptr-virtual`: 60 条（全部来自 Tier 4，`vtable.get_state_map_by_name` vs 同名字段）
- `fnptr-global-struct-array`: 62 条（48 条来自 Tier 3 同文件碰撞，14 条来自 Tier 4）
- `fnptr-library`: 13 条 field_call 误报

### 根因分析

**根因 1：未追踪条目绕过 Tier 3 文件过滤**

写入 `ScopedStore.struct_fields` 的 12 个调用点中，5 个不传 `filepath`：
- `field_call.analyze()` (line 227-228) —— 与 `collect()` 写相同的 key，但不带 filepath
- `dataflow.resolve_call_site_param()` (line 143) —— 旧式 key 转换
- `param_assign.py` 的 3 处遗留写入 (lines 688, 697, 708)

这些未追踪条目在 Tier 3 的 `if files and filepath not in files: continue` 检查中直接通过。由于 `analyze()` 为每次字段赋值写入未追踪的重复条目，**没有任何字段赋值能被 Tier 3 的文件过滤可靠保护**。

**根因 2：Tier 3 仅按文件过滤，不按类型过滤**

同文件内不同 struct 的同名字段（如 `struct tcp_conn.cb` 和 `struct udp_conn.cb`）都通过 Tier 3 匹配到同一个调用点。48 条 `fnptr-global-struct-array` 的 "high" 置信度误报说明同文件碰撞是现实问题。

**根因 3：Tier 4 是全局网**

Tier 4 无任何过滤，匹配 `struct_fields` 中所有以 `.{field_tail}` 结尾的 key，包括完全不相关的 struct 类型。

### 解决方案设计

#### 策略 1：消除未追踪写入——为所有 `assign_struct_field()` 调用传入 filepath

**改动的调用点：**

1. **`field_call.analyze()` (line 227-228)**：传入 `filepath`（已在函数参数中可用）
2. **`dataflow.resolve_call_site_param()` (line 143)**：传入调用点所在文件的路径（需新增参数）
3. **`param_assign.py` (lines 688, 697, 708)**：作为遗留模块，此类修复优先级低——迁移完成后自然消失

**关键改动**：`field_call.analyze()` 是 `collect()` 的重复执行路径，`collect()` 已传入 filepath。只需让 `analyze()` 中的同一条目也传入，即可消除未追踪重复。

**预期效果**：Tier 3 文件过滤开始生效，约 30% 的 suffix 误报被拦截。

#### 策略 2：Tier 3 增加 struct 类型约束

当前 Tier 3 逻辑：
```python
for key, vals in self._store.struct_fields.items():
    if not key.endswith(suffix): continue
    files = self._store.struct_field_files.get(key)
    if files and filepath not in files: continue
    targets.update(vals)  # passes even if struct type differs
```

改进后的 Tier 3：
```python
for key, vals in self._store.struct_fields.items():
    if not key.endswith(suffix): continue
    files = self._store.struct_field_files.get(key)
    if files and filepath not in files: continue
    # NEW: struct type check
    if struct_type:
        # key format: gstruct:<prefix>.<field_tail>
        key_prefix = key.split('.')[0][len('gstruct:'):]
        # if the caller's struct type is known, only match keys of same type
        type_keys = self._symbol_table.get_struct_fields(struct_type)
        field_name = suffix.lstrip('.')
        if key_prefix != base_var and field_name not in type_keys:
            continue  # mismatched struct type
    targets.update(vals)
```

更简洁的实现——通过 `struct_field_files` 已有的类型感知 key 做精确路由：

1. 如果 Tier 1 未命中（类型已知但精确 key 不存在），说明该类型确实没有此字段的数据——不应回退到 suffix 扫描
2. 如果 Tier 1 未命中是因为类型未知，说明没有类型信息可供约束——允许 suffix 扫描（保持召回率）

```python
# Tier 3 改进
targets = set()
if struct_type:
    # 类型已知但 Tier 1 未命中 → 该类型确实无此字段的数据
    # 不应再通过 suffix 扫描捡起其他类型的同名字段
    return targets, 'none', ''
# 类型未知时才 fall through 到 suffix 扫描
suffix = f'.{field_tail}'
for key, vals in self._store.struct_fields.items():
    if not key.endswith(suffix): continue
    files = self._store.struct_field_files.get(key)
    if files and filepath not in files: continue
    targets.update(vals)
```

**关键洞察**：当 `struct_type` 已知时，Tier 1 的精确 key `gstruct:{struct_type}.{field_tail}` 已经是最精确的查询。如果它没命中，说明此类型没有该字段的数据。此时回退到 suffix 扫描只会带回不相关类型的数据。

**预期效果**：彻底消除同文件内不同 struct 类型的字段名碰撞，约 40% 的 suffix 误报被拦截。

#### 策略 3：Tier 4 增加 struct 类型约束

当前 Tier 4 无条件扫描全表。改进后施加与 Tier 3 相同的约束：
- 如果 `struct_type` 已知：跳过 Tier 4（理由同上——类型已知时 suffix 扫描无意义）
- 仅在类型完全未知时作为最后的尽力而为策略

**预期效果**：约 25% 的 suffix 误报被拦截。

#### 综合预期效果

| 策略 | 预计削减 | 影响场景 |
|------|---------|---------|
| 修复未追踪写入 | ~30% suffix 误报 | 跨文件后缀匹配获得文件过滤 |
| Tier 3 类型约束 | ~40% suffix 误报 | 同文件不同 struct 碰撞消除 |
| Tier 4 类型约束 | ~25% suffix 误报 | 全局 suffix 网收窄 |
| **总计** | **158→~20** | FPR 从 31.33% 降至 **~24%** |

**召回率风险**：类型约束策略 2 和 3 仅在 `struct_type` 已知时生效。类型未知时保持现有行为，召回率不受影响。

---

## P2：双轨迁移完成

### 当前状态

编排器同时运行两套并行系统：

```
旧 (param_assign):      analyze() → Pass 1/2/3/4 → callback_reg + callback_param 边
新 (param_binding 等):  analyze() → 写入 dataflow → param_dispatch + callback_reg 消费
```

### 差异矩阵

| 功能 | 旧 (param_assign) | 新 (param_binding+dispatch+callback_reg) | 迁移状态 |
|------|-------------------|------------------------------------------|---------|
| Pass 1: 调用点参数绑定 | 直接产 callback_reg 边 | 写入 registration_sites[]，无延迟 | **覆盖** |
| Pass 2: struct 字段解析 | 写入 dataflow (旧 key) | 写入 dataflow + ScopedStore + filepath | **完全覆盖** |
| Pass 3: callee 体内 fnptr 调用检测 | 产 callback_param 边，list 存储（重复可能） | 产 callback_param 边，set 去重，置信度 high | **完全覆盖** |
| Pass 4: 调用点 caller→target 边 | 产 callback_param 边，无 Pass 3/4 去重 | 产 callback_param 边，Pass A/B 去重 | **完全覆盖** |
| callback_reg 边 | Pass 1 直接产，无 coverage 检查 | 三阶段滤波（行为+覆盖+启发式） | **完全覆盖** |
| param_usage 前向/存储过滤 | Pass 1 阶段检查 | callback_reg Stage 1 检查 | **覆盖** |
| `_is_registration` 回退 | 无 func_fp_params 时启用 | 相同 | **覆盖** |
| 置信度/证据 | 全部默认 medium | 分级 high/medium/low | **超出** |
| ScopedStore 写入 | 否 | 是 | **超出** |
| 文件路径来源追踪 | 否 | 是 | **超出** |

**结论**：新系统在功能上完全覆盖旧系统，并在置信度分级、去重、来源追踪等方面超出旧系统。

### 当前仍然保留旧模块的唯一原因

```python
# orchestrator.py:106
param_assign.analyze(tree, filepath, symbol_table, engine)
# kept for backward compat while migration completes
```

存在一个语义差异需要处理：

**param_binding 不检查 param_usage 就收集 registration sites。** 旧模块在 Pass 1 阶段就过滤了 forwarder/storage。新模块中，所有 site 都进入 `registration_sites[]`，由 `callback_reg` Stage 1 过滤。这导致新模块的 `registration_sites[]` 列表比旧模块实际产出的 registration 更大。这不是正确性问题（最终被过滤），但增加不必要的计算。

### 迁移方案

#### 步骤 1：在 param_binding 中增加 param_usage 预过滤

```python
# param_binding.py analyze() 中，添加到 registration_sites 之前：
usage = dataflow.state.param_usage.get((call_name, arg_idx), 'unknown')
if usage in ('forwarder', 'storage'):
    continue  # 不注册 forwarder/storage 参数
```

这将使新模块的行为与旧模块完全一致。

#### 步骤 2：验证新旧模块边等价性

添加测试脚本，对所有 et_bench fixture 运行两种模式：
- 模式 A：仅旧模块（移除新模块）
- 模式 B：仅新模块（移除旧模块）

验证两个模式的 `(caller, callee)` 对集合相等（允许新模块额外的 高置信度边）。

#### 步骤 3：移除旧模块的 analyze() 调用

```python
# orchestrator.py 删除 lines 106-114
# 保留 register_phase()（纯元数据收集，由 param_helpers.prepare 处理后为 noop）
```

#### 步骤 4：最终统一——合并 register_phase 到 prepare

`param_assign.register_phase()` 中的 `hasattr` 回退在 `param_helpers.prepare()` 已经运行后全部为 noop。待迁移验证后，可完全移除 `register_phase()` 调用。

---

## P2：类型系统扩展

### 当前类型记录点

| 模式 | 处理函数 | 记录内容 |
|------|---------|---------|
| `struct type *ptr;` (局部声明) | `_collect_local_var_types` | `(func, ptr) → type` |
| `type *ptr;` (typedef 局部) | `_collect_local_var_types` | `(func, ptr) → type` |
| `((struct type*)ptr)->field` | `_collect_cast_types` | `(func, ptr) → type` |
| `struct type param` (函数参数) | `_collect_param_types` | `(func, param) → type` |
| `struct type var = {...}` (全局) | `initializer_assign._visit` | `var → type` |

### 缺失的类型记录点

1. **非指针局部声明**：`struct my_type var;` 不被 `_collect_local_var_types` 捕获（仅检查 `pointer_declarator`）
2. **声明（非定义）中的参数**：`_collect_param_types` 仅扫描 `function_definition`，不扫描 `declaration`
3. **函数返回值类型**：`struct my_type *get_obj(void);` —— 无记录
4. **struct 成员类型传播**：`ptr->inner` 的类型无法传播到接收变量
5. **通过赋值的类型推导**：`void *ptr; ptr = some_struct_ptr;`

### 设计方案

#### 扩展 1：扩展 `_collect_local_var_types` 覆盖非指针声明

```python
# field_call.py _collect_local_var_types() 当前逻辑：
for child in func_node.children:
    if child.type == 'declaration':
        type_node = child.child_by_field_name('type')
        declarator = child.child_by_field_name('declarator')
        # 仅查找 pointer_declarator → 未找到 → 跳过
        ptr = declarator.child_by_field_name('declarator')  # pointer_declarator

# 新增：也处理无 pointer_declarator 的声明
for child in func_node.children:
    if child.type == 'declaration':
        type_node = child.child_by_field_name('type')
        if type_node is None: continue
        # 获取声明变量名
        declarator = child.child_by_field_name('declarator')
        var_name = _extract_declarator_name(declarator)
        if var_name is None: continue
        # 获取类型名
        type_name = _extract_type_name_from_specifier(type_node, symbol_table)
        if type_name:
            symbol_table.record_func_var_type(func_name, var_name, type_name)
```

`_extract_declarator_name()` 递归进入 `pointer_declarator`、`field_identifier`、`identifier` 节点提取变量名。
`_extract_type_name_from_specifier()` 从 `type_descriptor` / `struct_specifier` / `type_identifier` 提取类型名。

#### 扩展 2：扩展 `_collect_param_types` 覆盖函数声明

```python
# param_helpers.py _collect_param_types() 当前：
for node in root.children:
    if node.type != 'function_definition':  # 仅处理定义
        continue

# 新增：也处理 declaration 节点
for node in root.children:
    if node.type == 'function_definition':
        self._record_param_types(node, symbol_table)
    elif node.type == 'declaration':
        # 检查是否为函数声明（有 function_declarator）
        declarator = node.child_by_field_name('declarator')
        if declarator and declarator.type == 'function_declarator':
            self._record_param_types_for_decl(node, symbol_table)
```

#### 扩展 3：函数返回值类型记录

在 `param_helpers.py` 中新增 `_collect_return_types()`：

```python
def _collect_return_types(self, root, func_name, symbol_table):
    """记录返回 struct 指针的函数：func_name → 返回类型"""
    for node in root.children:
        if node.type != 'function_definition': continue
        type_node = node.child_by_field_name('type')
        ret_type = _extract_return_struct_type(type_node, symbol_table)
        if ret_type:
            # 新增 SymbolTable 方法
            symbol_table.record_func_return_type(func_name, ret_type)
```

这需要在 `SymbolTable` 中新增 `_func_return_types: dict[str, str]` 和 `get_func_return_type()`。

#### 扩展 4：struct 成员类型传播

当一个 `field_expression` 访问 `ptr->inner` 且 `ptr` 的类型已知时，可推导 `inner` 的成员类型：

```python
# 在 field_call 的 collect 阶段新增
def _collect_member_types(self, root, func_name, symbol_table):
    for node in all_field_expressions(root):
        base_var = extract_base_identifier(node)
        field_name = node.child_by_field_name('field').text.decode()
        base_type = symbol_table.get_func_var_type(func_name, base_var)
        if base_type:
            field_type = symbol_table.resolve_member_type(base_type, field_name)
            # 此信息可用于后续的 type-aware 查询
```

此项扩展较复杂，建议作为独立改进项。`SymbolTable` 需要 `_struct_member_types: dict[tuple[str, str], str]` 来存储 struct 的字段类型信息。

#### 优先级建议

| 优先级 | 扩展 | 影响 | 复杂度 |
|--------|------|------|--------|
| 1 | 非指针声明类型记录 | 提升 Tier 1 命中率 | 低 |
| 2 | 函数声明参数类型 | 覆盖跨文件调用 | 低 |
| 3 | 返回值类型 | 支持 `get_ctx()->field` 模式 | 中 |
| 4 | 成员类型传播 | 支持链式访问 | 高 |

---

## P3：`_is_registration()` 替换

### 当前机制

`REG_PATTERNS` 是 21 个硬编码子串：
```python
['register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_',
 'once', 'submit', 'post', 'work', 'spawn', 'scandir', 'sort', 'filter',
 'notify', 'watch', 'dispatch', 'schedule']
```

`_is_registration(name)` 对函数名做大小写不敏感的子串匹配。这是**纯启发式**，不基于任何语义分析。

### 使用场景分析

`_is_registration()` 在两个位置被调用：

1. **`param_binding.analyze()`（Phase 1）**：当 `func_fp_params` 中没有某个 callee 的记录时（即跨文件调用，callee 的定义不在当前编译单元），作为回退来判断某次调用是否为注册调用。

2. **`callback_reg.analyze()` Stage 3**：当 `param_usage == 'unknown'` 时，判断 callee 是否"看起来像"注册函数。

关键观察：**`_is_registration` 仅在跨文件场景（func_fp_params 不可用）中起作用**。当 callee 在同一编译单元中定义时，`func_fp_params` 提供了精确的 fnptr 参数位置信息，完全绕过了启发式。

### 替代方案

#### 方案 A：基于被调用者函数签名的类型分析（推荐）

当 callee 在同一编译单元中定义时，`func_fp_params` 已经提供了精确信息。当 callee 是**跨文件**的（不在当前编译单元），当前回退到 `_is_registration`。

替代方案：从 callee 的**声明**（即使是 forward declaration）中提取 fnptr 参数位置。

```python
def _is_registration_by_signature(callee_name, func_params, func_fp_params, symbol_table):
    """基于函数签名的注册检测。
    
    优先级：
    1. func_fp_params 中有 callee → 精确位置信息
    2. func_params 中有 callee → 检查参数列表中的 fnptr typedef/语法
    3. 都没有 → 返回 unknown
    """
    if callee_name in func_fp_params:
        return True, func_fp_params[callee_name]  # 精确
    
    if callee_name in func_params:
        params = func_params[callee_name]
        fp_positions = set()
        for idx, pname in enumerate(params):
            if _is_fnptr_param_syntactic(callee_name, idx, symbol_table):
                fp_positions.add(idx)
        if fp_positions:
            return True, fp_positions
    
    return False, set()  # 无法确定
```

**关键改进**：`_is_fnptr_param_syntactic` 从函数声明中检测参数是否为 fnptr（通过 `function_declarator` 嵌套或已知 fnptr typedef），而非依赖名称匹配。

#### 方案 B：从函数声明语法中直接检测 fnptr 参数

扩展 `param_helpers._collect_func_params()` 使其**也从 `declaration` 节点**（不仅是 `function_definition`）收集 fnptr 参数信息。

当前 `func_fp_params` 仅从 `function_definition` 节点收集。如果同时扫描 `declaration` 节点中的函数声明（`function_declarator` + `parameter_list`），跨文件的 callee 也能拥有精确的 fnptr 参数位置信息。

```python
# param_helpers.py _collect_func_params() 扩展
for node in root.children:
    if node.type == 'function_definition':
        self._collect_from_definition(node, func_params, func_fp_params)
    elif node.type == 'declaration':
        decl = node.child_by_field_name('declarator')
        if decl and decl.type == 'function_declarator':
            self._collect_from_declarator(decl, node, func_params, func_fp_params)
```

这使 `_is_registration()` 回退的触发频率从"所有跨文件 callee"降低到"仅在当前文件及被分析的文件中都没有声明的 callee"——接近于零。

#### 方案 C：保守化未知场景

当两种信息源都不可用时（callee 完全不在任何被分析的翻译单元中），默认不假定其为注册函数：

```python
# callback_reg.py Stage 3 改进
if usage == 'unknown':
    # 旧: if _is_registration(callee) → emit low
    # 新: 不处理——需要 fnptr 参数的确认才能注册
    continue  # 直接跳过
```

这会牺牲少量边界场景的召回率（那些真正的注册函数但 callee 定义不可见的情况），但消除了所有基于名称猜测的误报。

### 推荐路径

**分阶段执行**：

1. **Phase 1**：实施方案 B——扩展 `func_fp_params` 收集到函数声明，使 `_is_registration()` 回退仅在极少数情况下触发
2. **Phase 2**：实施方案 C——对未知 callee 默认不注册
3. **Phase 3**：如果 Phase 2 导致明显的召回率回落（通过 benchmark 验证），针对具体缺失模式添加方案 A 的签名分析

**预期效果**：15 条 `callback_reg` 误报 + 部分 `callback_param` 误报被消除。

---

## P3：置信度形式化

### 当前问题

1. **字符串枚举无约束**：`'high'`/`'medium'`/`'low'` 是自由字符串，任何模块可以写任何值
2. **序列化丢失**：`to_dict()` 省略默认值 `'medium'`，round-trip 后恢复但无法区分"显式 medium"和"未设置"
3. **去重排序硬编码**：`_confidence_rank = {'high': 3, 'medium': 2, 'low': 1}` 散落在 orchestrator 中
4. **evidence 自由文本**：不可程序化分析
5. **不一致性**：
   - `array_call`（间接，数据流解析）→ `'high'`，但 `field_resolver` same-file suffix（也是间接，也是数据流解析）→ `'medium'`
   - `field_call` 的 macro 和 callback-of-callback 路径不设置 confidence → 静默默认 `'medium'`
   - `direct_call_fp` 两个不同的代码路径（ScopedStore vs VariableState）使用相同的 evidence 字符串

### 解决方案设计

#### 步骤 1：定义 Confidence 枚举

```python
# src/ethunter/graph/model.py
from enum import Enum

class Confidence(Enum):
    HIGH = 'high'      # 精确：AST 结构确认或精确 key 匹配
    MEDIUM = 'medium'  # 证据丰富但非精确：作用域内 suffix 或行为确认
    LOW = 'low'        # 启发式：跨文件 suffix 或名称匹配

    def ordinal(self) -> int:
        return _CONFIDENCE_ORDINAL[self]

_CONFIDENCE_ORDINAL = {Confidence.HIGH: 3, Confidence.MEDIUM: 2, Confidence.LOW: 1}
```

#### 步骤 2：定义 Evidence 结构

```python
from dataclasses import dataclass

@dataclass(frozen=True)
class Evidence:
    """边发现的结构化证据。"""
    method: str          # 检测方法名（如 'type_aware_exact', 'same_file_suffix'）
    tier: int | None = None  # 可选：multi-tier 解析中的 tier 编号
    source: str | None = None  # 可选：数据源（如 'scoped_store', 'dataflow'）

    def __str__(self) -> str:
        parts = [self.method]
        if self.tier is not None:
            parts.append(f'tier={self.tier}')
        return ':'.join(parts)
```

#### 步骤 3：修改 CallEdge

```python
@dataclass(frozen=True)
class CallEdge:
    caller: str
    callee: str
    caller_file: str
    callee_file: str
    type: CallType
    indirect_kind: str | None = None
    caller_line: int | None = None
    confidence: Confidence = Confidence.MEDIUM
    evidence: Evidence | None = None

    def to_dict(self) -> dict:
        d = { ... }
        d['confidence'] = self.confidence.value  # 始终序列化
        if self.evidence:
            d['evidence'] = str(self.evidence)
        return d
```

#### 步骤 4：统一各模块的置信度赋值

| 模块 | 检测方法 | 当前 confidence | 当前 evidence | 新 confidence | 新 evidence |
|------|---------|----------------|---------------|---------------|-------------|
| direct_call | AST call_expression | high | 'direct call expression' | HIGH | Evidence('direct_call') |
| direct_call_fp | ScopedStore 解析 | high | 'scoped variable resolution' | HIGH | Evidence('scoped_fp_resolve', source='scoped_store') |
| direct_call_fp | VariableState 回退 | medium | 'direct_assign resolution' | MEDIUM | Evidence('flat_fp_resolve', source='dataflow') |
| field_resolver Tier 1 | 类型感知精确 | high | 'type-aware: ...' | HIGH | Evidence('type_aware_exact', tier=1) |
| field_resolver Tier 2 | 精确路径 | high | 'exact path: ...' | HIGH | Evidence('exact_path', tier=2) |
| field_resolver Tier 3 | 同文件 suffix | medium | 'same-file suffix: ...' | MEDIUM | Evidence('same_file_suffix', tier=3) |
| field_resolver Tier 4 | 跨文件 suffix | low | 'cross-file suffix: ...' | LOW | Evidence('cross_file_suffix', tier=4) |
| array_call | 全局数组分发 | high | 'global array dispatch' | MEDIUM | Evidence('array_dispatch') |
| param_dispatch A | callee 体 fnptr 调用 | high | 'fnptr call in callee body' | HIGH | Evidence('callee_body_call') |
| param_dispatch B | 调用点传播 | medium | 'call-site caller -> target' | MEDIUM | Evidence('call_site_propagation') |
| callback_reg caller | 行为确认 | medium | 'behavioral: fnptr ...' | MEDIUM | Evidence('behavioral_registration') |
| callback_reg heuristic | 名称匹配 | low | 'heuristic: registration ...' | LOW | Evidence('heuristic_registration') |
| dlsym_fp | 字符串匹配 | low | 'dlsym string literal match' | LOW | Evidence('dlsym_string_match') |

**关键调整**：`array_call` 从 `HIGH` 降为 `MEDIUM`——它解析的是同一分析阶段写入的 dataflow key，依赖调用链的正确性，并非 AST 级别的直接确认。

#### 步骤 5：去重逻辑使用枚举 Ordinal

```python
# orchestrator.py
def _dedup_edges(edges: list[CallEdge]) -> list[CallEdge]:
    best: dict[tuple[str, str], CallEdge] = {}
    for edge in edges:
        key = (edge.caller, edge.callee)
        if key not in best:
            best[key] = edge
        else:
            if edge.confidence.ordinal() > best[key].confidence.ordinal():
                best[key] = edge
            elif (edge.confidence.ordinal() == best[key].confidence.ordinal()
                  and edge.type == CallType.DIRECT
                  and best[key].type != CallType.DIRECT):
                best[key] = edge
    return list(best.values())
```

#### 步骤 6：修复序列化

移除 `to_dict()` 中的条件省略。始终序列化 `confidence` 和 `evidence`：

```python
def to_dict(self) -> dict:
    return {
        'caller': self.caller,
        'callee': self.callee,
        'caller_file': self.caller_file,
        'callee_file': self.callee_file,
        'type': self.type.value,
        'indirect_kind': self.indirect_kind or '',
        'caller_line': self.caller_line or 0,
        'confidence': self.confidence.value,  # 始终序列化
        'evidence': str(self.evidence) if self.evidence else '',
    }
```

---

## 综合影响预估

| 缺陷 | 改进措施 | 预计削减误报 | 召回率影响 |
|------|---------|-------------|-----------|
| P0: Suffix 扫描 | 未追踪修复 + Tier 3/4 类型约束 | ~140 条 (field_call) | 无 |
| P2: 双轨迁移 | 移除 param_assign.analyze() | ~5 条 (重复边) | 无 |
| P2: 类型系统 | 扩展声明/参数/返回值类型 | ~10 条 (field_call) | 提升 Tier 1 命中率 |
| P3: Registration | 签名分析 + 保守化 unknown | ~15 条 (callback_reg) | 极低（边缘跨文件场景） |
| P3: 置信度 | 形式化 + 一致化 | 0 条直接削减 | 下游可按置信度过滤 |
| **合计** | | **~170 条** | FPR: 31.33% → **~12%** |

剩余的 ~106 条误报将主要由以下难以消除的来源组成：
- `callback_param` 边（~90 条）的内生精度限制——多 caller 到多 target 的乘积效应在缺少完整调用上下文时无法根本消除
- `direct_assign` 边（~10 条）的跨函数变量名污染
- `dlsym_fp` 边（~3 条）的 caller sentinel 问题
