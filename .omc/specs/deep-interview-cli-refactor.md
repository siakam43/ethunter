# Deep Interview Spec: CLI Refactor --analyze / --from-json Split

## Metadata
- Interview ID: find-entry-opt-002
- Rounds: 4
- Final Ambiguity Score: 10%
- Type: brownfield
- Generated: 2026-05-08
- Threshold: 20%
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.95 | 0.35 | 0.3325 |
| Constraint Clarity | 0.9 | 0.25 | 0.225 |
| Success Criteria | 0.9 | 0.25 | 0.225 |
| Context Clarity | 0.95 | 0.15 | 0.1425 |
| **Total Clarity** | | | **0.925** |
| **Ambiguity** | | | **0.075** |

## Goal

Refactor the ethunter CLI to have a strict two-mode split:

1. **Analyze mode** (`--analyze`): Analyze a C project directory and output JSON call graph
   ```
   ethunter --analyze /path/to/project [-o graph.json]
   ```

2. **Query mode** (`--from-json`): Load a pre-generated JSON call graph and perform operations
   ```
   ethunter --from-json graph.json --query my_function
   ethunter --from-json graph.json --to-dot
   ethunter --from-json graph.json --find-entry
   ```

The `--analyze` and `--from-json` flags are mutually exclusive. `--query`, `--to-dot`, and `--find-entry` can ONLY be used with `--from-json` — attempting to use them with `--analyze` should produce an error message and exit(1).

## Constraints

- Remove the positional `project_dir` argument entirely
- `--analyze` takes a directory path as its value
- `--analyze` and `--from-json` are mutually exclusive
- `--query`, `--to-dot`, `--find-entry` are only valid with `--from-json`
- Without `--from-json`, `--query`, `--to-dot`, `--find-entry` produce an error message and exit(1)
- `-o` works with both modes

## Non-Goals

- No backward compatibility with the old positional `project_dir` argument

## Acceptance Criteria

- [ ] `--analyze` flag added, takes project directory path
- [ ] `--analyze` and `--from-json` mutually exclusive (error + exit(1) if both used)
- [ ] `--query`, `--to-dot`, `--find-entry` require `--from-json` (error + exit(1) if used without)
- [ ] Positional `project_dir` argument removed
- [ ] `ethunter --analyze /path/to/project` produces JSON output (same as before)
- [ ] `ethunter --from-json graph.json --find-entry` works (from previous feature)
- [ ] `ethunter --from-json graph.json --query NAME` works
- [ ] `ethunter --from-json graph.json --to-dot` works
- [ ] All existing tests updated and passing
- [ ] CLAUDE.md updated with new CLI usage examples

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| project_dir positional arg should be kept | Asked if it should be removed | Removed entirely, use --analyze instead |
| --analyze without -o should output JSON | Implicit from existing behavior | Confirmed: same behavior, just different flag |
| Tests need updating | Asked about backward compat | All tests updated to use --analyze |

## Technical Context

### Current CLI (`src/ethunter/cli.py`)

```
parser.add_argument('project_dir', nargs='?', ...)         # positional
parser.add_argument('--from-json', metavar='FILE', ...)    # load JSON
parser.add_argument('--query', metavar='FUNC_NAME', ...)   # query function
parser.add_argument('--to-dot', action='store_true', ...)  # DOT output
parser.add_argument('--find-entry', action='store_true', ...) # find entry points
parser.add_argument('--output', '-o', metavar='FILE', ...) # output file
```

Current flow:
- If `project_dir` provided → full analysis pipeline → JSON output (or --query/--to-dot on fresh graph)
- If `--from-json` provided → load JSON → dispatch to --query/--to-dot/--find-entry/output

### New CLI Design

```
parser.add_argument('--analyze', metavar='DIR', ...)       # analyze project
parser.add_argument('--from-json', metavar='FILE', ...)    # load JSON
parser.add_argument('--query', metavar='FUNC_NAME', ...)   # query function
parser.add_argument('--to-dot', action='store_true', ...)  # DOT output
parser.add_argument('--find-entry', action='store_true', ...) # find entry points
parser.add_argument('--output', '-o', metavar='FILE', ...) # output file
```

New flow:
- `--analyze DIR` → full analysis pipeline → JSON output (only)
- `--from-json FILE` → load JSON → dispatch to --query/--to-dot/--find-entry/output
- `--analyze` + any of `--query`/`--to-dot`/`--find-entry` → error
- `--analyze` + `--from-json` → error

### Affected Files

- `src/ethunter/cli.py` — main CLI logic (argparse + dispatch)
- `tests/test_query_json.py` — uses subprocess CLI calls, needs --analyze update
- `CLAUDE.md` — CLI usage examples need updating

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| AnalyzeMode | core domain | project_dir, output | Generates CallGraph JSON |
| FromJsonMode | core domain | json_file, operations | Loads CallGraph, dispatches to query/to-dot/find-entry |
| QueryOperation | core domain | func_name | Requires FromJsonMode |
| ToDotOperation | core domain | (none) | Requires FromJsonMode |
| FindEntryOperation | core domain | (none) | Requires FromJsonMode |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 1 | 5 | 5 | - | - | N/A |
| 4 | 5 | 0 | 0 | 5 | 100% |

## Interview Transcript
<details>
<summary>Full Q&A (4 rounds)</summary>

### Round 1
**Q:** 当用户提供project_dir（不带--from-json）时，CLI的行为是什么？是只生成JSON输出，还是仍然保持现在的行为？
**A:** Strict split: project_dir只输出JSON，--query/--to-dot/--find-entry只能和--from-json搭配
**Ambiguity:** 50% (Goal: 0.7, Constraints: 0.6, Criteria: 0.7, Context: 0.8)

### Round 2
**Q:** 当用户同时使用project_dir和--query/--to-dot/--find-entry（无--from-json）时CLI应该怎么做？
**A:** 添加--analyze参数与--from-json互斥。--analyze后接project_dir。--query/--to-dot/--find-entry只能和--from-json搭配，算是--from-json的子命令
**Ambiguity:** 35% (Goal: 0.8, Constraints: 0.75, Criteria: 0.8, Context: 0.85)

### Round 3
**Q:** CLI设计：去掉位置参数project_dir还是保留两者？
**A:** 去掉project_dir位置参数，只用--analyze
**Ambiguity:** 20% (Goal: 0.9, Constraints: 0.85, Criteria: 0.85, Context: 0.9)

### Round 4
**Q:** 现有测试需要更新为新的--analyze格式吗？
**A:** 更新全部测试用例，测试通过后，更新CLAUDE.md
**Ambiguity:** 10% (Goal: 0.95, Constraints: 0.9, Criteria: 0.9, Context: 0.95)
</details>
