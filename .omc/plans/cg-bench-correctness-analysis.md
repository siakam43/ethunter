# Plan: CG-Bench Fixture Correctness Analysis

## Requirements Summary

对 `tests/benchmark/cg_bench/` 下全部 104 个 fixture 用例进行正确性分析，输出按分类汇总的 markdown 报告。分析两个维度：

1. **语法正确性**：每个 fixture.c 能否被 tree-sitter 成功解析为 AST
2. **Ground Truth 语义一致性**：双向验证 `ground_truth.json` 中记录的间接调用边与 fixture.c 代码实际语义

## Acceptance Criteria

- [ ] 104 个 fixture.c 全部完成 tree-sitter 解析检查，每个用例有明确的 PASS/FAIL 结论
- [ ] 每个 ground_truth.json 中的每条 caller→callee 边经过人工验证存在且为间接调用（非直接调用）
- [ ] 每个 fixture.c 中的所有间接调用边经过人工检查确认无遗漏
- [ ] 每个 fixture.c 注释中的 fnptr 变量名、targets 声明与实际代码行为一致
- [ ] 输出一份按分类组织的 markdown 报告，每个用例包含语法检查结果 + ground truth 语义一致性分析 + 问题列表
- [ ] 报告完整覆盖全部 11 个分类共 104 个用例，无遗漏

## RALPLAN-DR Summary

### Principles
1. **LLM 人工分析优先**：语义一致性由 LLM 逐行阅读代码并分析，不使用脚本进行粗略判断，也不要求用户手动操作
2. **只报告不修复**：发现问题只记录到报告中，不修改 fixture 或 ground truth 文件
3. **双向验证**：ground truth 的边必须在代码中存在（forward）；代码中的间接调用必须全部在 ground truth 中（backward）
4. **多 agent 并行**：利用多个 agent 并行分析不同分类的 fixture，提高效率

### Decision Drivers
1. 104 个用例的 LLM 分析工作量大，需要多 agent 并行分发
2. 输出格式为按分类汇总的 markdown
3. tree-sitter 解析是唯一的语法检查标准
4. 每个 agent 的分析结果需要统一汇总到一份报告中

### Options

**Option A: 多 agent 并行分析，每个 agent 负责若干分类，最后汇总报告（推荐）**
- Pros: 充分利用并行能力，11 个分类可分发给多个 agent 同时分析，大幅缩短时间
- Cons: 需要协调多个 agent 的结果，确保格式统一；最后需要汇总步骤
- Trade-off: 并行加速 vs 结果汇总的额外复杂度

**Option B: 单 agent 逐个分析所有 104 个用例**
- Pros: 结果格式统一，无需协调
- Cons: 104 个用例串行分析极其耗时，可能超时或 context 溢出

**Option C: 写脚本自动分析语义**
- Pros: 最快
- Cons: 违反用户"不使用脚本进行粗略分析"的要求，脚本无法做精确的语义判断

**Recommendation: Option A** — 多 agent 并行分析，每个 agent 负责 1-2 个分类，最后汇总成一份报告。

## Implementation Steps

### Step 1: tree-sitter 解析检查（自动化）

编写一个临时辅助脚本，遍历所有 104 个 fixture.c，调用 `parse_file()` 尝试解析，收集成功/失败列表。

**文件**: 临时脚本（不入库），使用 `ethunter.parser.ast_builder.parse_file`

逻辑：
```python
from ethunter.parser.ast_builder import parse_file
# 遍历 tests/benchmark/cg_bench/*/{example_N}/fixture.c
# 对每个文件调用 parse_file()，捕获异常
# 关键：检查 tree.root_node.has_error，而不仅仅是是否抛异常
# tree-sitter 对无效代码也会返回 tree（含 error nodes），必须检查 has_error
# 输出成功/失败列表 + 每类解析结果汇总
```

将解析结果写入 shared memory，供后续分析 agent 使用。

### Step 2: 分发多 agent 并行分析（核心步骤）

将 11 个分类分发给多个 general-purpose agent，每个 agent 负责 1-2 个分类。每个 agent 的任务：

1. 读取其所负责分类下所有 example 的 fixture.c 代码和 ground_truth.json
2. 读取 Step 1 的 tree-sitter 解析结果
3. 对每个用例进行逐行人工语义分析：
   - 确认注释中的 fnptr 变量名和 targets 列表与实际代码语义一致
   - 对 ground_truth.json 中的每条边 (caller, callee)：
     a. 在代码中找到 caller 函数的定义
     b. 追踪函数指针的赋值链：哪个变量被赋值为 callee 的地址？
     c. 确认 caller 通过该函数指针变量间接调用了 callee（而非直接调用 `callee()`）
     d. 标记结果：FOUND-INDIRECT / NOT FOUND / FOUND-BUT-DIRECT (误标)
   - 扫描代码中所有函数指针变量（全局数组、全局 struct、局部变量、参数传递等）：
     a. 确认每个函数指针变量的所有赋值 targets
     b. 确认每个间接调用点 (ptr(), arr[i](), struct.fp(), etc.)
     c. 标记结果：COVERED (在 ground truth 中) / MISSING (未记录)
   - 特别注意：direct call 不应出现在 ground truth 中
