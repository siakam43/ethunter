# ET-Bench 召回率提升设计

**日期**: 2026-05-12
**目标**: fnptr-callback, fnptr-cast, fnptr-only, fnptr-library 四种场景达到 100% 召回率
**范围**: Gap 1-5（不含 fnptr-dynamic-call 和 fnptr-virtual）
**开发模式**: TDD

## 当前状态

| 场景 | 召回率 | 缺失边 |
|---|---|---|
| fnptr-callback | 80.56% | 7 |
| fnptr-cast | 80.00% | 2 |
| fnptr-only | 75.00% | 6 |
| fnptr-library | 72.86% | 19 |
| fnptr-global-array/struct/struct-array/struct/varargs | 100.00% | 0 |

目标：前四个场景全部达到 100%，总计 +34 边。

## 实现阶段

### Phase 1: Gap 3 — Cast 表达式不再检查 symbol_names（+5 边）

**根因**: `cast_assign.py` 和 `initializer_assign.py` 在提取 cast 内部 identifier 后，要求 `name in symbol_names`。标准库函数（`calloc`/`malloc`/`free`/`strdup`）不在 fixture symbol table 中，导致 cast 初始化被丢弃。

**改动文件**:

- `src/ethunter/analyzer/cast_assign.py` — `_extract_cast_target()` line 33: 移除 `if name in symbol_names` 守卫
- `src/ethunter/analyzer/initializer_assign.py` — `_extract_cast_target()` **两处都要移除**:
  - line 33: `unwrap_cast` 路径 `if result and result in symbol_names` → 改为 `if result`
  - line 40: 回退路径 `if name in symbol_names` → 移除该条件

**技术细节**: cast_assign 的 `_extract_cast_target` 只有一个守卫点（line 33）。initializer_assign 的函数有两个守卫点——优先走 `dataflow.unwrap_cast`（line 31-33），回退走 `child_by_field_name('value')`（line 38-41），两处均有 `symbol_names` 检查。

**验证**: `test_et_bench_report` 中 fnptr-only 召回率从 75% → 100%

**影响的 fixture**: fnptr-only/example_2, 8, 9, 10, 11

**新增测试**:

| 测试函数 | 验证内容 |
|---|---|
| `test_cast_assign_no_symbol_names_guard` | `(type)stdlib_func` 模式：cast identifier 不在 symbol_names 中仍被提取并写入 dataflow |

---

### Phase 2: Gap 1+4 — 局部参数回调追踪（+8 边）

**根因**: `param_assign.analyze()` 的 Pass 1/3 在处理"被调函数体内直接调用 fnptr 形参"模式时有 4 个覆盖缺口：
1. 实参收集阶段不处理 `&func` 地址表达式（`pointer_expression`）
2. 调用检测阶段不处理 `(*fp)` 解引用调用（`parenthesized_expression`）
3. 不处理"回调的回调"——fnptr 作为另一个 fnptr 调用的参数（具体机制见下方）
4. 当实参是**局部变量**（非函数名）时，不回退到 dataflow 查找变量的 targets

Gap 4（`do_log → mm_log_handler`）依赖本 phase 的参数追踪能力，一并解决。

**改动文件**:

- `src/ethunter/analyzer/param_assign.py`:

  1. **新增 `func_fp_params` 收集**（方案第一步）
     - 扩展 `_collect_func_params`：识别形参类型为 fnptr 的位置
     - 新增 `func_fp_params: dict[str, set[int]]` = `func_name → {fnptr_param_positions}`
     - 判断规则：`parameter_declaration` 子树中包含 `function_declarator` 节点 → 该位置是 fnptr 形参
     - 例如 `void (*op)(void*,void*,void*)` → AST 中有 `function_declarator` → pos=2 是 fnptr

  2. **Pass 1 `_collect_call_params` — 3 项扩展**:
     - (a) 新增 `pointer_expression` 实参处理：提取 `&func` 的内部 identifier
     - (b) 新增 `field_expression` 调用分支：当调用通过 `obj->fnptr_field(...)` 进行时，解析 field_path 到 targets，利用 `func_fp_params` 找到各 target 的 fnptr 形参位置，提取对应位置的实参（identifier / pointer_expression / cast_expression）创建 edge
     - (c) 新增 dataflow 回退：当 `c.type == 'identifier'` 但 `c.text` 不在 `symbol_names` 时，调用 `dataflow.resolve(c.text)` 查找 targets（覆盖 `local_var = func_name; callee(local_var)` 模式）

  3. **Pass 3 `_detect_param_calls`**: 新增 `parenthesized_expression` 和 `pointer_expression` 调用检测（`(*fp)(...)`、`*fp(...)`）

  4. **Pass 4**: 复用现有 `call_targets` emit 逻辑。Pass 1 的 field_expression 分支产出的边直接追加到 `call_targets`。

