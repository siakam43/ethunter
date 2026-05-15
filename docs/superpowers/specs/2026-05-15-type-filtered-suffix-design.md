# Spec: 类型过滤 Suffix Scan + 关闭 Path B

**日期**: 2026-05-15
**状态**: design
**目标**: 在 suffix scan 中加入 struct 类型约束，消除跨类型误报，安全关闭 Path B

## 背景

Path B（legacy suffix scan in field_call._visit()）存在两个问题：
1. **无类型约束**：匹配所有同名字段，无论属于哪个 struct，产生大量误报
2. **绕过 FieldResolver**：resolver 的类型门控对 Path B 无效

但直接删除 Path B 会导致 10 条 recall 回归（链式访问依赖 suffix scan 找到数据）。

**方案**：不改删除 Path B，改为在 Path B 中加入类型约束——仅当 suffix 匹配的 key 属于**同 struct 类型**时才接受。这样既保留召回，又消除跨类型误报。

## Part B: 类型过滤 Suffix Scan

### B.1 FieldResolver Tier 3/4 — 加入类型约束

**文件**: `src/ethunter/analyzer/field_resolver.py`

当前 Tier 3 逻辑（无类型过滤）：

```python
# Tier 3: same-file suffix
suffix = f'.{field_tail}'
for key, vals in self._store.struct_fields.items():
    if not key.endswith(suffix): continue
    files = self._store.struct_field_files.get(key)
    if files and filepath not in files: continue
    targets.update(vals)
```

改为**可达性门控** suffix scan——仅接受可通过 struct_fields 从 base_var 的 struct_type 到达的 key：

```python
# Tier 3: reachability-gated same-file suffix
suffix = f'.{field_tail}'
for key, vals in self._store.struct_fields.items():
    if not key.endswith(suffix): continue
    # Reachability gate: if struct_type known, only accept reachable keys
    if struct_type:
        key_prefix = key[len('gstruct:'):].split('.')[0]
        if key_prefix == struct_type:
            pass  # Case 1: prefix IS the struct_type → accept
        else:
            # Case 2: check if key_prefix is reachable via struct_type field assignments
            has_field_mappings = False
            reachable = False
            for sk, sv in self._store.struct_fields.items():
                if sk.startswith(f'gstruct:{struct_type}.'):
                    has_field_mappings = True
                    if key_prefix in sv:
                        reachable = True
                        break
            # Case 3: no field mappings for struct_type at all → conservative pass
            # (struct type info exists but no field assignments tracked yet)
            if has_field_mappings and not reachable:
                continue  # prefix NOT reachable from struct_type → skip
    files = self._store.struct_field_files.get(key)
    if files and filepath not in files: continue
    targets.update(vals)
```

Tier 4 同逻辑（去掉文件过滤）。

**三 case 语义**：
| Case | 条件 | 行为 |
|------|------|------|
| 1 | `key_prefix == struct_type` | 直接接受 |
| 2 | struct_type 有字段映射，key_prefix 不在其中 | 拒绝（unreachable） |
| 3 | struct_type 完全无字段映射 | 保守接受（防止新类型无数据时全部被过滤） |

**正确性论证**：
- fnptr-virtual: `struct_type=region_model_context`，struct_fields 中无 `region_model_context.* → decorator_vtable` 映射 → unreachable → 60 FPs 被过滤 ✅
- fnptr-struct: `struct_type=SSL`，struct_fields 有 `SSL.method → ssl3_method` → reachable → 真实边通过 ✅

### B.2 移除类型门控

当前 resolver 的类型门控逻辑（line 199-203）阻断所有 suffix 扫描：

```python
# Type gate: known type + Tier 1 miss → skip Tier 3/4 suffix
if struct_type:
    return set(), None, None
```

**移除这个类型门控**——Tier 3/4 现在有类型约束，不再需要提前阻断。

```python
# Removed: type gate. Tier 3/4 now have type filtering, safe to run always.
```

### B.3 Path B suffix scan 加 reachability 门控（不删除）

**文件**: `src/ethunter/analyzer/field_call.py`

Path B 的 suffix scan 保留，但加入与 Tier 3/4 相同的 reachability 检查。理由：
- Path B 扫描旧 store（`dataflow.targets`），Tier 3/4 扫描新 store（`struct_fields`）
- 两个 store 中的数据不完全重叠——删除任何一个都会导致召回回落
- 加入 reachability 门控后，两个 suffix scan 都变得类型安全

```python
# Legacy suffix scan with reachability gate
if '.' in field_path:
    garray_targets = dataflow.resolve(f'<garray:{base_var}>')
    if garray_targets:
        targets.update(garray_targets)
    parts = field_path.split('.')
    for i in range(1, len(parts)):
        sfx = '.'.join(parts[i:])
        for key, vals in dataflow.targets.items():
            if key.endswith(f'.{sfx}>') and vals:
                # Reachability gate: same 3-case logic as Tier 3/4
                if struct_type:
                    inner = key[1:-1]  # strip <> → 'gstruct:ssl3_method.put_cb'
                    key_prefix = inner.split('.')[0].split(':')[-1]
                    if key_prefix != struct_type:
                        has_mappings = False
                        reachable = False
                        if hasattr(dataflow, 'store'):
                            for sk, sv in dataflow.store.struct_fields.items():
                                if sk.startswith(f'gstruct:{struct_type}.'):
                                    has_mappings = True
                                    if key_prefix in sv:
                                        reachable = True; break
                        # Case 3: no store or no field mappings → conservative pass
                        if has_mappings and not reachable:
                            continue
                targets.update(vals)
        if targets and confidence is None:
            confidence, evidence = Confidence.LOW, Evidence('legacy_fallback')
```

