# Analyzer 架构重构：阶段解耦与 et_bench 覆盖率提升

## 1. 背景与目标

ethunter 现有 13 个 analyzer 模块按"检测什么间接调用模式"划分（fp_assign、vtable、callback_param 等），导致分配侧和调用侧耦合在同一模块中。ET-Bench 召回率 73.26%（452/617），主要缺失集中在以下场景：

| 分类 | 召回率 | 缺失数 | 根因 |
|---|---|---|---|
| fnptr-global-struct | 0% | 68 | 无法处理 init_declarator + designated initializer |
| fnptr-cast | 10% | 9 | 无法处理 cast_expression 包裹的函数赋值 |
| fnptr-struct | 19% | 17 | struct 成员赋值追踪不完整 |
| fnptr-library | 41% | 41 | 参数传递 + 结构体链式调用追踪不完整 |
| fnptr-callback | 69% | 11 | 参数传递追踪不完整 |
| fnptr-only | 62% | 9 | 全局变量赋值后间接调用，覆盖不完整 |

**排除场景**：fnptr-dynamic-call（dlopen/dlsym 动态加载）、fnptr-virtual（C++ 虚表多态），暂不处理。

**目标**：重构 analyzer 架构，按"目标解析 vs 调用检测"两阶段解耦，一次性覆盖上述所有场景，ET-Bench 总召回率提升至 95%+。

## 2. 架构设计

### 2.1 两阶段模型

```
┌─────────────────────────────────┐     ┌─────────────────────────────────┐
│  Target Resolution               │     │  Call Detection                  │
│  (目标解析：赋值 → dataflow)     │────▶│  (调用检测：dataflow → CallEdge) │
│                                  │     │                                  │
│  • direct_assign                 │     │  • direct_call_fp                │
│  • initializer_assign            │     │  • field_call                    │
│  • cast_assign                   │     │  • array_call                    │
│  • param_assign                  │     │                                  │
└─────────────────────────────────┘     └─────────────────────────────────┘
```

**Target Resolution** — 扫描赋值语法，建立"变量/路径 → 函数目标集合"映射，写入 dataflow。

**Call Detection** — 扫描调用语法，从 dataflow 查找映射，生成 CallEdge。

### 2.2 Target Resolution 模块

#### 2.2.1 `direct_assign`（直接赋值）

覆盖现有模块：`fp_assign`、`fp_alias`、`typedef_fp`、`fp_return`、`fp_only`

处理的 AST 模式：
- `assignment_expression`：`fp = func_name`
- `init_declarator`：`void (*fp)(void) = func_name`
- 别名链：`fp2 = fp1`（RHS 已在 dataflow 中有映射时继承）

覆盖 et_bench 场景：fnptr-only

dataflow key：变量名（如 `fp`、`zmalloc_oom_handler`）

#### 2.2.2 `initializer_assign`（初始化器赋值）

覆盖现有模块：`fp_array`、`vtable`（分配侧）、`lazy_init`、`union_fp`

处理的 AST 模式：
- `init_declarator` + `initializer_list`（纯标识符列表）：全局数组 `object_viewer[] = { func_a, func_b }`
- `init_declarator` + `initializer_list` + `pair_list`（designated initializer）：全局结构体 `ops = { .field = func }`
- 嵌套结构体数组：`structs[i] = { .field = func }`

覆盖 et_bench 场景：fnptr-global-struct、fnptr-global-array、fnptr-global-struct-array

dataflow key 格式：
- 纯数组：`<garray:name>`（如 `<garray:object_viewer>`）
- 全局结构体字段：`<gstruct:name.field>`（如 `<gstruct:vdev_indirect_ops.vdev_op_remap>`）
- 全局结构体数组字段：`<gstructarray:name[i].field>`

#### 2.2.3 `cast_assign`（类型转换赋值）

新模块，覆盖 fnptr-cast 场景

处理的 AST 模式：
- `init_declarator` + `cast_expression` → `identifier`：`fn_t *fp = (fn_t *)func_name`
- `assignment_expression` + `cast_expression` → `identifier`：`fp = (fn_t *)func_name`
- macro 展开后的 cast：`#define CAST(type, func) (type)(func)`

dataflow key：变量名（如 `nstime_update`、`md5params`）

#### 2.2.4 `param_assign`（参数传递）

覆盖现有模块：`callback_param`、`callback_reg`

处理的 AST 模式：
- 函数定义中的函数指针类型参数：`void fn(void (*cb)(void))`
- 调用处传入的函数实参：`fn(callback_func)`
- 多跳参数传递：`fn1(cb) → fn2(cb) → cb()`
- 参数存入结构体字段：`handler.finalizeResultEmission = param`

覆盖 et_bench 场景：fnptr-callback、fnptr-library

