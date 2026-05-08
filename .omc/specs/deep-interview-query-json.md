# Deep Interview Spec: 优化 Query 功能 - 基于 JSON 文件查询

## Metadata
- Interview ID: `query-json-20260508`
- Rounds: 4
- Final Ambiguity Score: 14.5%
- Type: brownfield
- Generated: 2026-05-08
- Threshold: 0.2
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.95 | 35% | 0.333 |
| Constraint Clarity | 0.85 | 25% | 0.213 |
| Success Criteria | 0.7 | 25% | 0.175 |
| Context Clarity | 0.9 | 15% | 0.135 |
| **Total Clarity** | | | **0.855** |
| **Ambiguity** | | | **14.5%** |

## Goal

为 ethunter 的 `--query` 功能增加基于预生成 JSON 文件的查询模式，避免每次查询都重新分析整个 C 项目。同时移除 `--format` 参数，默认输出 JSON，新增 `--to-dot` 参数实现 JSON→DOT 格式转换（不重新分析代码）。

## CLI 交互定义

三条独立路径：

```bash
# 路径 1: 分析 C 项目，默认输出 JSON
ethunter project_dir

# 路径 2: 基于已有 JSON 文件查询调用关系
ethunter --from-json graph.json --query FUNC_NAME

# 路径 3: 基于已有 JSON 文件转换为 DOT 格式
ethunter --from-json graph.json --to-dot
```

## Constraints

- `--from-json` 参数接受一个 JSON 文件路径，文件必须是之前 `ethunter project_dir` 生成的 call graph JSON
- 如果指定 `--from-json`，则跳过 scan→parse→analyze 全流程，直接从 JSON 反序列化 CallGraph
- `--from-json` 与 `--query` 配合使用时输出查询结果（调用者/被调用者）
- `--from-json` 与 `--to-dot` 配合使用时输出 DOT 格式的图
- `--from-json` 是查询/转换模式的必选参数，必须预先生成 call graph JSON
- `--format` 参数完全移除，不再接受
- `--query` 参数语义不变，仍然查询调用者/被调用者
- `--output` / `-o` 参数行为不变，写入文件而非 stdout
- 现有 `ethunter project_dir`（无 `--from-json`）行为不变，仍输出 JSON

## Non-Goals

- 不实现 JSON 增量更新（重新分析后合并）
- 不改变现有分析器逻辑或 call graph 结构
- 不实现 DOT 转 JSON（只支持 JSON 转 DOT）
- 不改变查询语法或增加新的查询类型

## Acceptance Criteria

- [ ] `ethunter --from-json graph.json --query FUNC_NAME` 正确返回调用者/被调用者信息
- [ ] `ethunter --from-json graph.json --to-dot` 输出正确的 DOT 格式
- [ ] 传入无效 JSON 文件时给出清晰的错误提示
- [ ] 查询结果与原 `--query` 模式结果一致
- [ ] DOT 输出与原 `--format dot` 模式结果一致
- [ ] 新增 3 个测试用例：query from json、to-dot、无效 JSON 错误处理
- [ ] 现有测试 `test_analyzers.py` 全部通过
- [ ] 手动验证 benchmark/cjson 的查询结果正确性

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| 保留 `--format` 向后兼容 | 移除 `--format` 更干净，但会破坏现有脚本 | 完全移除，干净重构 |
| `--format` 内部从 JSON 转换 | 不需要 --format，用独立 `--to-dot` 参数更清晰 | 新增 `--to-dot` |
| 需要大量 benchmark 对比测试 | 当前只需测试 + 手动验证 | 仅测试 + 手动验证 |

## Technical Context

当前 `cli.py` 的 5 阶段流程：
1. scan_files → 发现 .c/.h
2. parse_file → tree-sitter ASTs
3. SymbolTable + VariableState → 符号表
4. run_all_analyses → CallGraph
5. query / output

需要修改的核心文件：
- `src/ethunter/cli.py` — 主入口，新增 --from-json 和 --to-dot，移除 --format
- 新增 `src/ethunter/output/from_json.py` 或直接在 cli.py 中反序列化（需实现 `CallGraph.from_dict()`）
- `src/ethunter/graph/model.py` — 可能需要添加 `CallGraph.from_dict()` 静态方法

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| CallGraph | core domain | functions, edges, source_files | contains CallEdge, Function |
| CallEdge | core domain | caller, callee, type, indirect_kind | links two Function instances |
| Function | core domain | name, file, line, signature | referenced by CallEdge |
| JSON File | external system | file path, content | serialized CallGraph |
| CLI Arguments | supporting | --from-json, --query, --to-dot, --output | drives the execution path |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 1 | 5 | 5 | - | - | N/A |
| 2 | 5 | 0 | 0 | 5 | 100% |
| 3 | 5 | 0 | 0 | 5 | 100% |
| 4 | 5 | 0 | 0 | 5 | 100% |

Ontology 在第一轮后已完全收敛。

## Interview Transcript
<details>
<summary>Full Q&A (4 rounds)</summary>

### Round 1
**Q:** 你更倾向于哪种交互方式？（A: 新增独立参数 vs 子命令 vs 改造 --query 自动检测）
**A:** 新增独立参数 --from-json，与 --query FUNC 配合使用。且新增参数为必选参数。意思是必须预先生成callgraph，再进行查询操作。同理，我希望--format功能也进行优化。不需要指定输出格式，默认必须输出json格式。但增加一个格式转换功能。将json转化成dot，而不是重新分析代码生成DOT
**Ambiguity:** 62% (Goal: 0.8, Constraints: 0.3, Criteria: 0.2, Context: 0.9)

### Round 2
**Q:** 现有用户可能已经在脚本中使用了 `--format dot`，你希望如何处理向后兼容？
**A:** 完全移除 --format
**Ambiguity:** 37.5% (Goal: 0.9, Constraints: 0.5, Criteria: 0.2, Context: 0.9)

### Round 3
**Q:** JSON 转 DOT 的 CLI 交互你更倾向哪种？
**A:** --to-dot 参数 (Recommended)
**Ambiguity:** 25% (Goal: 0.9, Constraints: 0.85, Criteria: 0.2, Context: 0.9)

### Round 4
**Q:** 你希望如何验证这个改动是正确的？
**A:** 仅测试 + 手动验证 (Recommended)
**Ambiguity:** 14.5% (Goal: 0.95, Constraints: 0.85, Criteria: 0.7, Context: 0.9)
</details>
