# fnptr-struct Dataflow Engine Implementation Plan (v2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade ethunter's dataflow engine to support cross-function parameter propagation, nested cast unwrapping, and two-pass field resolution, raising fnptr-struct recall from 57% to 100% on ET-Bench.

**Architecture:** Add `DataflowEngine` class wrapping `VariableState` with new methods for param tracking, return value tracking, and cast unwrapping. Refactor `field_call.py` to two-pass. Enhance `param_assign.py` with extracted helpers. Add Phase 1a pre-scan in orchestrator for cross-file registration support.

**Tech Stack:** Python 3.11, pytest, tree-sitter, existing ethunter analyzer framework

---

## File Structure

| Action | File | Responsibility |
|---|---|---|
| Modify | `src/ethunter/analyzer/dataflow.py` | Add `DataflowEngine` class |
| Modify | `src/ethunter/analyzer/field_call.py` | Split `_visit` into Pass 1 + Pass 2 |
| Modify | `src/ethunter/analyzer/param_assign.py` | Add 4 helper functions + wire into existing passes |
| Modify | `src/ethunter/analyzer/initializer_assign.py` | Call `unwrap_cast` in `_extract_cast_target` |
| Modify | `src/ethunter/analyzer/orchestrator.py` | Wrap `VariableState` as `DataflowEngine` + Phase 1a pre-scan |
| Create | `tests/test_dataflow_engine.py` | Unit tests for all new capabilities + downgrade path |
| Modify | `tests/test_et_bench.py` | Add ET-Bench integration tests for failing examples |

---

### Task 1: DataflowEngine Core Class + Unit Tests

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py`
- Create: `tests/test_dataflow_engine.py`

- [ ] **Step 1: Write failing unit tests for DataflowEngine**

Create `tests/test_dataflow_engine.py`:

```python
"""Unit tests for DataflowEngine."""

import pytest
import tree_sitter_c as tsc
from tree_sitter import Language, Parser
from ethunter.analyzer.dataflow import VariableState, DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions


def _find_node(node, target_type):
    """Helper to find a node of a given type in the tree."""
    if node.type == target_type:
        return node
    for child in node.children:
        result = _find_node(child, target_type)
        if result:
            return result
    return None


class TestDataflowEngineBasic:
    """Backward compatibility: DataflowEngine proxies VariableState."""

    def setup_method(self):
        self.state = VariableState()
        self.engine = DataflowEngine(state=self.state)

    def test_assign_and_resolve(self):
        self.engine.assign('<gstruct:obj.cb>', 'my_handler')
        assert self.engine.resolve('<gstruct:obj.cb>') == {'my_handler'}

    def test_merge(self):
        self.engine.assign('src', 'func_a')
        self.engine.merge('src', 'dst')
        assert self.engine.resolve('dst') == {'func_a'}

    def test_targets_property(self):
        self.engine.assign('<gstruct:x>', 'fn')
        assert '<gstruct:x>' in self.engine.targets


class TestDataflowEngineParamTracker:
    """ParamTracker: register and resolve call-site param mappings."""

    def setup_method(self):
        self.engine = DataflowEngine()

    def test_register_param_mapping(self):
        self.engine.register_param_mapping(
            "SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb"
        )
        assert ("SSL_CTX_set_alpn_select_cb", 1) in self.engine.param_fields

    def test_resolve_call_site_propagates_targets(self):
        self.engine.register_param_mapping(
            "SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb"
        )
        result = self.engine.resolve_call_site_param(
            "SSL_CTX_set_alpn_select_cb", 1, "alpn_cb",
            symbol_names={"alpn_cb"}
        )
        assert "alpn_cb" in result
        assert self.engine.resolve('<gstruct:ctx->ext.alpn_select_cb>') == {'alpn_cb'}

    def test_resolve_call_site_bare_function(self):
        """resolve_call_site_param recognizes bare function names not in dataflow."""
        self.engine.register_param_mapping(
            "register_callback", 0, "ctx->on_event"
        )
        result = self.engine.resolve_call_site_param(
            "register_callback", 0, "on_init",
            symbol_names={"on_init", "other_fn"}
        )
        assert "on_init" in result

    def test_resolve_call_site_no_mapping(self):
        result = self.engine.resolve_call_site_param("unknown_fn", 0, "arg")
        assert result == set()


class TestDataflowEngineRetTracker:
    """RetTracker: register and resolve return value tracking."""

    def setup_method(self):
        self.engine = DataflowEngine()

    def test_register_return(self):
        self.engine.register_return(
            "SSL_CTX_get_security_callback", "cert->sec_cb"
        )
        assert "SSL_CTX_get_security_callback" in self.engine.ret_fields

    def test_resolve_returned_field(self):
        self.engine.register_return(
            "SSL_CTX_get_security_callback", "cert->sec_cb"
        )
        self.engine.assign('<gstruct:cert->sec_cb>', 'ssl_security_default')
        result = self.engine.resolve_returned_field("SSL_CTX_get_security_callback")
        assert 'ssl_security_default' in result

    def test_resolve_returned_field_no_register(self):
        result = self.engine.resolve_returned_field("unknown_fn")
        assert result == set()


class TestDataflowEngineCastResolver:
    """CastResolver: unwrap nested cast expressions."""

    def setup_method(self):
        self.engine = DataflowEngine()

    def test_unwrap_cast_simple_identifier(self):
        lang = Language(tsc.language())
        parser = Parser(lang)
        tree = parser.parse(b'void fn() { (block128_f)aesni_encrypt; }')
        cast_node = _find_node(tree.root_node, 'cast_expression')
        assert cast_node is not None
        result = self.engine.unwrap_cast(cast_node)
        assert result == 'aesni_encrypt'

    def test_unwrap_cast_nested(self):
        """(T1)(T2)func -> func."""
        lang = Language(tsc.language())
        parser = Parser(lang)
        tree = parser.parse(b'void fn() { (unflushed_iter_fn_t *)(uintptr_t)cb; }')
        cast_node = _find_node(tree.root_node, 'cast_expression')
        assert cast_node is not None
        result = self.engine.unwrap_cast(cast_node)
        assert result == 'cb'

    def test_unwrap_cast_returns_none_for_non_cast(self):
        """Non-cast node -> None."""
        result = self.engine.unwrap_cast(type('FakeNode', (), {'type': 'binary_expression'})())
        assert result is None


