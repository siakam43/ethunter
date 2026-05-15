# Spec: 链式字段访问解析

**日期**: 2026-05-15
**状态**: design
**背景**: Step 3（删除 Path B）受阻——FieldResolver 无法解析 `s.method.put_cb` 形式的链式字段访问

## 问题描述

### 当前行为

`fnptr-struct` 场景中，`s->method->put_cipher_by_char(...)` 调用：

1. `extract_field_path` 产生 `s.method.put_cipher_by_char`
2. `base_var = s`，`struct_type = SSL`（从 `_collect_param_types` 获取）
3. Tier 1 查询 `gstruct:SSL.method.put_cipher_by_char` → miss
4. Tier 2 查询 `gstruct:s.method.put_cipher_by_char` → miss
5. 类型门控：`struct_type` 已知 → 跳过 Tier 3/4 → 返回空
6. Path B legacy suffix 扫描 `dataflow.targets` 找到 `.put_cipher_by_char` → `{ssl3_put_cipher_by_char}`

**根因**：中间字段 `s.method` 需要解析到具体变量 `ssl3_method`，然后以其类型 `SSL_METHOD` 进行精确匹配。当前 resolver 将整个 path 视为扁平字符串。

### 影响范围

`fnptr-struct` 中 4 条缺失边（example_8, 10, 11, 12），以及其他场景中类似链式访问（如 `ctx->vtable->method`）。

## 设计方案

### 层 1：修复 `_unwrap_identifier` 处理 `pointer_expression` RHS

**文件**: `src/ethunter/analyzer/helpers.py`

**问题**: `_unwrap_identifier`（line 138）当前只处理 `identifier` 和 `cast_expression` 节点。对于 `s->method = &ssl3_method`，RHS 是 `pointer_expression`，`resolved_value` 返回 `None`。

**改动**: 新增 `pointer_expression` 分支：

```python
def _unwrap_identifier(node: ts.Node, unwrap_fn=None) -> str | None:
    """Extract identifier text from a node, unwrapping cast & pointer expressions."""
    if node.type == 'identifier' and node.text:
        return node.text.decode('utf-8')
    if node.type == 'cast_expression':
        if unwrap_fn:
            result = unwrap_fn(node)
            if result:
                return result
        for c in reversed(node.children):
            result = _unwrap_identifier(c, unwrap_fn)
            if result:
                return result
    if node.type == 'pointer_expression' and node.children:
        # Handle &func_ref, &variable
        inner = node.children[-1]
        return _unwrap_identifier(inner, unwrap_fn)
    return None
```

**效果**: `s->method = &ssl3_method` 被 `collect_field_assignments` 捕获为：
```
FieldAssignment(field_path="s.method", resolved_value="ssl3_method", form="assign")
```

后续 `field_call.collect()` / `param_binding._resolve_fields()` 写入 `gstruct:s.method → ssl3_method`（通过已有代码路径）。

### 层 2：链式访问分解

**文件**: `src/ethunter/analyzer/field_resolver.py`

**改动**: 在 `resolve_field_call()` 中，Tier 2 未命中后、类型门控之前，新增链式分解步骤：

```python
def resolve_field_call(self, field_path, base_var, caller_func, filepath):
    field_tail = self._store.compute_field_tail(field_path)
    targets = set()

    # Tier 1: type-aware exact match
    struct_type = None
    if caller_func:
        struct_type = self._symbol_table.get_func_var_type(caller_func, base_var)
    if not struct_type:
        struct_type = self._symbol_table.get_var_type(base_var)
    if struct_type:
        targets = self._store.resolve_struct_field(f'gstruct:{struct_type}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('type_aware', tier=1)

    # Tier 2: exact path match
    targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
    if targets:
        return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

    # === NEW: Chain decomposition ===
    # Handle s.method.put_cb where s.method resolves to a concrete struct
    parts = field_path.split('.')
    if len(parts) >= 3:
        # Try progressive prefixes: s.method, s.method.put_cb
        for cut in range(2, len(parts)):
            prefix = '.'.join(parts[:cut])       # e.g., "s.method"
            suffix = '.'.join(parts[cut:])        # e.g., "put_cipher_by_char"

            # Look up prefix → what variable does it resolve to?
            resolved_vars = self._store.resolve_struct_field(f'gstruct:{prefix}')
            if not resolved_vars:
                continue

            for var_name in resolved_vars:
                # Get the struct type of the resolved variable
                var_type = self._symbol_table.get_var_type(var_name)
                if not var_type:
                    continue
                # Try type-aware lookup with remaining suffix
                targets = self._store.resolve_struct_field(f'gstruct:{var_type}.{suffix}')
                if targets:
                    return targets, Confidence.HIGH, Evidence('chain_resolve', tier=1)
                # Also try exact: gstruct:{var_name}.{suffix}
                targets = self._store.resolve_struct_field(f'gstruct:{var_name}.{suffix}')
                if targets:
                    return targets, Confidence.HIGH, Evidence('chain_resolve_exact', tier=2)

    # Type gate: known type + Tier 1 miss + no chain success → skip suffix
    if struct_type:
        return set(), None, None

    # Tier 3: same-file suffix
    # ... (unchanged) ...

    # Tier 4: cross-file suffix
    # ... (unchanged) ...
    return set(), None, None
```

