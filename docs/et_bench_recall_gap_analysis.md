# ET-Bench 低召回率场景 Gap 分析

**日期**: 2026-05-12
**总召回**: 93.33% (574/615)
**分析范围**: 6 个低召回率场景，共 41 条缺失边

## 召回率总览

| 场景 | 匹配 | 期望 | 召回率 |
|---|---|---|---|
| fnptr-global-array | 307 | 307 | 100.00% |
| fnptr-global-struct | 68 | 68 | 100.00% |
| fnptr-global-struct-array | 70 | 70 | 100.00% |
| fnptr-struct | 21 | 21 | 100.00% |
| fnptr-varargs | 1 | 1 | 100.00% |
| **fnptr-callback** | 29 | 36 | **80.56%** |
| **fnptr-cast** | 8 | 10 | **80.00%** |
| **fnptr-only** | 18 | 24 | **75.00%** |
| **fnptr-library** | 51 | 70 | **72.86%** |
| **fnptr-dynamic-call** | 1 | 6 | **16.67%** |
| **fnptr-virtual** | 0 | 2 | **0.00%** |

---

## Gap 1: 局部参数回调 (7条, fnptr-callback) — P0

**模式**：调用者 A 将函数 F 作为 fnptr 实参传给被调者 B，B 体内通过形参名直接调用该 fnptr。

`param_assign` 主要追踪"参数存入 struct/global"的注册模式，对"参数在函数体内直接被调用"覆盖不完整。

### 缺失明细

| Example | 缺失边 | 具体模式 |
|---------|--------|---------|
| callback/2 | `print_units` → `format_time_us`, `format_metric` | `fmt(n)` 形参直接调用 |
| callback/6 | `tcache_bin_flush_edatas_lookup` → `tcache_bin_flush_ptr_getter` | `ptr_getter(...)` 形参调用（`&func` 取地址传递） |
| callback/8 | `georadiusGeneric` → `sort_gp_asc`, `sort_gp_desc` | 局部变量赋值→传参→`cmp()` 形参调用 |
| callback/13 | `ccp_fold` → `valueize_op` | `(*valueize)(arg0)` 形参解引用调用（switch 语句内） |
| callback/14 | `gt_pch_save` → `relocate_ptrs` | fnptr 调用的参数本身也是 fnptr（回调的回调） |

### 修复方向

扩展 `param_assign`，使其在检测到调用表达式 `callee(arg1, ..., fn_target)` 时：
1. 分析 `callee` 的函数体，找到 fnptr 形参的直接调用点
2. 产出 `caller → fn_target` 间接边

---

## Gap 2: Cast 赋值到 struct 字段 (2条, fnptr-cast) — P2

**模式**：函数先 cast 再存入 struct 字段，后续通过字段指针调用。

```c
ddura.ddura_holdfunc = (dsl_holdfunc_t)dsl_dataset_hold_obj_string;
// 在 dsl_dataset_user_release_sync 中:
dsl_holdfunc_t *holdfunc = ddura->ddura_holdfunc;
holdfunc(dp, name, FTAG, &ds);
```

`cast_assign` → struct 字段存储 → `field_call` 的链路断裂。

### 缺失明细

| Example | 缺失边 |
|---------|--------|
| cast/6 | `dsl_dataset_user_release_sync` → `dsl_dataset_hold`, `dsl_dataset_hold_obj_string` |

### 修复方向

扩展 `cast_assign` 使其在 struct 字段赋值场景中写入 dataflow，使 `field_call` 能解析目标。

---

## Gap 3: 全局 fnptr 变量 + cast 初始化器 (5条, fnptr-only) — P1

**模式**：全局 fnptr 变量通过 cast 初始化为标准库函数，后续在其他函数中调用 `global_fp()`。

```c
curl_calloc_callback Curl_ccalloc = (curl_calloc_callback)calloc;
// 在 Curl_new 中:
as = Curl_ccalloc(1, sizeof(altsvc_t));
```

`initializer_assign` 未处理 `(typedef)func_name` cast 形式的初始化器。

### 缺失明细

| Example | 缺失边 |
|---------|--------|
| only/2 | `Curl_new` → `calloc` |
| only/8 | `Curl_cookie_add` → `free` |
| only/9 | `Curl_output_digest` → `free` |
| only/10 | `Curl_smtp_escape_eob` → `malloc` |
| only/11 | `Curl_cookie_add` → `strdup` |

### 修复方向

扩展 `initializer_assign`，在 `declaration` 节点的 `init_declarator` 中识别 `(type)func` 形式的 cast 表达式作为 fnptr 初始化器。

---

## Gap 4: 全局 fnptr 指针变量 (1条, fnptr-only) — P2

**模式**：fnptr 存储在全局 `log_handler_fn *` 类型变量（指向 fnptr typedef 的指针），读取到局部变量后调用。

```c
log_handler_fn *log_handler;          // 全局，类型为指向 fnptr 的指针
log_handler = handler;                // 存储 mm_log_handler
// 在 do_log 中:
tmp_handler = log_handler;            // 读到局部
tmp_handler(level, force, ...);       // 通过局部调用
```

双重间接（pointer-to-fnptr-typedef）导致 dataflow 追踪丢失目标。

### 缺失明细

| Example | 缺失边 |
|---------|--------|
| only/5 | `do_log` → `mm_log_handler` |

### 修复方向

在 `direct_call_fp` 中处理 `pointer_to_fnptr_var(...)` 调用模式，追踪 `pointer_to_fnptr_var = global_fnptr_ptr` 的赋值链。

