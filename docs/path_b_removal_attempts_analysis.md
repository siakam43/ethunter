# Path B 删除尝试全记录

**日期**: 2026-05-15
**目标**: 完全删除旧 store（`VariableState.targets`）和 Path B（`field_call._visit()` 中的 legacy suffix scan）

---

## 1. 问题定义

ethunter 的 field_call 模块存在两套并行的 suffix 扫描机制：

| 机制 | 扫描对象 | Key 格式 | 扫描策略 | 文件 |
|------|---------|---------|---------|------|
| **Path A** (FieldResolver Tier 3/4) | `ScopedStore.struct_fields` | `gstruct:{prefix}.{tail}` | 单次 suffix 匹配 | field_resolver.py |
| **Path B** (Legacy suffix scan) | `VariableState.targets` | `<gstruct:{prefix}.{tail}>` | 渐进式 suffix | field_call.py |

两套机制存在两个问题：
1. **数据重复**：同一份数据以两种格式存储在两个 store 中
2. **Path B 无类型约束**：匹配所有同名字段，无论属于哪个 struct，产生大量跨类型误报

---

## 2. 尝试过程（5 轮）

### 第 1 轮：直接删除 Path B

**做法**：删除 `field_call._visit()` 中的 entire legacy fallback 块（lines 273-289）。

**结果**：❌ 9 个 test 失败，recall 从 98.86% 大幅下降。Path B 的旧 store suffix scan 覆盖了 Path A 无法找到的数据。

### 第 2 轮：数据审计 + 补齐写入 + 链分解 + 删除

**做法**：
- 数据审计发现新 store 中有旧 store 的所有 `<gstruct:>` 数据
- 补齐 `param_assign`、`param_binding` 的写入到新 store
- 添加链分解（`s.method.put_cb` → `s.method` → `ssl3_method` → `SSL_METHOD.put_cb`）
- `collect()` 无条件写新 store
- Pipeline 重排（`collect_var_types()` 提前到 `collect()` 之前）
- 删除 Path B

**结果**：❌ 10 个 test 失败。数据**理论上**在新 store 中存在，但 `field_call.analyze()` 运行时找不到。

### 第 3 轮：渐进式 suffix 扫描

**发现**：Path B 使用**渐进式 suffix**（先匹配 `.alpn_select_cb`，再匹配 `.ext.alpn_select_cb`，最后 `.ctx.ext.alpn_select_cb`），而 Path A 只匹配一次完整的 `field_tail`。

**做法**：让 Tier 3/4 也使用渐进式 suffix（`for i in range(1, len(parts))`），并删除类型门控。

**结果**：⚠️ 从 10 个失败降至 7 个。但与 Path B 存在时仍有差距。

### 第 4 轮：全量数据写入审计

**审计发现**：38 个旧 store 写入点中，12 个 `<gstruct:>`/`<struct:>` 写入点**全部**已双写到新 store。新 store 是旧 store 的超集。

**做法**：基于审计结果再次尝试删除 Path B。

**结果**：❌ 仍然 7 个 test 失败。数据存在但 `field_call.analyze()` 产边数大幅下降（example_5：6 edges → 1 edge）。

### 第 5 轮：Orchestrator 管道调查

**做法**：
- 手工分步执行 orchestrator pipeline（与 `run_all_analyses` 逐步骤对比）
- 在 orchestrator 中加 debug 打印，追踪边丢失的精确时间点
- 对比 Path B 存在/删除时的 `field_call.analyze()` 输出

**关键发现**：
- **手工分步 pipeline**：field_call 产出 6 条边（全部正确），test 通过 ✅
- **`run_all_analyses`**：field_call 产出 1 条边（丢失 5 条），test 失败 ❌
- 丢失发生在 Phase 1（param_assign.analyze()）和 Phase 2（field_call.analyze()）之间

**根因**：Path B 删除后，`field_call.analyze()` 产边效率依赖于 Phase 1 中写入的数据。Path B 存在时，它的 suffix scan 在 `_visit()` 内部补充了 resolver 缺失的 targets。删除后，resolver 的输出直接成为最终结果，没有 fallback 来补充数据。

---

## 3. 当前架构状态

| 组件 | 状态 |
|------|------|
| `VariableState.targets`（旧 store） | ✅ 仍然使用 |
| `ScopedStore.struct_fields`（新 store） | ✅ 数据完整（旧 store 超集） |
| Path A (FieldResolver Tier 1-4) | ✅ 渐进式 suffix + 链分解 + 无类型门控 |
| Path B (legacy suffix scan) | ✅ 保留（删除会导致 7 个 test 失败） |
| 12 个 `<gstruct:>`/`<struct:>` 双写 | ✅ 全部完成 |
| collect_var_types() + 管道重排 | ✅ 已实现 |
| 测试通过数 | 196 passed, 2 个 pre-existing 失败 |

---

## 4. 阻塞点

### 阻塞点 1：Orchestrator 管道差异

`run_all_analyses` 与手工分步 pipeline 产生不同结果。手工 pipeline 能找到所有 correct edges，但 `run_all_analyses` 不行。差异在于 `DataflowEngine` 封装层与各 analyzer 模块之间的数据传递。

### 阻塞点 2：Path B 的副作用依赖

Path B 的 suffix scan 不仅产生 edges，还**修改了 resolver 的局部状态**（`targets` 集合）。这个状态在后续的 callback-of-callback 检测中使用。删除 Path B 后，这个状态不再被补充。

### 阻塞点 3：非 field_call 的 recall gap

`connUnixRead -> connTLSRead` 边（fnptr-global-struct/example_5）在所有版本中都缺失，这是 pre-existing bug，与 Path B 无关。

---

## 5. 已完成的核心改进

虽然 Path B 未被删除，但以下架构改进已成功落地：

| 改进 | 文件 | 效果 |
|------|------|------|
| 渐进式 suffix（匹配 Path B 策略） | field_resolver.py | Path A 覆盖 Path B 扫描能力 |
| 删除类型门控 | field_resolver.py | 有类型时 suffix 仍然运行 |
| 多层链分解（cut=2..N-1） | field_resolver.py | 4-segment path 可解析 |
| `<gstruct:>`/`<struct:>` 全双写 | 5 files | 12/12 数据迁移完成 |
| collect_var_types() + 管道重排 | initializer_assign.py, orchestrator.py | `_var_types` 在 collect() 前填充 |
| collect() 无条件写新 store | field_call.py | 所有 resolved_value 入新 store |
| `_unwrap_identifier` pointer_expression | helpers.py | `&expr` RHS 正确捕获 |
| Confidence 枚举 + Evidence 结构化 | model.py + 12 analyzers | 置信度形式化 |
| Type system expansion | 3 files | 非指针声明、声明参数、返回值类型 |

---

## 6. 结论

**Path B 可以被架构性替换**（渐进式 suffix + 新 store 数据已就绪），但**当前不能在生产中删除**，原因是一个 orchestrator 层的交互差异：手工分步 pipeline 可正确工作，而 `run_all_analyses` 产生的 field_call 边数大幅减少。

**下一步**：隔离 orchestrator 中 `field_call.analyze()` 的调用方式差异，找出为何同一个函数在不同调用上下文中产生不同结果。
