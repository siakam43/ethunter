# ethunter 架构系统性分析

**分析日期**: 2026-05-14
**分析方法**: 基于 et_bench 误报率与召回率数据，追溯架构根因

## 当前状态

| 指标 | 修复前 (5/13) | 修复后 (5/14) |
|---|---|---|
| 总体 FPR | 60.98% | **31.33%** |
| 总体召回率 | 98.86% | 98.86% |
| 误报数 | 950 | **276** |

### 各场景详情

| 场景 | 命中 | 期望 | 误报 | 召回率 | FPR |
|---|---|---|---|---|---|
| fnptr-callback | 33 | 33 | 60 | 100.00% | 64.52% |
| fnptr-cast | 10 | 10 | 19 | 100.00% | 65.52% |
| fnptr-dynamic-call | 1 | 6 | 3 | 16.67% | 75.00% |
| fnptr-global-array | 307 | 307 | 0 | 100.00% | 0.00% |
| fnptr-global-struct | 68 | 68 | 41 | 100.00% | 37.61% |
| fnptr-global-struct-array | 70 | 70 | 62 | 100.00% | 46.97% |
| fnptr-library | 70 | 70 | 15 | 100.00% | 17.65% |
| fnptr-only | 24 | 24 | 2 | 100.00% | 7.69% |
| fnptr-struct | 21 | 21 | 13 | 100.00% | 38.24% |
| fnptr-varargs | 1 | 1 | 1 | 100.00% | 50.00% |
| fnptr-virtual | 0 | 2 | 60 | 0.00% | 100.00% |
| **总计** | **605** | **612** | **276** | **98.86%** | **31.33%** |

最近的架构重构（将 `param_assign` 拆分为 `param_binding` + `param_dispatch` + `callback_reg`，引入作用域键和类型感知查找）已解决最严重的 FPR 问题——尤其是 P0 N×M 交叉边和 P2 callback_reg 过度匹配。`fnptr-global-struct` FPR 从 89.97% 降至 37.61%。

---

## 缺陷 1：无作用域的全局数据流命名空间

**位置**: `src/ethunter/analyzer/dataflow.py:15` — `VariableState.targets: dict[str, set[str]]`

### 机制

`param_binding.analyze()` 在 `dataflow.py:90-91` 处写入裸参数名：

```python
# param_binding.py:91
dataflow.assign(pname, target)  
# func1 中的 "cb" -> {"handler_a"} 
# func2 中的 "cb" -> {"handler_b"}
```

然后在 `dataflow.py:26` 处，`resolve("cb")` 返回 `{"handler_a", "handler_b"}` —— 从不同函数合并后的 targets，无法区分哪个 target 属于哪个调用上下文。

### 为什么这会造成损害

- 参数名 "handler" 在 104 个 fixture 中出现了数百次——不同函数中的每个 `handler` 参数都共享同一个 dataflow 条目
- `field_call` 的后缀回退扫描 `dataflow.targets` 中的**每一个 key**（`field_call.py:188-189`），包括来自不相关文件的条目
- 作用域键模式 `<var>:<func>:<name>`（`direct_call_fp.py:41`）仅用于 direct_assign/cast_assign 的输出，未用于 param_binding 的输出

### 修复方向

当消费者可以访问调用上下文时，将作用域键模式扩展到 param_binding 的输出；否则默认使用 `callee:param_name` 前缀键（`f'{call_name}:{pname}'`）。

---

## 缺陷 2：field_call 的回退堆叠反模式

**位置**: `src/ethunter/analyzer/field_call.py:108-218` — `_visit()` 中 15 层以上回退

### 回退层列表

```
Layer  0: type-aware key (<gstruct>:<type>.<path>)        — 新增，精确匹配则早返回
Layer  1: <gstruct:path>                                   — 来自 initializer_assign
Layer  2: <struct:path>                                    — 来自 param_assign
Layer  3: <chain:path>                                     — 复杂链式访问
Layer  4: <garray:base>                                    — 全局数组初始化器
Layer  5: suffix scan: 所有以 .field> 结尾的 key           — 高危：遍历全表
Layer  6: struct alias 解析                                — Curl_ssl -> Curl_ssl_openssl
Layer  7: suffix <struct:*.field> 匹配                     — 渐进式缩短后缀
Layer  8: middle component 匹配                            — 匹配路径中间段
Layer  9: bare last component + 扫描全表                    — 高危：遍历全表
Layer 10: 扫描所有以 .{last_part}> 结尾的 key              — 高危：遍历全表
Layer 11: param alias binding                              — Fix B 修复
Layer 12: local_fp_tracker                                 — Fix C3 修复
Layer 13: pointer alias resolution                         — Fix A 修复
Layer 14: <vtable:path>                                    — 旧格式兼容
Layer 15: <vtable_init>                                    — 全局初始化列表
```

