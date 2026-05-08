# Deep Interview Spec: Enhance Test Coverage for ethunter

## Metadata
- Interview ID: e2393ba8-deep-interview-test-coverage
- Rounds: 5
- Final Ambiguity Score: 18%
- Type: brownfield
- Generated: 2026-05-07T15:00:00Z
- Threshold: 20%
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.90 | 0.35 | 0.315 |
| Constraint Clarity | 0.75 | 0.25 | 0.1875 |
| Success Criteria | 0.85 | 0.25 | 0.2125 |
| Context Clarity | 0.80 | 0.15 | 0.120 |
| **Total Clarity** | | | **0.835** |
| **Ambiguity** | | | **0.165 (16.5%)** |

## Goal
Enhance ethunter test coverage in two areas:
1. **Strengthen existing analyzer tests**: Each of the 13 analyzer modules gets 1-2 complex scenario tests + 2 cross-file combination tests, going beyond the current minimal single-file fixtures.
2. **Add real-project benchmarks**: Two open-source C projects (lightweight + medium) with ground truth call graphs for accuracy validation.

## Constraints
- Use open-source C projects only (no proprietary code)
- Lightweight project: cJSON (~800 LOC, single file, clear call structure)
- Medium project: libuv core module (~5K LOC, has callback registration and function pointer arrays)
- Direct calls must achieve 100% recall (0 false negatives)
- Indirect calls must achieve >=80% recall rate
- Ground truth call graphs generated via manual analysis stored as JSON files, verified by test assertions
- Each analyzer module needs: 1 complex scenario test (multiple FP variables, conditional assignments) + 2 cross-file combination tests

## Non-Goals
- Testing parser/scanner/CLI/query/output modules (user deferred to future work)
- Achieving 100% indirect call precision (conservative over-approximation is acceptable)
- Performance benchmarks (focus is on accuracy, not speed)

## Acceptance Criteria
- [ ] Each of the 13 analyzer modules has at least 3 test cases (existing + 1-2 complex + 2 cross-file)
- [ ] Direct call recall is 100% on both benchmark projects (verified against ground truth)
- [ ] Indirect call recall is >=80% on both benchmark projects (verified against ground truth)
- [ ] All tests pass: pytest returns 0 failures
- [ ] Benchmark JSON ground truth files exist under tests/benchmark/ for both projects
- [ ] Integration test runs full scan on benchmark projects and compares output against ground truth

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| "Test is insufficient" was vague | Asked which categories | User wants complex analyzer tests + benchmarks, not new module coverage |
| Benchmark project choice unknown | Asked for specifics | cJSON (light) + libuv (medium), open-source only |
| Accuracy target undefined | Asked for recall/precision balance | Direct: 100% recall; Indirect: >=80% recall, reduce false positives while maintaining recall |
| Ground truth generation method unknown | Asked for preference | Manual analysis stored as JSON, verified by test assertions |

## Technical Context
- Existing codebase: 13 analyzer modules under src/ethunter/analyzer/, each with a minimal test fixture in tests/fixtures/
- Current test: tests/test_analyzers.py with 16 test cases (1 per analyzer + extras)
- Benchmark directory: tests/benchmark/ (empty)
- Test framework: pytest, Python 3.11
- Parser: tree-sitter-c with shared VariableState dataflow engine

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| Analyzer Module | core domain | name, pattern type, test fixtures | Each needs complex + cross-file tests |
| Cross-File Test | supporting | caller file, callee file, shared symbol | Tests analyzer across .c/.h boundaries |
| Benchmark Project | external system | name, LOC, complexity level | Has ground truth JSON file |
| Ground Truth | supporting | expected edges, direct count, indirect count | Used to validate analyzer output |
| Test Suite | core domain | test cases, assertions, fixtures | Contains all analyzer + benchmark tests |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|-----------------|
| 1 | 3 | 3 | - | - | N/A |
| 2 | 4 | 2 | 1 | 2 | 75% |
| 3 | 5 | 1 | 1 | 3 | 80% |
| 4 | 5 | 0 | 0 | 5 | 100% |
| 5 | 5 | 0 | 0 | 5 | 100% |

Ontology converged at round 4 — all 5 entities stable across 2 consecutive rounds.

## Interview Transcript
<details>
<summary>Full Q&A (5 rounds)</summary>

### Round 1
**Q:** 目前已有 16 个测试覆盖了 13 个分析器模块。但未测试的模块包括：parser 层 (scanner/preprocessor/ast_builder)、CLI、query engine、output (JSON/DOT)、orchestrator。你最想优先补充哪类测试？
**A:** 需要补充两类测试：第一：13个分析器模块的功能测试用例太简单，每个场景再补充一个更复杂场景。同时再补充一些多个场景组合的测试用例。尽量测到跨文件的情况。第二：需要补充真实项目作为benchmark，现在的benchmark文件夹是空的
**Ambiguity:** 57% (Goal: 0.5, Constraints: 0.3, Criteria: 0.3, Context: 0.7)

### Round 2
**Q:** 关于 benchmark 使用的真实项目，你有什么具体要求？是希望用一个知名的开源C项目（如 cJSON、libuv、busybox 等），还是你有具体的内部项目路径可以提供？
**A:** 可以选择一个轻量的项目和一个中等的项目作为benchmark，最好知道实际call graph，方便做准确率验证。项目帮我选择开源项目即可
**Ambiguity:** 42% (Goal: 0.6, Constraints: 0.4, Criteria: 0.4, Context: 0.7)

### Round 3
**Q:** 关于benchmark的准确率验证，你更关注哪个指标？间接调用分析天然会产生过近似——你希望优先保证召回率还是精确率？
**A:** 平衡
**Ambiguity:** 37% (Goal: 0.7, Constraints: 0.5, Criteria: 0.5, Context: 0.8)

### Round 4 (Contrarian)
**Q:** 你提到 "平衡" 召回率和精确率，但平衡的阈值是多少？例如：直接调用必须 100% 召回，间接调用要求至少 60% 召回率？每个分析器模块 "更复杂" 的测试需要多少个才算达标？
**A:** 直接调用需要100%召回，间接调用至少80%以上召回率，在保证召回率的前提下，降低误报即可。每个分析器模块新增 1-2 个复杂场景测试 + 2 个跨文件组合测试；benchmark 包含 2 个项目，1个轻量项目，1个中等项目，ground truth 由手动分析生成
**Ambiguity:** 28% (Goal: 0.85, Constraints: 0.6, Criteria: 0.75, Context: 0.8)

### Round 5
**Q:** cJSON（~800 LOC）+ libuv（~5K LOC）作为benchmark，ground truth 用手动分析+脚本辅助生成，每个分析器1个复杂场景+2个跨文件组合测试。你倾向哪种方式？
**A:** 这个方案可行
**Ambiguity:** 16.5% (Goal: 0.9, Constraints: 0.75, Criteria: 0.85, Context: 0.8)

</details>