class TestHasattrDowngrade:
    """Verify analyzers work correctly when passed VariableState instead of DataflowEngine."""

    def test_variable_state_has_no_new_methods(self):
        """VariableState does not have DataflowEngine methods — hasattr checks should return False."""
        vs = VariableState()
        assert not hasattr(vs, 'unwrap_cast')
        assert not hasattr(vs, 'register_param_mapping')
        assert not hasattr(vs, 'resolve_call_site_param')
        assert not hasattr(vs, 'register_return')
        assert not hasattr(vs, 'resolve_returned_field')

    def test_engine_has_all_methods(self):
        """DataflowEngine has all expected methods."""
        eng = DataflowEngine()
        assert hasattr(eng, 'unwrap_cast')
        assert hasattr(eng, 'register_param_mapping')
        assert hasattr(eng, 'resolve_call_site_param')
        assert hasattr(eng, 'register_return')
        assert hasattr(eng, 'resolve_returned_field')
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py -v
```
Expected: All tests FAIL (DataflowEngine class does not exist yet)

- [ ] **Step 3: Implement DataflowEngine class**

Add to the end of `src/ethunter/analyzer/dataflow.py`:

```python
@dataclass
class DataflowEngine:
    """Cross-function dataflow engine for function pointer tracking.

    Wraps VariableState (backward compatible) and adds:
    - ParamTracker: parameter-to-field propagation across function calls
    - RetTracker: return value tracking for struct field function pointers
    - CastResolver: nested cast expression unwrapping
    """
    state: VariableState = field(default_factory=VariableState)

    # Parameter propagation: (func_name, param_position) -> {field_path}
    param_fields: dict[tuple[str, int], set[str]] = field(default_factory=dict)

    # Return value tracking: func_name -> set of field paths returned
    ret_fields: dict[str, set[str]] = field(default_factory=dict)

    # Alias tracking: reserved for future use
    aliases: dict[str, str] = field(default_factory=dict)

    # === Backward compatible interface ===

    def assign(self, var_name: str, target: str) -> None:
        self.state.assign(var_name, target)

    def resolve(self, var_name: str) -> set[str]:
        return self.state.resolve(var_name)

    def merge(self, src_var: str, dst_var: str) -> None:
        self.state.merge(src_var, dst_var)

    @property
    def targets(self) -> dict[str, set[str]]:
        return self.state.targets

    # === New: ParamTracker ===

    def register_param_mapping(
        self,
        func_name: str,
        param_idx: int,
        field_path: str,
        struct_param_idx: int = 0,
    ) -> None:
        """Register that param_idx of func_name stores into a struct field.

        Example: SSL_CTX_set_alpn_select_cb(ctx, cb) stores cb into ctx->ext.alpn_select_cb
        -> register_param_mapping("SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb")
        """
        key = (func_name, param_idx)
        if key not in self.param_fields:
            self.param_fields[key] = set()
        self.param_fields[key].add(f"<gstruct:{field_path}>")

    def resolve_call_site_param(
        self,
        func_name: str,
        param_idx: int,
        arg_name: str,
        symbol_names: set[str] | None = None,
    ) -> set[str]:
        """Resolve what targets the call-site argument has, and propagate to field paths.

        Returns the set of function names that arg_name resolves to.
        Also writes those targets into the registered field paths in dataflow.
        """
        key = (func_name, param_idx)
        if key not in self.param_fields:
            return set()

        # Step 1: Try dataflow resolve (for variables that were assigned)
        arg_targets = self.state.resolve(arg_name)

        # Step 2: If arg_name itself is a known function name, add it directly
        if symbol_names and arg_name in symbol_names:
            arg_targets.add(arg_name)

        if not arg_targets:
            return set()

        for target in arg_targets:
            for field_key in self.param_fields[key]:
                self.state.assign(field_key, target)

        return arg_targets

    # === New: RetTracker ===

    def register_return(self, func_name: str, field_path: str) -> None:
        """Register that a function returns a struct field function pointer.

        Example: SSL_CTX_get_security_callback returns ctx->cert->sec_cb
        -> register_return("SSL_CTX_get_security_callback", "cert->sec_cb")
        """
        if func_name not in self.ret_fields:
            self.ret_fields[func_name] = set()
        self.ret_fields[func_name].add(field_path)

    def resolve_returned_field(self, func_name: str) -> set[str]:
        """Resolve the targets of the field path that func_name returns."""
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            targets = self.state.resolve(f"<gstruct:{field_path}>")
            results.update(targets)
        return results

    # === New: CastResolver ===

    def unwrap_cast(self, node) -> str | None:
        """Recursively unwrap nested cast expressions.

        (T1)(T2)func  ->  "func"
        (T1)(uintptr_t)cb  ->  "cb"

        Uses child_by_field_name('value') for cast_expression (robust across tree-sitter versions).
        Returns None if the node is not a cast/pointer/paren expression.
        """
        if node.type == 'identifier' and node.text:
            return node.text.decode('utf-8')

        if node.type == 'cast_expression':
            # Prefer child_by_field_name for robustness, fallback to iteration
            value = node.child_by_field_name('value')
            if value:
                return self.unwrap_cast(value)
            for child in reversed(node.children):
                result = self.unwrap_cast(child)
                if result:
                    return result
            return None

        if node.type == 'pointer_expression':
            operand = node.child_by_field_name('argument')
            if operand:
                return self.unwrap_cast(operand)

        if node.type == 'parenthesized_expression':
            inner = node.child_by_field_name('expression')
            if inner is None and len(node.children) >= 2:
                inner = node.children[1]
            if inner:
                return self.unwrap_cast(inner)

        return None
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py -v
```
Expected: All tests PASS

- [ ] **Step 5: Run existing tests to verify no regression**

```bash
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -v
```
Expected: All existing tests PASS

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py tests/test_dataflow_engine.py
git commit -m "feat: add DataflowEngine with backward-compatible interface and new tracking methods"
```

---

