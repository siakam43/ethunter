# fnptr-global-struct 场景召回率提升 Spec

## 1. 现状

| 指标 | 值 |
|---|---|
| 召回率 | 14.7% (10/68) |
| 通过 | example_1,2,3,5,6,7,8,10,11（9 个） |
| 失败 | example_4（57 edges 全丢）, example_9（1 edge 丢失） |

### 1.1 example_4 失败根因

`zfs_ioc_vec[vecnum]->zvec_legacy_func(zc)` — struct 数组通过索引访问 + field 调用。

**两层 gap：**

1. **field_call 层**：`extract_field_path` 不支持 subscript_expression。`arr[i]->field` 在 tree-sitter 中是 `field_expression(subscript_expression, ->)`，当前代码遇到 subscript 直接返回 None
2. **initializer_assign 层**：目标函数通过 `zfs_ioctl_register_legacy(ioc, func, ...)` 注册，内部做 `vec->zvec_legacy_func = func`。当前只处理 `init_declarator`（声明时初始化），不处理函数体内的 struct 指针字段赋值

### 1.2 example_9 失败根因

`stream_read_tree(ib, data_in)` 是一个 C 宏，展开后等价于 `streamer_hooks.read_tree(ib, data_in)`。但 tree-sitter 在预处理前的 AST 中看到的是普通的 `call_expression(identifier)`，不是 `field_expression`，所以 `field_call` 匹配不到。

## 2. 修改方案

### 2.1 helpers.py — `extract_field_path` 支持 subscript

**当前行为**：只处理 `field_expression` 的 `. `和 `->` 链式访问，遇到 `subscript_expression` 返回 None。

**新行为**：当 field_expression 的 operand 是 subscript_expression 时，提取 subscript 的 base 名称（如 `zfs_ioc_vec`），拼接上 field 名（如 `zvec_legacy_func`），返回 `zfs_ioc_vec.zvec_legacy_func`。

```
arr[i]->field  →  "arr.field"
arr[i].field   →  "arr.field"
arr[i]->chain->field  →  "arr.chain.field"
```

不做索引值解析（静态分析中索引通常是运行时变量），用 base array name 做通配匹配。initializer_assign 注册的 `<gstruct:arr.field>` 就能被匹配。

### 2.2 initializer_assign.py — struct 指针字段赋值追踪

**当前行为**：只处理 `init_declarator`（声明时初始化），如 `vdev_ops_t ops = { .field = func }`。

**新行为**：新增对 `assignment_expression` 的处理，识别 struct 指针字段赋值模式：

```c
vec->zvec_legacy_func = func;          // 直接赋值
(*vec).zvec_legacy_func = func;        // 解引用赋值
```

**实现逻辑**：

1. 遍历 AST 找到 `assignment_expression` 节点
2. 检查 LHS 是否为 `field_expression`（`. `或 `->` 访问）
3. 提取变量名（如 `vec`）和字段名（如 `zvec_legacy_func`）
4. 检查 RHS 是否为 `identifier` 且在 symbol_names 中
5. 如果是，注册到 dataflow：`<gstruct:var.field>` → `func`

**局部指针来源追踪**：`vec` 是 `zfs_ioctl_register_legacy` 的函数参数，`zfs_ioctl_register_legacy` 被调用时传入 `&zfs_ioc_vec[...]`。静态分析难以跨函数追踪参数来源。

**实际方案**：在 initializer_assign 中做**函数内**局部指针追踪：
- 扫描函数体内 `var = &global_name[...]` 或 `var = &global_name` 形式的赋值
- 建立 `var → global_name` 的映射
- 当遇到 `var->field = func` 时，将 `var` 替换为 `global_name`，注册为 `<gstruct:global_name.field>`
- 不追踪跨函数参数传递（`vec` 作为参数传入的情况），因为 example_4 中 `vec` 在 `zfs_ioctl_register_legacy` 内部直接接收 `&zfs_ioc_vec[...]` 的地址，调用方的局部变量 `vec` 也是通过 `&zfs_ioc_vec[vecnum]` 赋值的——追踪函数内赋值即可覆盖

**注意**：example_4 中的调用侧 `vec = &zfs_ioc_vec[vecnum]` 在 `zfsdev_ioctl_common` 函数内，注册侧 `vec` 参数指向 `&zfs_ioc_vec[...]` 在 `zfs_ioctl_register_legacy` 函数内。两者不在同一函数，但 field_call 侧通过 `extract_field_path` 提取 subscript base name 得到 `zfs_ioc_vec`，所以调用侧不需要指针追踪——只需要 initializer_assign 在注册侧注册 `<gstruct:zfs_ioc_vec.field>` 即可。

具体实现：在 initializer_assign 的 assignment_expression 处理中，当 LHS 是 `field_expression` 且 operand 是 `identifier`（如 `vec`）时：
1. 先尝试直接注册 `<gstruct:vec.field>`
2. 扫描同一函数体内是否有 `vec = &<identifier>[...]` 或 `vec = &<identifier>` 的赋值
3. 如果有，用该 identifier 替换 vec，额外注册 `<gstruct:<identifier>.field>`

### 2.3 field_call.py — 宏展开调用处理（example_9）

**问题**：`stream_read_tree(...)` 是宏，tree-sitter 看到普通标识符调用。

**方案**：在 `field_call` 中新增宏调用回退逻辑：
1. 处理 `call_expression` 时，如果 `function` 子节点是 `identifier` 而非 `field_expression`
2. 先走 direct_call_fp 的正常路径（查 dataflow 变量映射），如果未命中
3. 检查该 identifier 是否在当前文件中被 `#define` 为包含 `.` 或 `->` 的宏体
4. 如果是，从宏体中提取 `struct_var.field` 模式，用该 field_path 查 dataflow

**宏解析实现**：
- 扫描 `preproc_def` / `preproc_function_def` 节点，提取宏名和宏体
- 在宏体中查找 `identifier '.' identifier` 或 `identifier '->' identifier` 模式
- 提取 struct 变量名和 field 名，构造 field_path
- 示例：`#define stream_read_tree(IB, DATA_IN) streamer_hooks.read_tree(IB, DATA_IN)` → 提取 `streamer_hooks.read_tree` → field_path = `streamer_hooks.read_tree` → 查 `<gstruct:streamer_hooks.read_tree>` → 找到 `lto_input_tree`

## 3. 涉及文件

| 文件 | 变更 |
|---|---|
| `src/ethunter/analyzer/helpers.py` | `extract_field_path` 增加 subscript_expression 支持 |
| `src/ethunter/analyzer/initializer_assign.py` | 新增 assignment_expression 处理，追踪 struct 指针字段赋值 + 局部指针来源追踪 |
| `src/ethunter/analyzer/field_call.py` | 新增宏调用回退匹配逻辑 |

## 4. 验收标准

| 指标 | 当前 | 目标 |
|---|---|---|
| fnptr-global-struct 召回率 | 14.7% (10/68) | 95%+ |
| example_4 | 0/57 | 55+/57 |
| example_9 | 0/1 | 1/1 |
| 全量测试 | 全通过 | 全通过 |

## 5. 风险

| 风险 | 缓解 |
|---|---|
| 局部指针追踪复杂 | 限定只追踪 `var = &global_array[...]` 这种直接赋值，不追踪链式传递 |
| 宏处理误匹配 | 仅当宏体中包含 `.` 或 `->` 且包含 struct 变量名时才匹配 |
| 影响其他场景 | 新逻辑都是新增 fallback，不影响已有路径 |