**保留的 garray lookup**：`dataflow.resolve(f'<garray:{base_var}>')` — 数组索引访问，独立于类型过滤。

## Part A: 扩展链分解

### A.1 多层前缀分解

**文件**: `src/ethunter/analyzer/field_resolver.py`

当前链分解只尝试一个 cut=2 前缀。扩展为**多层渐进**：对 N 段 path，尝试 cut=2, 3, ..., N-1。

```python
# Chain decomposition (multi-level)
parts = field_path.split('.')
if len(parts) >= 3:
    for cut in range(2, len(parts)):
        prefix = '.'.join(parts[:cut])
        suffix = '.'.join(parts[cut:])
        resolved_vars = self._store.resolve_struct_field(f'gstruct:{prefix}')
        if not resolved_vars:
            continue
        for var_name in resolved_vars:
            var_type = self._symbol_table.get_var_type(var_name)
            if var_type:
                targets = self._store.resolve_struct_field(f'gstruct:{var_type}.{suffix}')
                if targets:
                    return targets, Confidence.HIGH, Evidence('chain_resolve', tier=1)
            targets = self._store.resolve_struct_field(f'gstruct:{var_name}.{suffix}')
            if targets:
                return targets, Confidence.HIGH, Evidence('chain_resolve_exact', tier=2)
```

### A.2 链分解数据来源扩展

当前链分解只从 `struct_fields`（新 store）查前缀。同时扩展到从 `dataflow.targets`（旧 store）查：

```python
# Also check old store for prefix resolution
if not resolved_vars:
    old_key = f'<gstruct:{prefix}>'
    resolved_vars = dataflow.resolve(old_key)  # via dataflow engine
```

但 resolver 没有 `dataflow` 引用。替代方案：在 `resolve_field_call()` 中接受可选的 `dataflow` 参数，或由调用方在 resolver 返回空后做额外查询。

**简化处理**：调用方（`field_call._visit()`）在 resolver 返回空后，用旧 store 数据做一次补充的链分解查询（不加 suffix scan，只查前缀解析）：

```python
# After resolver returns empty, try old-store chain decomposition
if not targets and '.' in field_path:
    parts = field_path.split('.')
    if len(parts) >= 3:
        for cut in range(2, len(parts)):
            prefix = '.'.join(parts[:cut])
            suffix = '.'.join(parts[cut:])
            # Try old store
            resolved = dataflow.resolve(f'<gstruct:{prefix}>')
            for var_name in resolved:
                var_type = symbol_table.get_var_type(var_name)
                if var_type:
                    targets2 = dataflow.store.resolve_struct_field(f'gstruct:{var_type}.{suffix}')
                    if targets2: targets.update(targets2)
```

但这段逻辑与 FieldResolver 重复。更好的方式：传入 dataflow 给 resolver。但 resolver 当前没有 dataflow 引用。

**最简方案**：当前 struct_fields 已有覆盖后，不需要旧 store 查询。如果 struct_fields 中没有前缀数据，说明数据迁移不完整——此时交给 Path B 的 garray 处理（已保留）。

这意味着 Part A.2 实际上不需要单独实现——struct_fields 已经有 struct variable 存储（上一个 commit 的改动），链分解只需多层即可。

简化 A：只做多层分解（A.1），不做旧 store 查询（A.2 去掉）。

## 数据流总览

```
resolve_field_call("s.method.put_cb", "s", "ssl_cipher_list_to_bytes"):
  Tier 1: gstruct:SSL.method.put_cb → miss
  Tier 2: gstruct:s.method.put_cb → miss
  Chain decomp cut=2: prefix=s.method, suffix=put_cb
    struct_fields["gstruct:s.method"] → {ssl3_method}
    var_type["ssl3_method"] → SSL_METHOD
    gstruct:SSL_METHOD.put_cb → {ssl3_put_cb} ✓ HIT
  
  // 如果没有命中，继续：
  Tier 3: type-filtered same-file suffix for .put_cb
    only accepts keys where prefix type == SSL or unknown → 过滤掉 TLS_method 的 key
  Tier 4: type-filtered cross-file suffix
  
  // Tier 3: reachability-gated same-file suffix (new store)
  // Tier 4: reachability-gated cross-file suffix (new store)
  // Legacy suffix scan: reachability-gated (old store), kept as safety net
  // garray lookup: retained
```

## 验证标准

- 所有 et_bench 197 tests 通过
- recall 不劣化（≥98.86%）
- FPR 从 31.33% 下降（预期 fnptr-virtual 60 FPs 被消除）
- 新增 test_type_filtered_suffix: 验证跨类型 suffix 被过滤
- 新增 test_chain_decomp_multi_level: 验证 4-segment 链分解

## 文件改动

| 文件 | 改动 | LoC |
|------|------|-----|
| `field_resolver.py` | Tier 3/4 加类型过滤；移除类型门控；多层链分解 | +15 / -5 |
| `field_call.py` | Path B suffix scan 加 reachability 门控 | +15 |
| `tests/test_et_bench.py` | 类型过滤 + 多层链分解测试 | +35 |