### Task 2: field_call.py Two-Pass Scan

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py`
- Test: `tests/test_dataflow_engine.py` (add test class)

- [ ] **Step 1: Write failing test for two-pass order**

Add to `tests/test_dataflow_engine.py` (after existing classes):

```python
class TestFieldCallTwoPass:
    """field_call two-pass: assignments collected before call detection."""

    def test_assignment_after_call_still_detected(self):
        """When field assignment appears after call site in source, edge is still found."""
        from ethunter.analyzer import field_call

        lang = Language(tsc.language())
        parser = Parser(lang)

        # Call site BEFORE assignment in source order
        source = b'''
void handler(void) {}
void caller(void) {
    obj.cb();
}
void init(void) {
    obj.cb = handler;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        edges = field_call.analyze(tree, 'test.c', st, df)

        callers_callees = {(e.caller, e.callee) for e in edges}
        assert ('caller', 'handler') in callers_callees
```

- [ ] **Step 2: Run test to verify it fails**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestFieldCallTwoPass -v
```
Expected: FAIL — current single-pass field_call doesn't see the assignment when it visits the call site first.

- [ ] **Step 3: Refactor field_call.analyze() to two-pass**

Replace the `analyze` function in `src/ethunter/analyzer/field_call.py`. The change is:

1. **Move lines 86-94** (the assignment tracking block inside `_visit`) into a new `_collect_assignments` function
2. **Call `_collect_assignments` before `_visit`**
3. **Remove lines 86-94** from `_visit` (the assignment handling)

Specifically, replace the entire `analyze` function body. The `_visit` function inside it is **identical to the current one except the block at lines 86-94 is removed**. All call detection logic (lines 96-205) is unchanged.

```python
def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through struct field expressions."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names
    macro_map = _collect_macros(tree)

    def _extract_field_expression(node: ts.Node | None) -> ts.Node | None:
        """Extract a field_expression, unwrapping parentheses and pointer expressions."""
        if not node:
            return None
        if node.type == 'field_expression':
            return node
        if node.type == 'parenthesized_expression':
            for c in node.children:
                if c.type == 'pointer_expression':
                    for cc in c.children:
                        if cc.type == 'field_expression':
                            return cc
        return node if node.type == 'field_expression' else None

    # Pass 1: collect all field assignments across the entire file
    def _collect_assignments(node: ts.Node) -> None:
        """Collect field = func_name assignments (extracted from the old _visit block)."""
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                target = rhs.text.decode('utf-8')
                if target in symbol_names:
                    field_path = extract_field_path(lhs)
                    if field_path:
                        dataflow.assign(f'<gstruct:{field_path}>', target)
        for child in node.children:
            _collect_assignments(child)

    _collect_assignments(tree.root_node)

    # Pass 2: detect call sites — this is the EXISTING _visit function
    # MINUS the assignment handling block (old lines 86-94).
    # All call detection code from old lines 96-205 is pasted here unchanged.
    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            field_expr = _extract_field_expression(func_node)
            if field_expr:
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(field_expr)
                if field_path:
                    targets = set()
                    targets = dataflow.resolve(f'<gstruct:{field_path}>')
                    if not targets:
                        targets = dataflow.resolve(f'<struct:{field_path}>')
                    if not targets:
                        targets = dataflow.resolve(f'<chain:{field_path}>')
                    if not targets:
                        base_name = field_path.split('.')[0]
                        garray_targets = dataflow.resolve(f'<garray:{base_name}>')
                        if garray_targets:
                            targets = garray_targets
                            for key, vals in dataflow.targets.items():
                                if key.startswith(f'<gstruct:{base_name}.') and vals:
                                    targets.update(vals)
                        elif '.' in field_path:
                            base_name = field_path.split('.')[0]
                            garray_targets = dataflow.resolve(f'<garray:{base_name}>')
                            if garray_targets:
                                targets.update(garray_targets)
                    if not targets and '.' in field_path:
                        parts = field_path.split('.')
                        alias_targets = dataflow.resolve(parts[0])
                        if alias_targets:
                            for resolved in alias_targets:
                                resolved_path = resolved + '.' + '.'.join(parts[1:])
                                targets = dataflow.resolve(f'<gstruct:{resolved_path}>')
                                if targets:
                                    break
                    if not targets and '.' in field_path:
                        parts = field_path.split('.')
                        for i in range(1, len(parts)):
                            suffix = '.'.join(parts[i:])
                            targets = dataflow.resolve(f'<struct:{suffix}>')
                            if targets:
                                break
                            targets = dataflow.resolve(f'<gstruct:{suffix}>')
                            if targets:
                                break
                        if not targets and len(parts) > 1:
                            for part in parts[1:-1]:
                                targets = dataflow.resolve(f'<struct:{part}>')
                                if targets:
                                    break
                                targets = dataflow.resolve(f'<gstruct:{part}>')
                                if targets:
                                    break
                    if not targets:
                        last_part = field_path.split('.')[-1]
                        targets = dataflow.resolve(last_part)
                        if not targets:
                            for key, vals in dataflow.targets.items():
                                if key.endswith(f'.{last_part}>') and vals:
                                    targets.update(vals)
                    if not targets:
                        targets = dataflow.resolve(f'<vtable:{field_path}>')
                    if not targets:
                        targets = dataflow.resolve('<vtable_init>')

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='field_call',
                            caller_line=node.start_point[0] + 1,
                        ))
            elif func_node.type == 'identifier' and func_node.text:
                call_name = func_node.text.decode('utf-8')
                if call_name in macro_map:
                    body = macro_map[call_name]
                    resolved_path = _extract_field_path_from_macro_body(body)
                    if resolved_path:
                        targets = dataflow.resolve(f'<gstruct:{resolved_path}>')
                        if targets:
                            caller = find_enclosing_function(node, tree.root_node)
                            for target in targets:
                                edges.append(CallEdge(
                                    caller=caller or '<unknown>',
                                    callee=target,
                                    caller_file=filepath,
                                    callee_file='',
                                    type=CallType.INDIRECT,
                                    indirect_kind='field_call',
                                    caller_line=node.start_point[0] + 1,
                                ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestFieldCallTwoPass -v
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py tests/test_et_bench.py -v
```
Expected: All tests PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/field_call.py tests/test_dataflow_engine.py
git commit -m "refactor: split field_call.py into two-pass scan (collect assignments, then detect calls)"
```

---

### Task 3: initializer_assign.py unwrap_cast Integration

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py`
- Test: `tests/test_dataflow_engine.py` (add test class)

- [ ] **Step 1: Write failing test for nested cast extraction**

Add to `tests/test_dataflow_engine.py`:

```python
class TestInitializerAssignUnwrapCast:
    """initializer_assign with nested cast in designated initializer."""

    def test_nested_cast_in_designated_initializer(self):
        """.field = (T1)(T2)func should extract func as target."""
        from ethunter.analyzer import initializer_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void my_handler(void) {}
void other_handler(void) {}
typedef struct {
    void (*cb)(void);
} ops_t;
void init(void) {
    ops_t o = { .cb = (void (*)(void))my_handler };
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        initializer_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<gstruct:o.cb>')
        assert 'my_handler' in targets

    def test_variable_state_still_works(self):
        """When VariableState is passed (not DataflowEngine), existing behavior is preserved."""
        from ethunter.analyzer import initializer_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void my_handler(void) {}
void init(void) {
    ops_t o = { .cb = my_handler };
}
typedef struct { void (*cb)(void); } ops_t;
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        vs = VariableState()
        initializer_assign.analyze(tree, 'test.c', st, vs)

        targets = vs.resolve('<gstruct:o.cb>')
        assert 'my_handler' in targets
```

- [ ] **Step 2: Run test to verify it fails**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestInitializerAssignUnwrapCast -v
```
Expected: FAIL — current `_extract_cast_target` only handles one level of cast.

- [ ] **Step 3: Enhance `_extract_cast_target` in initializer_assign.py**

In `src/ethunter/analyzer/initializer_assign.py`, replace the `_extract_cast_target` function (lines 28-36):

```python
    def _extract_cast_target(node: ts.Node) -> str | None:
        """Extract function name from inside a cast_expression."""
        # Try unwrap_cast if dataflow has it (DataflowEngine)
        if hasattr(dataflow, 'unwrap_cast'):
            result = dataflow.unwrap_cast(node)
            if result and result in symbol_names:
                return result
        # Fallback: original single-level logic
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
                if name in symbol_names:
                    return name
        return None
```

Note: `_extract_cast_target` is a nested function inside `analyze`, so it has closure access to `dataflow` and `symbol_names`. No parameter changes needed.

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestInitializerAssignUnwrapCast -v
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -v
```
Expected: All PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py tests/test_dataflow_engine.py
git commit -m "feat: enhance initializer_assign to unwrap nested cast expressions via DataflowEngine"
```

---

### Task 4: param_assign.py — Extracted Helpers + register_param_mapping + register_return

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py`
- Test: `tests/test_dataflow_engine.py` (add test class)

- [ ] **Step 1: Write failing tests for param mapping registration**

Add to `tests/test_dataflow_engine.py`:

```python
class TestParamAssignRegistration:
    """param_assign registers param->field mappings and return value tracking."""

    def test_register_param_to_field_mapping(self):
        """ctx->ext.alpn_select_cb = cb -> register_param_mapping called."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void alpn_cb(void *ctx) {}
void SSL_CTX_set_alpn_select_cb(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        assert ("SSL_CTX_set_alpn_select_cb", 1) in df.param_fields

    def test_register_return_from_field_expression(self):
        """return ctx->cert->sec_cb -> register_return called."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void *ssl_security_default_callback(void) { return NULL; }
void *SSL_CTX_get_security_callback(void *ctx) {
    return ctx->cert->sec_cb;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        assert "SSL_CTX_get_security_callback" in df.ret_fields

    def test_variable_state_downgrade(self):
        """When VariableState is passed, no AttributeError from hasattr guards."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void alpn_cb(void *ctx) {}
void SSL_CTX_set_alpn_select_cb(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        vs = VariableState()
        edges = param_assign.analyze(tree, 'test.c', st, vs)
        # Should not crash, should return edges for any callback patterns
        assert isinstance(edges, list)
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestParamAssignRegistration -v
```
Expected: FAIL

- [ ] **Step 3: Add helper functions at module level (before `analyze`)**

Add these helpers **before** the `analyze` function in `src/ethunter/analyzer/param_assign.py`. Extracting them as module-level functions keeps `_visit` clean and makes them independently testable.

```python
def _extract_field_operand(field_expr) -> str | None:
    """Extract the base identifier from a field_expression.

    e.g., 'ctx->ext.alpn_select_cb' -> 'ctx'
    """
    for child in field_expr.children:
        if child.type == 'field_expression':
            return _extract_field_operand(child)
        if child.type == 'identifier' and child.text:
            return child.text.decode('utf-8')
    return None


def _try_register_param_to_field(
    lhs, rhs, param_name: str, field_path: str,
    enclosing_func: str | None, func_params: dict,
    dataflow,
) -> None:
    """Register param->field mapping if RHS is a function parameter.

    Called from _visit when we detect: field_expression = identifier(param_name).
    """
    if not enclosing_func or enclosing_func not in func_params:
        return
    params = func_params[enclosing_func]
    if param_name not in params:
        return
    param_idx = params.index(param_name)
    lhs_operand = _extract_field_operand(lhs)
    if not lhs_operand or lhs_operand not in params:
        return
    struct_param_idx = params.index(lhs_operand)
    if hasattr(dataflow, 'register_param_mapping'):
        dataflow.register_param_mapping(
            enclosing_func, param_idx, field_path, struct_param_idx
        )
```

- [ ] **Step 4: Wire `_try_register_param_to_field` into `_visit`**

In `_visit` (line 153-177), after the existing block that writes to dataflow (after line 175), add:

```python
                # NEW: Register param->field mapping for cross-function propagation
                enclosing_func = find_enclosing_function(node, tree.root_node)
                _try_register_param_to_field(
                    lhs, rhs, param_name, field_path,
                    enclosing_func, func_params, dataflow
                )
```

This is inserted right after the existing inner block (after line 175, inside the `if field_path:` block, after the last `dataflow.assign(...)` call). The full `_visit` function's structure becomes:

```python
    def _visit(node: ts.Node) -> None:
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression':
                field_path = extract_field_path(lhs)
                if field_path:
                    # === Case A: RHS is identifier (existing + registration) ===
                    if rhs.type == 'identifier' and rhs.text:
                        param_name = rhs.text.decode('utf-8')
                        # --- EXISTING: resolve param to actual functions ---
                        targets = param_mappings.get(param_name, set())
                        for t in targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                        df_targets = dataflow.resolve(param_name)
                        if not df_targets:
                            df_targets = dataflow.resolve(f'<garray:{param_name}>')
                        for t in df_targets:
                            dataflow.assign(f'<struct:{field_path}>', t)
                            field_name = field_path.split('.')[-1]
                            dataflow.assign(f'<struct:{field_name}>', t)
                        # --- NEW: register for cross-function propagation ---
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        _try_register_param_to_field(
                            lhs, rhs, param_name, field_path,
                            enclosing_func, func_params, dataflow
                        )
                    # === Case B: RHS is call_expression (return value tracking) ===
                    elif rhs.type == 'call_expression':
                        call_func = rhs.child_by_field_name('function') or rhs.children[0]
                        if call_func and call_func.type == 'identifier' and call_func.text:
                            func_name = call_func.text.decode('utf-8')
                            if hasattr(dataflow, 'resolve_returned_field'):
                                ret_targets = dataflow.resolve_returned_field(func_name)
                                for t in ret_targets:
                                    dataflow.assign(f'<gstruct:{field_path}>', t)
        for child in node.children:
            _visit(child)
```

Note: The `call_expression` branch is now a **sibling** of the `identifier` branch under `if lhs.type == 'field_expression'`, not nested inside the `rhs.type == 'identifier'` condition. This fixes a critical logic bug where the `elif` was unreachable.

- [ ] **Step 5: Add return value tracking (register_return)**

Add a new pass after `_collect_func_params` (after line 106, before the `param_mappings` dict creation):

```python
    # === Collect return value tracking ===
    if hasattr(dataflow, 'register_return'):
        def _collect_returns(node: ts.Node) -> None:
            if node.type == 'function_definition':
                # Get function name
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
                        _collect_returns(child)
                    return
                fname_node = _find_child(decl, 'identifier')
                if not fname_node or not fname_node.text:
                    for child in node.children:
                        _collect_returns(child)
                    return
                fname = fname_node.text.decode('utf-8')
                params = func_params.get(fname, [])

                # Scan body for return statements with field_expression
                body = _find_child(node, 'compound_statement')
                if body:
                    def _scan_returns(n: ts.Node) -> None:
                        if n.type == 'return_statement':
                            for c in n.children:
                                if c.type == 'field_expression':
                                    field_path = extract_field_path(c)
                                    if field_path:
                                        operand = _extract_field_operand(c)
                                        if operand and operand in params:
                                            dataflow.register_return(fname, field_path)
                        for child in n.children:
                            _scan_returns(child)
                    _scan_returns(body)
            for child in node.children:
                _collect_returns(child)

        _collect_returns(tree.root_node)
```

- [ ] **Step 6: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestParamAssignRegistration -v
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -v
```
Expected: All PASS

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_dataflow_engine.py
git commit -m "feat: add param->field mapping registration and return value tracking in param_assign"
```

---

### Task 5: param_assign.py — call-site propagation + cast arg extraction

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py`
- Test: `tests/test_dataflow_engine.py` (add test class)

- [ ] **Step 1: Write failing tests for call-site propagation**

Add to `tests/test_dataflow_engine.py`:

```python
class TestParamAssignCallSitePropagation:
    """param_assign propagates targets at call sites and handles cast args."""

    def test_call_site_propagates_bare_function_to_field(self):
        """SSL_CTX_set_alpn_select_cb(ctx, alpn_cb) -> field gets alpn_cb target."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void alpn_cb(void *ctx) {}
void SSL_CTX_set_alpn_select_cb(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
void s_server_main(void) {
    SSL_CTX_set_alpn_select_cb(ctx, alpn_cb);
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<gstruct:ctx->ext.alpn_select_cb>')
        assert 'alpn_cb' in targets

    def test_call_site_cast_wrapped_arg(self):
        """CRYPTO_gcm128_init(..., (block128_f)aesni_encrypt) -> extracts aesni_encrypt."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void aesni_encrypt(void *ctx) {}
void CRYPTO_gcm128_init(void *ctx, void *key, void (*block)(void *k)) {
    ctx->block = block;
}
void aesni_gcm_init_key(void) {
    CRYPTO_gcm128_init(&gctx, &ks, (void (*)(void *))aesni_encrypt);
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<struct:ctx->block>')
        assert 'aesni_encrypt' in targets

    def test_rhs_call_expression_assignment(self):
        """sdb.old_cb = SSL_CTX_get_security_callback(ctx) -> resolves via ret_fields."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void ssl_security_default_callback(void) {}
void *SSL_CTX_get_security_callback(void *ctx) {
    return ctx->cert->sec_cb;
}
void ssl_ctx_security_debug(void *ctx) {
    struct { void (*old_cb)(void); } sdb;
    sdb.old_cb = SSL_CTX_get_security_callback(ctx);
}
void setup(void *ctx) {
    ctx->cert->sec_cb = ssl_security_default_callback;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<gstruct:sdb.old_cb>')
        assert 'ssl_security_default_callback' in targets

    def test_example_13_chain_through_local_fp(self):
        """End-to-end: param_assign -> dataflow -> local_fp_tracker -> direct_call_fp.

        Verifies the full chain for example_13:
        1. param_assign extracts aesni_encrypt from cast arg
        2. param_assign registers ctx->block = aesni_encrypt
        3. local_fp_tracker reads <struct:ctx->block> -> {aesni_encrypt}
        """
        from ethunter.analyzer import param_assign
        from ethunter.analyzer.local_fp_tracker import collect_local_fp_assignments

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void aesni_encrypt(void *ctx) {}
void CRYPTO_gcm128_init(void *ctx, void *key, void (*block)(void *k)) {
    ctx->block = block;
}
void CRYPTO_gcm128_encrypt(void *ctx) {
    void (*block)(void *k) = ctx->block;
    (*block)(ctx);
}
void aesni_gcm_init_key(void) {
    CRYPTO_gcm128_init(&gctx, &ks, (void (*)(void *))aesni_encrypt);
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        # Step 1+2: dataflow has ctx->block mapped
        assert 'aesni_encrypt' in df.resolve('<struct:ctx->block>')

        # Step 3: local_fp_tracker can read it
        symbol_names = st.all_function_names
        local_mapping = collect_local_fp_assignments(tree, df, symbol_names)
        assert 'block' in local_mapping
        assert 'aesni_encrypt' in local_mapping['block']
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestParamAssignCallSitePropagation -v
```
Expected: FAIL

- [ ] **Step 3: Add helper function for call-site propagation**

Add this helper **before** the `analyze` function in `src/ethunter/analyzer/param_assign.py`:

```python
def _propagate_call_site(
    call_name: str, arg_idx: int, target: str,
    dataflow, symbol_names: set[str],
) -> None:
    """Propagate a call-site argument target to registered field paths.

    Uses DataflowEngine.resolve_call_site_param if available (hasattr guard).
    """
    if hasattr(dataflow, 'resolve_call_site_param'):
        dataflow.resolve_call_site_param(
            call_name, arg_idx, target, symbol_names=symbol_names
        )


def _collect_func_params(root: ts.Node) -> dict[str, list[str]]:
    """Collect function definitions with their parameter lists.

    Extracted as a module-level helper so both analyze() and _register_phase()
    can reuse it without duplication.
    """
    func_params: dict[str, list[str]] = {}

    def _extract_param_name(param_decl: ts.Node) -> str | None:
        def _search(node: ts.Node, depth: int = 0) -> str | None:
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

    def _visit(node: ts.Node) -> None:
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
                fname_node = _find_child(decl, 'identifier')
                if fname_node and fname_node.text:
                    fname = fname_node.text.decode('utf-8')
                    params = []
                    plist = _find_child(decl, 'parameter_list')
                    if plist:
                        for p in plist.children:
                            if p.type == 'parameter_declaration':
                                pname = _extract_param_name(p)
                                if pname:
                                    params.append(pname)
                    func_params[fname] = params
        for child in node.children:
            _visit(child)

    _visit(root)
    return func_params
```

In `analyze()`, **replace** the existing `_extract_param_name` and `_collect_func_params` definitions (lines 43-84) with a single call:

```python
    func_params = _collect_func_params(tree.root_node)
```

And remove the old `_find_child` nested function from `analyze()` — it's now module-level.

- [ ] **Step 4: Enhance `_collect_call_params` with cast handling + propagation**

In `_collect_call_params` (line 111-148), replace the inner loop (lines 122-146) with:

```python
                    comma_count = 0
                    for c in args.children:
                        if c.type == ',':
                            comma_count += 1
                        elif c.type == 'identifier' and c.text:
                            arg_idx = comma_count
                            target = c.text.decode('utf-8')
                            if target in symbol_names:
                                if _is_registration(call_name):
                                    dataflow.register_callback(target)
                                    edges.append(CallEdge(
                                        caller=caller or '<registration>',
                                        callee=target,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='callback_reg',
                                        caller_line=node.start_point[0] + 1,
                                    ))
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                # NEW: propagate to registered field paths
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
                        elif c.type == 'cast_expression':
                            # NEW: extract identifier from nested cast
                            extracted = None
                            if hasattr(dataflow, 'unwrap_cast'):
                                extracted = dataflow.unwrap_cast(c)
                            if not extracted:
                                # Fallback: find last identifier in cast children
                                for cc in reversed(c.children):
                                    if cc.type == 'identifier' and cc.text:
                                        extracted = cc.text.decode('utf-8')
                                        break
                            if extracted and extracted in symbol_names:
                                arg_idx = comma_count
                                target = extracted
                                if _is_registration(call_name):
                                    dataflow.register_callback(target)
                                    edges.append(CallEdge(
                                        caller=caller or '<registration>',
                                        callee=target,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='callback_reg',
                                        caller_line=node.start_point[0] + 1,
                                    ))
                                elif arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].add(target)
                                _propagate_call_site(
                                    call_name, arg_idx, target,
                                    dataflow, symbol_names
                                )
```

Key design decisions:
- `arg_idx = comma_count` is set **at the top** of each branch, before any nested conditionals (fixes Risk 8)
- `_propagate_call_site` is a standalone helper (fixes Risk 9 — invasive code)
- The cast fallback (last identifier in children) handles cases where `unwrap_cast` returns None

- [ ] **Step 5: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_dataflow_engine.py::TestParamAssignCallSitePropagation -v
.venv/bin/python -m pytest tests/test_analyzers.py tests/test_cross_file.py -v
```
Expected: All PASS

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py tests/test_dataflow_engine.py
git commit -m "feat: add call-site propagation, cast arg extraction, and RHS call_expression handling"
```

---

### Task 6: Orchestrator Integration — DataflowEngine + Phase 1a Pre-scan

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py`

- [ ] **Step 1: Run current ET-Bench to establish baseline**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```
Record the current recall numbers.

- [ ] **Step 2: Wrap VariableState as DataflowEngine + add Phase 1a**

Replace the `run_all_analyses` function in `src/ethunter/analyzer/orchestrator.py`:

```python
def run_all_analyses(
    trees: dict[str, ts.Tree],
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> CallGraph:
    """Run all analyzer modules on the parsed trees and build the CallGraph."""
    from ethunter.analyzer.dataflow import DataflowEngine

    graph = CallGraph()
    symbol_names = symbol_table.all_function_names

    # Wrap dataflow in DataflowEngine for cross-function tracking
    engine = DataflowEngine(state=dataflow)

    # Add all functions to the graph
    for func_name in symbol_names:
        for f in symbol_table.lookup(func_name):
            graph.add_function(f)

    # Direct call analyzer
    for filepath, tree in trees.items():
        edges = direct_call.analyze(tree, filepath, symbol_names)
        for edge in edges:
            graph.add_edge(edge)

    # Phase 1a (NEW): Pre-scan all files for param->field registrations.
    # This ensures engine.param_fields is fully populated BEFORE any file's
    # Phase 1b call-site propagation tries to use it (fixes cross-file timing).
    for filepath, tree in trees.items():
        param_assign._register_phase(tree, filepath, symbol_table, engine)

    # Phase 1: Target resolution (writes to dataflow via engine)
    for filepath, tree in trees.items():
        for resolver in TARGET_RESOLVERS:
            resolver.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=engine,
            )

    # Phase 1b: param_assign callback detection
    for filepath, tree in trees.items():
        edges = param_assign.analyze(
            tree=tree,
            filepath=filepath,
            symbol_table=symbol_table,
            dataflow=engine,
        )
        for edge in edges:
            graph.add_edge(edge)

    # Phase 2: Call detection (reads from dataflow via engine)
    for filepath, tree in trees.items():
        for detector in CALL_DETECTORS:
            edges = detector.analyze(
                tree=tree,
                filepath=filepath,
                symbol_table=symbol_table,
                dataflow=engine,
            )
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

The only change from the existing orchestrator is:
1. `engine = DataflowEngine(state=dataflow)` — wrap dataflow
2. Phase 1a loop — call `param_assign._register_phase()` before Phase 1
3. Pass `engine` instead of `dataflow` to all analyzers

- [ ] **Step 3: Add `_register_phase` to param_assign.py**

Add this function to `src/ethunter/analyzer/param_assign.py` (before the `analyze` function). It extracts ONLY the registration logic from `analyze` (what `_visit` does for `register_param_mapping` and the return collection for `register_return`), without any edge emission or dataflow writing.

```python
def _register_phase(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow,
) -> None:
    """Phase 1a: pre-scan for param->field registrations only.

    This populates engine.param_fields and engine.ret_fields across ALL files
    BEFORE any call-site propagation runs. No edges are emitted, no dataflow writes.

    Called from orchestrator.run_all_analyses() before Phase 1.
    """
    if not hasattr(dataflow, 'register_param_mapping') and not hasattr(dataflow, 'register_return'):
        return  # VariableState passed — nothing to register

    # Reuse _collect_func_params (module-level, shared with analyze)
    func_params = _collect_func_params(tree.root_node)

    # Scan for field = param patterns -> register_param_mapping
    if hasattr(dataflow, 'register_param_mapping'):
        def _scan_field_assigns(node: ts.Node) -> None:
            if node.type == 'assignment_expression':
                lhs = node.child_by_field_name('left') or node.children[0]
                rhs = node.child_by_field_name('right') or node.children[1]
                if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                    param_name = rhs.text.decode('utf-8')
                    field_path = extract_field_path(lhs)
                    if field_path:
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        _try_register_param_to_field(
                            lhs, rhs, param_name, field_path,
                            enclosing_func, func_params, dataflow
                        )
            for child in node.children:
                _scan_field_assigns(child)

        _scan_field_assigns(tree.root_node)

    # Scan for return field_expression -> register_return
    if hasattr(dataflow, 'register_return'):
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
                fname_node = _find_child(decl, 'identifier')
                if not fname_node or not fname_node.text:
                    for child in node.children:
                        _scan_returns(child)
                    return
                fname = fname_node.text.decode('utf-8')
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
```

Note: `_find_child` is already defined inside `analyze`. The `_register_phase` function needs its own copy or we can extract it as a module-level helper. Extract as module-level:

```python
def _find_child(node: ts.Node, type_name: str) -> ts.Node | None:
    """Find first child of a given type."""
    for c in node.children:
        if c.type == type_name:
            return c
    return None
```

Move `_find_child` from inside `analyze` to module level (before `_is_registration`), and remove the inner definition from `analyze`.

- [ ] **Step 4: Run full test suite**

```bash
.venv/bin/python -m pytest tests/ -v
```
Expected: All existing tests PASS

- [ ] **Step 5: Run ET-Bench to check recall improvement**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s
```
Expected: Improved recall for fnptr-struct category (targeting 100%)

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py src/ethunter/analyzer/param_assign.py
git commit -m "feat: wrap VariableState as DataflowEngine + Phase 1a pre-scan for cross-file registration"
```

---

### Task 7: ET-Bench Integration Tests + Final Verification

**Files:**
- Modify: `tests/test_et_bench.py`

- [ ] **Step 1: Add per-example integration tests**

Add to `tests/test_et_bench.py` (after the existing `test_et_bench_report` function):

```python
def _run_fixture(example_dir):
    """Helper: run ethunter on a fixture directory and return graph."""
    from ethunter.parser.ast_builder import parse_file
    trees = {}
    st = SymbolTable()
    df = VariableState()
    for root, dirs, files in os.walk(example_dir):
        for f in files:
            if f.endswith(('.c', '.h')):
                path = os.path.join(root, f)
                tree = parse_file(path)
                trees[path] = tree
                for func in extract_functions(tree, path):
                    st.add_function(func)
    return run_all_analyses(trees, st, df)


def test_et_bench_fnptr_struct_example_2():
    """cpp_pop_definition -> dump_queued_macros (two-pass field_call fix)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_2')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('cpp_pop_definition', 'dump_queued_macros') in pairs


def test_et_bench_fnptr_struct_example_13():
    """CRYPTO_gcm128_encrypt -> aesni_encrypt (cast unwrap + param propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_13')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('CRYPTO_gcm128_encrypt', 'aesni_encrypt') in pairs


def test_et_bench_fnptr_struct_example_12():
    """s_server_main -> alpn_cb (param->field registration + call-site propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_12')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('s_server_main', 'alpn_cb') in pairs


def test_et_bench_fnptr_struct_example_9():
    """security_callback_debug -> ssl_security_default_callback (return value tracking)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_9')
    graph = _run_fixture(ex_dir)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('security_callback_debug', 'ssl_security_default_callback') in pairs


def test_et_bench_fnptr_struct_example_5():
    """iterate_through_spacemap_logs_cb -> count_unflushed_space_cb et al (cast + param propagation)."""
    ex_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct', 'example_5')
    gt = _load_example_ground_truth(ex_dir)
    expected_pairs = {(e['caller'], e['callee']) for e in gt}
    graph = _run_fixture(ex_dir)
    found_pairs = {(e.caller, e.callee) for e in graph.edges}
    matched = found_pairs & expected_pairs
    assert len(matched) == len(expected_pairs), f"Missing: {expected_pairs - matched}"


def test_et_bench_fnptr_struct_full_recall():
    """fnptr-struct category should achieve 100% recall."""
    cat_dir = os.path.join(ET_BENCH_DIR, 'fnptr-struct')
    total_matched = 0
    total_expected = 0
    for example in sorted(os.listdir(cat_dir)):
        if not example.startswith('example_'):
            continue
        example_dir = os.path.join(cat_dir, example)
        expected = _load_example_ground_truth(example_dir)
        if not expected:
            continue
        total_expected += len(expected)
        graph = _run_analysis_on_fixture(example_dir)
        indirect_edges = [e for e in graph.edges if e.type.value == 'indirect']
        _, matched = compute_recall(indirect_edges, expected)
        total_matched += len(matched)
    recall = total_matched / total_expected if total_expected > 0 else 1.0
    assert recall == 1.0, f"fnptr-struct recall is {recall:.2%}, expected 100%"


def test_cross_file_param_registration():
    """Verify Phase 1a: registration function in file A, call site in file B.

    file_a.c:
        void SSL_CTX_set_alpn_select_cb(SSL_CTX *ctx, void (*cb)(void)) {
            ctx->ext.alpn_select_cb = cb;
        }

    file_b.c:
        void s_server_main(void) {
            SSL_CTX_set_alpn_select_cb(ctx, alpn_cb);
        }
        void alpn_cb(void *ctx) {}

    Without Phase 1a, the registration from file_a.c would not be visible
    when file_b.c's call-site propagation runs.
    """
    from ethunter.parser.ast_builder import parse_file
    trees = {}
    st = SymbolTable()
    df = VariableState()

    source_a = b'''
