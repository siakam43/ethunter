# Spec: 完全迁移到新 Store + 删除旧 Store + Path B

**日期**: 2026-05-15
**状态**: design
**目标**: 新 store 完全覆盖旧 store 数据，安全删除 Path B 和旧 store 写入

## 背景

数据审计发现 54 条预期边仅通过旧 store 获取。根因分为三类：

| 问题 | 数量 | 根因 |
|------|------|------|
| 链式访问前缀缺失 | 26 | 中间跳转未记录 |
| 2 段 path 数据不完整 | 27 | 写入路径不完整 |
| Pipeline 时序 | cross-cutting | `collect()` 早于 `var_types` 填充 |

## Step 1: 补齐所有写入路径到新 store

### 1.1 `param_assign.py` Pass 2 的三个写入点

**文件**: `src/ethunter/analyzer/param_assign.py:688, 697, 708`

当前代码已调用 `store.assign_struct_field()` 但不传 `filepath`。改动：传入 `filepath`（已在 Task 18 中实现）。同时确保 key 格式与 `param_binding` 一致——使用 `compute_field_tail` 而非原始 `field_path`。

```python
# line 688: Case B (RHS = call_expression)
if hasattr(dataflow, 'store'):
    base_var = field_path.split('.')[0]
    field_tail = dataflow.store.compute_field_tail(field_path)
    dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', t, filepath)

# line 697: Case A Prong 1 (param_mappings)
# line 708: Case A Prong 2 (dataflow resolve)
# same pattern
```

### 1.2 `param_binding._resolve_fields()` field_name-only fallback

**文件**: `src/ethunter/analyzer/param_binding.py:258`

当前只写旧 store `<struct:{field_name}>`。补齐新 store 写入：

```python
dataflow.assign(f'<struct:{field_name}>', t)
if hasattr(dataflow, 'store'):
    dataflow.store.assign_struct_field(f'gstruct:{field_name}', t, filepath)
```

### 1.3 `initializer_assign._track_pointer_field_assignments()`

**文件**: `src/ethunter/analyzer/initializer_assign.py:412, 416`

（已在之前的 Step 1 中实现——line 412/416 补齐了新 store 写入）

---

## Step 2: 建立"中间跳转"追踪（链分解增强）

### 2.1 问题

当前 `collect()` 仅在 `resolved_value in symbol_names` 时写 `struct_fields`。对于 `s->method = &ssl3_method`，`ssl3_method` 不在 `symbol_names`（它是 struct 变量，不是函数名），所以**不被写入**。

链分解需要 `gstruct:s.method → ssl3_method` 才能工作。

### 2.2 方案

`collect()` 中对**所有** `resolved_value is not None` 的赋值都写新 store（无条件）。这样 `gstruct:s.method → ssl3_method` 进入 `struct_fields`，链分解可以找到它。

但**不**写旧 store——旧 store 只接受 `symbol_names` 中的函数名，避免向后兼容问题。

```python
# collect() 修改后：
for fa in collect_field_assignments(tree, ...):
    if fa.resolved_value is not None:
        # Old store: only for known function names
        if fa.resolved_value in symbol_names:
            dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)
        # New store: ALL resolved values (functions + struct vars)
        if hasattr(dataflow, 'store'):
            base_var = fa.field_path.split('.')[0]
            field_tail = dataflow.store.compute_field_tail(fa.field_path)
            dataflow.store.assign_struct_field(
                f'gstruct:{base_var}.{field_tail}', fa.resolved_value, filepath)
            # Type-aware key (only if type info is available)
            struct_type = symbol_table.get_func_var_type(fa.enclosing_func, base_var)
            if struct_type:
                dataflow.store.assign_struct_field(
                    f'gstruct:{struct_type}.{field_tail}', fa.resolved_value, filepath)
```

### 2.3 链分解增强

当链分解的 prefix 匹配到 `struct_fields` 中的值后，判断该值是"中间跳转"还是"最终目标"：

- 如果 `resolved_value` 在 `var_types` 中或作为 `struct_fields` 的 prefix 存在 → **中间跳转**，继续递归分解
- 否则 → **最终目标**，直接返回

```python
# Tier 2 之后，类型门控之前：
parts = field_path.split('.')
if len(parts) >= 3:
    for cut in range(2, len(parts)):
        prefix = '.'.join(parts[:cut])
        suffix = '.'.join(parts[cut:])
        resolved_vars = self._store.resolve_struct_field(f'gstruct:{prefix}')
        if not resolved_vars:
            # Fallback: check old store (during migration)
            continue
        for var_name in resolved_vars:
            # Case A: var_name is a struct variable → intermediate jump
            var_type = self._symbol_table.get_var_type(var_name)
            if var_type:
                targets = self._store.resolve_struct_field(
                    f'gstruct:{var_type}.{suffix}')
                if targets:
                    return targets, Confidence.HIGH, Evidence('chain_resolve', tier=1)
            # Case B: var_name is an exact target → try exact match
            targets = self._store.resolve_struct_field(
                f'gstruct:{var_name}.{suffix}')
            if targets:
                return targets, Confidence.HIGH, Evidence('chain_resolve_exact', tier=2)
```

