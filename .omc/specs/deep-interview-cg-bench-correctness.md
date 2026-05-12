# Deep Interview Spec: CG-Bench Fixture.c 语法正确性修复

## Metadata
- Interview ID: cg-bench-fix-syntax-20260509
- Rounds: 5
- Final Ambiguity Score: 12%
- Type: brownfield
- Generated: 2026-05-09
- Threshold: 0.2
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.9 | 0.40 | 0.36 |
| Constraint Clarity | 0.9 | 0.30 | 0.27 |
| Success Criteria | 0.9 | 0.30 | 0.27 |
| **Total Clarity** | | | **0.90** |
| **Ambiguity** | | | **0.10** |

## Goal

修复 `tests/benchmark/cg_bench/` 目录下 54 个存在 tree-sitter parse error 的 fixture.c 文件，使所有 104 个 fixture 都能通过 tree-sitter 无错误解析（0 ERROR 节点）。修复后与 `tests/benchmark/CG-Bench/fnptr-*.md` 原始文档对比验证语义一致性，确保间接调用逻辑（fnptr 变量、targets、caller-callee 关系）未被破坏。

## Constraints

1. **LLM 直接修改**：不使用脚本批量处理，通过 LLM 分析每个 fixture.c 并直接调用 Edit 工具
2. **多 agent 并行**：使用 Claude Code team mode 启动多个 worker agent 并行修复不同类别的 fixture
3. **最小修改原则**：
   - `...` 省略号 → 删除该行或替换为空语句 `;`，不改变周围代码
   - 预处理器 `#if`/`#ifdef` → 删除预处理指令，保留内部有效代码
   - 截断/不完整 → 补充缺失的花括号、函数签名参数
   - C++ 关键字 `class` → 替换为 `struct`
   - `#error` → 删除该行
4. **保护间接调用逻辑**：caller、callee、函数指针赋值/调用关系绝对不可破坏
5. **ground_truth.json 只读**：发现错误时记录到报告中，禁止自动修改
6. **保留冗余**：即使是冗余代码也不删除，保持分析复杂度
7. **验证对比**：修复后与 CG-Bench 原始 md 文档（`tests/benchmark/CG-Bench/fnptr-*.md`）对比确保语义未改变

## Non-Goals

- 不修复 ground_truth.json 中的语义错误（仅记录）
- 不重写或大幅重构 fixture.c
- 不开发自动化修复脚本
- 不修改 test_cg_bench.py 测试逻辑

## Acceptance Criteria

- [ ] 所有 104 个 fixture.c 文件通过 tree-sitter 解析（0 ERROR 节点）
- [ ] 每个修复后的 fixture 与 CG-Bench 原始 md 文档对比，确认间接调用逻辑（fnptr 变量、targets、caller-callee 关系）未改变
- [ ] 运行 `test_cg_bench.py` 确认所有 fixture 都参与了评估（无跳过）
- [ ] 生成一份 ground_truth.json 潜在问题报告
- [ ] 无修复引入新的语法错误

## Assumptions Exposed & Resolved

| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| correctness-report.md 反映当前状态 | 最近有多个修复 commit，报告可能过时 | 完全忽略该报告，重新验证当前实际状态 |
| fixture.c 的语法错误都可以自动修复 | 某些截断代码可能无法在不大幅改写的情况下修复 | 对于确实无法修复的 fixture，记录为不可修复，单独报告 |
| CG-Bench md 文档是权威参考 | md 文档中的代码片段可能与 fixture.c 不完全一致 | md 文档用于语义验证（fnptr、targets 是否一致），不要求逐行对齐 |

## Technical Context

### 当前状态（实时验证）
- 104 fixtures: 50 PASS (48.1%), 54 FAIL (51.9%)
- 错误分类：
  - `...` 省略号：37 个
  - 预处理器条件：8 个
  - 截断/不完整：6 个
  - C++ 关键字：2 个
  - `#error` 指令：1 个

### 修复分布（按类别/错误类型）

**cpp_keyword (2)**:
- fnptr-global-struct/example_9
- fnptr-virtual/example_1

**error_directive (1)**:
- fnptr-global-struct/example_8

**ellipsis (37)**:
- fnptr-global-array: example_3,4,5,6
- fnptr-global-struct: example_2,4,5,6,7
- fnptr-global-struct-array: example_2,3,4,6
- fnptr-library: example_2,3,4,5,6,7,8,9
- fnptr-only: example_2,3,5,6,7,8,9,10,11
- fnptr-struct: example_2,3,4,5,6,7
- fnptr-varargs: example_1

**preprocessor (8)**:
- fnptr-global-struct-array: example_7,10,11
- fnptr-library: example_11,15,16
- fnptr-only: example_12
- fnptr-struct: example_14

**truncated (6)**:
- fnptr-callback: example_6,14
- fnptr-global-struct: example_1
- fnptr-global-struct-array: example_5
- fnptr-library: example_14,18

### 参考文档
- `tests/benchmark/CG-Bench/fnptr-*.md` — 11 个类别的原始代码片段来源

### 项目环境
- Python 3.11 via `.venv/bin/python`
- `PYTHONPATH=src` 用于 CLI 运行
- tree-sitter-c 作为 C 语言解析器

## Interview Transcript

<details>
<summary>Full Q&A (5 rounds)</summary>

### Round 1
**Q:** 你希望检查的正确性是指哪部分代码？1. fixture.c语法 2. ground_truth.json语义 3. test_cg_bench.py逻辑 4. ethunter检测能力
**A:** 1 — fixture.c 语法正确性
**Ambiguity:** 73% (Goal: 0.8, Constraints: 0.3, Criteria: 0.2)

### Round 2
**Q:** 修复策略：最小修改/中等修改/激进修改？
**A:** 最小修改，保留代码结构，不破坏间接调用逻辑，ground_truth.json只读不改，不删除代码保留冗余
**Ambiguity:** 52% (Goal: 0.9, Constraints: 0.7, Criteria: 0.4)

### Round 3
**Q:** 怎样才算'修复完成'？
**A:** 100% 语法通过
**Ambiguity:** 26% (Goal: 0.9, Constraints: 0.7, Criteria: 0.9)

### Round 4
**Q:** 修复 fixture.c 语法时如果发现 ground_truth.json 确实有错误，应该如何处理？
**A:** 仅记录，不处理
**Ambiguity:** 18% (Goal: 0.9, Constraints: 0.9, Criteria: 0.9)

### Round 5
**Q:** 修复策略：先验证当前状态还是直接基于报告修复？
**A:** 先验证后修复，完全忽略correctness-report.md
**Ambiguity:** 12% (Goal: 0.9, Constraints: 0.9, Criteria: 0.9)

</details>