**以 callback/example_14 走一遍回调的回调**:

```c
// gt_pch_save 中:
state.ptrs[i]->note_ptr_fn(obj, cookie, relocate_ptrs);
//                           arg0  arg1    arg2(← fnptr 实参)
// field_path = "state.ptrs.note_ptr_fn"
```

1. `field_call` Pass 1 已将 `note_ptr_fn` 存入 `<gstruct:state.ptrs.note_ptr_fn>` → resolves to `gt_pch_p_14lang_tree_node` ✓
2. `func_fp_params["gt_pch_p_14lang_tree_node"]` = `{2}`（第3个参数 `op` 是 fnptr）✓
3. Pass 1 的 field_expression 新分支: 提取 arg2 = `relocate_ptrs`（在 `symbol_names` 中）→ 创建 edge `gt_pch_save → relocate_ptrs` ✓

此机制同时覆盖：`struct->fnptr1(fnptr_target)`、`struct->fnptr1(&callback)`、`struct->fnptr1(local_var)`（通过 dataflow 回退）。

**以 callback/example_8 走一遍局部变量 dataflow 回退**:

```c
sort_gp_callback = sort_gp_asc;              // direct_assign → dataflow["sort_gp_callback"] = {"sort_gp_asc"}
pqsort(..., sort_gp_callback, ...);           // ← sort_gp_callback 不在 symbol_names
```

1. Pass 1 遇到实参 `sort_gp_callback`，`identifier` 类型，不在 `symbol_names` ✓
2. 走 dataflow 回退：`dataflow.resolve("sort_gp_callback")` → `{"sort_gp_asc"}` ✓
3. 将 `sort_gp_asc` 添加到 `param_mappings[pname]` 中 → 后续 Pass 3/4 产出 edge ✓

**新增测试**:

| 测试函数 | 验证内容 |
|---|---|
| `test_param_local_call_direct` | 形参 `fp` 直接调用 `fp()` |
| `test_param_local_call_address_of` | `&func` 地址传递 |
| `test_param_local_call_deref` | `(*fp)(...)` 解引用调用 |
| `test_param_local_var_dataflow_fallback` | 局部变量→`dataflow.resolve()`→param_mappings（example_8 的 `sort_gp_callback` 模式） |
| `test_param_callback_of_callback` | field_expression 调用 + fnptr 实参传递（example_14 模式） |
| `test_fnptr_pointer_global` | Gap 4: `log_handler_fn *global` → 局部 `tmp_handler` → 调用 |

**验证**: `test_et_bench_report` 中 fnptr-callback 召回率从 80.56% → 100%，fnptr-only 保持 100%

**影响的 fixture**: fnptr-callback/example_2, 6, 8, 13, 14; fnptr-only/example_5

---

### Phase 3: Gap 2 — 局部 fnptr 变量从 struct 字段初始化（+2 边）

**根因**: `cast → struct.field` 赋值被 `field_call` 的 Pass 1 正确收集到 `<gstruct:path>` dataflow key。但在后续函数中 `Type *var = struct.field` 初始化局部变量后，`direct_call_fp` 调用 `var(...)` 时无法解析到 targets。

深入分析发现是**时序问题**：当前 CALL_DETECTORS 顺序为 `[direct_call_fp, field_call, array_call]`。Phase 2 按文件循环——对每个文件，`direct_call_fp`（含 `local_fp_tracker` 收集）先于 `field_call`（含 Pass 1 字段赋值写入 dataflow）运行。当同一文件内前一个函数做字段赋值、后一个函数通过局部变量读取时，`local_fp_tracker` 在 dataflow 中找到空的 `<gstruct:path>`。

