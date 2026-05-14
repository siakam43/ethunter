# 架构级重构实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 2-Phase pipeline + God Module (param_assign 786行) 重构为 3-Phase pipeline + 4 focused modules + type-aware dataflow key 系统，误报率从 35.76% 降至 <15%，召回无回归。

**Architecture:** Phase 1a (param_helpers.prepare) 预扫描跨文件元数据 → Phase 1 (TARGET_RESOLVERS) 只写 dataflow 不产边 → Phase 2 (CALL_DETECTORS) 读 dataflow 产边 + 构建 covered_callees → Phase 3 (callback_reg) 检查 covered_callees 后产边。Dataflow key 统一为 4 种 type-aware 格式，field_call 从 12 层 fallback 简化为 5 层 type-aware 查找。

**Tech Stack:** Python 3.11, tree-sitter-c, pytest (`.venv/bin/python`)

---

## File Structure

```
src/ethunter/analyzer/
├─ dataflow.py          MODIFY  208→280  新增: func_params, registration_sites, covered_callees, param_alias_map
├─ symbol_table.py       MODIFY  141→180  新增: record_var_type, get_var_type, record_struct_fields, get_struct_fields
├─ param_helpers.py     CREATE  ~210    _collect_func_params, _collect_fnptr_typedefs, _classify_param_usage, _collect_simple_macros, prepare()
├─ param_binding.py     CREATE  ~220    Phase 1: analyze() — 调点参数映射 + field 赋值 → dataflow + registration_sites
├─ param_dispatch.py    CREATE  ~180    Phase 2: analyze() — Pass A (fnptr call) + Pass B (call-site edges) + dedup
├─ callback_reg.py      CREATE  ~140    Phase 3: analyze() — param_usage + covered_callees + _is_registration fallback
├─ orchestrator.py       MODIFY  150→180  3-Phase pipeline, remove Fix B
├─ direct_assign.py     MODIFY  121→130  写 <var>:<func>:<name>
├─ cast_assign.py        MODIFY   62→70   写 <var>:<func>:<name>
├─ direct_call_fp.py    MODIFY   84→90   读 <var>:<func>:<name>
├─ initializer_assign.py MODIFY  420→460  写 type-aware <gstruct>:<type>.<var>.<field>, 调用 symbol_table.record_var_type
├─ field_call.py         MODIFY  282→220  5-layer type-aware lookup, 双读新旧格式
├─ local_fp_tracker.py  MODIFY   91→100  接收 symbol_table 参数, type-aware lookup
├─ (delete) param_assign.py
```

---

### Task 1: DataflowEngine + SymbolTable 基础设施

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`
- Modify: `src/ethunter/analyzer/symbol_table.py`

- [ ] **Step 1: Add new fields to DataflowEngine**

Edit `src/ethunter/analyzer/dataflow.py`. After the `aliases` field (line 54), add:

```python
    # Parameter alias map: (enclosing_func, local_var) -> global_struct_name
    param_alias_map: dict[tuple[str, str], str] = field(default_factory=dict)

    # Cross-file function metadata (populated by param_helpers.prepare)
    func_fp_params: dict[str, set[int]] = field(default_factory=dict)
    param_usage: dict[tuple[str, int], str] = field(default_factory=dict)
    func_params: dict[str, list[str]] = field(default_factory=dict)

    # Phase 3 registration tracking
    registration_sites: list = field(default_factory=list)
    covered_callees: set[str] = field(default_factory=set)
```

Remove the `aliases: dict[str, str] = field(default_factory=dict)` if redundant — keep it.

- [ ] **Step 2: Add type tracking methods to SymbolTable**

Edit `src/ethunter/analyzer/symbol_table.py`. Before the `all_function_names` property, add:

```python
    def __init__(self):
        self._functions: dict[str, list[Function]] = {}
        self._var_types: dict[str, str] = {}
        self._struct_fields: dict[str, list[str]] = {}

    def record_var_type(self, var_name: str, struct_type: str) -> None:
        """Record that a variable is declared as a struct type."""
        self._var_types[var_name] = struct_type

    def get_var_type(self, var_name: str) -> str | None:
        """Get the struct type of a variable, or None if unknown."""
        return self._var_types.get(var_name)

    def record_struct_fields(self, struct_type: str, fields: list[str]) -> None:
        """Record the field names for a struct type."""
        if struct_type not in self._struct_fields:
            self._struct_fields[struct_type] = list(fields)

    def get_struct_fields(self, struct_type: str) -> list[str]:
        """Get field names for a struct type."""
        return self._struct_fields.get(struct_type, [])
```

If `SymbolTable.__init__` already exists, merge the new `_var_types` and `_struct_fields` into it instead of creating a new `__init__`.

- [ ] **Step 3: Run full test suite to verify no regression**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All 146 tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py src/ethunter/analyzer/symbol_table.py
git commit -m "feat: add DataflowEngine fields and SymbolTable type tracking for arch refactor

DataflowEngine: func_params, func_fp_params, param_usage, registration_sites,
covered_callees, param_alias_map. SymbolTable: record_var_type, get_var_type,
record_struct_fields, get_struct_fields. Foundation for 3-phase pipeline and
type-aware dataflow key system.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: 创建 param_helpers.py — 纯提取 + prepare()

**Files:**
- Create: `src/ethunter/analyzer/param_helpers.py`
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: Write TDD test for prepare()**

Append to `tests/test_et_bench.py`:

```python
def test_param_helpers_prepare_populates_engine():
    """param_helpers.prepare() writes func_params, func_fp_params, param_usage to engine."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*cb_t)(int x);
    static void handler(int x) { (void)x; }
    static void direct_caller(cb_t cb) { cb(42); }
    static void forwarder(cb_t cb) { direct_caller(cb); }
    static void setter(void *s, cb_t cb) { ((struct s*)s)->handler = cb; }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    assert "direct_caller" in engine.func_params, f"func_params missing direct_caller: {engine.func_params}"
    assert "forwarder" in engine.func_params, f"func_params missing forwarder"
    assert "setter" in engine.func_params, f"func_params missing setter"
    assert engine.func_params["direct_caller"] == ["cb"], f"unexpected params: {engine.func_params['direct_caller']}"
    assert 0 in engine.func_fp_params["direct_caller"], f"direct_caller pos 0 should be fnptr"
    assert engine.param_usage[("direct_caller", 0)] == "caller", \
        f"expected caller, got {engine.param_usage.get(('direct_caller', 0))}"
    assert engine.param_usage[("forwarder", 0)] == "forwarder", \
        f"expected forwarder, got {engine.param_usage.get(('forwarder', 0))}"
    assert ("setter", 1) in engine.param_fields, f"setter cb param should map to field"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_helpers_prepare_populates_engine -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'ethunter.analyzer.param_helpers'`

- [ ] **Step 3: Create param_helpers.py**

Create `src/ethunter/analyzer/param_helpers.py`:

```python
"""Shared helpers for parametric function pointer tracking.

