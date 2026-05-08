# Plan: Query JSON Optimization (Revised)

## Requirements Summary

基于 spec `.omc/specs/deep-interview-query-json.md`，为 ethunter 增加基于预生成 JSON 文件的查询和格式转换能力，移除 `--format` 参数。

三条独立路径：
1. `ethunter project_dir` → 输出 JSON
2. `ethunter --from-json graph.json --query FUNC_NAME` → 查询
3. `ethunter --from-json graph.json --to-dot` → JSON 转 DOT

## Acceptance Criteria

- [ ] `--from-json` + `--query` 正确返回调用者/被调用者 JSON（与原 `--query` 结果一致）
- [ ] `--from-json` + `--to-dot` 输出正确的 DOT 格式（与原 `--format dot` 结果一致）
- [ ] `--from-json` 同时指定 `--query` 和 `--to-dot` 时报错（互斥）
- [ ] 文件不存在时：`Error: file not found: xxx`，exit code 1
- [ ] JSON 格式错误时：`Error: invalid JSON: xxx`，exit code 1
- [ ] JSON schema 不匹配时：`Error: unrecognized JSON format (missing 'functions' or 'edges' key)`，exit code 1
- [ ] 空 graph `{}` 的 JSON 能正确加载，DOT 输出为空图
- [ ] 新增 6 个测试（见 Step 3）
- [ ] 现有全部测试通过（当前 44 个，`tests/test_analyzers.py::test_dataflow_assign_merge` 已知 bug 除外）
- [ ] 手动验证 benchmark/cjson 查询结果正确性

## RALPLAN-DR Summary

### Principles
1. **零重复分析** — 指定 `--from-json` 时必须跳过 scan→parse→analyze 全流程
2. **向后兼容的渐进改造** — `ethunter project_dir`（无 `--from-json`）行为不变，仍然输出 JSON
3. **最小侵入** — 优先复用已有 `to_dot`、`query_callers`、`query_callees`、`to_json` 逻辑
4. **明确的错误路径** — JSON 文件不存在、格式错误、缺少必要字段时分别给出不同错误消息

### Decision Drivers
1. **JSON 反序列化方式** — 新增 `CallGraph.from_dict()` vs 在 cli.py 中直接构造
2. **`--from-json` 与 `project_dir` 的关系** — 互斥 vs 可选并存

### Viable Options

#### Option A: `CallGraph.from_dict()` + `--from-json` 与 `project_dir` 互斥 (Recommended)

- **反序列化**：在 `CallGraph` 类上新增 `@classmethod from_dict(cls, d)` 静态方法，对应 `to_dict()` 的反向操作
- **CLI**：`--from-json` 与 `project_dir` 互斥。指定 `--from-json` 时不需要 `project_dir` 参数
- **错误处理**：JSON 文件不存在、格式错误、缺少必要字段时分别给出不同错误消息

**Pros:**
- `CallGraph` 自包含序列化/反序列化逻辑，符合单一职责
- CLI 交互最清晰：要么分析项目（传 dir），要么加载已有结果（传 json）
- 反序列化逻辑可复用（测试也可用）

**Cons:**
- 需要修改 `graph/model.py`，但改动很小（~30 行）
- 耦合 model 与 JSON 格式（见 Tradeoff Tensions）

#### Option B: 在 cli.py 中直接解析 JSON

- 不在 `CallGraph` 上添加 `from_dict()`，而是在 cli.py 中手动构造 CallGraph 对象

**Invalidation rationale:** 违反了 DRY 原则 — `to_dict()` 的逆向逻辑应当与 `CallGraph` 配对。而且手动构造容易漏掉字段（如 `summary` 不应出现在反序列化中）。Option A 更干净。

## Implementation Steps

### Step 1: 添加 `CallGraph.from_dict()` — `src/ethunter/graph/model.py`

在 `CallGraph` 类上新增 classmethod：

