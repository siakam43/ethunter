# Deep Interview Spec: CG-Bench to ethunter Benchmark

## Metadata
- Interview ID: cg-bench-test-conversion
- Rounds: 8
- Final Ambiguity Score: 14%
- Type: brownfield
- Generated: 2026-05-08T13:10:00Z
- Threshold: 0.2
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.9 | 0.35 | 0.315 |
| Constraint Clarity | 0.85 | 0.25 | 0.2125 |
| Success Criteria | 0.8 | 0.25 | 0.2 |
| Context Clarity | 0.9 | 0.15 | 0.135 |
| **Total Clarity** | | | **0.8625** |
| **Ambiguity** | | | **14%** |

## Goal

Convert all 104 CG-Bench examples into a benchmark-style evaluation harness for ethunter. Each example becomes a directory with C fixture files preserving the original project's complexity. One ground_truth.json per category records expected indirect call edges. A pytest benchmark test runs ethunter on each fixture, compares detected edges against ground truth, and reports recall ratios per category and overall (measurement only, no pass/fail threshold).

## Constraints

- Each CG-Bench example goes into `tests/benchmark/cg_bench/<category>/example_xxx/` directory
- Preserve original code complexity and cross-file structure where possible — do NOT oversimplify into single .c files unless necessary
- Trim code unrelated to indirect calls, but preserve indirect-call-related code as-is
- ground_truth.json per category is generated from `cgbench.json` (not manual markdown parsing)
- Test modules should NOT map 1:1 to analyzers — evaluate ethunter's overall indirect call detection capability
- Report recall ratio only, no pass/fail threshold

## Non-Goals

- Creating new analyzer modules (only evaluate existing ones)
- Adding pass/fail assertions to the benchmark
- Converting examples that don't involve indirect calls (e.g., pure direct call patterns)

## Acceptance Criteria

- [ ] All 104 CG-Bench examples have fixture directories under `tests/benchmark/cg_bench/<category>/example_xxx/`
- [ ] 11 ground_truth.json files exist (one per CG-Bench category) containing expected indirect edges
- [ ] A pytest benchmark test module exists that runs ethunter on each fixture and reports recall
- [ ] `pytest tests/` runs without errors on the new test module
- [ ] Recall is reported per category and overall

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| 1:1 mapping to analyzers | ethunter's analyzer set is incomplete | Benchmark evaluates overall capability, not per-analyzer |
| Individual fixture files | User wants to preserve real-project complexity | Directory-per-example layout with cross-file support |
| Pass/fail assertions | User wants measurement-first approach | Report recall only, no threshold |
| Manual data extraction | cgbench.json already has structured data | Parse cgbench.json as source of truth |

## Technical Context

**ethunter**: Static analysis tool for C call graph generation. Uses tree-sitter to parse C projects. 13 analyzer modules detect direct and indirect call patterns. Test infrastructure: `tests/fixtures/` (single-file), `tests/fixtures/cross_file/` (multi-file), `tests/test_benchmark.py` (recall-based benchmark).

**CG-Bench**: 104 annotated indirect call examples from 7 projects (openssh, curl, redis, zfs, wrk, ffmpeg, gcc) across 11 categories. Data in `cgbench.json` (structured) and `fnptr-*.md` (markdown with code snippets).

**11 CG-Bench categories**:
| Category | Count | Maps to ethunter |
|----------|-------|-----------------|
| fnptr-callback | 15 | callback_param, callback_reg |
| fnptr-global-array | 6 | fp_array |
| fnptr-global-struct | 11 | vtable, typedef_fp |
| fnptr-struct | 14 | vtable |
| fnptr-only | 12 | fp_assign |
| fnptr-cast | 7 | (no direct mapping) |
| fnptr-global-struct-array | 12 | fp_array |
| fnptr-library | 20 | (callback-style) |
| fnptr-dynamic-call | 5 | dlsym_fp |
| fnptr-varargs | 1 | (no direct mapping) |
| fnptr-virtual | 1 | vtable |

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| CG-Bench examples | core domain | category, callsite, fnptr, targets, code_snippets | Belongs to a category, has ground truth targets |
| Fixture directories | supporting | path, .c/.h files, category | Contains C code extracted from CG-Bench examples |
| ground_truth.json | supporting | category, expected_edges, caller, callee, indirect_kind | One per category, generated from cgbench.json |
| ethunter analyzers | external system | module_name, indirect_kind, detection_logic | Detects indirect call patterns in fixture files |
| Benchmark test module | supporting | pytest, recall computation, per-category reporting | Runs ethunter, compares vs ground_truth.json |
| cgbench.json | external system | project, callsite, type, chain_summary, callees | Source data for ground_truth.json generation |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 4 | 3 | 3 | - | - | - |
| 5 | 4 | 1 | - | 3 | 75% |
| 6 | 4 | 0 | - | 4 | 100% |
| 7 | 6 | 2 | - | 4 | 67% |
| 8 | 6 | 0 | - | 6 | 100% |