### 根本问题

此函数的架构模型是"尝试所有方法，直到找到某个结果"——这是一个通过实验构建的临时解析器。每个缺失的边 → 添加新的回退 → 所有回退均等应用于每个查询。

### 为什么这会造成损害

- 第 5、9、10 层（后缀/最后组件扫描）迭代整个 `dataflow.targets` 字典，匹配来自完全不相关结构体的 `.{field_name}>` 键
- 即使在精确匹配成功之后，第 9 层仍然运行（`field_call.py:143-149`："Always merge suffix-matched targets even when `<gstruct:>` had partial hits"）
- `fnptr-virtual` 中的 60 条误报边：vtable 结构体的字段 `.get_state_map_by_name` 匹配到来自其他结构体中不同 `get_state_map_by_name` 条目的后缀

### 修复方向

用**优先级解析策略**替换回退堆叠——可以是策略模式——其中：
1. 精确匹配（第 0-4 层）首先返回，无回退
2. 后缀回退明确要求**同结构体类型**匹配
3. 扫描整表的通配符回退被消除，或通过调用上下文进行限定

---

## 缺陷 3：双重模块迁移债务

**位置**: `src/ethunter/analyzer/orchestrator.py:83-138`

### 机制

编排器同时在两套并行模块中运行参数追踪：

```python
# orchestrator.py:83-84 — 新: param_binding 写入 dataflow + registration_sites (无边)
param_binding.analyze(tree, filepath, symbol_table, engine)

# orchestrator.py:101-108 — 旧: param_assign 产生 callback_reg + callback_param 边
edges = param_assign.analyze(tree, filepath, symbol_table, engine)

# orchestrator.py:124-127 — 新: param_dispatch 产生 callback_param 边
edges = param_dispatch.analyze(tree, filepath, engine)

# orchestrator.py:135-138 — 新: callback_reg 产生 callback_reg 边
edges = callback_reg.analyze(tree, filepath, engine)
```

旧模块 `param_assign.py`（787 行）仍然运行其完整的 4-pass 流程，而新模块（`param_binding.py` 247 行 + `param_dispatch.py` 121 行 + `callback_reg.py` 60 行）也在运行。编排器然后通过后处理去重：

```python
# orchestrator.py:152-161 — 抑制 field_call 已覆盖的被调用者
# orchestrator.py:164-182 — 通过 caller+callee 去重
```

### 根本问题

这不是增量迁移——而是具有冗余计算（每个模块独立遍历 AST）和冲突输出的**并行运行**。去重逻辑掩盖了这种重复。

### 修复方向

一旦新模块经过实战验证，应完全移除 `param_assign.analyze()`。`_register_phase()` 可以保留（纯元数据收集），但 `analyze()`（Pass 1-4）应由新模块替换。

---

## 缺陷 4：无置信度/证据模型

**位置**: `src/ethunter/graph/model.py` — `CallEdge` 无 `confidence` 字段

### 机制

`CallEdge` 有 `type`（`DIRECT`/`INDIRECT`）和 `indirect_kind`（`callback_param`、`field_call`、`callback_reg`、`dlsym_fp`、`direct_assign`），但没有置信度或证据来源跟踪。所有边在最终去重步骤中平等对待——唯一的排序是"直接边优于间接边"（`orchestrator.py:171`）。

### 为什么这会造成损害

- 通过精确 `<gstruct:exact.path>` 匹配发现的 `field_call` 边，与通过通配符后缀扫描发现的 `field_call` 边无法区分
- `callback_param` 边（来自被调用者体内的 fnptr 调用 = 高置信度）和 `callback_param` 边（来自外部调用者 = 较低置信度）仅在 `param_dispatch` 中通过 Pass A/B 优先级区分，而不是在最终图中
- 没有方法可以过滤图以仅包含高置信度边，或按证据强度对结果进行排序

### 修复方向

向 `CallEdge` 添加 `confidence` 字段（或 `evidence` 元数据），记录边是如何发现的（精确键匹配 = high，后缀回退 = medium，名称启发式 = low）。这使得下游消费者能够根据用例进行过滤。

---

## 缺陷 5：缺失检测能力——结构性无法修复的模式

### 5a. Vtable 分发（fnptr-virtual: 0/2 召回率, 60 误报）