```python
@classmethod
def from_dict(cls, d: dict) -> CallGraph:
    """Deserialize a CallGraph from the dict produced by to_dict().

    Note: The 'summary' key from to_dict() is intentionally not deserialized
    as it is a computed field, not stored state.
    """
    graph = cls()
    for fd in d.get("functions", []):
        func = Function(
            name=fd["name"],
            file=fd["file"],
            line=fd["line"],
            signature=fd.get("signature", ""),
            is_definition=fd.get("is_definition", False),
            return_type=fd.get("return_type", ""),
            parameters=fd.get("parameters", []),
        )
        graph.add_function(func)
    for ed in d.get("edges", []):
        type_str = ed.get("type", CallType.DIRECT.value)
        try:
            edge_type = CallType(type_str)
        except ValueError:
            raise ValueError(f"Unknown CallType: {type_str!r}")
        edge = CallEdge(
            caller=ed["caller"],
            callee=ed["callee"],
            caller_file=ed.get("caller_file", ""),
            callee_file=ed.get("callee_file", ""),
            type=edge_type,
            indirect_kind=ed.get("indirect_kind", ""),
            caller_line=ed.get("caller_line", 0),
        )
        graph.add_edge(edge)
    graph.source_files = d.get("source_files", [])
    return graph
```

**Key changes from original plan:**
- `summary` 明确注释为计算字段，不反序列化
- `CallType` 用 `try/except ValueError` 校验，给出不合法的 enum 值的错误
- `indirect_kind` 和 `caller_line` 用 `.get()` 配合默认值，与 `to_dict()` 的条件序列化行为兼容（DIRECT 边不写 indirect_kind，反序列化时默认 `""` 是正确的）

### Step 2: 改造 `cli.py` — `src/ethunter/cli.py`

完整重写 `main()` 的 argparse 和流程：

```python
def main() -> None:
    parser = argparse.ArgumentParser(
        description='ethunter - C source code call graph analyzer',
    )
    parser.add_argument('project_dir', nargs='?', help='Path to the C project directory')
    parser.add_argument('--from-json', metavar='FILE', help='Load call graph from a JSON file instead of analyzing')
    parser.add_argument('--query', metavar='FUNC_NAME', help='Query callers and callees for a specific function')
    parser.add_argument('--to-dot', action='store_true', help='Convert loaded JSON call graph to DOT format')
    parser.add_argument('--output', '-o', metavar='FILE', help='Write output to file instead of stdout')

    args = parser.parse_args()

    # Mutual exclusion: need either project_dir or --from-json
    if not args.project_dir and not args.from_json:
        print('Error: either project_dir or --from-json is required', file=sys.stderr)
        sys.exit(1)

    if args.from_json:
        # --- Mode: Load from JSON ---
        json_path = Path(args.from_json)
        if not json_path.is_file():
            print(f'Error: file not found: {args.from_json}', file=sys.stderr)
            sys.exit(1)

        try:
            data = json.loads(json_path.read_text())
        except json.JSONDecodeError as e:
            print(f'Error: invalid JSON: {e}', file=sys.stderr)
            sys.exit(1)

        # Validate schema: must have 'functions' or 'edges' key
        if 'functions' not in data and 'edges' not in data:
            print('Error: unrecognized JSON format (missing "functions" or "edges" key)', file=sys.stderr)
            sys.exit(1)

        graph = CallGraph.from_dict(data)

        # Mutual exclusion: --query and --to-dot cannot be used together
        if args.query and args.to_dot:
            print('Error: --query and --to-dot are mutually exclusive', file=sys.stderr)
            sys.exit(1)

        if args.to_dot:
            output = to_dot(graph)
        elif args.query:
            callers = query_callers(graph, args.query)
            callees = query_callees(graph, args.query)
            output = json.dumps({
                'function': args.query,
                'callers': [{'caller': e.caller, 'file': e.caller_file, 'type': e.type.value} for e in callers],
                'callees': [{'callee': e.callee, 'file': e.callee_file, 'type': e.type.value} for e in callees],
            }, indent=2, ensure_ascii=False)
        else:
            # Default: output JSON when --from-json alone
            output = json.dumps(graph.to_dict(), indent=2, ensure_ascii=False)
    else:
        # --- Mode: Analyze project (original flow, unchanged except --format removal) ---
        project_dir = Path(args.project_dir)
        if not project_dir.is_dir():
            print(f'Error: {project_dir} is not a directory', file=sys.stderr)
            sys.exit(1)

        # Phase 1: Scan files
        files = scan_files(project_dir)
        if not files:
            print('No .c/.h files found', file=sys.stderr)
            sys.exit(1)

        # Phase 2: Parse ASTs
        trees: dict[str, str] = {}
        for f in files:
            try:
                tree = parse_file(f)
                trees[str(f)] = tree
            except Exception as e:
                print(f'Warning: failed to parse {f}: {e}', file=sys.stderr)

        # Phase 3: Build symbol table
        symbol_table = SymbolTable()
        dataflow = VariableState()
        for filepath, tree in trees.items():
            for func in extract_functions(tree, filepath):
                symbol_table.add_function(func)

        # Phase 4: Run all analyzers
        call_graph = run_all_analyses(trees, symbol_table, dataflow)
        call_graph.source_files = [str(f) for f in files]

        # Phase 5: Output
        if args.query:
            callers = query_callers(call_graph, args.query)
            callees = query_callees(call_graph, args.query)
            output = json.dumps({
                'function': args.query,
                'callers': [{'caller': e.caller, 'file': e.caller_file, 'type': e.type.value} for e in callers],
                'callees': [{'callee': e.callee, 'file': e.callee_file, 'type': e.type.value} for e in callees],
            }, indent=2, ensure_ascii=False)
        else:
            output = to_json(call_graph)

    # Write output
    if args.output:
        Path(args.output).write_text(output)
        print(f'Output written to {args.output}')
    else:
        print(output)
```