void SSL_CTX_set_alpn_select_cb(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
'''
    source_b = b'''
void alpn_cb(void *ctx) {}
void s_server_main(void) {
    SSL_CTX_set_alpn_select_cb(ctx, alpn_cb);
}
'''
    tree_a = parse_file('file_a.c')
    tree_b = parse_file('file_b.c')
    # Manually parse since parse_file expects real files
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree_a = parser.parse(source_a)
    tree_b = parser.parse(source_b)
    trees['file_a.c'] = tree_a
    trees['file_b.c'] = tree_b
    for tree, fp in [(tree_a, 'file_a.c'), (tree_b, 'file_b.c')]:
        for func in extract_functions(tree, fp):
            st.add_function(func)

    graph = run_all_analyses(trees, st, df)
    # The edge s_server_main -> alpn_cb should exist
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('s_server_main', 'alpn_cb') in pairs, f"Missing cross-file edge. Got: {pairs}"
```

- [ ] **Step 2: Run the new integration tests**

```bash
.venv/bin/python -m pytest tests/test_et_bench.py -v
```
Expected: All tests PASS including new integration tests

- [ ] **Step 3: Run full test suite for final regression check**

```bash
.venv/bin/python -m pytest tests/ -v --tb=short
```
Expected: All tests PASS, zero failures

- [ ] **Step 4: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add ET-Bench integration tests for fnptr-struct recall verification"
```

---

## Self-Review

### 1. Spec coverage check

| Spec Requirement | Task | Status |
|---|---|---|
| DataflowEngine core class with backward compat | Task 1 | Covered |
| register_param_mapping | Task 4 Step 3-4 | Covered |
| resolve_call_site_param with symbol_names | Task 1 + Task 5 Step 3-4 | Covered |
| register_return | Task 4 Step 5 | Covered |
| resolve_returned_field | Task 1 + Task 4 Step 4 (elif block) | Covered |
| unwrap_cast nested cast (child_by_field_name) | Task 1 Step 3 | Covered (improved over v1) |
| field_call two-pass scan | Task 2 | Covered |
| initializer_assign unwrap_cast call | Task 3 | Covered |
| param_assign call-site cast arg extraction | Task 5 Step 4 | Covered |
| param_assign RHS call_expression | Task 4 Step 4 | Covered |
| orchestrator DataflowEngine wrap + Phase 1a | Task 6 | Covered (improved over v1) |
| hasattr downgrade safety | Tasks 1, 3, 4, 5 | Covered + explicit tests |
| Unit tests for all 5 capabilities | Tasks 1-5 | Covered |
| ET-Bench integration tests | Task 7 | Covered (+ example_9, example_5) |
| No regression in existing tests | Every task's verification step | Covered |
| Cross-file registration timing (Risk 1) | Task 6 Step 2-3 | Covered (Phase 1a) |
| arg_idx scope fix (Risk 8) | Task 5 Step 4 | Covered |
| Intermediate example_13 chain test (Risk 7) | Task 5 Step 1 | Covered |
| Extracted helpers (Risk 9) | Tasks 4-5 | Covered |
| hasattr downgrade test (Risk 10) | Task 1 Step 1, Task 3 Step 1, Task 4 Step 1 | Covered |

### 2. Placeholder scan
No TBD/TODO/fill-in-later patterns. All code steps contain actual implementation.

### 3. Type consistency
- `DataflowEngine.assign/resolve/merge/targets` proxies `VariableState` — consistent
- `unwrap_cast` returns `str | None` — callers check consistently
- `symbol_names` is `set[str]` from `SymbolTable.all_function_names`
- `param_fields` key is `(str, int)` tuple — consistent in register and resolve
- `_propagate_call_site`, `_try_register_param_to_field`, `_extract_field_operand` — new module-level helpers with clear signatures
- `_find_child` moved to module level — reused by `_register_phase`

### 4. Scope check
Plan is focused on the 5 failing fnptr-struct examples. No unrelated refactoring. Each task is independently testable. Cross-file timing issue (Risk 1) is addressed with Phase 1a pre-scan, not deferred.

### 5. Risk mitigation summary

| Risk | v1 Status | v2 Fix |
|---|---|---|
| 1. Cross-file registration timing | Dropped silently | Phase 1a pre-scan in orchestrator (Task 6) |
| 8. arg_idx variable scope | Present in code | `arg_idx = comma_count` at top of each branch (Task 5 Step 4) |
| 7. No intermediate example_13 test | Only end-to-end | `test_example_13_chain_through_local_fp` verifies dataflow + local_fp (Task 5 Step 1) |
| 9. Invasive code changes | Inline nested conditionals | Extracted `_propagate_call_site`, `_try_register_param_to_field`, `_extract_field_operand` (Tasks 4-5) |
| 3. unwrap_cast robustness | reversed() iteration only | `child_by_field_name('value')` primary, reversed() fallback (Task 1 Step 3) |
| 4. field_call copy-paste risk | Full code re-paste | Explicitly states "old lines 86-94 removed, 96-205 unchanged" (Task 2 Step 3) |
| 10. No hasattr downgrade test | Missing | `TestHasattrDowngrade`, `test_variable_state_still_works`, `test_variable_state_downgrade` (Tasks 1, 3, 4) |
| 11. `elif` unreachable (dead code) | Not previously identified | Restructured `_visit`: `field_expression` as outer guard, `identifier`/`call_expression` as sibling branches (Task 4 Step 4) |
| 12. `_scan_field_assigns` infinite recursion | Not previously identified | Fixed: `node` → `child` in recursive call (Task 6 Step 3) |
| 13. `_propagate_call_site` called for non-functions | Not previously identified | Moved inside `if target in symbol_names` block (Task 5 Step 4) |
| 14. `_register_phase` duplicates `_collect_func_params` | Not previously identified | Extracted `_collect_func_params` as module-level, shared by both functions (Task 4 Step 3) |
| 15. No cross-file registration test | Not previously identified | Added `test_cross_file_param_registration` (Task 7 Step 1) |
