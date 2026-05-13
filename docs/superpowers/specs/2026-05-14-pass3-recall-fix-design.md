# Pass 3 召回缺陷修复方案设计

**日期**: 2026-05-14
**目标**: 修复 3 条已知 recall gap，fnptr-callback 100% 召回

## 当前状态

| 缺失边 | example | ethunter 产出 | 根因 |
|--------|---------|-------------|------|
| `(_pqsort, sort_gp_asc)` | fnptr-callback/example_8 | `(georadiusGeneric, sort_gp_asc)` | fallback 分支不查 param_mappings |
| `(_pqsort, sort_gp_desc)` | fnptr-callback/example_8 | `(georadiusGeneric, sort_gp_desc)` | 同上 |
| `(gt_pch_p_14lang_tree_node, relocate_ptrs)` | fnptr-callback/example_14 | `(gt_pch_save, relocate_ptrs)` | field_call callback-of-callback caller 错误 |

## 修复设计

### Fix 1: Pass 1 fallback 分支加 `param_mappings` 检查

**根因**: 当 fnptr 经过**两级转发**时（`outer → mid(param) → inner(param) → param()`），第一级写入 `param_mappings` 但不写 `dataflow`。第二级 fallback 只查 `dataflow.resolve(target)` 而遗漏 `param_mappings`。

**数据流**（example_8）:
```
georadiusGeneric → pqsort(..., sort_gp_callback, ...)
  → target="sort_gp_callback" NOT in symbol_names → fallback
  → dataflow.resolve("sort_gp_callback") = {sort_gp_asc, sort_gp_desc}  // from direct_assign
  → param_mappings["cmp"].update({sort_gp_asc, sort_gp_desc})  ✓
  → call_site_targets[("georadiusGeneric", "pqsort", 3)] populated  ✓
  → dataflow.assign("cmp", ...)  NOT called  ← GAP

pqsort → _pqsort(..., cmp, ...)
  → target="cmp" NOT in symbol_names → fallback
  → dataflow.resolve("cmp") = {sortCompare}  // only from direct calls to pqsort
  → param_mappings["cmp"] = {sort_gp_asc, sort_gp_desc, sortCompare}  // full set!
  → BUT fallback doesn't check param_mappings → call_site_targets misses sort_gp_asc/desc
  → Pass 3 in _pqsort can't find sort_gp_asc/desc
```

**修复**: `param_assign.py` Pass 1 fallback 分支（`_collect_call_params` 中 `else` 块），在现有 `dataflow.resolve(target)` 之后，合并 `param_mappings.get(target, set())` 作为额外 target 来源。

```python
# OLD (~line 462):
df_targets = dataflow.resolve(f'{caller}:{target}')
if not df_targets:
    df_targets = dataflow.resolve(target)

# NEW:
df_targets = dataflow.resolve(f'{caller}:{target}')
if not df_targets:
    df_targets = dataflow.resolve(target)
# Also check param_mappings for multi-level forwarding chains (Fix: recall gap)
pm_targets = param_mappings.get(target, set())
if pm_targets:
    df_targets = df_targets | pm_targets
```

**原理**: `param_mappings` 包含了**所有**通过该参数名到达的 targets（跨调用点合并），对于内层函数解析外层传入的 fnptr 时，dataflow 可能不完整但 param_mappings 有完整信息。

**召回安全性**: 此分支仅处理 "target 不在 symbol_names" 的情况（即局部变量/参数名转发场景），不改变已知函数名直接作为实参的处理。`param_mappings` 在此时已包含所有通过该参数名流转的 targets。

### Fix 2: field_call callback-of-callback caller 修正

**根因**: `field_call.py` 中 callback-of-callback 检测（第 216-246 行）的 caller 使用 `find_enclosing_function(node, tree.root_node)` —— 即包含 `obj->field(fnptr)` 调用的外部函数。但正确的 caller 应该是 **field target 函数本身**（即 `obj->field` 解析到的函数名）。

**数据流**（example_14）:
```
gt_pch_save():
  slot->note_ptr_fn(obj, cookie, relocate_ptrs)
    → field_call resolves note_ptr_fn → gt_pch_p_14lang_tree_node
    → gt_pch_p_14lang_tree_node has fnptr param at position 2 (op)
    → relocate_ptrs is at arg position 2
    → creates edge: (gt_pch_save, relocate_ptrs)  ← WRONG caller!

GT expects: (gt_pch_p_14lang_tree_node, relocate_ptrs)  ← field target is the correct caller
```

**修复**: callback-of-callback 边使用 resolved field target 函数名作为 caller，而非 enclosing function。

```python
# OLD (~line 239, field_call.py):
edges.append(CallEdge(
    caller=caller or '<unknown>',  # enclosing function of field call expression
    callee=actual,
    ...

# NEW:
edges.append(CallEdge(
    caller=ftarget,  # resolved field target function (e.g., gt_pch_p_14lang_tree_node)
    callee=actual,
    ...
```

**变量来源**: `ftarget` 来自外层 `for ftarget in targets:` 循环（line 226），是 field target 的解析结果。

## 涉及文件

| 文件 | 变更 |
|------|------|
| `src/ethunter/analyzer/param_assign.py` | Fix 1: fallback 分支 ~line 462 加 param_mappings 合并 |
| `src/ethunter/analyzer/field_call.py` | Fix 2: callback-of-callback ~line 239 caller 改为 ftarget |
| `tests/test_et_bench.py` | 新增 TDD 测试；fnptr-callback recall gate 恢复 100% |

## 测试策略（TDD）

### Fix 1 测试

**test_pass3_multi_level_forwarding**: 两级转发场景——outer 传 fnptr 给 mid，mid 转发给 inner，inner 调用 fnptr。断言 Pass 3 产出的边 caller = inner（而非 outer 或 mid）。

```c
void inner(cb_fn cb) { cb(42); }
void mid(cb_fn cb) { inner(cb); }
void outer_a(void) { mid(handler_a); }
void outer_b(void) { mid(handler_b); }
// 断言: (inner, handler_a) 和 (inner, handler_b) 存在
```

### Fix 2 测试

**test_field_call_callback_of_callback_caller**: field dispatch + fnptr arg 场景。node->fn(fnptr) 解析 fn→target_func。断言 callback_of_callback 边 caller = target_func（而非包含 node->fn(fnptr) 的外层函数）。

### 回归守卫

- fnptr-callback recall gate 恢复为 `assert recall == 1.0`
- 其余 8 场景保持 100% recall
- 全量 tests/ 无回归