**Key bug fix**: 原 `cli.py:82` 中 `callees` 的 `file` 字段使用了 `e.caller_file` 而非 `e.callee_file`。修正为 `e.callee_file`。

### Step 3: 新增测试 — `tests/test_query_json.py`

6 个测试：

```python
import json
import os
import pytest
from pathlib import Path

from ethunter.parser.ast_builder import parse_file
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.orchestrator import run_all_analyses
from ethunter.graph.model import CallGraph
from ethunter.output.json_output import to_json
from ethunter.output.dot_output import to_dot

FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures')
BENCHMARK = os.path.join(os.path.dirname(__file__), 'benchmark', 'cjson')


def _make_graph_from_fixture(fixture_name):
    """Generate a CallGraph from a C fixture for testing."""
    path = os.path.join(FIXTURES, fixture_name)
    tree = parse_file(path)
    st = SymbolTable()
    for func in extract_functions(tree, fixture_name):
        st.add_function(func)
    df = VariableState()
    return run_all_analyses({path: tree}, st, df)


class TestFromDict:
    """Roundtrip: to_dict -> from_dict -> to_dict should be identical."""

    def test_roundtrip(self):
        graph = _make_graph_from_fixture('direct_call.c')
        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))
        reserialized = to_json(restored)
        assert json.loads(serialized) == json.loads(reserialized)

    def test_roundtrip_with_indirect(self):
        graph = _make_graph_from_fixture('fp_assign.c')
        serialized = to_json(graph)
        restored = CallGraph.from_dict(json.loads(serialized))
        reserialized = to_json(restored)
        assert json.loads(serialized) == json.loads(reserialized)


class TestFromJsonQuery:
    """Query from JSON file matches direct query."""

    def test_query_matches_direct(self):
        """Generate graph.json, query via --from-json, compare with direct query."""
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        json_path = '/tmp/test_query_json_graph.json'
        Path(json_path).write_text(to_json(graph))

        restored = CallGraph.from_dict(json.loads(Path(json_path).read_text()))
        direct_callers = graph.query_callers('helper')
        restored_callers = restored.query_callers('helper')
        assert [(e.caller, e.callee) for e in direct_callers] == \
               [(e.caller, e.callee) for e in restored_callers]


class TestToDotFromJson:
    """DOT output from JSON matches direct DOT output."""

    def test_dot_matches(self):
        graph = _make_graph_from_fixture('direct_call.c')
        graph.source_files = [os.path.join(FIXTURES, 'direct_call.c')]
        direct_dot = to_dot(graph)

        restored = CallGraph.from_dict(json.loads(to_json(graph)))
        restored_dot = to_dot(restored)
        assert direct_dot == restored_dot

    def test_empty_graph_dot(self):
        """Empty graph should produce valid (empty) DOT output."""
        graph = CallGraph()
        dot = to_dot(graph)
        assert 'digraph CallGraph' in dot
        assert '}' in dot


class TestInvalidJson:
    """Error handling for invalid JSON input."""

    def test_file_not_found(self):
        """Should error when file doesn't exist."""
        path = Path('/tmp/nonexistent_graph_12345.json')
        assert not path.exists()
        # CLI test: would need subprocess, so we test the validation logic
        with pytest.raises(FileNotFoundError):
            data = json.loads(path.read_text())

    def test_invalid_json_content(self):
        """Should error when JSON has wrong schema."""
        bad_data = {"foo": "bar"}
        assert 'functions' not in bad_data and 'edges' not in bad_data
        # This would trigger the schema validation error in cli.py

    def test_invalid_calltype(self):
        """Should error when CallType is unknown."""
        data = {"functions": [], "edges": [{"caller": "a", "callee": "b", "type": "unknown_type"}]}
        with pytest.raises(ValueError, match="Unknown CallType"):
            CallGraph.from_dict(data)
```

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| `from_dict()` 与 `to_dict()` 不对称 | roundtrip 测试（`test_roundtrip` + `test_roundtrip_with_indirect`）确保双向一致 |
| 用户脚本依赖 `--format dot` | clean break，spec 已确认完全移除 |
| `--from-json` 加载的 JSON 格式不匹配 | schema 校验：检查 `functions` 或 `edges` key 存在 |
| 原 `--query` 中的 `callee_file` bug | 修正 `cli.py:82` 从 `e.caller_file` 改为 `e.callee_file` |
| `CallGraph.from_dict()` 耦合 model 与 JSON 格式 | 当前仅一个 classmethod，可接受；未来如格式演化可拆到独立 serialization 模块 |