Ontology converged by round 8 — same 6 entities appeared in consecutive rounds with no changes.

## Interview Transcript
<details>
<summary>Full Q&A (8 rounds)</summary>

### Round 1
**Q:** CG-Bench has 104 examples across 11 categories. How many and which examples do you want converted into ethunter test cases?
**A:** All 104 examples exhaustively.
**Ambiguity:** 71% (Goal: 0.8, Constraints: 0.3, Criteria: 0.3)

### Round 2
**Q:** How should each CG-Bench example be converted? Individual fixture files + pytest, benchmark-style ground_truth.json, or both?
**A:** Individual fixture files + pytest functions.
**Ambiguity:** 52% (Goal: 0.85, Constraints: 0.6, Criteria: 0.3)

### Round 3
**Q:** For each of the 104 converted test cases, how should we validate correctness? Assert exact targets, or just create fixtures and see what passes?
**A:** Compare against ground truth, record recall.
**Ambiguity:** 43% (Goal: 0.85, Constraints: 0.65, Criteria: 0.7)

### Round 4 (Contrarian Mode)
**Q:** Some CG-Bench categories (fnptr-cast, fnptr-varargs) don't map to any existing ethunter analyzer. Should we create fixtures without assertions for unmapped categories, or write new analyzers?
**A:** 新的测试用例不应该和analyzer一一对应，因为目前ethunter的analyzer种类还不全，以后会补充。我们应该把新的测试用例从Fixtures形式改为benchmark-style，ground_truth可以仅关注间接调用和函数指针相关的edges。评估ethunter对这些关键edges的识别率。
**Ambiguity:** 54% (Goal: 0.6, Constraints: 0.5, Criteria: 0.5)

### Round 5
**Q:** CG-Bench examples reference real code from openssh/curl/redis/zfs/etc. — we can't run ethunter on those directly. Should we extract simplified C fixture files, then create a ground_truth.json?
**A:** One ground_truth.json per category.
**Ambiguity:** 46% (Goal: 0.75, Constraints: 0.6, Criteria: 0.5)

### Round 6 (Simplifier Mode)
**Q:** What should the benchmark test actually do with the recall numbers? Report them, fail below a threshold, or per-example pass/fail?
**A:** Report recall ratio only, no pass/fail.
**Ambiguity:** 38% (Goal: 0.75, Constraints: 0.65, Criteria: 0.75)

### Round 7
**Q:** CG-Bench examples contain code snippets from real projects. Should we create minimal self-contained fixtures, preserve original code structure, or no C files at all?
**A:** 综合方案1和方案2。每个例子不是必须都简化为一个c文件，这会简化代码场景，降低分析难度。我希望尽量保留真实项目的复杂性。因此保留原数据结构和跨文件特性也比较重要。可以每个例子放在一个单独的文件夹中：tests/benchmark/cg_bench/<category>/example_xxx/ 如果例子依赖过多，可以适当裁剪和间接调用不相关的代码。但间接调用相关的部门尽量保持原来的代码样式。
**Ambiguity:** 31% (Goal: 0.85, Constraints: 0.8, Criteria: 0.75)

### Round 8 (Ontologist Mode)
**Q:** CG-Bench already has a structured cgbench.json file. Should this be the primary data source for generating the ground_truth.json files?
**A:** Parse cgbench.json for ground truth.
**Ambiguity:** 14% (Goal: 0.9, Constraints: 0.85, Criteria: 0.8)
</details>