---

## Gap 5: 库风格 field_call (19条, fnptr-library) — P0

**模式**：fixture 定义了库基础设施（struct + fnptr 字段 + 注册 API）和目标函数，但实际注册调用不在 fixture 中。Benchmark 期望 ethunter 通过类型匹配找到所有可能的目标。

子问题：部分示例有可见注册但涉及 **struct 字段间 fnptr 传播**（`field_a = field_b`），当前 `field_call` 不追踪跨字段复制。

### 缺失明细

| Example | 缺失 | 具体根因 |
|---------|------|---------|
| library/2 | 1 (`lj_mem_free`→`l_alloc`) | 注册调用不在 fixture 内 |
| library/4 | 2 (`channel_handle_rfd`→`client_simple_escape_filter`, `sys_tun_infilter`) | 注册调用不在 fixture 内 |
| library/9 | 8 (8个 dtor 函数) | 无注册调用可见 |
| library/10 | 1 (`get_crl_delta`→`crls_http_cb`) | `store->lookup_crls`→`ctx->lookup_crls` 字段传播 |
| library/18 | 1 (`kex_verify_host_key`→`key_print_wrapper`) | 注册调用不在 fixture 内 |
| library/19 | 1 (`channel_handle_wfd`→`sys_tun_outfilter`) | 注册调用不在 fixture 内 |
| library/20 | 5 (5个 `open_confirm` 回调) | 注册调用不在 fixture 内 |

### 修复方向

**短期（补齐 fixture）**：为 library/2, 4, 9, 18, 19, 20 补充注册调用，使现有 `field_call` + `param_assign` 能直接解析目标。

**中期（字段传播）**：扩展 `field_call` 追踪 `struct_a->fnptr_field = struct_b->fnptr_field` 赋值链，解决 library/10 类问题。

**长期（类型匹配）**：实现基于 fnptr 类型签名的 may-analysis，自动发现所有签名匹配的函数作为候选目标。

---

## Gap 6: dlsym 解析 (5条, fnptr-dynamic-call) — P1

**模式**：`dlsym_fp` 模块创建的边使用合成 caller `<dlsym>`，而非实际封闭函数名。Benchmark 期望 caller 为调用 dlsym 的函数。

```c
onload = dlsym(handle, "RedisModule_OnLoad");
// dlsym_fp 产出: <dlsym> -> RedisModule_OnLoad
// 期望: onload_caller -> RedisModule_OnLoad
```

唯一成功的 example_5 通过了直接字段赋值路径（`ret->sk_enroll = ssh_sk_enroll`），不是 dlsym。

### 缺失明细

| Example | 缺失边 |
|---------|--------|
| dynamic-call/1 | `onload_caller` → `RedisModule_OnLoad` |
| dynamic-call/2 | `sk_api_version_caller` → `sk_api_version` |
| dynamic-call/3 | `omx_init` → `OMX_Init` |
| dynamic-call/4 | `dynamic_load` → `bind_engine` |
| dynamic-call/5 | `sshsk_enroll` → `sk_enroll` |

### 修复方向

修改 `dlsym_fp`，通过 `find_enclosing_function` 获取调用 dlsym 的外层函数名作为 caller。同时追踪 `fp = dlsym(...)` 的赋值目标，在后续 `fp()` 调用时产出正确的 caller→callee 边。

---

## Gap 7: 虚函数表派发 (2条, fnptr-virtual) — P3

**模式**：手动 vtable 模式 `ctx->vtable->get_state_map_by_name(...)`。

需要处理：
1. 双重间接：`ctx->vtable` 是指针字段 → `->method` 是 vtable struct 的字段
2. vtable 全局初始化器解析：`noop_vtable = {..., noop_region_model_context_get_state_map_by_name, ...}`
3. vtable 赋值：`ctx->base.vtable = &noop_vtable`

当前 ethunter 完全不支持 vtable 派发模式。

### 缺失明细

| Example | 缺失边 |
|---------|--------|
| virtual/1 | `get_fd_map` → `noop_region_model_context::get_state_map_by_name`, `region_model_context_decorator::get_state_map_by_name` |

### 修复方向

全新功能，需要：
1. 在 `initializer_assign` 中识别 vtable struct 的初始化器并建立字段→目标函数的映射
2. 扩展 `field_call` 支持 `ptr->vtable_field->method()` 双重间接调用
3. 通过 vtable 赋值 (`ptr->vtable_field = &vtable_instance`) 关联 vtable 实例

---

## 修复优先级总结

| 优先级 | Gap | 影响边数 | 预计修复策略 |
|--------|-----|---------|------------|
| **P0** | Gap 1: 局部参数回调 | 7 | 扩展 `param_assign` 追踪被调体内形参调用 |
| **P0** | Gap 5: 库风格 field_call | 19 | 补齐 fixture + 字段传播 |
| **P1** | Gap 3: Cast 初始化器 | 5 | 扩展 `initializer_assign` 支持 cast |
| **P1** | Gap 6: dlsym caller | 5 | 修改 `dlsym_fp` 使用实际 caller |
| **P2** | Gap 2: Cast + struct 字段 | 2 | 扩展 `cast_assign` + `field_call` 链路 |
| **P2** | Gap 4: fnptr 指针变量 | 1 | 支持 pointer-to-fnptr-typedef 追踪 |
| **P3** | Gap 7: Vtable | 2 | 新功能：vtable 初始化器解析 + 双重间接 |

P0 项合计影响 26/41 (63%) 缺失边，优先投入可获得最大召回率提升。