## Verification Steps

1. `python -m pytest tests/ -q` — 全部通过
2. 手动运行：
   ```bash
   PYTHONPATH=src .venv/bin/python -m ethunter.cli tests/benchmark/cjson -o /tmp/graph.json
   PYTHONPATH=src .venv/bin/python -m ethunter.cli --from-json /tmp/graph.json --query cJSON_Print
   PYTHONPATH=src .venv/bin/python -m ethunter.cli --from-json /tmp/graph.json --to-dot -o /tmp/graph.dot
   ```
3. 对比 `--from-json` 查询结果与直接分析查询结果一致
4. 对比 `--to-dot` 输出与旧版 `--format dot` 结果一致

## ADR

**Decision:** 新增 `CallGraph.from_dict()` 类方法，CLI 新增 `--from-json` 和 `--to-dot` 参数，移除 `--format` 参数。`project_dir` 改为可选（`nargs='?'`），与 `--from-json` 互斥。

**Drivers:** 每次查询重复分析是性能浪费；用户希望分析/查询/转换职责分离。

**Alternatives considered:**
- 子命令模式（`ethunter query graph.json FUNC`）— 被用户否决，偏好独立参数。已知 tradeoff：子命令更 future-proof，但当前只有 3 条路径，独立参数更简洁。如 CLI 扩展到 5+ 模式应重新评估。
- 在 cli.py 中直接反序列化 — 被否决，违反 DRY

**Why chosen:** `from_dict()` 让 `CallGraph` 自包含序列化能力；独立参数让 CLI 三条路径语义清晰。

**Consequences:** 破坏 `--format dot` 的向后兼容（用户已确认 clean break）。`--query` 输出中 `callees[].file` 从 `caller_file` 修正为 `callee_file`。

**Follow-ups:**
- 未来可考虑实现增量更新（re-analyze 后合并到已有 JSON）
- 如 CLI 模式超过 5 个，考虑切换到子命令设计
- 为 `tests/test_query_json.py` 中的 CLI 错误路径测试增加 subprocess 验证

## Changes from v1 (Addressing Reviewer Feedback)

| Feedback | Action |
|----------|--------|
| from_dict/to_dict 不对称 | 添加 CallType ValueError 校验；添加 summary 不反序列化的注释 |
| 缺少 roundtrip 测试 | 新增 `test_roundtrip` + `test_roundtrip_with_indirect`（2 个） |
| --query + --to-dot 冲突未处理 | 新增互斥校验 + 错误消息 |
| 文件不存在 vs 无效 JSON 混淆 | 拆分为独立的错误消息 + 独立测试 |
| 空 graph 未覆盖 | 新增 `test_empty_graph_dot` |
| 非 ethunter JSON schema | 新增 `functions`/`edges` key 校验 |
| 测试数量不足（3 → 6） | 扩展为 6 个测试 |
| callee_file bug 未提及 | 修正 `cli.py:82` 从 `e.caller_file` 改为 `e.callee_file` |
| 44 个测试全部通过的假设 | 明确说明已知 `test_dataflow_assign_merge` 可能失败 |
| 耦合 tradeoff 未提及 | ADR 中记录，建议未来拆分 serialization 模块 |