dataflow key 格式：
- 直接参数：`<param:func_name.param_name>`（如 `<param:auth_create_digest_http_message.convert_to_ascii>`）
- 结构体参数链：`<struct_field:funcs.read>`

### 2.3 Call Detection 模块

#### 2.3.1 `direct_call_fp`（直接标识符调用）

覆盖现有模块：`fp_assign`（调用侧）、`fp_return`、`dlsym_fp`

处理的 AST 模式：
- `call_expression` + `identifier`：`fp()`
- RHS 为函数指针的间接调用

覆盖 et_bench 场景：fnptr-only、fnptr-cast

从 dataflow 查询 key：标识符名

#### 2.3.2 `field_call`（字段访问调用）

覆盖现有模块：`vtable`（调用侧）

处理的 AST 模式：
- `call_expression` + `field_expression`（`.` 运算符）：`obj.field()`
- `call_expression` + `field_expression`（`->` 运算符）：`ptr->field()`
- 链式访问：`c->funcs->read()`、`ctx->vtable->get_state_map_by_name()`

覆盖 et_bench 场景：fnptr-global-struct、fnptr-struct、fnptr-library、fnptr-global-struct-array

从 dataflow 查询 key：
- 简单字段：`<gstruct:obj.field>`
- 链式字段：`<chain:c.funcs.read>`

#### 2.3.3 `array_call`（数组下标调用）

覆盖现有模块：`fp_array`（调用侧）

处理的 AST 模式：
- `call_expression` + `subscript_expression`：`arr[i]()`
- 结构体数组下标 + 字段：`structs[i].field()`

覆盖 et_bench 场景：fnptr-global-array、fnptr-global-struct-array

从 dataflow 查询 key：数组名或 `structs[i].field`

### 2.4 保持独立的模块

- `direct_call.py`：检测 DIRECT 调用（非间接调用），不受重构影响
- `dlsym_fp.py`：dlsym 动态加载（fnptr-dynamic-call 场景，暂不优化）
- `macro_fp.py`：合并到 `direct_assign` 或 `cast_assign` 中处理

## 3. 模块文件映射

### 3.1 新文件（平放在 `src/ethunter/analyzer/` 目录下）

| 文件 | 分类 | 说明 |
|---|---|---|
| `direct_assign.py` | Target Resolution | 直接赋值 |
| `initializer_assign.py` | Target Resolution | 初始化器赋值 |
| `cast_assign.py` | Target Resolution | 类型转换赋值 |
| `param_assign.py` | Target Resolution | 参数传递 |
| `direct_call_fp.py` | Call Detection | 直接标识符调用 |
| `field_call.py` | Call Detection | 字段访问调用 |
| `array_call.py` | Call Detection | 数组下标调用 |

### 3.2 保留文件

| 文件 | 变更 |
|---|---|
| `direct_call.py` | 不变 |
| `dlsym_fp.py` | 不变 |
| `dataflow.py` | 不变（key 格式扩展兼容） |
| `symbol_table.py` | 不变 |
| `helpers.py` | 增强：添加提取 field_expression 完整路径的辅助函数 |

### 3.3 废弃文件（逻辑合并到新模块中）

| 旧文件 | 合并到 |
|---|---|
| `fp_assign.py` | `direct_assign` + `direct_call_fp` |
| `fp_array.py` | `initializer_assign` + `array_call` |
| `vtable.py` | `initializer_assign`（分配侧） + `field_call`（调用侧） |
| `callback_param.py` | `param_assign` |
| `callback_reg.py` | `param_assign` |
| `typedef_fp.py` | `direct_assign` |
| `fp_alias.py` | `direct_assign` |
| `fp_return.py` | `direct_assign` |
| `lazy_init.py` | `initializer_assign` |
| `union_fp.py` | `initializer_assign` |
| `macro_fp.py` | `direct_assign` / `cast_assign` |

## 4. Orchestrator 变更

### 4.1 运行顺序

```
1. direct_call（DIRECT，独立）
2. Target Resolution（全部，按文件遍历，写入 dataflow）
   - direct_assign
   - initializer_assign
   - cast_assign
   - param_assign
3. Call Detection（全部，按文件遍历，从 dataflow 读取）
   - direct_call_fp
   - field_call
   - array_call
4. dlsym_fp（独立）
5. 去重合并
```

### 4.2 orchestrator.py 结构

```python
TARGET_RESOLVERS = [
    direct_assign,
    initializer_assign,
    cast_assign,
    param_assign,
]

CALL_DETECTORS = [
    direct_call_fp,
    field_call,
    array_call,
]

# Target resolution first
for filepath, tree in trees.items():
    for resolver in TARGET_RESOLVERS:
        resolver.analyze(tree, filepath, symbol_table, dataflow)

# Call detection after dataflow is populated
for filepath, tree in trees.items():
    for detector in CALL_DETECTORS:
        edges = detector.analyze(tree, filepath, symbol_table, dataflow)
        ...
```