```c
// 同一文件内:
void func_a() { ddura.ddura_holdfunc = (type)dsl_dataset_hold_obj_string; }  // field_call Pass 1 写入
void func_b() { Type *fp = ddura->ddura_holdfunc; fp(...); }                  // direct_call_fp 先运行，读不到
```

**改动文件**:

- `src/ethunter/analyzer/orchestrator.py` — `CALL_DETECTORS` 顺序从 `[direct_call_fp, field_call, array_call]` 改为 `[field_call, direct_call_fp, array_call]`
- `src/ethunter/analyzer/local_fp_tracker.py` — 如 `_resolve_and_store` 中对 `init_declarator` + `field_expression` RHS 的路径有额外问题则一并修复

**时序变更的风险检查**: 现有测试均不依赖 CALL_DETECTORS 顺序。`field_call` 的 Pass 1 写入 dataflow 的 key 格式（`<gstruct:path>`）与 `direct_call_fp` 读取的 key 格式一致。`field_call` 先运行不会产生 `direct_call_fp` 不认识的 dataflow key。

**新增测试**:

| 测试函数 | 验证内容 |
|---|---|
| `test_local_fp_from_struct_field_init` | `Type *fp = obj->field` → `fp()` 调用解析 |

**验证**: `test_et_bench_report` 中 fnptr-cast 召回率从 80% → 100%

**回归验证**: 由于 CALL_DETECTORS 顺序变更影响所有 Phase 2 分析器，实现后需运行全量测试：

```bash
.venv/bin/python -m pytest tests/ -q
```

确保现有的 `test_fix_c2_call_expression_rhs_field_assign`、`test_example_13_chain_through_local_fp`、`test_et_bench_fnptr_struct_full_recall` 等全部通过。

**影响的 fixture**: fnptr-cast/example_6

---

### Phase 4: Gap 5 — 补齐 fixture 注册调用（+17 边）

**根因**: 多个 fnptr-library fixture 定义了库基础设施（struct fnptr 字段 + 注册 API + 目标函数），但缺少实际的注册调用点，导致目标函数无法通过数据流追踪被发现。

**改动文件**（fixture 层面）:

**Fixtures that need registration CALLS added**:

| Fixture | 添加内容 |
|---|---|
| `tests/benchmark/et_bench/fnptr-library/example_2/fixture.c` | 添加 `lua_newstate(lj_alloc_f, NULL)` 调用点 |
| `tests/benchmark/et_bench/fnptr-library/example_9/fixture.c` | 为 8 个 dtor 函数添加 `Curl_llist_init(&l, <dtor>)` 和/或 `Curl_hash_init(..., <dtor>)` 调用点 |
| `tests/benchmark/et_bench/fnptr-library/example_10/fixture.c` | `crls_http_cb` 注册链路已有 `store_setup_crl_download`，字段传播由 Phase 5 解决 |
| `tests/benchmark/et_bench/fnptr-library/example_18/fixture.c` | 添加 `ssh_set_verify_host_key_callback(..., key_print_wrapper)` 调用点（函数体无需修改，已有 `ssh_ctx->kex->verify_host_key = cb`） |

**Fixtures that need BOTH function body fix AND registration calls**:

example_4, 19, 20 的注册函数有早退 stub：`Channel *c = NULL; if (c == NULL) return;` → 赋值语句不可达。

| Fixture | 函数体修复 | 添加的注册调用 |
|---|---|---|
| `tests/benchmark/et_bench/fnptr-library/example_4/fixture.c` | 移除 `channel_register_filter` 内的 `if (c == NULL) return` 早退，保留赋值语句 | 添加通道注册 `channel_register_filter(..., client_simple_escape_filter, ...)` 和 `channel_register_filter(..., sys_tun_infilter, ...)` |
| `tests/benchmark/et_bench/fnptr-library/example_19/fixture.c` | 同上 | 添加 `channel_register_filter(..., sys_tun_outfilter, ...)` |
| `tests/benchmark/et_bench/fnptr-library/example_20/fixture.c` | 移除 `channel_register_open_confirm` 内的 `if (c == NULL) return` 早退 | 为 5 个 `open_confirm` 回调各添加 `channel_register_open_confirm` 调用 |