4. 输出该分类的分析结果（markdown 格式片段）

**分类分配方案**（11 个分类，建议 5-6 个 agent 并行）：

| Agent | 分类 | 用例数 |
|-------|------|--------|
| Agent 1 | fnptr-library (20) + fnptr-varargs (1) | 21 |
| Agent 2 | fnptr-struct (14) | 14 |
| Agent 3 | fnptr-only (12) + fnptr-virtual (1) | 13 |
| Agent 4 | fnptr-global-struct-array (12) | 12 |
| Agent 5 | fnptr-callback (15) | 15 |
| Agent 6 | fnptr-global-struct (11) + fnptr-global-array (6) + fnptr-cast (7) + fnptr-dynamic-call (5) | 29 |

**Agent prompt 模板**：

每个 agent 收到如下指令：
- 读取指定分类下所有 example 的 fixture.c 和 ground_truth.json
- 逐用例阅读代码，按下方分析模板输出每个用例的分析结果
- 将分析结果写入 shared memory（key: `cg_bench_result_{category}`）

**每用例分析模板**：
```
### example_N
- **Syntax**: PASS / FAIL (error detail if FAIL)
- **Comment header**: `/* fnptr: <var>, targets: <list> */`
- **Comment matches code**: YES / NO (detail)
- **Forward check**:
  - caller_X → callee_Y: FOUND-INDIRECT / NOT FOUND / FOUND-BUT-DIRECT
  - ...
- **Backward check**:
  - indirect call via <var>: COVERED / MISSING
  - ...
- **Verdict**: PASS / FAIL-SEMANTIC / FAIL-SYNTAX / FAIL-BOTH
- **Issues**: (none / list)
```

### Step 3: 汇总报告

等待所有 agent 完成后，从 shared memory 读取各分类的分析结果，按分类组织合并为一份完整的 markdown 报告。

报告结构：
```markdown
# CG-Bench Fixture Correctness Report

## Summary
- Total fixtures: 104
- Syntax PASS: N / FAIL: N
- Semantic PASS: N / FAIL: N
- Issues found: N (list)

## Category: fnptr-callback (15 examples)
... (Agent 5's output)

## Category: fnptr-library (20 examples)
... (Agent 1's output)

... (remaining categories)
```

### Step 4: 验证报告完整性

抽查 2-3 个用例，对比报告中的分析结果与 ground truth 和代码的一致性，确认分析质量。

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| 104 个用例人工分析耗时过长 | tree-sitter 解析部分自动化，报告骨架自动生成，人工只做核心语义判断 |
| 某些 fixture.c 依赖 .h 文件但不在 example 目录中 | scanner 已处理跨文件解析，但需确认每个 example 目录自包含 |
| ground_truth.json 中可能存在直接调用被误标为间接调用 | 人工验证时会标记此类问题 |
| 间接调用判断标准模糊（如通过函数指针数组调用） | 参照 analyzer 模块的 indirect_kind 定义作为判断依据 |
| 临时辅助脚本可能有 bug 导致解析检查结果不可靠 | 抽查 3-5 个 fixture 手动验证脚本结果；脚本只负责机械性解析检查 |

## Verification Steps

1. 运行 `test_cg_bench.py` 确认所有 fixture 当前能被 tree-sitter 解析
2. 抽查 2-3 个已分析用例，对比分析结果与 ground truth 和代码的一致性
3. 最终报告完整性检查：104 个用例全部有记录

## ADR

**Decision**: 采用 Option A — 自动化 tree-sitter 解析 + 多 agent 并行 LLM 语义分析 + 汇总报告

**Drivers**: 104 个用例纯串行分析耗时过长；用户要求 LLM 阅读代码做精确语义判断（非脚本粗略分析）；多 agent 并行可大幅缩短总时间

**Alternatives considered**:
- Option B (单 agent 串行)：结果一致性好但效率极低，可能超时
- Option C (全自动脚本分析语义)：违反用户"不使用脚本进行粗略分析"的要求

**Why chosen**: 在保持 LLM 精确语义判断的前提下，通过多 agent 并行最大化分析吞吐量

**Consequences**:
- 需要协调多个 agent 的分析结果，确保格式统一
- shared memory 用于 agent 间传递结果
- 最后需要汇总步骤整合报告

**Follow-ups**:
- 分析完成后，讨论是否需要修复发现的问题
- 考虑将验证逻辑整合到测试框架中作为长期改进

## Changelog

- Initial plan created from deep-interview spec