**逻辑**：
1. 仅当 path 有 3+ 段时触发（2 段不需要分解）
2. 渐进式前缀：先试 `s.method`，再试 `s.method.put_cb`
3. 每个前缀查 `struct_fields` → 获取解析到的变量名
4. 对变量名查 `var_types` → 获取 struct 类型
5. 用该类型 + 剩余后缀构造 type-aware key 查询
6. 也尝试 `gstruct:{var_name}.{suffix}` 精确查询
7. 命中即返回 HIGH 置信度
8. 都不命中则继续现有流程（类型门控 → suffix 扫描）

### 数据流示例

```
Fixture: s->method = &ssl3_method; s->method->put_cipher_by_char(...)

[Layer 1] collect_field_assignments:
  s->method = &ssl3_method
    RHS: pointer_expression → _unwrap_identifier → "ssl3_method"
    field_path="s.method", resolved_value="ssl3_method"

[Existing] field_call.collect() → dataflow:
  old store: <gstruct:s.method> → {ssl3_method}
  new store: gstruct:s.method → {ssl3_method}  (with filepath)
  new store: gstruct:SSL.method → {ssl3_method} (if type known)

[Existing] initializer_assign:
  .put_cipher_by_char = ssl3_put_cipher_by_char
  old store: <gstruct:ssl3_method.put_cipher_by_char> → {ssl3_put_cipher_by_char}
  new store: gstruct:ssl3_method.put_cipher_by_char → {ssl3_put_cipher_by_char}
  new store: gstruct:SSL_METHOD.put_cipher_by_char → {ssl3_put_cipher_by_char} (type-aware)

[Layer 2] resolve_field_call("s.method.put_cipher_by_char", "s", "ssl_cipher_list_to_bytes"):
  Tier 1: gstruct:SSL.method.put_cipher_by_char → miss (field_tail wrong)
  Tier 2: gstruct:s.method.put_cipher_by_char → miss
  Chain decomp: parts=["s","method","put_cipher_by_char"], cut=2:
    prefix="s.method", suffix="put_cipher_by_char"
    resolved_vars = struct_fields["gstruct:s.method"] → {"ssl3_method"}
    var_name="ssl3_method", var_type = SSL_METHOD
    gstruct:SSL_METHOD.put_cipher_by_char → {"ssl3_put_cipher_by_char"} ✓ HIT
  return ({ssl3_put_cipher_by_char}, HIGH, 'chain_resolve:tier=1')
```

### 边界情况

1. **链长 > 3**: 如 `a.b.c.d(...)`，渐进式前缀从 cut=2 到 cut=N-1，找到第一个成功的前缀即返回
2. **多个解析结果**: 一个前缀可能解析到多个变量（如 `s.method` 在不同分支指向 `ssl3_method` 或 `tls1_method`），遍历所有
3. **var_type 未知**: 跳过该变量，尝试下一个。同时仍尝试精确路径 `gstruct:{var_name}.{suffix}`
4. **与类型门控的交互**: 链分解在类型门控之前执行。链分解命中 → 提前返回 HIGH。未命中 → 类型门控仍可阻断无类型时的 suffix 扫描
5. **recursion**: resolve_struct_field 返回的值如果本身是中间引用（如 `gstruct:s.method → gstruct:ssl3_method`），不会无限循环——只做一层分解

### 验证标准

- 所有 et_bench 195 tests 通过
- `fnptr-struct` recall 恢复至 100%（4 条缺失边找回）
- FPR ≤ 31.33%（不增加）
- 新增 `test_chain_resolve_s_method_get_cb`: 验证 `s.method.put_cipher_by_char` 能正确解析
- 新增 `test_unwrap_pointer_expression`: 验证 `&func_name` 被正确提取
- 对问题定义中列出的 4 个 fnptr-struct example 逐个回归测试

### 文件改动清单

| 文件 | 改动 | LoC |
|------|------|-----|
| `helpers.py` | `_unwrap_identifier` 新增 pointer_expression 分支 | +5 |
| `field_resolver.py` | `resolve_field_call` 新增链式分解逻辑 | +25 |
| `tests/test_et_bench.py` | 新增链式分解和 pointer_expression 测试 | +40 |