（此逻辑已在当前代码中实现——确认保留。）

---

## Step 3: Pipeline 重排序

### 3.1 问题

`collect()` 中的 type-aware key 写入依赖 `get_func_var_type(fa.enclosing_func, base_var)`（如 `SSL.method → ssl3_method`）。这对**函数参数**可用（由 `prepare()` 中的 `_collect_param_types` 填充），但对**全局/局部变量**不可用（由 `initializer_assign.analyze()` 填充 `_var_types`，晚于 `collect()`）。

### 3.2 方案

在 `initializer_assign.analyze()` 中拆分出一个纯类型收集函数 `collect_var_types()`，提前到 `field_call.collect()` 之前调用。

**新增函数**:

```python
# initializer_assign.py 新增：
def collect_var_types(tree, filepath, symbol_table, dataflow) -> None:
    """Phase 1a: collect struct variable types from init_declarators.
    Must run BEFORE field_call.collect() so var_types are available."""
    def _scan(node):
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            if declarator:
                var_name = extract_identifier_from_declarator(declarator)
                if var_name:
                    struct_type = _resolve_struct_type_from_decl(node, symbol_table)
                    if struct_type:
                        symbol_table.record_var_type(var_name, struct_type)
        for child in node.children:
            _scan(child)
    _scan(tree.root_node)
```

**Orchestrator 新顺序**:

```python
# Phase 1a:
for filepath, tree in trees.items():
    param_helpers.prepare(tree, filepath, engine, symbol_table)
for filepath, tree in trees.items():
    param_assign.register_phase(tree, filepath, symbol_table, engine)
# NEW: collect var types BEFORE field assignments
for filepath, tree in trees.items():
    initializer_assign.collect_var_types(tree, filepath, symbol_table, engine)
# Phase 1a*:
for filepath, tree in trees.items():
    field_call.collect(tree, filepath, engine, symbol_table, symbol_names)
# ... rest unchanged
```

### 3.3 效果

`collect()` 执行时 `_var_types` 已填充 → `get_func_var_type` 对全局变量有数据 → type-aware key 完整写入 → 链分解 prefix 数据完整。

---

## 数据流总览（三步完成后）

```
pipeline:
  prepare() → register_phase() → collect_var_types() → collect() 
    → param_binding.analyze() → TARGET_RESOLVERS → _resolve_fields()
    → CALL_DETECTORS + param_dispatch + callback_reg

collect():
  所有 resolved_value ≠ None → 写 struct_fields (base + type-aware)
  仅 symbol_names 中的 → 写旧 store (向后兼容)

resolve_field_call("s.method.put_cb"):
  Tier 1: gstruct:SSL.method.put_cb → (type-aware key, 因 Step 3 现在有)
  Tier 2: gstruct:s.method.put_cb → (exact key)
  Chain: prefix=s.method → struct_fields[gstruct:s.method] → ssl3_method
         → var_type[ssl3_method]=SSL_METHOD → gstruct:SSL_METHOD.put_cb ✓
  
  // 链分解失败？继续：
  类型门控 or suffix scan (取决于 struct_type 是否已知)
```

---

## 删除旧 Store + Path B 的判定条件

三步完成后，满足以下所有条件即可删除：

1. **写入覆盖**：所有旧 store `<gstruct:>` 和 `<struct:>` 写入都有对应的新 store key（Step 1）
2. **链分解覆盖**：所有 3+ 段 path 的 prefix 数据都在 `struct_fields` 中（Step 2 + 3）
3. **Tier 2 覆盖**：所有 2 段 path 的数据都在新 store 中（Step 1 + 3）

**验证方法**：三步每步完成后运行全量 et_bench，Path B 的"独有预期边"数量应逐步下降至 0。当降至 0 时，Path B 可安全删除。

---

## 文件改动

| 文件 | Step | 改动 | LoC |
|------|------|------|-----|
| `param_assign.py` | 1.1 | Pass 2 写 `struct_fields` + filepath | +12 |
| `param_binding.py` | 1.2 | field_name fallback 写新 store | +2 |
| `initializer_assign.py` | 3.2 | 新增 `collect_var_types()` | +20 |
| `field_call.py` | 2.2 | `collect()` 无条件写新 store | +5 |
| `orchestrator.py` | 3.2 | 重排序 pipeline | +3 |
| `field_call.py` | 删除 | 删除 Path B（最终步） | -15 |

## 验证

- 全量 et_bench 测试通过（198+）
- 每步后 Path B gaps 计数下降
- 最终步后 Path B gaps = 0 → 删除 Path B
- FPR 无增加（fnptr-virtual 60 FPs 来自 `<gstruct:>` 旧 store key，删除后自然消失）