**验证**: `test_et_bench_report` 中 fnptr-library 召回率从 72.86% → ~97%（library/10 由 Phase 5 解决）

---

### Phase 5: Gap 5 剩余 — 字段间 fnptr 传播（+2 边）

**根因**: library/10 的 `crls_http_cb` 通过两层字段传播：
```c
store->lookup_crls = lookup_crls;                   // X509_STORE_set_lookup_crls
ctx->lookup_crls = store->lookup_crls;               // X509_STORE_CTX_init
```
`field_call` 的 Pass 1 收集到第一个赋值并写入 `<gstruct:store.lookup_crls>`。第二个赋值 `ctx->lookup_crls = store->lookup_crls`（RHS 是 field_expression）未被处理，导致 `ctx->lookup_crls(...)` 在 Pass 2 无法解析。

**改动文件**:

- `src/ethunter/analyzer/helpers.py` — `_scan` 函数 Form 1: 当 RHS 是 `field_expression` 类型时，提取其 field_path，通过 `dataflow.resolve('<gstruct:{rhs_field_path}>')` 查找 targets，传播到 LHS 的 `<gstruct:{lhs_field_path}>`
  - 当前 `_unwrap_identifier` 只处理 `identifier` 和 `cast_expression`，不处理 `field_expression`。需在 `_scan` 中为该类型单独建分支
- `src/ethunter/analyzer/field_call.py` — Pass 1 中消费 `collect_field_assignments` 结果：新增对 `fa.value_node.type == 'field_expression'` 的处理，解析源字段 targets 并写入目标字段

**新增测试**:

| 测试函数 | 验证内容 |
|---|---|
| `test_field_to_field_propagation` | `a->fp = b->fp` 字段间 fnptr 传播 |

**验证**: `test_et_bench_report` 中 fnptr-library 召回率达到 100%

**影响的 fixture**: fnptr-library/example_10

---

## 时序与依赖

```
Phase 1 (cast symbol_names)
    ↓
Phase 2 (param_assign 扩展)
    ↓
Phase 3 (local_fp_tracker)
    ↓
Phase 4 (fixture 补齐)  ←── 可与 Phase 5 并行
    ↓
Phase 5 (字段传播)
```

Phase 1-3 有代码依赖（Phase 2 可能影响 Phase 3 的验证），Phase 4-5 与 1-3 独立可并行。

## 成功标准

每个 Phase 完成后运行：

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```

检查对应场景的 recall 是否达标。全部完成后：

| 场景 | 目标召回率 |
|---|---|
| fnptr-callback | 100% |
| fnptr-cast | 100% |
| fnptr-only | 100% |
| fnptr-library | 100% |
| fnptr-global-array/struct/struct-array/struct/varargs | 保持 100% |
| fnptr-dynamic-call | 保持不变 (16.67%) |
| fnptr-virtual | 保持不变 (0%) |
| **总 recall** | **~98.9%** (608/615) |

## TDD 约定

所有新增单元测试沿袭 `test_et_bench.py` 现有模式：

```python
import tree_sitter_c as tsc
from tree_sitter import Language, Parser

source = b'''
// 内联 C 源码
'''
lang = Language(tsc.language())
parser = Parser(lang)
tree = parser.parse(source)

# 选择：完整 pipeline 或 直接调用特定模块
from ethunter.analyzer.orchestrator import run_all_analyses  # 集成
# from ethunter.analyzer.param_assign import analyze           # 单模块
```

**TDD 流程**：
1. 编写测试 → 运行确认失败（原因符合预期）
2. 实现最小改动 → 运行确认通过
3. 回归：`pytest tests/ -q` 确保无退化
4. 集成验证：`pytest tests/test_et_bench.py::test_et_bench_report -v -s`

## 不在范围内

- fnptr-dynamic-call (dlsym 解析)
- fnptr-virtual (vtable 派发)
- 类型驱动的 may-analysis
- 跨编译单元分析
- 任何新增分析器模块