### 4.3 向后兼容

- 现有 `--analyze` / `--from-json` CLI 接口不变
- `CallEdge` 的 `indirect_kind` 字段保留原有值（`fp_assign`、`callback_param` 等），新模块使用新的 kind 值（`direct_assign`、`initializer`、`cast`、`param`、`field_call`、`array_call`）
- 现有测试用例全部通过（旧 fixture 的调用模式仍被覆盖）

## 5. Dataflow 扩展

### 5.1 新增 key 格式

| Key 格式 | 示例 | 来源 |
|---|---|---|
| `<gstruct:name.field>` | `<gstruct:vdev_indirect_ops.vdev_op_remap>` | `initializer_assign` |
| `<garray:name>` | `<garray:object_viewer>` | `initializer_assign` |
| `<param:func.param>` | `<param:auth_create_digest_http_message.convert_to_ascii>` | `param_assign` |
| `<gstructarray:name[i].field>` | `<gstructarray:auxFieldHandlers[j].setter>` | `initializer_assign` |
| `<chain:c.funcs.read>` | `<chain:c.funcs.read>` | `field_call`（链式访问） |

### 5.2 `VariableState` 接口不变

`assign(key, target)` 和 `resolve(key)` 接口保持不变，只是 key 的命名约定扩展。

### 5.3 `helpers.py` 新增函数

```python
def extract_field_path(node: ts.Node) -> str | None:
    """递归提取 field_expression 的完整路径字符串。
    
    支持 . 和 -> 运算符，以及链式访问。
    例如：c->funcs->read → "c.funcs.read"
    """
```

## 6. 实施步骤

### Step 1: 创建 Target Resolution 模块

1. `initializer_assign.py`：处理 designated initializer（`{ .field = func }`）和纯数组初始化器
2. `cast_assign.py`：处理 cast_expression 赋值
3. `direct_assign.py`：从现有 fp_assign.py 移植 assignment/init_declarator 逻辑
4. `param_assign.py`：追踪函数参数传递链路

### Step 2: 创建 Call Detection 模块

1. `direct_call_fp.py`：从现有 fp_assign.py 移植调用侧逻辑
2. `field_call.py`：从现有 vtable.py 移植调用侧逻辑，增强支持链式 `->` 访问
3. `array_call.py`：从现有 fp_array.py 移植调用侧逻辑

### Step 3: helpers.py 增强

1. 新增 `extract_field_path()`：递归提取 field_expression 完整路径
2. 增强 `find_child()` 支持更灵活的子节点查找

### Step 4: Orchestrator 变更

1. 更新 `orchestrator.py`：引入 TARGET_RESOLVERS 和 CALL_DETECTORS
2. 确保 Target Resolution 先于 Call Detection 执行

### Step 5: 废弃旧模块

1. 从 orchestrator 中移除旧模块引用
2. 删除旧模块文件（fp_assign、fp_array、vtable、callback_param、callback_reg、typedef_fp、fp_alias、fp_return、lazy_init、union_fp、macro_fp）

### Step 6: 测试验证

1. 运行 et_bench 测试验证召回率
2. 运行全量测试确保无回归

## 7. 验收标准

| 指标 | 当前 | 目标 |
|---|---|---|
| 总召回率 | 73.26% | 95%+ |
| fnptr-global-struct | 0% | 95%+ |
| fnptr-cast | 10% | 95%+ |
| fnptr-struct | 19% | 80%+ |
| fnptr-library | 41% | 80%+ |
| fnptr-callback | 69% | 90%+ |
| fnptr-global-array | 100% | 100%（保持） |
| fnptr-global-struct-array | 97% | 97%+（保持） |
| fnptr-only | 62% | 90%+ |
| fnptr-varargs | 100% | 100%（保持） |
| 现有测试 | 全通过 | 全通过 |

## 8. 风险与缓解

| 风险 | 缓解 |
|---|---|
| 重构破坏现有模块 | 先新增模块，不立即删除旧模块；通过测试后再移除 |
| dataflow key 冲突 | 新 key 使用 `<gstruct:>`、`<garray:>`、`<param:>`、`<chain:>` 前缀，与现有变量名不冲突 |
| 多文件追踪 | 当前架构不跨文件追踪 dataflow（现有行为不变），部分场景可能仍无法覆盖 |
| `->` 链式访问 AST 复杂 | 使用递归函数逐层解析 field_expression，提取完整路径字符串 |
| 参数传递多跳追踪 | 限制追踪深度（默认 3 跳），避免无限递归 |
