# fnptr-global-struct-array 100% Recall Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Achieve 100% recall for the `fnptr-global-struct-array` ET-Bench scenario by fixing positional index tracking in struct initializers, local pointer alias resolution, parameter-to-global-array binding, and multi-hop return value tracking.

**Architecture:** Four sequential fixes in `initializer_assign.py`, `helpers.py`, and `field_call.py`. Bug 0 (prerequisite) corrects positional struct field mapping. Fix A resolves local pointers to global arrays. Fix B binds function parameters to caller-side global arrays. Fix C adds precision enhancements for multi-hop return chains.

**Tech Stack:** Python 3.11, pytest, tree-sitter

---

### Task 1: Bug 0 — Fix positional index tracking in `_process_init_list`

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py:191-212`

- [ ] **Step 1: Write the failing test**

Add to `tests/test_et_bench.py`:

```python
def test_bug0_positional_index_correctness():
    """Bug 0: string/number/null values in positional initializers must increment index."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef struct ops {
        const char *name;
        void (*init)(void);
        int (*cleanup)(void);
        void *extra;
    } ops_t;

    static void my_init(void) {}
    static int my_cleanup(void) { return 0; }

    static const ops_t my_ops = {
        "myops",
        my_init,
        my_cleanup,
        NULL,
    };
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.initializer_assign import analyze as init_analyze
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    init_analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=df)

    # After fix: index 0 ("myops", string) increments index; index 1 gets my_init -> field "init"
    assert 'my_init' in df.resolve('<gstruct:my_ops.init>'), \
        f"Expected my_init in .init, got: {df.resolve('<gstruct:my_ops.init>')}"
    # index 2 gets my_cleanup -> field "cleanup"
    assert 'my_cleanup' in df.resolve('<gstruct:my_ops.cleanup>'), \
        f"Expected my_cleanup in .cleanup, got: {df.resolve('<gstruct:my_ops.cleanup>')}"
    # NULL at index 3 should be skipped for function target storage
    assert not df.resolve('<gstruct:my_ops.extra>'), \
        f"Expected empty .extra, got: {df.resolve('<gstruct:my_ops.extra>')}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_bug0_positional_index_correctness -v`

Expected: FAIL — `<gstruct:my_ops.init>` contains wrong target or is empty because string literal at index 0 was skipped without increment.

- [ ] **Step 3: Implement the fix**

In `src/ethunter/analyzer/initializer_assign.py`, replace lines 194-206:

```python
        # Positional (pure identifier/cast list): { func_a, (type)func_b, ... }
        # For structs, map positional index → field name from struct_field_map
        field_names = struct_field_map.get(struct_type, []) if struct_type else []
        index = 0
        for c in init_list.children:
            if c.type in ('identifier', 'cast_expression', 'call_expression'):
                target = _extract_function_from_value(c)
                if target:
                    dataflow.assign(f'<garray:{var_name}>', target)
                    # Store with numeric index
                    dataflow.assign(f'<gstruct:{var_name}.{index}>', target)
                    # Also store with field name if we can map the index
                    if index < len(field_names):
                        field_name = field_names[index]
                        dataflow.assign(f'<gstruct:{var_name}.{field_name}>', target)
                index += 1
            elif c.type == 'initializer_list':
                for inner in c.children:
                    if inner.type in ('identifier', 'cast_expression'):
                        target = _extract_function_from_value(inner)
                        if target:
                            dataflow.assign(f'<garray:{var_name}>', target)
```

With:

```python
        # Positional (pure identifier/cast list): { func_a, (type)func_b, ... }
        # For structs, map positional index → field name from struct_field_map
        field_names = struct_field_map.get(struct_type, []) if struct_type else []

        # Node types that represent value positions (increment index)
        _VALUE_TYPES = {
            'identifier', 'cast_expression', 'call_expression',
            'string_literal', 'number_literal', 'null',
            'pointer_expression', 'field_expression',
            'parenthesized_expression', 'char_literal', 'concatenated_string',
            'sizeof_expression', 'conditional_expression',
            'binary_expression', 'unary_expression', 'subscript_expression',
        }
        # Node types that carry function targets (store to dataflow)
        _STORE_TYPES = {'identifier', 'cast_expression', 'call_expression'}

        index = 0
        for c in init_list.children:
            if c.type in _VALUE_TYPES:
                if c.type in _STORE_TYPES:
                    target = _extract_function_from_value(c)
                    if target:
                        dataflow.assign(f'<garray:{var_name}>', target)
                        dataflow.assign(f'<gstruct:{var_name}.{index}>', target)
                        if index < len(field_names):
                            field_name = field_names[index]
                            dataflow.assign(f'<gstruct:{var_name}.{field_name}>', target)
                index += 1
            elif c.type == 'initializer_list':
                for inner in c.children:
                    if inner.type in ('identifier', 'cast_expression'):
                        target = _extract_function_from_value(inner)
                        if target:
                            dataflow.assign(f'<garray:{var_name}>', target)
```

- [ ] **Step 4: Run the new test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_bug0_positional_index_correctness -v`

Expected: PASS

- [ ] **Step 5: Verify no regression in all existing ET-Bench tests**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All existing tests still PASS

- [ ] **Step 6: Run the ET-Bench report to check recall improvement**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -s 2>&1 | tail -20`

Expected: `fnptr-global-struct-array` recall increases from 77.78%. Example_7 may now pass (via suffix matching on correct `.transform` keys). Note the new recall value.

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py tests/test_et_bench.py
git commit -m "fix: increment positional index for all value types in _process_init_list

String/number/null literals were skipped without incrementing the positional
index, causing struct field name mappings to be offset for all subsequent values.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Fix A — Extract and extend pointer resolutions for local alias resolution

**Files:**
- Modify: `src/ethunter/analyzer/helpers.py` (add public `collect_pointer_resolutions`)
- Modify: `src/ethunter/analyzer/initializer_assign.py` (replace private with call to public)
- Modify: `src/ethunter/analyzer/field_call.py` (use resolution map in Pass 2)

- [ ] **Step 1: Write the failing test**

Add to `tests/test_et_bench.py`:

```python
def test_fix_a_local_pointer_alias_resolution():
    """Fix A: p = &arr[i]; p->field() should resolve to global array field."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*cb_fn)(void);

    struct feat {
        const char *name;
        cb_fn present;
    };

    static void my_callback(void) {}

    static const struct feat table[2] = {
        {"item1", my_callback},
        {NULL, NULL}
    };

    void caller(void) {
        int i = 0;
        const struct feat *p = &table[i];
        if (p->present)
            p->present();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('caller', 'my_callback') in pairs, f"Missing edge. Got: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_a_local_pointer_alias_resolution -v`

Expected: FAIL — `caller -> my_callback` edge not found.

- [ ] **Step 3: Move `_collect_pointer_resolutions` to `helpers.py`**

In `src/ethunter/analyzer/helpers.py`, add the public function after `collect_field_assignments`. Add handling for `&field_expression`:

```python
def collect_pointer_resolutions(tree: ts.Tree) -> dict[str, str]:
    """Scan function bodies for ptr = &global_name[...] or ptr = &field_expr patterns.
    
    Returns mapping: local_var_name -> resolved_name_or_path
    
    Handles:
    - ptr = &global_array[i]  ->  var_name -> global_array
    - ptr = &global_struct    ->  var_name -> global_struct
    - ptr = &obj->field       ->  var_name -> obj.field  (field path preserved)
    """
    resolutions: dict[str, str] = {}

    def _scan(n: ts.Node) -> None:
        if n.type == 'assignment_expression':
            _handle_assignment(n, resolutions)
        elif n.type == 'init_declarator':
            _handle_init(n, resolutions)
        for child in n.children:
            _scan(child)

    def _handle_assignment(node: ts.Node, resolutions: dict[str, str]) -> None:
        lhs = node.child_by_field_name('left') or (node.children[0] if node.children else None)
        rhs = node.child_by_field_name('right') or (node.children[-1] if len(node.children) >= 2 else None)
        if not lhs or not rhs or lhs.type != 'identifier' or not lhs.text:
            return
        var_name = lhs.text.decode('utf-8')
        resolved = _resolve_pointer_target(rhs)
        if resolved:
            resolutions[var_name] = resolved

    def _handle_init(node: ts.Node, resolutions: dict[str, str]) -> None:
        declarator = node.child_by_field_name('declarator')
        value = node.child_by_field_name('value')
        if not declarator or not value or value.type != 'pointer_expression':
            return
        var_name = extract_identifier_from_declarator(declarator)
        if not var_name:
            return
        resolved = _resolve_pointer_target(value)
        if resolved:
            resolutions[var_name] = resolved

    def _resolve_pointer_target(rhs: ts.Node) -> str | None:
        """Extract target name/path from the operand of a pointer_expression (&expr)."""
        if rhs.type != 'pointer_expression' or not rhs.children:
            return None
        inner = rhs.children[-1]
        if inner.type == 'identifier' and inner.text:
            return inner.text.decode('utf-8')
        if inner.type == 'subscript_expression' and inner.children:
            base = inner.children[0]
            if base.type == 'identifier' and base.text:
                return base.text.decode('utf-8')
        # NEW: &obj->field or &obj.field
        if inner.type == 'field_expression':
            field_path = extract_field_path(inner)
            if field_path:
                return field_path
        return None

    _scan(tree.root_node)
    return resolutions
```

- [ ] **Step 4: Update `initializer_assign.py` to use public function**

In `src/ethunter/analyzer/initializer_assign.py`:

Remove the private `_collect_pointer_resolutions` function (lines 214-251). Replace with import:

Add at top:
```python
from ethunter.analyzer.helpers import collect_pointer_resolutions
```

In `_track_pointer_field_assignments`, change line 265:
```python
        resolutions = _collect_pointer_resolutions()
```
To:
```python
        resolutions = collect_pointer_resolutions(tree)
```

- [ ] **Step 5: Update `field_call.py` to use pointer resolutions in Pass 2**

In `src/ethunter/analyzer/field_call.py`, add import at top:
```python
from ethunter.analyzer.helpers import collect_pointer_resolutions
```

In `analyze()`, after Pass 1 (after `collect_field_assignments` call), add:
```python
    # Pass 1b: collect pointer resolutions (local var -> global array name)
    pointer_resolutions = collect_pointer_resolutions(tree)
```

In Pass 2's `_visit` function, in the `if field_path:` block, add a new fallback BEFORE the last `<vtable:path>` fallback. Insert after line 174 (before `# Fallback: try <vtable:path>`):

```python
                    # Fallback: pointer alias resolution (Fix A)
                    if not targets and '.' in field_path:
                        base_name = field_path.split('.')[0]
                        if base_name in pointer_resolutions:
                            resolved_base = pointer_resolutions[base_name]
                            field_suffix = '.'.join(field_path.split('.')[1:])
                            targets = dataflow.resolve(f'<gstruct:{resolved_base}.{field_suffix}>')
                            if not targets and '.' in resolved_base:
                                targets = dataflow.resolve(f'<gstruct:{resolved_base}.{field_suffix}>')
```

- [ ] **Step 6: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_a_local_pointer_alias_resolution -v`

Expected: PASS

- [ ] **Step 7: Verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py tests/test_benchmark.py -v`

Expected: All existing tests PASS

- [ ] **Step 8: Check ET-Bench report for example_2 and example_4**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -s 2>&1 | tail -20`

Expected: `fnptr-global-struct-array` recall should now be higher. example_2 and example_4 should pass.

- [ ] **Step 9: Commit**

```bash
git add src/ethunter/analyzer/helpers.py src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: local pointer alias resolution for global struct array field calls

Extract _collect_pointer_resolutions to helpers.py as public function with
&field_expression support. Use in field_call Pass 2 to resolve local pointers
that alias global array elements (e.g., p = &arr[i]; p->field()).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Fix B — Parameter-to-global-array binding

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py` (register param→global mapping)
- Modify: `src/ethunter/analyzer/field_call.py` (use mapping in Pass 2)

- [ ] **Step 1: Write the failing test**

Add to `tests/test_et_bench.py`:

```python
def test_fix_b_param_to_global_array_binding():
    """Fix B: function parameter array should bind to caller-side global array."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef int (*write_fn)(void);

    struct writer {
        const char *name;
        write_fn fn;
    };

    static int write_a(void) { return 0; }
    static int write_b(void) { return 0; }

    static const struct writer table[] = {
        {"a", write_a},
        {"b", write_b},
        {NULL, NULL},
    };

    void do_write(const struct writer mappings[]) {
        for (int i = 0; mappings[i].name; i++) {
            if (mappings[i].fn)
                mappings[i].fn();
        }
    }

    void caller(void) {
        do_write(table);
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('do_write', 'write_a') in pairs, f"Missing write_a. Got: {pairs}"
    assert ('do_write', 'write_b') in pairs, f"Missing write_b. Got: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_b_param_to_global_array_binding -v`

Expected: FAIL — `do_write -> write_a` / `write_b` edges not found.

- [ ] **Step 3: Implement param→global array registration**

In `src/ethunter/analyzer/initializer_assign.py`, add at the end of the `analyze()` function (before `return edges`), after the call to `_track_pointer_field_assignments`:

```python
    # Pass 3: Register param->global_array bindings (Fix B)
    # When caller passes a global array name to a function parameter,
    # register the mapping so field_call can resolve through the parameter.
    def _register_param_global_aliases(tree, dataflow, symbol_names):
        """Detect f(global_array) calls and register param->global mapping."""
        # Collect function parameter names
        func_params = {}
        def _collect_params(n):
            if n.type == 'function_definition':
                decl = None
                for c in n.children:
                    if c.type == 'function_declarator':
                        decl = c
                        break
                if decl:
                    fname = None
                    for c in decl.children:
                        if c.type == 'identifier' and c.text:
                            fname = c.text.decode('utf-8')
                            break
                    if fname:
                        params = []
                        plist = None
                        for c in decl.children:
                            if c.type == 'parameter_list':
                                plist = c
                                break
                        if plist:
                            for p in plist.children:
                                if p.type == 'parameter_declaration':
                                    pname = _extract_param_name(p)
                                    if pname:
                                        params.append(pname)
                        func_params[fname] = params
            for child in n.children:
                _collect_params(child)

        def _extract_param_name(node):
            if node.type == 'identifier' and node.text:
                return node.text.decode('utf-8')
            for c in node.children:
                if c.type in ('pointer_declarator', 'array_declarator'):
                    for cc in c.children:
                        if cc.type == 'identifier' and cc.text:
                            return cc.text.decode('utf-8')
                if c.type == 'identifier' and c.text:
                    return c.text.decode('utf-8')
            return None

        _collect_params(tree.root_node)

        # Detect call sites with global array arguments
        def _scan_calls(n):
            if n.type == 'call_expression':
                func_node = n.child_by_field_name('function') or n.children[0]
                if func_node and func_node.text:
                    callee = func_node.text.decode('utf-8')
                    if callee in func_params:
                        args = n.child_by_field_name('arguments')
                        if args:
                            param_names = func_params[callee]
                            arg_idx = 0
                            for c in args.children:
                                if c.type in ('(', ')', ','):
                                    continue
                                if c.type == 'identifier' and c.text:
                                    arg_name = c.text.decode('utf-8')
                                    # Check if arg_name is a known global array
                                    has_gstruct = any(
                                        k.startswith(f'<gstruct:{arg_name}.') and v
                                        for k, v in dataflow.targets.items()
                                    )
                                    has_garray = bool(dataflow.resolve(f'<garray:{arg_name}>'))
                                    if (has_gstruct or has_garray) and arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        # Register via DataflowEngine if available
                                        if hasattr(dataflow, 'param_alias_map'):
                                            if not hasattr(dataflow, 'param_alias_map'):
                                                dataflow.param_alias_map = {}
                                            dataflow.param_alias_map[(callee, pname)] = arg_name
                                arg_idx += 1
            for child in n.children:
                _scan_calls(child)

        _scan_calls(tree.root_node)

    _register_param_global_aliases(tree, dataflow, symbol_names)
```

- [ ] **Step 4: Update `field_call.py` to use param alias in Pass 2**

In `src/ethunter/analyzer/field_call.py`, in Pass 2's `_visit` function, add after the pointer alias fallback (from Fix A):

```python
                    # Fallback: parameter-to-global-array binding (Fix B)
                    if not targets and '.' in field_path:
                        base_name = field_path.split('.')[0]
                        enclosing_func = find_enclosing_function(node, tree.root_node)
                        if enclosing_func and hasattr(dataflow, 'param_alias_map'):
                            alias_key = (enclosing_func, base_name)
                            if alias_key in dataflow.param_alias_map:
                                global_name = dataflow.param_alias_map[alias_key]
                                field_suffix = '.'.join(field_path.split('.')[1:])
                                targets = dataflow.resolve(f'<gstruct:{global_name}.{field_suffix}>')
```

- [ ] **Step 5: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_b_param_to_global_array_binding -v`

Expected: PASS

- [ ] **Step 6: Verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py tests/test_benchmark.py -v`

Expected: All existing tests PASS

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: parameter-to-global-array binding for field call resolution

Register call-site param->global array bindings so field_call can resolve
through function parameters (e.g., f(global_arr) where callee uses
param_name[i].field()).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Fix C1 — Handle `pointer_expression` in array initializers

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py:_process_init_list`

- [ ] **Step 1: Write the failing test**

Add to `tests/test_et_bench.py`:

```python
def test_fix_c1_pointer_expression_in_array_init():
    """Fix C1: &struct_name in array initializers should produce garray entries."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*transform_fn)(void);

    struct impl {
        const char *name;
        transform_fn transform;
    };

    static void my_transform(void) {}

    static const struct impl my_impl = {
        "generic", my_transform
    };

    static const struct impl *const impls[] = {
        &my_impl,
    };
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.initializer_assign import analyze as init_analyze
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    init_analyze(tree=tree, filepath="test.c", symbol_table=st, dataflow=df)

    # After C1: <garray:impls> should contain the struct name (for later field resolution)
    targets = df.resolve('<garray:impls>')
    assert 'my_impl' in targets, f"Expected my_impl in <garray:impls>, got: {targets}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_c1_pointer_expression_in_array_init -v`

Expected: FAIL — `<garray:impls>` is empty.

- [ ] **Step 3: Implement pointer_expression handling in `_process_init_list`**

In `src/ethunter/analyzer/initializer_assign.py`, in the `_process_init_list` function, extend `_STORE_TYPES` handling to also extract struct names from `pointer_expression`:

After the existing `if c.type in _STORE_TYPES:` block, add:

```python
                elif c.type == 'pointer_expression':
                    # &struct_name -> store struct name for field resolution
                    inner = c.children[-1] if c.children else None
                    if inner and inner.type == 'identifier' and inner.text:
                        struct_name = inner.text.decode('utf-8')
                        # Store as garray (list of struct instances) for resolution
                        dataflow.assign(f'<garray:{var_name}>', struct_name)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_c1_pointer_expression_in_array_init -v`

Expected: PASS

- [ ] **Step 5: Verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py tests/test_benchmark.py -v`

Expected: All existing tests PASS

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py tests/test_et_bench.py
git commit -m "feat: handle pointer_expression in array initializers for garray tracking

&struct_name in array initializers now produces <garray:> entries storing
the struct name for downstream field resolution.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Fix C2/C3 — Call expression RHS handling + field_call local var integration

**Files:**
- Modify: `src/ethunter/analyzer/initializer_assign.py:_track_pointer_field_assignments`
- Modify: `src/ethunter/analyzer/field_call.py` (use local_fp_tracker mapping)

- [ ] **Step 1: Write the failing test**

Add to `tests/test_et_bench.py`:

```python
def test_fix_c2_call_expression_rhs_field_assign():
    """Fix C2/C3: obj->field = func_call() where func_call returns from global array."""
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser

    source = b'''
    typedef void (*transform_fn)(void);

    struct ops {
        const char *name;
        transform_fn transform;
    };

    static void my_transform(void) {}

    static const struct ops my_impl = {
        "generic", my_transform,
    };

    static const struct ops *const impls[] = {
        &my_impl,
    };

    static const struct ops *get_ops(void) {
        return impls[0];
    }

    struct ctx {
        const struct ops *ops;
    };

    void use_ops(struct ctx *ctx) {
        const struct ops *ops = ctx->ops;
        if (ops && ops->transform)
            ops->transform();
    }

    void init(struct ctx *ctx) {
        ctx->ops = get_ops();
    }
    '''
    lang = Language(tsc.language())
    parser = Parser(lang)
    tree = parser.parse(source)

    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    st = SymbolTable()
    for func in extract_functions(tree, "test.c"):
        st.add_function(func)
    df = VariableState()

    graph = run_all_analyses({"test.c": tree}, st, df)
    pairs = {(e.caller, e.callee) for e in graph.edges}
    assert ('use_ops', 'my_transform') in pairs, \
        f"Missing use_ops -> my_transform. Got: {pairs}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_c2_call_expression_rhs_field_assign -v`

Expected: FAIL — `use_ops -> my_transform` edge not found.

- [ ] **Step 3: Implement C2 — call_expression RHS in `_track_pointer_field_assignments`**

In `src/ethunter/analyzer/initializer_assign.py`, in `_track_pointer_field_assignments._visit`, extend the RHS handling. Replace the `if var_name and field_name and rhs.type == 'identifier' and rhs.text:` block with:

```python
                    if var_name and field_name and rhs.type == 'identifier' and rhs.text:
                        raw_name = rhs.text.decode('utf-8')
                        resolved = resolutions.get(var_name, var_name)
                        if raw_name in symbol_names:
                            dataflow.assign(f'<gstruct:{resolved}.{field_name}>', raw_name)
                        elif raw_name in param_mappings:
                            for t in param_mappings[raw_name]:
                                dataflow.assign(f'<gstruct:{resolved}.{field_name}>', t)
                    # NEW (Fix C2): obj->field = func_call()
                    elif var_name and field_name and rhs.type == 'call_expression':
                        _handle_call_expr_rhs(rhs, var_name, field_name, resolutions, dataflow, symbol_names)
```

Add the helper function before `_visit`:

```python
        def _handle_call_expr_rhs(call_node, var_name, field_name, resolutions, dataflow, symbol_names):
            """Handle obj->field = callee() by scanning callee for return sources."""
            func_node = call_node.child_by_field_name('function') or call_node.children[0]
            if not func_node or not func_node.text:
                return
            callee_name = func_node.text.decode('utf-8')

            # Find the callee's function definition in the same tree
            callee_body = _find_function_body(tree.root_node, callee_name)
            if not callee_body:
                return

            # Collect return sources from callee body (depth-limited)
            return_sources = _collect_return_sources(callee_body)

            resolved_var = resolutions.get(var_name, var_name)
            for source_name in return_sources:
                # Check if source is a global array with entries
                garray_targets = dataflow.resolve(f'<garray:{source_name}>')
                if garray_targets:
                    for struct_name in garray_targets:
                        field_targets = dataflow.resolve(f'<gstruct:{struct_name}.{field_name}>')
                        for t in field_targets:
                            dataflow.assign(f'<gstruct:{resolved_var}.{field_name}>', t)
                # Check if source is a direct struct name
                else:
                    field_targets = dataflow.resolve(f'<gstruct:{source_name}.{field_name}>')
                    for t in field_targets:
                        dataflow.assign(f'<gstruct:{resolved_var}.{field_name}>', t)

        def _find_function_body(root, func_name):
            """Find the body node of a function definition by name."""
            result = [None]
            def _scan(n):
                if result[0]:
                    return
                if n.type == 'function_definition':
                    decl = None
                    for c in n.children:
                        if c.type == 'function_declarator':
                            decl = c
                            break
                    if decl:
                        for c in decl.children:
                            if c.type == 'identifier' and c.text:
                                if c.text.decode('utf-8') == func_name:
                                    body = n.child_by_field_name('body')
                                    if body:
                                        result[0] = body
                                    return
                for child in n.children:
                    _scan(child)
            _scan(root)
            return result[0]

        def _collect_return_sources(body):
            """Collect identifiers referenced in return statements within a function body."""
            sources = set()
            def _scan(n):
                if n.type == 'return_statement':
                    for c in n.children:
                        if c.type == 'identifier' and c.text:
                            sources.add(c.text.decode('utf-8'))
                        elif c.type == 'pointer_expression' and c.children:
                            inner = c.children[-1]
                            if inner.type == 'identifier' and inner.text:
                                sources.add(inner.text.decode('utf-8'))
                        elif c.type == 'subscript_expression' and c.children:
                            base = c.children[0]
                            if base.type == 'identifier' and base.text:
                                sources.add(base.text.decode('utf-8'))
                for child in n.children:
                    _scan(child)
            _scan(body)
            return sources
```

- [ ] **Step 4: Implement C3 — `field_call` integration with `local_fp_tracker`**

In `src/ethunter/analyzer/field_call.py`, add import at top:
```python
from ethunter.analyzer.local_fp_tracker import collect_local_fp_assignments
```

In Pass 2's `_visit` function, add before the loop of suffix/fallback lookups (around line 96, before `targets = set()`):

```python
                if field_path:
                    # Try local_fp_tracker mapping first (Fix C3)
                    local_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names)
                    base_name = field_path.split('.')[0]
                    if base_name in local_mapping:
                        targets = local_mapping[base_name].copy()
                    else:
                        targets = set()
```

Replace the line `targets = set()` (at original line ~98) with the above block — i.e., initialize `targets = set()` as the else branch.

- [ ] **Step 5: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_fix_c2_call_expression_rhs_field_assign -v`

Expected: PASS

- [ ] **Step 6: Verify no regression**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py tests/test_benchmark.py -v`

Expected: All existing tests PASS

- [ ] **Step 7: Commit**

```bash
git add src/ethunter/analyzer/initializer_assign.py src/ethunter/analyzer/field_call.py tests/test_et_bench.py
git commit -m "feat: call_expression RHS handling in field assignments + field_call local var integration

C2: Track obj->field = callee() by scanning callee for return sources from
global arrays. C3: Integrate local_fp_tracker mapping in field_call Pass 2
for local var -> field call resolution.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Final verification — 100% recall check

**Files:**
- None (verification only)

- [ ] **Step 1: Run full ET-Bench report**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -s 2>&1 | tail -20`

Expected: `fnptr-global-struct-array` recall = 100.00%. OVERALL recall >= 90.60% (should be higher).

- [ ] **Step 2: Run all tests**

Run: `.venv/bin/python -m pytest tests/ -q`

Expected: All tests PASS.

- [ ] **Step 3: Run fnptr-struct full recall test**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_fnptr_struct_full_recall -v`

Expected: PASS (fnptr-struct still at 100%).

- [ ] **Step 4: if recall is 100%, commit any final state**

```bash
git add -A
git diff --cached --stat
# Only commit if there are lingering changes
```
