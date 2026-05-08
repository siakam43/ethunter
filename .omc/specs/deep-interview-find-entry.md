# Deep Interview Spec: --find-entry (Uncalled Functions)

## Metadata
- Interview ID: uncalled-funcs-001
- Rounds: 6
- Final Ambiguity Score: 14%
- Type: brownfield
- Generated: 2026-05-08
- Threshold: 20%
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.9 | 0.35 | 0.315 |
| Constraint Clarity | 0.85 | 0.25 | 0.2125 |
| Success Criteria | 0.9 | 0.25 | 0.225 |
| Context Clarity | 0.9 | 0.15 | 0.135 |
| **Total Clarity** | | | **0.86** |
| **Ambiguity** | | | **0.14** |

## Goal

Add a new CLI flag `--find-entry` to `ethunter.cli` that, given a loaded call graph (via `--from-json`), finds all functions that:
1. Have a function implementation (`is_definition=True`)
2. Never appear as `callee` in any `CallEdge` (strict: zero incoming edges)

Output the results as a JSON file with the structure:
```json
{
  "uncalled_functions": [
    {"name": "func_name", "file": "path/to/file.c", "line": 42}
  ]
}
```

## Constraints

- The `--find-entry` flag is mutually exclusive with `--query` and `--to-dot`
- Only works when a call graph is available (either from `--from-json` or from full analysis)
- Output format is JSON only, written via `-o` flag or stdout

## Non-Goals

- No support for combining `--find-entry` with `--query` or `--to-dot`
- No summary/statistics in the output (just the function list)
- No changes to the analysis pipeline — this is purely a graph traversal feature

## Acceptance Criteria

- [ ] `--find-entry` CLI flag is registered in argparse
- [ ] `--find-entry` is mutually exclusive with `--query` and `--to-dot` (error + exit(1))
- [ ] When used with `--from-json`, correctly loads the graph and finds uncalled functions
- [ ] Filter: only functions with `is_definition=True` are considered
- [ ] Filter: only functions that never appear as `callee` in any edge
- [ ] Output JSON structure: `{"uncalled_functions": [{"name", "file", "line"}]}`
- [ ] Supports `-o` output flag to write to file
- [ ] Also works without `--from-json` (runs full analysis pipeline first, then finds entries)
- [ ] Tests exist for the new functionality

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| "Not called" could mean different things | Asked for strict vs entry-point definition | Strict: never appears as callee in any edge |
| Output was originally "apis.txt" | Asked about format since JSON was requested | Output is JSON, filename controlled by `-o` flag |
| CLI flag name was unspecified | Asked for specific name | `--find-entry` |
| Summary stats might be needed | Asked if summary needed | No summary, just the function list |

## Technical Context

### Relevant Codebase Findings

- **`src/ethunter/cli.py`**: CLI entry point with argparse. Current flow:
  - Lines 31-42: Argument parsing (`--from-json`, `--query`, `--to-dot`, `-o`)
  - Lines 51-82: `--from-json` branch — loads JSON, dispatches to `--to-dot` or `--query`
  - Lines 72-74: Mutual exclusion check for `--query` and `--to-dot`
  - Lines 83-123: Full analysis pipeline branch

- **`src/ethunter/graph/model.py`**: Data model
  - `Function`: `name`, `file`, `line`, `is_definition`, `key` (line 15-27)
  - `CallEdge`: `caller`, `callee`, `caller_file`, `callee_file` (line 31-53)
  - `CallGraph`: `functions` dict, `edges` list, `query_callers()`, `query_callees()` (line 56-73)

### Implementation Approach

Add a new function `_find_entry_points(graph)` in `cli.py` that:
1. Collects all `callee` values from `graph.edges` into a set
2. Iterates `graph.functions.values()`, filtering `is_definition=True` and `name not in callees`
3. Returns list of `{"name": f.name, "file": f.file, "line": f.line}`

In `main()`:
1. Add `--find-entry` argparse argument
2. Extend mutual exclusion check to include `--find-entry`
3. Add dispatch in both `--from-json` and full analysis branches

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| Function | core domain | name, file, line, is_definition, signature, return_type, parameters | Has zero or more incoming CallEdges; has zero or more outgoing CallEdges |
| CallEdge | core domain | caller, callee, caller_file, callee_file, type, indirect_kind, caller_line | Links a caller Function to a callee Function |
| CallGraph | core domain | functions (dict), edges (list), source_files | Contains Functions and CallEdges |
| UncalledFunction | derived | name, file, line | A Function with is_definition=True and zero incoming CallEdges |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 1 | 4 | 4 | - | - | N/A |
| 6 | 4 | 0 | 0 | 4 | 100% |

## Interview Transcript
<details>
<summary>Full Q&A (6 rounds)</summary>

### Round 1
**Q:** 当你说"没有被调用过"，你指的是哪种定义？Strict（任何CallEdge中都没有作为callee出现）还是 Entry-point（不被其他项目函数调用，但可能被回调引用）？
**A:** Strict — 任何CallEdge中都没有作为callee出现
**Ambiguity:** 75% (Goal: 0.7, Constraints: 0.6, Criteria: 0.7, Context: 0.8)

### Round 2
**Q:** 输出到 apis.txt 文件的格式是什么？每行只包含函数名，还是需要包含文件路径、行号等额外信息？
**A:** JSON 格式，包含函数名、文件、行号
**Ambiguity:** 60% (Goal: 0.8, Constraints: 0.6, Criteria: 0.8, Context: 0.8)

### Round 3
**Q:** CLI 参数名叫什么？和 --query、--to-dot 并列的参数叫什么？
**A:** --find-entry
**Ambiguity:** 50% (Goal: 0.85, Constraints: 0.7, Criteria: 0.8, Context: 0.85)

### Round 4
**Q:** JSON 中包含 summary 统计信息吗？还是只包含 uncalled 函数列表？
**A:** 只包含 uncalled 函数列表，key 为 uncalled_functions
**Ambiguity:** 35% (Goal: 0.9, Constraints: 0.75, Criteria: 0.85, Context: 0.85)

### Round 5
**Q:** --find-entry 是否需要和已有的 --query、--to-dot 互斥？
**A:** 和 --query、--to-dot 三者互斥
**Ambiguity:** 25% (Goal: 0.9, Constraints: 0.8, Criteria: 0.85, Context: 0.9)

### Round 6
**Q:** "有函数实现" 是否指 is_definition==True？
**A:** 只输出 is_definition=True 的函数
**Ambiguity:** 14% (Goal: 0.9, Constraints: 0.85, Criteria: 0.9, Context: 0.9)
</details>