Provides AST scanning utilities and the prepare() entry point
used by param_binding, param_dispatch, and callback_reg modules.
"""

from __future__ import annotations

import re
import tree_sitter as ts

REG_PATTERNS = [
    'register', 'callback', 'hook', 'attach', 'subscribe', 'set_', 'on_', 'add_',
    'once', 'submit', 'post', 'work', 'spawn', 'scandir', 'sort', 'filter',
    'notify', 'watch', 'dispatch', 'schedule',
]


def _is_registration(name: str) -> bool:
    lower = name.lower()
    return any(p in lower for p in REG_PATTERNS)


def _find_child(node, type_name: str):
    for c in node.children:
        if c.type == type_name:
            return c
    return None


def _find_func_name_from_decl(decl):
    """Extract function name and inner declarator from a function_declarator."""
    ident = _find_child(decl, 'identifier')
    if ident and ident.text:
        return ident.text.decode('utf-8'), decl

    def _search_inner(node):
        if node.type == 'function_declarator':
            inner_ident = _find_child(node, 'identifier')
            if inner_ident and inner_ident.text:
                return inner_ident.text.decode('utf-8'), node
        for c in node.children:
            result = _search_inner(c)
            if result[0]:
                return result
        return None, None

    for c in decl.children:
        if c.type in ('parenthesized_declarator', 'pointer_declarator'):
            name, inner = _search_inner(c)
            if name:
                return name, inner
    return None, None


def _extract_param_name(param_decl) -> str | None:
    """Extract parameter name from parameter_declaration, recursively."""
    def _search(node, depth: int = 0) -> str | None:
        if node.type == 'identifier' and node.text and depth < 10:
            return node.text.decode('utf-8')
        for c in node.children:
            if c.type in ('parenthesized_declarator', 'pointer_declarator',
                          'array_declarator', 'function_declarator'):
                result = _search(c, depth + 1)
                if result:
                    return result
            if c.type == 'identifier' and c.text:
                return c.text.decode('utf-8')
        return None
    return _search(param_decl)


def _extract_field_operand(field_expr) -> str | None:
    """Extract the base identifier from a field_expression."""
    for child in field_expr.children:
        if child.type == 'field_expression':
            return _extract_field_operand(child)
        if child.type == 'identifier' and child.text:
            return child.text.decode('utf-8')
    return None


def _collect_fnptr_typedefs(tree) -> set[str]:
    """Collect typedef names that are function pointer types from the AST."""
    fnptr_typedefs: set[str] = set()

    def _scan(n) -> None:
        if n.type == 'type_definition':
            for child in n.children:
                if child.type == 'function_declarator':
                    def _extract_name(node) -> str | None:
                        if node.type == 'type_identifier' and node.text:
                            return node.text.decode('utf-8')
                        for c in node.children:
                            result = _extract_name(c)
                            if result:
                                return result
                        return None
                    name = _extract_name(child)
                    if name:
                        fnptr_typedefs.add(name)
                    break
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return fnptr_typedefs


def _has_fnptr_declarator(node, fnptr_typedefs: set[str] | None = None) -> bool:
    """Check if a parameter_declaration subtree contains a function_declarator."""
    if node.type == 'function_declarator':
        return True
    if fnptr_typedefs is not None:
        for c in node.children:
            if c.type in ('type_identifier', 'primitive_type') and c.text:
                type_name = c.text.decode('utf-8')
                if type_name in fnptr_typedefs:
                    return True
    for c in node.children:
        if _has_fnptr_declarator(c, fnptr_typedefs):
            return True
    return False


def _collect_simple_macros(tree) -> dict[str, tuple[str, list[str]]]:
    """Collect function-wrapper macros: macro_name -> (real_func_name, [param_names])."""
    macros: dict[str, tuple[str, list[str]]] = {}

    def _scan(n) -> None:
        if n.type == 'preproc_function_def':
            name_node = None
            body_text = None
            param_idents = []
            for child in n.children:
                if child.type == 'identifier' and child.text and name_node is None:
                    name_node = child
                elif child.type == 'preproc_params':
                    for pc in child.children:
                        if pc.type == 'identifier' and pc.text:
                            param_idents.append(pc.text.decode('utf-8'))
                elif child.type == 'preproc_arg' and child.text:
                    body_text = child.text.decode('utf-8')
            if name_node and name_node.text and body_text:
                macro_name = name_node.text.decode('utf-8')
                func_match = re.match(r'\s*(\w+)\s*\(', body_text)
                if func_match and func_match.group(1) != macro_name:
                    macros[macro_name] = (func_match.group(1), param_idents)
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros


def _collect_func_params(node, func_params: dict, func_fp_params: dict | None = None,
                        fnptr_typedefs: set[str] | None = None) -> None:
    """Collect function parameter lists and optionally fnptr parameter positions."""
    if node.type == 'function_definition':
        decl = _find_child(node, 'function_declarator')
        if not decl:
            for c in node.children:
                if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                    d = _find_child(c, 'function_declarator')
                    if d:
                        decl = d
                        break
        if decl:
            fname, inner_decl = _find_func_name_from_decl(decl)
            if fname:
                params = []
                fp_positions = set()
                plist = _find_child(inner_decl, 'parameter_list')
                if plist:
                    pos = 0
                    for p in plist.children:
                        if p.type == 'parameter_declaration':
                            pname = _extract_param_name(p)
                            if pname:
                                params.append(pname)
                                if func_fp_params is not None and _has_fnptr_declarator(p, fnptr_typedefs):
                                    fp_positions.add(pos)
                                pos += 1
                func_params[fname] = params
                if func_fp_params is not None and fp_positions:
                    func_fp_params[fname] = fp_positions
    for child in node.children:
        _collect_func_params(child, func_params, func_fp_params, fnptr_typedefs)


def _classify_param_usage(node, func_fp_params, func_params, param_usage):
    """Classify each fnptr param's usage: 'caller', 'forwarder', or 'storage'.

    Caller: param(args) or (*param)(args) in function body
    Forwarder: other_func(param) in function body (param forwarded as arg)
    Storage: handled by prepare() via param_fields registration
    """
    def _scan(n):
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if not decl:
                for c in n.children:
                    if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                        d = _find_child(c, 'function_declarator')
                        if d:
                            decl = d
                            break
            if decl:
                fname, _ = _find_func_name_from_decl(decl)
                if fname and fname in func_fp_params:
                    fp_positions = func_fp_params[fname]
                    body = _find_child(n, 'compound_statement')
                    if body:
                        def _scan_calls(cn, results):
                            if cn.type == 'call_expression':
                                func_node = cn.child_by_field_name('function') or cn.children[0]
                                called_name = None
                                if func_node and func_node.type == 'identifier' and func_node.text:
                                    called_name = func_node.text.decode('utf-8')
                                elif func_node and func_node.type == 'parenthesized_expression':
                                    for cc in func_node.children:
                                        if cc.type == 'pointer_expression' and cc.children:
                                            inner = cc.children[-1]
                                            if inner.type == 'identifier' and inner.text:
                                                called_name = '*' + inner.text.decode('utf-8')
                                elif func_node and func_node.type == 'pointer_expression' and func_node.children:
                                    inner = func_node.children[-1]
                                    if inner.type == 'identifier' and inner.text:
                                        called_name = '*' + inner.text.decode('utf-8')

                                args = cn.child_by_field_name('arguments')
                                arg_names = []
                                if args:
                                    for cc in args.children:
                                        if cc.type == 'identifier' and cc.text:
                                            arg_names.append(cc.text.decode('utf-8'))

                                results.append((called_name, arg_names))
                            for child in cn.children:
                                _scan_calls(child, results)

                        calls = []
                        _scan_calls(body, calls)

                        params = func_params.get(fname, [])
                        for pos in fp_positions:
                            if pos >= len(params):
                                continue
                            pname = params[pos]
                            role = 'unknown'
                            for called_name, arg_names in calls:
                                if called_name == pname or called_name == '*' + pname:
                                    role = 'caller'
                                    break
                                if pname in arg_names:
                                    if role != 'caller':
                                        role = 'forwarder'
                            key = (fname, pos)
                            if key not in param_usage:
                                param_usage[key] = role

        for child in n.children:
            _scan(child)

    _scan(node)


def prepare(tree: ts.Tree, filepath: str, dataflow) -> None:
    """Phase 1a: Cross-file pre-scan. Collect function metadata and register
    param→field / return→field mappings. Writes to engine only, no edges.

    Populates: engine.func_params, engine.func_fp_params, engine.param_usage,
               engine.param_fields, engine.ret_fields
    """
    from ethunter.analyzer.helpers import extract_field_path, collect_field_assignments, find_enclosing_function

    func_params: dict[str, list[str]] = {}
    func_fp_params: dict[str, set[int]] = {}
    fnptr_typedefs = _collect_fnptr_typedefs(tree)
    _collect_func_params(tree.root_node, func_params, func_fp_params, fnptr_typedefs)

    # Store on engine (cross-file accumulation)
    dataflow.func_params.update(func_params)
    dataflow.func_fp_params.update(func_fp_params)

    # Scan for field = param patterns -> register_param_mapping
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.enclosing_func is None or fa.enclosing_func not in func_params:
            continue
        if fa.resolved_value is None:
            continue
        params = func_params[fa.enclosing_func]
        if fa.resolved_value not in params:
            continue
        param_idx = params.index(fa.resolved_value)
        dataflow.register_param_mapping(
            fa.enclosing_func, param_idx, fa.field_path
        )

    # Scan for return field_expression -> register_return
    def _scan_returns(node: ts.Node) -> None:
        if node.type == 'function_definition':
            decl = _find_child(node, 'function_declarator')
            if not decl:
                for c in node.children:
                    if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                        d = _find_child(c, 'function_declarator')
                        if d:
                            decl = d
                            break
            if not decl:
                for child in node.children:
                    _scan_returns(child)
                return
            fname, inner_decl = _find_func_name_from_decl(decl)
            if not fname:
                for child in node.children:
                    _scan_returns(child)
                return
            params = func_params.get(fname, [])
            body = _find_child(node, 'compound_statement')
            if body:
                def _scan_body(n: ts.Node) -> None:
                    if n.type == 'return_statement':
                        for c in n.children:
                            if c.type == 'field_expression':
                                fp = extract_field_path(c)
                                if fp:
                                    operand = _extract_field_operand(c)
                                    if operand and operand in params:
                                        dataflow.register_return(fname, fp)
                    for child in n.children:
                        _scan_body(child)
                _scan_body(body)
        for child in node.children:
            _scan_returns(child)

    _scan_returns(tree.root_node)

    # Classify fnptr param usage
    param_usage: dict[tuple[str, int], str] = {}
    if func_fp_params:
        _classify_param_usage(tree.root_node, func_fp_params, func_params, param_usage)
        dataflow.param_usage.update(param_usage)
```

This is a pure extraction from `param_assign.py` with two changes:
1. `prepare()` stores `func_params` on engine via `dataflow.func_params.update()`
2. `prepare()` does NOT produce any CallEdge — it only writes to engine fields

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_helpers_prepare_populates_engine -v`
Expected: PASS

- [ ] **Step 5: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS (param_helpers not yet wired into pipeline, no impact)

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_helpers.py tests/test_et_bench.py
git commit -m "feat: add param_helpers module — pure extraction + prepare() entry point

Extracted _collect_func_params, _collect_fnptr_typedefs, _collect_simple_macros,
_classify_param_usage, and auxiliary helpers from param_assign.py. New prepare()
entry point runs Phase 1a cross-file pre-scan: collects func_params, func_fp_params,
param_usage, param_fields, and ret_fields. No edges emitted.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: 创建 param_binding.py — Phase 1 Target Resolution

**Files:**
- Create: `src/ethunter/analyzer/param_binding.py`
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: Write TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_param_binding_writes_dataflow_no_edges():
    """param_binding writes dataflow + registration_sites, returns NO edges."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void my_handler(void) {}
    static void reg(void *s, fn_t f) { ((struct s*)s)->handler = f; }
    void setup(void) { struct s o; reg(&o, my_handler); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    edges = param_binding_analyze(tree, "test.c", engine)
    assert len(edges) == 0, f"param_binding should return 0 edges, got {len(edges)}"
    assert len(engine.registration_sites) > 0, f"should have registration_sites"
    # Verify registration_site entry
    site = engine.registration_sites[0]
    assert site["caller"] == "setup", f"unexpected caller: {site}"
    assert site["callee"] == "reg", f"unexpected callee: {site}"
    assert site["target"] == "my_handler", f"unexpected target: {site}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_binding_writes_dataflow_no_edges -v`
Expected: FAIL — `ModuleNotFoundError`

- [ ] **Step 3: Create param_binding.py**

Create `src/ethunter/analyzer/param_binding.py`:

```python
"""Phase 1: Parameter binding — writes dataflow + registration_sites, no edges."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path, collect_field_assignments
from ethunter.analyzer.param_helpers import (
    _is_registration,
    _find_child,
    _extract_field_operand,
    _collect_simple_macros,
)


def _propagate_call_site(
    call_name: str, arg_idx: int, target: str,
    dataflow, symbol_names: set[str],
) -> None:
    """Propagate a call-site argument target to registered field paths."""
    dataflow.resolve_call_site_param(
        call_name, arg_idx, target, symbol_names=symbol_names
    )


def analyze(
    tree: ts.Tree,
    filepath: str,
    dataflow,
) -> list:
    """Phase 1: Bind call-site arguments to function targets. Writes dataflow
    and registration_sites. Returns empty list (no edges).

    Reads: engine.func_params, engine.func_fp_params (from prepare)
    Writes: dataflow.targets (param→target mappings), engine.registration_sites
    """
    func_params = dataflow.func_params
    func_fp_params = dataflow.func_fp_params
    symbol_names = set(func_params.keys())
    macros = _collect_simple_macros(tree)

    param_mappings: dict[str, set[str]] = {}  # param_name -> {target_func, ...}
    call_site_targets: dict[tuple[str, str, int], set[str]] = {}

    def _collect_call_params(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)

                    # Macro expansion: replace macro call with real function name
                    if call_name not in func_params and call_name in macros:
                        real_func, _ = macros[call_name]
                        if real_func in func_params:
                            call_name = real_func

                    param_names = func_params.get(call_name, [])
                    comma_count = 0
                    for c in args.children:
                        if c.type == ',':
                            comma_count += 1
                        elif c.type == 'identifier' and c.text:
                            arg_idx = comma_count
                            target = c.text.decode('utf-8')
                            if target in symbol_names:
                                fp_params_positions = func_fp_params.get(call_name, set())
                                if not fp_params_positions or arg_idx in fp_params_positions:
                                    # Registration site: record for Phase 3
                                    dataflow.registration_sites.append({
                                        "caller": caller or '<unknown>',
                                        "callee": call_name,
                                        "arg_idx": arg_idx,
                                        "target": target,
                                        "file": filepath,
                                        "line": node.start_point[0] + 1,
                                    })
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        cs_key = (caller or '<unknown>', call_name, arg_idx)
                                        if cs_key not in call_site_targets:
                                            call_site_targets[cs_key] = set()
                                        call_site_targets[cs_key].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
                            else:
                                # Fallback: check dataflow for local var assigned to fnptr
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
                                if df_targets and arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].update(df_targets)
                                    cs_key = (caller or '<unknown>', call_name, arg_idx)
                                    if cs_key not in call_site_targets:
                                        call_site_targets[cs_key] = set()
                                    call_site_targets[cs_key].update(df_targets)
                        elif c.type == 'cast_expression':
                            extracted = None
                            if hasattr(dataflow, 'unwrap_cast'):
                                extracted = dataflow.unwrap_cast(c)
                            if not extracted:
                                for cc in reversed(c.children):
                                    if cc.type == 'identifier' and cc.text:
                                        extracted = cc.text.decode('utf-8')
                                        break
                            if extracted and extracted in symbol_names:
                                arg_idx = comma_count
                                target = extracted
                                fp_params_positions = func_fp_params.get(call_name, set())
                                if not fp_params_positions or arg_idx in fp_params_positions:
                                    dataflow.registration_sites.append({
                                        "caller": caller or '<unknown>',
                                        "callee": call_name,
                                        "arg_idx": arg_idx,
                                        "target": target,
                                        "file": filepath,
                                        "line": node.start_point[0] + 1,
                                    })
                                elif arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
                        elif c.type == 'pointer_expression' and c.children:
                            inner = c.children[-1]
                            if inner.type == 'identifier' and inner.text:
                                target = inner.text.decode('utf-8')
                                if target in symbol_names:
                                    arg_idx = comma_count
                                    fp_params_positions = func_fp_params.get(call_name, set())
                                    if not fp_params_positions or arg_idx in fp_params_positions:
                                        dataflow.registration_sites.append({
                                            "caller": caller or '<unknown>',
                                            "callee": call_name,
                                            "arg_idx": arg_idx,
                                            "target": target,
                                            "file": filepath,
                                            "line": node.start_point[0] + 1,
                                        })
                                    elif arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        cs_key = (caller or '<unknown>', call_name, arg_idx)
                                        if cs_key not in call_site_targets:
                                            call_site_targets[cs_key] = set()
                                        call_site_targets[cs_key].add(target)
                                    _propagate_call_site(
                                        call_name, arg_idx, target,
                                        dataflow, symbol_names
                                    )
        for child in node.children:
            _collect_call_params(child)

    _collect_call_params(tree.root_node)

    # Struct member resolution (Pass 2 equivalent)
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.enclosing_func is None:
            continue
        field_path = fa.field_path
        field_name = field_path.split('.')[-1]

        if fa.value_node and fa.value_node.type == 'call_expression':
            call_func = fa.value_node.child_by_field_name('function') or fa.value_node.children[0]
            if call_func and call_func.type == 'identifier' and call_func.text:
                func_name = call_func.text.decode('utf-8')
                ret_targets = dataflow.resolve_returned_field(func_name)
                for t in ret_targets:
                    dataflow.assign(f'<gstruct:{field_path}>', t)
        elif fa.resolved_value is not None:
            param_name = fa.resolved_value
            # Prong 1: resolve via param_mappings
            targets = param_mappings.get(param_name, set())
            for t in targets:
                dataflow.assign(f'<struct:{field_path}>', t)
            # Prong 2: resolve via dataflow
            df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
            if not df_targets:
                df_targets = dataflow.resolve(param_name)
            if not df_targets:
                df_targets = dataflow.resolve(f'<garray:{param_name}>')
            for t in df_targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                dataflow.assign(f'<struct:{field_name}>', t)
            # Prong 3: register for cross-function propagation
            if fa.enclosing_func in func_params:
                params = func_params[fa.enclosing_func]
                if param_name in params:
                    param_idx = params.index(param_name)
                    dataflow.register_param_mapping(
                        fa.enclosing_func, param_idx, field_path
                    )

    return []  # Phase 1 returns NO edges
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_binding_writes_dataflow_no_edges -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py tests/test_et_bench.py
git commit -m "feat: add param_binding module — Phase 1 target resolution

Writes call-site param→target mappings to dataflow and records
registration_sites. Returns no edges. Reads func_params/func_fp_params
from engine (populated by param_helpers.prepare).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: 创建 param_dispatch.py — Phase 2 Call Detection

**Files:**
- Create: `src/ethunter/analyzer/param_dispatch.py`
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: Write TDD test**

Append to `tests/test_et_bench.py`:

```python
def test_param_dispatch_produces_callback_param_edges():
    """param_dispatch detects fnptr param calls and emits callback_param edges."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void my_handler(void) {}
    static void dispatcher(fn_t cb) { cb(); }
    void setup(void) { dispatcher(my_handler); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.param_dispatch import analyze as param_dispatch_analyze
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", engine)
    edges = param_dispatch_analyze(tree, "test.c", engine)
    pairs = {(e.caller, e.callee) for e in edges}
    assert ("dispatcher", "my_handler") in pairs, \
        f"Expected dispatcher->my_handler in {pairs}"
    assert e.indirect_kind == "callback_param" for e in edges
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_dispatch_produces_callback_param_edges -v`
Expected: FAIL — `ModuleNotFoundError`

- [ ] **Step 3: Create param_dispatch.py**

Create `src/ethunter/analyzer/param_dispatch.py`:

```python
"""Phase 2: Fnptr parameter call detection — produces callback_param edges.

Pass A: Detect calls through fnptr params in function body (cb() / (*cb)())
Pass B: Emit call-site edges from caller -> target
Pass A/B dedup: when Pass A produces (inner_func, target), Pass B skips
  (outer_caller, target) for the same (target, arg_idx) pair.
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.helpers import find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    dataflow,
) -> list[CallEdge]:
    """Detect indirect calls through fnptr params and produce callback_param edges."""
    edges: list[CallEdge] = []
    func_params = dataflow.func_params

    # Collect per-call-site targets from registration_sites (populated by param_binding)
    call_site_targets: dict[tuple[str, str, int], set[str]] = {}
    for site in dataflow.registration_sites:
        cs_key = (site["caller"], site["callee"], site["arg_idx"])
        if cs_key not in call_site_targets:
            call_site_targets[cs_key] = set()
        call_site_targets[cs_key].add(site["target"])

    # Phase 2 also reconstructs param_mappings from dataflow keys
    # (for non-registration call sites that param_binding wrote via pname keys)
    param_mappings: dict[str, set[str]] = {}
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            param_name = key.split(':')[-1]
            if param_name not in param_mappings:
                param_mappings[param_name] = set()
            param_mappings[param_name].update(vals)

    # === Pass A: detect calls through fnptr params ===
    pass_a_edges: set[tuple[str, str, str, int]] = set()  # (caller, target, file, line)

    def _detect_param_calls(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            call_target_name = None
            if func_node and func_node.type == 'identifier' and func_node.text:
                call_target_name = func_node.text.decode('utf-8')
            elif func_node and func_node.type == 'parenthesized_expression':
                for c in func_node.children:
                    if c.type == 'pointer_expression' and c.children:
                        inner = c.children[-1]
                        if inner.type == 'identifier' and inner.text:
                            call_target_name = inner.text.decode('utf-8')
                            break
            elif func_node and func_node.type == 'pointer_expression' and func_node.children:
                inner = func_node.children[-1]
                if inner.type == 'identifier' and inner.text:
                    call_target_name = inner.text.decode('utf-8')

            if call_target_name:
                enclosing_func = find_enclosing_function(node, tree.root_node)
                targets = set()

                # Per-call-site resolution
                if enclosing_func and enclosing_func in func_params:
                    params = func_params[enclosing_func]
                    if call_target_name in params:
                        arg_idx = params.index(call_target_name)
                        for (clr, cn, ai), tgs in call_site_targets.items():
                            if cn == enclosing_func and ai == arg_idx:
                                targets.update(tgs)

                # Merge param_mappings
                pm_targets = param_mappings.get(call_target_name, set())
                if pm_targets:
                    targets = targets | pm_targets

                if targets:
                    for target in targets:
                        pass_a_edges.add(
                            (enclosing_func or '<unknown>', target, filepath,
                             node.start_point[0] + 1))

        for child in node.children:
            _detect_param_calls(child)

    _detect_param_calls(tree.root_node)

    # Emit Pass A edges
    for (caller, target, fp, line) in pass_a_edges:
        edges.append(CallEdge(
            caller=caller,
            callee=target,
            caller_file=fp,
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_param',
            caller_line=line,
        ))

    # === Pass B: call-site caller edges (dedup against Pass A) ===
    pass_a_targets = {(tgt, caller) for (caller, tgt, _, _) in pass_a_edges}
    seen_pass4: set[tuple[str, str]] = set()

    for (caller, callee, arg_idx), targets in call_site_targets.items():
        for target in targets:
            key = (caller, target)
            if key in seen_pass4:
                continue
            # Pass A/B dedup: if Pass A already produced (callee, target),
            # skip (outer_caller, target) for the same target
            if (target, callee) in pass_a_targets:
                continue
            seen_pass4.add(key)
            edges.append(CallEdge(
                caller=caller,
                callee=target,
                caller_file=filepath,
                callee_file='',
                type=CallType.INDIRECT,
                indirect_kind='callback_param',
                caller_line=0,
            ))

    return edges
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_param_dispatch_produces_callback_param_edges -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/param_dispatch.py tests/test_et_bench.py
git commit -m "feat: add param_dispatch module — Phase 2 fnptr call detection

Pass A detects fnptr calls in function body. Pass B emits call-site caller
edges. Pass A/B dedup: when Pass A covers a (callee, target) pair, Pass B
skips the outer caller to prevent O(N×M) edge explosion.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: 创建 callback_reg.py — Phase 3 Registration Detection

**Files:**
- Create: `src/ethunter/analyzer/callback_reg.py`
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: Write TDD test — behavior suppression**

Append to `tests/test_et_bench.py`:

```python
def test_callback_reg_suppresses_forwarder():
    """Forwarder should NOT emit callback_reg for forwarded fnptr."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*cb_t)(int);
    static void my_cb(int x) { (void)x; }
    static void direct_caller(cb_t cb) { cb(42); }
    static void forwarder(cb_t cb) { direct_caller(cb); }
    void setup(void) { forwarder(my_cb); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.param_helpers import prepare
    from ethunter.analyzer.param_binding import analyze as param_binding_analyze
    from ethunter.analyzer.callback_reg import analyze as callback_reg_analyze
    engine = DataflowEngine()
    prepare(tree, "test.c", engine)
    param_binding_analyze(tree, "test.c", engine)
    # Simulate Phase 2 covered_callees (empty in this test)
    engine.covered_callees = set()
    edges = callback_reg_analyze(tree, "test.c", engine)
    # forwarder(my_cb) should NOT produce callback_reg for my_cb
    cr_targets = {e.callee for e in edges}
    assert "my_cb" not in cr_targets, \
        f"forwarder should not emit callback_reg, got: {cr_targets}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_callback_reg_suppresses_forwarder -v`
Expected: FAIL — `ModuleNotFoundError`

- [ ] **Step 3: Create callback_reg.py**

Create `src/ethunter/analyzer/callback_reg.py`:

```python
"""Phase 3: Callback registration detection — produces callback_reg edges.

Three-stage determination:
  Stage 1: Behavior check (param_usage): forwarder/storage -> skip
  Stage 2: Coverage check (covered_callees): target already dispatched by field_call -> skip
  Stage 3: Heuristic fallback: usage == 'unknown' and _is_registration(callee) -> emit
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.param_helpers import _is_registration


def analyze(
    tree: ts.Tree,
    filepath: str,
    dataflow,
) -> list[CallEdge]:
    """Produce callback_reg edges for registration sites, applying behavioral
    and coverage checks.
    """
    edges: list[CallEdge] = []
    param_usage = dataflow.param_usage
    covered_callees = dataflow.covered_callees

    for site in dataflow.registration_sites:
        target = site["target"]
        callee = site["callee"]
        arg_idx = site["arg_idx"]

        # Stage 1: Behavior check
        usage = param_usage.get((callee, arg_idx), 'unknown')
        if usage in ('forwarder', 'storage'):
            continue
        if usage == 'caller':
            pass  # proceed to Stage 2

        # Stage 2: Coverage check
        if target in covered_callees:
            continue

        # Stage 3: Heuristic fallback for unknown usage
        if usage == 'unknown' and not _is_registration(callee):
            continue

        edges.append(CallEdge(
            caller=site["caller"],
            callee=target,
            caller_file=site["file"],
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_reg',
            caller_line=site["line"],
        ))

    return edges
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_callback_reg_suppresses_forwarder -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/callback_reg.py tests/test_et_bench.py
git commit -m "feat: add callback_reg module — Phase 3 registration detection

Three-stage: behavior check (param_usage forwarder/storage suppression),
coverage check (covered_callees from field_call), heuristic fallback
(_is_registration for unknown usage). Replaces _is_registration as
primary gate; demotes it to fallback-only.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: 更新 direct_assign, cast_assign, direct_call_fp — 作用域 key

**Files:**
- Modify: `src/ethunter/analyzer/direct_assign.py`
- Modify: `src/ethunter/analyzer/cast_assign.py`
- Modify: `src/ethunter/analyzer/direct_call_fp.py`

- [ ] **Step 1: Write TDD test for scoped key isolation**

Append to `tests/test_et_bench.py`:

```python
def test_scoped_key_isolates_same_name_vars():
    """fp in two different functions should not collide in dataflow."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void h_a(void) {}
    static void h_b(void) {}
    void setup_a(void) { fn_t fp = h_a; fp(); }
    void setup_b(void) { fn_t fp = h_b; fp(); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    graph = run_all_analyses({"test.c": tree}, st, engine)
    # setup_a should call h_a, NOT h_b
    pairs = {(e.caller, e.callee) for e in graph.edges if e.type.value == 'indirect'}
    assert ("setup_a", "h_a") in pairs, f"Expected setup_a->h_a in {pairs}"
    assert ("setup_a", "h_b") not in pairs, f"setup_a should NOT call h_b: {pairs}"
```

Note: This test will PASS only after Tasks 6+7 are complete and the orchestrator is updated (Task 10). For now, expect it to FAIL.

- [ ] **Step 2: Update direct_assign.py — write scoped keys**

In `src/ethunter/analyzer/direct_assign.py`, replace ALL `dataflow.assign(var_name, target)` calls with `dataflow.assign(f'<var>:{enclosing_func}:{var_name}', target)`.

The file uses `find_enclosing_function(node, tree.root_node)` to get the enclosing function. For each assignment point, capture the enclosing_func and use it as the scope prefix. Example transformation:

```python
# OLD (line ~40):
                        dataflow.assign(var_name, target)

# NEW:
                        enclosing = find_enclosing_function(node, tree.root_node) or '<global>'
                        dataflow.assign(f'<var>:{enclosing}:{var_name}', target)
```

Apply this pattern to ALL 8 `dataflow.assign` calls in direct_assign.py. The enclosing_func may already be available in scope at some call sites — verify and reuse.

- [ ] **Step 3: Update cast_assign.py — same pattern**

In `src/ethunter/analyzer/cast_assign.py`, apply the same transformation to both `dataflow.assign` calls:

```python
# OLD:
dataflow.assign(var_name, target)

# NEW:
enclosing = find_enclosing_function(node, tree.root_node) or '<global>'
dataflow.assign(f'<var>:{enclosing}:{var_name}', target)
```

- [ ] **Step 4: Update direct_call_fp.py — read scoped keys**

In `src/ethunter/analyzer/direct_call_fp.py`, replace bare variable name lookups with scoped format. For each `dataflow.resolve(var_name)` call, the enclosing function context is already available (caller_func variable). Change:

```python
# OLD:
targets = dataflow.resolve(var_name)

# NEW:
targets = dataflow.resolve(f'<var>:{caller_func}:{var_name}')
# Fallback for global scope (unscoped)
if not targets:
    targets = dataflow.resolve(var_name)
```

- [ ] **Step 5: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: Existing tests may FAIL because old param_assign still writes bare keys and reads bare keys. The new scoped keys coexist with old bare keys during migration. Record failures as expected; they will resolve in Task 10 (orchestrator restructuring).

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/direct_assign.py src/ethunter/analyzer/cast_assign.py src/ethunter/analyzer/direct_call_fp.py tests/test_et_bench.py
git commit -m "refactor: use scoped <var>:<func>:<name> keys in direct_assign/cast_assign/direct_call_fp

Replace bare variable name dataflow keys with scoped format. direct_assign
and cast_assign write <var>:<enclosing_func>:<var_name>. direct_call_fp reads
same format. Eliminates cross-function variable name pollution.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 7: 更新 initializer_assign.py — type-aware keys + SymbolTable 类型收集

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py`
- Test: `tests/test_et_bench.py` (追加)

- [ ] **Step 1: Write TDD test for type-aware key**

Append to `tests/test_et_bench.py`:

```python
def test_type_aware_key_isolates_different_struct_types():
    """Two struct types with same field name: targets must not mix."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    source = b'''
    typedef void (*fn_t)(void);
    static void h_a(void) {}
    static void h_b(void) {}
    struct type_a { const char *n; fn_t handler; };
    struct type_b { int id; fn_t handler; };
    static struct type_a o1 = {"a", h_a};
    static struct type_b o2 = {42, h_b};
    void use_a(void) { if (o1.handler) o1.handler(); }
    void use_b(void) { if (o2.handler) o2.handler(); }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import DataflowEngine
    from ethunter.analyzer.orchestrator import run_all_analyses
    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    engine = DataflowEngine()
    graph = run_all_analyses({"test.c": tree}, st, engine)
    fc = {(e.caller, e.callee) for e in graph.edges if e.indirect_kind == "field_call"}
    assert ("use_a", "h_b") not in fc, \
        f"type_a.handler should NOT resolve to h_b: {fc}"
    assert ("use_b", "h_a") not in fc, \
        f"type_b.handler should NOT resolve to h_a: {fc}"
```

This test will PASS only after Tasks 7+10. For now, expect FAIL.

- [ ] **Step 2: Update initializer_assign.py — write type-aware keys and record var types**

In `src/ethunter/analyzer/initializer_assign.py`, the existing `_collect_struct_field_names()` function already scans `struct_specifier` nodes and returns `dict[str, list[str]]`. Extend it to also call `symbol_table.record_struct_fields(struct_type, fields)`.

Then, for each variable declaration that initializes a struct, extract the struct type and call `symbol_table.record_var_type(var_name, struct_type)`.

When writing dataflow keys, use the type-aware format:

```python
# OLD:
dataflow.assign(f'<gstruct:{field_path}>', target)

# NEW:
base_var = field_path.split('.')[0]
struct_type = symbol_table.get_var_type(base_var)
if struct_type:
    dataflow.assign(f'<gstruct>:{struct_type}.{field_path}>', target)
else:
    dataflow.assign(f'<gstruct:{field_path}>', target)  # old format fallback
```

For `<garray>` keys, no change needed — array elements don't have struct types:

```python
dataflow.assign(f'<garray:{var_name}>', target)  # no change
```

- [ ] **Step 3: Run ET-Bench recall tests**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: FAIL for global-struct tests if new format keys aren't yet read by field_call (Task 8). This is expected — the old param_assign field_call still reads old format.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py tests/test_et_bench.py
git commit -m "refactor: write type-aware gstruct keys in initializer_assign

<gstruct:var.field> -> <gstruct>:<type>.<var>.<field> format.
Also calls symbol_table.record_var_type() and record_struct_fields()
to populate type metadata for field_call lookup.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 8: 重写 field_call.py — 5-layer type-aware lookup + 双读兼容

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py`

- [ ] **Step 1: Rewrite field_call.py with type-aware lookup**

Replace the current 12-layer fallback chain with 5-layer type-aware lookup.
Key changes:

1. `analyze()` receives `symbol_table` (the full SymbolTable, not just symbol_names)
2. Layer 1-4 + Layer D as specified
3. Dual-read compatibility: Layer 1 tries new format first, falls back to old format
4. Remove `hasattr(dataflow, 'func_fp_params')` — use `dataflow.func_fp_params` directly
5. Remove `hasattr(dataflow, 'param_alias_map')` — use `dataflow.param_alias_map` directly
6. Remove `hasattr(dataflow, 'state')` fallback chains

The core lookup function:

```python
def _resolve_field_targets(field_path, dataflow, symbol_table, pointer_resolutions,
                           local_fp_mapping):
    """Resolve targets for a field expression call path. 5-layer type-aware lookup."""
    base_var = field_path.split('.')[0]
    struct_type = symbol_table.get_var_type(base_var)
    targets = set()

    # Layer 1: Exact key (new format first, then old format for compat)
    if struct_type:
        targets = dataflow.resolve(f'<gstruct>:{struct_type}.{field_path}>')
        if targets:
            return targets
    # Old format fallback
    targets = dataflow.resolve(f'<gstruct:{field_path}>')
    if targets:
        return targets
    targets = dataflow.resolve(f'<struct:{field_path}>')
    if targets:
        return targets

    # Layer 2: Type-scoped suffix scan
    if struct_type:
        last_field = field_path.split('.')[-1]
        type_prefix = f'<gstruct>:{struct_type}.'
        for key, vals in dataflow.targets.items():
            if key.startswith(type_prefix) and key.endswith(f'.{last_field}>'):
                targets.update(vals)
        if targets:
            return targets

    # Layer 3: garray fallback
    targets = dataflow.resolve(f'<garray>{base_var}>')
    if targets:
        return targets

    # Layer 4: Pointer alias resolution
    if base_var in pointer_resolutions:
        resolved_base = pointer_resolutions[base_var]
        field_suffix = '.'.join(field_path.split('.')[1:])
        resolved_path = f'{resolved_base}.{field_suffix}'
        if struct_type:
            targets = dataflow.resolve(f'<gstruct>:{struct_type}.{resolved_path}>')
        if not targets:
            targets = dataflow.resolve(f'<gstruct>:{resolved_path}>')
        if targets:
            return targets

    # Layer D: Degradation — scoped wildcard on <gstruct>* keys only
    last_field = field_path.split('.')[-1]
    for key, vals in dataflow.targets.items():
        if key.startswith('<gstruct:') and key.endswith(f'.{last_field}>'):
            targets.update(vals)
    # Also try <struct:* keys (old format from param_binding)
    if not targets:
        for key, vals in dataflow.targets.items():
            if key.startswith('<struct:') and key.endswith(f'.{last_field}>'):
                targets.update(vals)

    return targets
```

Replace the entire target resolution logic in `_visit()` (current lines 108-209) with:

```python
targets = _resolve_field_targets(field_path, dataflow, symbol_table,
                                  pointer_resolutions, local_fp_mapping)
```

Keep the callback-of-callback logic (lines 211-246) but update `func_fp_params` access:

```python
# OLD:
func_fp_params = getattr(dataflow, 'func_fp_params', None)
if func_fp_params is None and hasattr(dataflow, 'state'):
    func_fp_params = getattr(dataflow.state, 'func_fp_params', None)

# NEW:
func_fp_params = dataflow.func_fp_params
```

- [ ] **Step 2: Update field_call.analyze() signature to accept symbol_table**

Change:

```python
def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table,  # was: symbol_table: SymbolTable
    dataflow,
) -> list[CallEdge]:
```

And extract `symbol_names` from `symbol_table.all_function_names` inside the function.

- [ ] **Step 3: Run ET-Bench to verify dual-read works**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: Recalls at 100% for 8/9 scenarios (dual-read finds old-format keys from param_assign while initializer_assign writes new format)

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "refactor: 5-layer type-aware lookup in field_call with dual-read compat

Replace 12-layer fallback chain with 5-layer: exact type-aware key,
type-scoped suffix scan, garray, pointer alias, scoped degradation.
Dual-reads both new <gstruct>:<type>.<var>.<field> and old <gstruct:var.field>
formats for migration compatibility. Removes hasattr fallback chains for
func_fp_params and param_alias_map.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 9: 更新 local_fp_tracker.py — type-aware lookup

**Files:**
- Modify: `src/ethunter/analyzer/local_fp_tracker.py`

- [ ] **Step 1: Update local_fp_tracker to use type-aware keys**

`local_fp_tracker._resolve_and_store()` currently queries old key formats. Update it to try new format first:

```python
def _resolve_and_store(
    var_name: str,
    field_expr: ts.Node,
    mapping: dict[str, set[str]],
    dataflow,
    symbol_table=None,
) -> None:
    """Build dataflow key from field expression and resolve targets."""
    field_path = extract_field_path(field_expr)
    if not field_path:
        return
    targets = set()
    base_var = field_path.split('.')[0]
    struct_type = symbol_table.get_var_type(base_var) if symbol_table else None
    
    # New format first
    if struct_type:
        targets = dataflow.resolve(f'<gstruct>:{struct_type}.{field_path}>')
    # Old format fallback
    if not targets:
        targets = dataflow.resolve(f'<gstruct:{field_path}>')
    if not targets:
        targets = dataflow.resolve(f'<struct:{field_path}>')
    if not targets:
        targets = dataflow.resolve(f'<chain:{field_path}>')
    if targets:
        if var_name not in mapping:
            mapping[var_name] = set()
        mapping[var_name].update(targets)
```

Update the function signature to accept optional `symbol_table`:

```python
def collect_local_fp_assignments(
    tree: ts.Tree,
    dataflow,
    symbol_names: set[str],
    symbol_table=None,  # NEW: optional
) -> dict[str, set[str]]:
```

Pass `symbol_table` to `_resolve_and_store`.

- [ ] **Step 2: Update callers to pass symbol_table**

In `field_call.py` line 97, change:

```python
# OLD:
local_fp_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names)

# NEW:
local_fp_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names, symbol_table)
```

In `direct_call_fp.py` line 29, change similarly:

```python
# OLD:
local_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names)

# NEW:
local_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names, symbol_table)
```

- [ ] **Step 3: Run ET-Bench tests**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`
Expected: Recalls maintained at 100% for 8/9 scenarios.

- [ ] **Step 4: Commit**

```bash
git add src/ethunter/analyzer/local_fp_tracker.py src/ethunter/analyzer/field_call.py src/ethunter/analyzer/direct_call_fp.py
git commit -m "refactor: type-aware lookup in local_fp_tracker with optional symbol_table

local_fp_tracker now tries type-aware key format first before falling back
to old formats. Accepts optional symbol_table parameter. Callers updated.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 10: 重组 orchestrator.py — 3-Phase 管线

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py`

- [ ] **Step 1: Rewrite orchestrator to 3-Phase pipeline**

Replace `src/ethunter/analyzer/orchestrator.py` with:

```python
"""Orchestrator: runs all analyzer modules and merges results into a single CallGraph.

3-Phase pipeline:
  Phase 1a: Cross-file pre-scan (param_helpers.prepare) — metadata + param→field registration
  Phase 1:  Target Resolution — write dataflow only, no edges
  Phase 2:  Call Detection — read dataflow, produce edges + covered_callees
  Phase 3:  Registration Detection — check covered_callees + param_usage
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallGraph, CallType, CallEdge
from ethunter.analyzer.dataflow import VariableState, DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer import (
    direct_call,
    dlsym_fp,
)
from ethunter.analyzer import (
    direct_assign,
    initializer_assign,
    cast_assign,
)
from ethunter.analyzer import (
    direct_call_fp,
    field_call,
    array_call,
)
from ethunter.analyzer import (
    param_helpers,
    param_binding,
    param_dispatch,
    callback_reg,
)

# Phase 1: Target Resolution (write dataflow only, no edges)
TARGET_RESOLVERS = [
    direct_assign,
    initializer_assign,
    cast_assign,
    param_binding,
]

# Phase 2: Call Detection (read dataflow, produce edges)
CALL_DETECTORS = [
    direct_call_fp,
    field_call,
    array_call,
    param_dispatch,
]


def run_all_analyses(
    trees: dict[str, ts.Tree],
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> CallGraph:
    """Run all analyzer modules on the parsed trees and build the CallGraph."""
    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    # Wrap dataflow in DataflowEngine
    engine = DataflowEngine(state=dataflow)

    # Add all functions to the graph
    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer (independent, runs first)
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # Phase 1a: Cross-file pre-scan
    for filepath, tree in trees.items():
        param_helpers.prepare(tree, filepath, engine)

    # Phase 1: Target Resolution (write dataflow only, no edges)
    for filepath, tree in trees.items():
        for resolver in TARGET_RESOLVERS:
            # param_binding only writes dataflow, returns []
            if resolver is param_binding:
                resolver.analyze(tree, filepath, engine)
            else:
                resolver.analyze(
                    tree=tree,
                    filepath=filepath,
                    symbol_table=symbol_table,
                    dataflow=engine,
                )

    # Phase 2: Call Detection (read dataflow, produce edges)
    for filepath, tree in trees.items():
        for detector in CALL_DETECTORS:
            if detector is param_dispatch:
                edges = detector.analyze(tree, filepath, engine)
            else:
                edges = detector.analyze(
                    tree=tree,
                    filepath=filepath,
                    symbol_table=symbol_table,
                    dataflow=engine,
                )
            for edge in edges:
                graph.add_edge(edge)

    # Build covered_callees from field_call edges
    covered_callees = {e.callee for e in graph.edges
                       if e.type == CallType.INDIRECT and e.indirect_kind == 'field_call'}
    engine.covered_callees = covered_callees

    # Phase 3: Registration Detection
    for filepath, tree in trees.items():
        edges = callback_reg.analyze(tree, filepath, engine)
        for edge in edges:
            graph.add_edge(edge)

    # dlsym_fp (independent)
    for filepath, tree in trees.items():
        edges = dlsym_fp.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=engine,
        )
        for edge in edges:
            graph.add_edge(edge)

    # Deduplicate: same caller+callee = one edge, prefer direct over indirect
    edge_map: dict[tuple[str, str], dict] = {}
    for edge in graph.edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge.to_dict()
        else:
            existing = edge_map[key]
            if existing.get('type') == 'indirect' and edge.type == CallType.DIRECT:
                edge_map[key] = edge.to_dict()

    graph.edges = [CallEdge(
        caller=d['caller'],
        callee=d['callee'],
        caller_file=d.get('caller_file', ''),
        callee_file=d.get('callee_file', ''),
        type=CallType(d.get('type', 'direct')),
        indirect_kind=d.get('indirect_kind', ''),
        caller_line=d.get('caller_line', 0),
    ) for d in edge_map.values()]

    return graph
```

Key changes from old orchestrator:
1. Phase 1a added: `param_helpers.prepare()` for all files
2. `param_binding` added to TARGET_RESOLVERS, called with different signature (only engine)
3. `param_dispatch` added to CALL_DETECTORS, called with different signature (only engine)
4. `callback_reg` added as Phase 3
5. Fix B post-processing (lines 116-126 of old orchestrator) REMOVED — replaced by `engine.covered_callees` Phase 3 check
6. `var_to_type` synced from SymbolTable to engine

- [ ] **Step 2: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: Tests using old param_assign directly may fail (they bypass orchestrator). Tests using `run_all_analyses` should pass or have minimal failures.

- [ ] **Step 3: Fix any test failures from direct param_assign usage**

Some tests may import and call param_assign.analyze() directly. Update them to use the orchestrator or the new modules.

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS or known failures documented.

- [ ] **Step 4: Run ET-Bench report to verify FP reduction**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s`
Expected: Recall ≥98.86% (no regression), FPR reduced from 35.76%.

- [ ] **Step 5: Update FPR ceilings in test_et_bench.py**

Adjust `fpr_ceilings` dict to match new observed values + 3% margin.

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py tests/test_et_bench.py
git commit -m "refactor: 3-phase pipeline orchestrator — remove Fix B post-processing

Phase 1a (param_helpers.prepare) → Phase 1 (TARGET_RESOLVERS) → Phase 2
(CALL_DETECTORS + covered_callees) → Phase 3 (callback_reg). Fix B
post-processing removed — replaced by covered_callees check in Phase 3.
param_binding + param_dispatch + callback_reg wired into pipeline.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 11: 清理 — 删除 param_assign.py + 移除双读兼容代码

**Files:**
- Delete: `src/ethunter/analyzer/param_assign.py`
- Modify: `src/ethunter/analyzer/field_call.py` (remove old format fallback in Layer 1)
- Modify: `src/ethunter/analyzer/orchestrator.py` (remove old key format compat code if any)
- Modify: `tests/test_et_bench.py` (update any tests still referencing param_assign)

- [ ] **Step 1: Delete param_assign.py**

Run: `rm src/ethunter/analyzer/param_assign.py`

- [ ] **Step 2: Remove dual-read compatibility from field_call.py**

In `field_call._resolve_field_targets()`, remove the old-format fallback in Layer 1:

```python
# REMOVE these lines:
# Old format fallback
targets = dataflow.resolve(f'<gstruct:{field_path}>')
if targets:
    return targets
targets = dataflow.resolve(f'<struct:{field_path}>')
if targets:
    return targets
```

Also remove the `<struct:*` fallback from Layer D.

- [ ] **Step 3: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`
Expected: All tests PASS.

- [ ] **Step 4: Run final ET-Bench report**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v -s 2>&1 | grep -E "(场景|召回率|FPR|检测|命中|误报|fnptr-|总)"`
Expected: All 8 active scenarios at 100% recall, FPR <15%.

- [ ] **Step 5: Commit**

```bash
git rm src/ethunter/analyzer/param_assign.py
git add src/ethunter/analyzer/field_call.py
git commit -m "refactor: remove param_assign.py and dual-read compat code

param_assign god module (786 lines) replaced by param_helpers + param_binding
+ param_dispatch + callback_reg. Old format fallback keys removed from
field_call lookup. All tests pass.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Verification Checklist

After all tasks complete:

- [ ] `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q` — all tests pass
- [ ] `tests/test_et_bench.py::test_et_bench_report` — recall ≥98.86%, FPR <15%
- [ ] `tests/test_et_bench.py::test_fnptr_callback_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_global_struct_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_global_struct_array_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_library_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_only_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_struct_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_cast_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_varargs_full_recall` — PASS
- [ ] `tests/test_et_bench.py::test_fnptr_global_array_full_recall` — PASS
- [ ] `PYTHONPATH=src .venv/bin/python -m ethunter.cli --analyze tests/benchmark/et_bench/fnptr-callback` — exits 0