**fixture 模式**: C++ 虚拟方法表在 C 中的等价实现——基类指针 `ctx->vtable->get_state_map_by_name(ctx, ...)` 根据运行时 vtable 指针分发到派生类实现。

**架构限制**:
- `field_call` 看到 `ctx->vtable->get_state_map_by_name` 并尝试解析——但 `vtable` 在 dataflow 中没有条目
- vtable 结构体 `region_model_context_vtable` 的初始化器是一个聚合初始值设定项：`{noop_warn, noop_add_note, ..., noop_region_model_context_get_state_map_by_name, ...}`
- **没有任何模块**跟踪结构体字段（vtable 的 `.get_state_map_by_name`）与结构体初始化位置（`noop_vtable = {..., noop_region_model_context_get_state_map_by_name, ...}`）之间的对应关系

**需要的模式**: 一个新的 vtable 跟踪器模块，识别 "vtable struct 定义 → 通过索引聚合初始化 → 基类指针赋值 → 通过基类指针调用的字段表达式" 链。

### 5b. dlsym 动态加载（fnptr-dynamic-call: 1/6 召回率）

**fixture 模式**: `onload = dlsym(handle, "RedisModule_OnLoad")`；`ret->sk_api_version = dlsym(...)`；`dlsym_prefixed(s->lib, "OMX_Init", prefix)`

**架构限制** (`dlsym_fp.py`):
- 仅处理 `dlsym(handle, "string_literal")` — 查找字符串字面量
- 不处理中间赋值：`onload = dlsym(...)` — `dlsym_fp` 不写入 dataflow
- 不处理结构体字段存储：`ret->sk_api_version = dlsym(...)` — 不跟踪字段赋值
- 不处理变量字符串：`dlsym_prefixed(s->lib, "OMX_Init", prefix)` — 无法解析

**修复方向**: `dlsym_fp` 需要将 dlsym 解析结果写入 dataflow（如同 `direct_assign`），而非直接产生边。

### 5c. Varargs 函数（fnptr-varargs: 1/1 召回率, 1 误报）

单个 varargs fixture 有正确的召回率但有一条额外边。由于 `...` 在 C 中是零信息参数的占位符，没有分析器能够推理出哪些 varargs 位置接收了 fnptr。

---

## 缺陷 6：结构体类型感知不完整

**位置**: `src/ethunter/analyzer/symbol_table.py:114-119` — `_var_types`；`field_call.py:110-116` — Layer 0

### 机制

类型感知的 `<gstruct>:<type>.<path>` 键仅在一个地方产生（`initializer_assign.py`），仅在一个地方消费（`field_call.py` 的 Layer 0）。

`SymbolTable._var_types` 存储变量名 → 结构体类型映射，但这是：
1. **稀疏的** —— 并非所有变量声明都被 `record_var_type()` 捕获
2. **仅限名称** —— `ctx` 映射到 `struct_context_type`，但没有 `ctx` 在不同函数中具有不同类型的机制
3. **未跨文件持久化** —— 每个文件的 SymbolTable 独立处理

### 修复方向

扩展类型跟踪到更多声明点（param_binding 的参数声明，field_call 中 struct 指针的局部变量声明），并增加 `SymbolTable` 的函数级作用域类型映射。

---

## 总结

| # | 缺陷 | 影响 | 严重程度 |
|---|---|---|---|
| 1 | 全局 dataflow 无作用域 | 跨函数污染，支持假后缀匹配 | **严重** |
| 2 | field_call 15 层回退堆叠 | 结构脆弱，难以维护和推理 | **严重** |
| 3 | 双重 param_assign + param_binding | 冗余计算，混淆边来源 | 高 |
| 4 | 无边置信度 | 无法区分高/低质量边 | 中 |
| 5 | 缺失能力（vtable, dlsym 赋值, varargs） | 某些类别 0% 召回 | 中 |
| 6 | 结构体类型信息不完整 | 精确匹配未充分利用 | 中 |

**最关键的架构缺陷是 #1（无作用域数据流）和 #2（回退堆叠）。** 它们共同导致了绝大多数剩余的 276 条误报。

**最有影响力的下一步**：不是添加更多回退层或分析器，而是引入**函数作用域数据流**（其中 `param_binding` 写入 `(callee_func, param_name)` 键而非裸 `param_name`），并将 `field_call` 的后缀回退限制为仅限**同结构体类型**的匹配。这将同时提高精度（更少的跨函数污染）和召回率（类型感知路由优先于通配符扫描）。
