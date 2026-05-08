# Implementation Plan: Enhance Test Coverage for ethunter

## Overview

This plan addresses two work items from the deep-interview spec at `.omc/specs/deep-interview-test-coverage.md`:

1. **Strengthen existing analyzer tests**: Add 1-2 complex scenario tests + 2 cross-file combination tests for each of the 13 analyzer modules.
2. **Add real-project benchmarks**: Download cJSON (lightweight) and libuv (medium) as benchmark projects with ground truth call graphs.

**Acceptance criteria**: Each of the 13 analyzers has at least 3 test cases; direct call recall 100% on both benchmarks; indirect call recall >=80%; all tests pass; benchmark ground truth JSON files exist under `tests/benchmark/`; integration test runs full scan and compares against ground truth.

**Current state**: 16 tests in `tests/test_analyzers.py`, 13 `.c` fixtures in `tests/fixtures/`, empty `tests/benchmark/` and `benchmarks/` directories. All 16 tests currently pass.

---

## Phase 1: Fix PYTHONPATH Issue in Test Configuration

### Problem

`pyproject.toml` does not configure the test runner to find `src/ethunter/`. Tests only pass when `PYTHONPATH=src` is set manually.

### Changes

- **File**: `/home/admin/cc/wksp/ethunter/pyproject.toml`
- Add `pythonpath = ["src"]` under `[tool.pytest.ini_options]`
- Alternatively, add a `setup.cfg` or switch to editable install (`pip install -e .`)

### Files to create/modify

- `pyproject.toml` (modify)

---

## Phase 2: Complex Scenario Fixtures (13 analyzers x 1-2 complex tests)

Each analyzer gets 1-2 new `.c` fixture files that exercise more complex patterns than the existing minimal fixtures. The complex scenarios target:

- Multiple function pointer variables with different targets
- Conditional assignments (if/else, ternary, switch)
- Nested or chained patterns
- Edge cases (NULL checks, reassignments, multi-parameter callbacks)

### 2.1 Direct Call Analyzer (`direct_call.py`)

**File to create**: `tests/fixtures/direct_call_complex.c`

**Content pattern**: A file with 6-8 functions including:
- Indirect-looking calls that are actually direct (e.g., `func_ptr` where `func_ptr` is a local variable name, not a function -- should NOT be detected)
- Nested function call chains: `a()` calls `b()` which calls `c()`, plus `a()` calls `d()` directly
- A function with multiple direct calls in sequence
- Calls within loops and conditionals

**Test to add in `test_analyzers.py`**: `test_direct_call_complex()` -- verify all 6+ expected direct edges are found, no spurious edges.

### 2.2 Function Pointer Assign (`fp_assign.py`)

**File to create**: `tests/fixtures/fp_assign_complex.c`

**Content pattern**: Multiple fp variables with conditional reassignment:
```c
void handler_a(void) {}
void handler_b(void) {}
void handler_c(void) {}

void dispatch(int mode) {
    void (*fp)(void) = handler_a;
    if (mode > 0) {
        fp = handler_b;
    } else {
        fp = handler_c;
    }
    fp();
    void (*fp2)(void) = fp;  // alias chain
    fp2();
}
```

**Test**: `test_fp_assign_complex()` -- expect edges to handler_a, handler_b, handler_c (conservative over-approximation), plus alias chain targets.

### 2.3 Callback Param (`callback_param.py`)

**File to create**: `tests/fixtures/callback_param_complex.c`

**Content pattern**: Multiple callback parameters, nested callback passing:
```c
typedef void (*cb_t)(int);
typedef void (*cb2_t)(void);

void inner_handler(int x) {}
void outer_handler(int x) {}

void execute(cb_t cb, int val) { cb(val); }
void wrapper(cb2_t cb) { execute(inner_handler, 42); }

int main(void) {
    execute(inner_handler, 1);
    execute(outer_handler, 2);
    wrapper(/* passing callback */);
    return 0;
}
```

**Test**: `test_callback_param_complex()` -- verify inner_handler and outer_handler are both detected as callback targets.

### 2.4 Function Pointer Return (`fp_return.py`)

**File to create**: `tests/fixtures/fp_return_complex.c`

**Content pattern**: Multiple return-value functions with switch-based selection:
```c
typedef void (*action_t)(void);
void action_read(void) {}
void action_write(void) {}
void action_delete(void) {}

action_t get_action(const char *op) {
    if (op[0] == 'r') return action_read;
    if (op[0] == 'w') return action_write;
    return action_delete;
}

int main(void) {
    get_action("read")();
    get_action("write")();
    return 0;
}
```

**Test**: `test_fp_return_complex()` -- all three actions should be detected as potential targets.

### 2.5 Function Pointer Array (`fp_array.py`)

**File to create**: `tests/fixtures/fp_array_complex.c`

**Content pattern**: Multiple dispatch tables, 2D array, named indices:
```c
void cmd_create(void) {}
void cmd_read(void) {}
void cmd_update(void) {}
void cmd_delete(void) {}

void (*cmd_table[])(void) = { cmd_create, cmd_read, cmd_update, cmd_delete };

enum { CREATE=0, READ, UPDATE, DELETE };

int process(int cmd) {
    cmd_table[cmd]();
    cmd_table[READ]();  // constant index
    return 0;
}

int main(void) {
    process(CREATE);
    return 0;
}
```

**Test**: `test_fp_array_complex()` -- all four commands should be detected.

### 2.6 Vtable (`vtable.py`)

**File to create**: `tests/fixtures/vtable_complex.c`

**Content pattern**: Multiple struct vtables, partial initialization:
```c
struct device {
    int (*open)(void);
    int (*close)(void);
    int (*read)(char *buf, int len);
};

int net_open(void) { return 0; }
int net_close(void) { return 0; }
int net_read(char *buf, int len) { return 0; }

int disk_open(void) { return 0; }
int disk_close(void) { return 0; }

int main(void) {
    struct device net;
    net.open = net_open;
    net.close = net_close;
    net.read = net_read;
    net.open();

    struct device disk;
    disk.open = disk_open;
    disk.close = disk_close;
    disk.open();  // partial init, read not assigned
    return 0;
}
```

**Test**: `test_vtable_complex()` -- net_open, net_close, net_read, disk_open, disk_close detected.

### 2.7 Callback Registration (`callback_reg.py`)

**File to create**: `tests/fixtures/callback_reg_complex.c`

**Content pattern**: Multiple registration sites with different callback function names:
```c
typedef void (*hook_t)(int);

void register_hook(hook_t h) {}
void register_exit_hook(hook_t h) {}

void on_connect(int fd) {}
void on_disconnect(int fd) {}
void on_error(int fd) {}
void cleanup(int fd) {}

int main(void) {
    register_hook(on_connect);
    register_hook(on_disconnect);
    register_exit_hook(cleanup);
    return 0;
}
```

**Test**: `test_callback_reg_complex()` -- on_connect, on_disconnect, cleanup all in registered_callbacks.

### 2.8 Union Function Pointer (`union_fp.py`)

**File to create**: `tests/fixtures/union_fp_complex.c`

**Content pattern**: Multiple union types, nested struct with union member:
```c
typedef void (*action_simple)(void);
typedef void (*action_param)(int);

union operation {
    action_simple simple;
    action_param with_param;
};

void op_init(void) {}
void op_process(int x) {}
void op_finish(void) {}

int main(void) {
    union operation op;
    op.simple = op_init;
    op.simple();
    op.with_param = op_process;
    op.with_param(42);
    return 0;
}
```

**Test**: `test_union_fp_complex()` -- op_init and op_process both detected.

### 2.9 Typedef Function Pointer (`typedef_fp.py`)

**File to create**: `tests/fixtures/typedef_fp_complex.c`

**Content pattern**: Multiple typedef layers, typedef of typedef:
```c
typedef void (*base_handler)(void);
typedef base_handler handler_wrapper;

void handle_request(void) {}
void handle_response(void) {}

int main(void) {
    handler_wrapper hw = handle_request;
    hw();
    hw = handle_response;
    hw();
    return 0;
}
```

**Test**: `test_typedef_fp_complex()` -- handle_request and handle_response detected via typedef chain.

### 2.10 Function Pointer Alias (`fp_alias.py`)

**File to create**: `tests/fixtures/fp_alias_complex.c`

**Content pattern**: Multi-level alias chains (3+ levels):
```c
void target_one(void) {}
void target_two(void) {}

int main(void) {
    void (*fp1)(void) = target_one;
    void (*fp2)(void) = fp1;
    void (*fp3)(void) = fp2;
    void (*fp4)(void) = target_two;
    void (*fp5)(void) = fp3;  // fp5 -> fp3 -> fp2 -> fp1 -> target_one
    fp5();
    fp4();
    return 0;
}
```

**Test**: `test_fp_alias_complex()` -- target_one (via chain) and target_two both detected.

### 2.11 Lazy Init (`lazy_init.py`)

**File to create**: `tests/fixtures/lazy_init_complex.c`

**Content pattern**: Multiple lazy-initialized pointers, nested lazy init:
```c
static void (*primary_handler)(void) = (void *)0;
static void (*secondary_handler)(void) = (void *)0;

void default_primary(void) {}
void custom_primary(void) {}
void default_secondary(void) {}

void init_primary(int use_custom) {
    if (!primary_handler) {
        if (use_custom) {
            primary_handler = custom_primary;
        } else {
            primary_handler = default_primary;
        }
    }
}

int main(void) {
    init_primary(1);
    primary_handler();
    return 0;
}
```

**Test**: `test_lazy_init_complex()` -- custom_primary and default_primary both detected (conservative).

### 2.12 Macro Function Pointer (`macro_fp.py`)

**File to create**: `tests/fixtures/macro_fp_complex.c`

**Content pattern**: Multiple macros, macro chaining, macro with multiple function references:
```c
#define CALL_ONE(fn) fn()
#define CALL_BOTH(a, b) a(); b()
#define DISPATCH(fn, val) fn(val)

void handler_x(void) {}
void handler_y(void) {}
void handler_z(int v) {}

int main(void) {
    CALL_ONE(handler_x);
    CALL_BOTH(handler_x, handler_y);
    return 0;
}
```

**Test**: `test_macro_fp_complex()` -- handler_x and handler_y both detected from macro expansion.

### 2.13 Dlsym Function Pointer (`dlsym_fp.py`)

**File to create**: `tests/fixtures/dlsym_fp_complex.c`

**Content pattern**: Multiple dlsym calls with different symbol strings:
```c
void plugin_start(void) {}
void plugin_stop(void) {}
void plugin_config(int v) {}

int main(void) {
    void *h = dlopen("libplugin.so", 1);
    void (*start_fn)(void) = dlsym(h, "plugin_start");
    void (*stop_fn)(void) = dlsym(h, "plugin_stop");
    start_fn();
    return 0;
}
```

**Test**: `test_dlsym_fp_complex()` -- plugin_start and plugin_stop detected from dlsym string literals.

---

## Phase 3: Cross-File Combination Tests (13 analyzers x 2 cross-file tests)

Each analyzer gets 2 cross-file test pairs. These use a `.c` caller file and a `.h` or `.c` callee file to test that the analyzer works when the caller and callee are in separate translation units.

### Directory structure

```
tests/fixtures/cross_file/
  direct_call/
    caller.c          # calls functions declared in callee.h
    callee.h           # declares helper functions
  fp_assign/
    caller.c
    callee.h
  ... (one subdirectory per analyzer)
```

### Cross-file test design

Each cross-file test uses a helper that loads multiple files, builds a shared `SymbolTable` and `VariableState`, then runs the analyzer on both files.

**Pattern for all cross-file tests**:

```python
def _make_cross_file_env(dir_name, files):
    """Create symbol_table + dataflow for cross-file fixture directory."""
    base = os.path.join(FIXTURES, 'cross_file', dir_name)
    trees = {}
    st = SymbolTable()
    df = VariableState()
    for f in files:
        path = os.path.join(base, f)
        tree = parse_file(path)
        trees[path] = tree
        for func in extract_functions(tree, f):
            st.add_function(func)
    return trees, st, df
```

### 3.1 Cross-file test pairs per analyzer

**direct_call**: `caller.c` declares `void helper(void);` and `void worker(int);`, defines `main()` that calls both. `callee.c` defines `helper()` and `worker()`. Two tests: (a) caller in .c, callee in .c; (b) caller in .c, callee declared in .h.

**fp_assign**: `caller.c` has `extern void (*get_fp)(void);` and `main()` that calls `get_fp()`. `callee.c` defines `void (*get_fp)(void) = actual_handler;` and `void actual_handler(void) {}`.

**callback_param**: `caller.c` calls `register(my_callback)` where `my_callback` is defined in `callee.c`. `callee.c` defines `my_callback(int)` and `register(callback_t)`.

**fp_return**: `caller.c` calls `get_handler()` (declared in `callee.h`) and invokes the result. `callee.c` defines `get_handler()` returning different function pointers.

**fp_array**: `caller.c` has `extern void (*table[])();` and calls `table[0]()`. `callee.c` defines the array populated with functions defined in the same file.

**vtable**: `caller.c` uses `extern struct driver d;` and calls `d.init()`. `callee.c` defines the struct instance with member assignments.

**callback_reg**: `caller.c` calls `register_callback(local_handler)`. `callee.c` defines `register_callback()` and `local_handler()`.

**union_fp**: `caller.c` declares union type and calls through union member. `callee.c` defines the union variable initialization.

**typedef_fp**: `caller.c` uses a typedef declared in `callee.h` to create a function pointer variable and call through it.

**fp_alias**: `caller.c` uses `extern void *fp1;` and creates `fp2 = fp1`. `callee.c` defines `fp1 = target_func`.

**lazy_init**: `caller.c` has the lazy init pattern for a handler declared `extern` in `callee.c`.

**macro_fp**: `caller.c` includes `callee.h` which has macro definitions, then uses the macro in `caller.c`.

**dlsym_fp**: `caller.c` has dlsym calls referencing functions declared in `callee.h` stubs.

### File: `tests/test_cross_file.py` (new)

This file contains all 26 cross-file tests (2 per analyzer). Tests are grouped by analyzer using pytest `mark.parametrize` or organized as separate test functions.

---

## Phase 4: Benchmark Project Setup

### 4.1 cJSON (Lightweight Benchmark)

**Source**: https://github.com/DaveGamble/cJSON
**Target version**: v1.7.18 (stable release, ~800 LOC in single `cJSON.c`)
**Files to download**: `cJSON.c`, `cJSON.h`

**Download strategy**:
1. Use `git clone --depth 1 --branch v1.7.18 https://github.com/DaveGamble/cJSON.git` into `benchmarks/cjson/`
2. Or download raw files: `cJSON.c` and `cJSON.h` from the release tag
3. Store in `tests/benchmark/cjson/` with only the core source files

**Ground truth analysis for cJSON**:

cJSON has a well-structured call graph:
- **Direct calls** (~30+): cJSON_Parse -> parse_value, parse_value -> parse_string/parse_number/parse_object/parse_array, etc.
- **Indirect calls**: cJSON uses a function pointer approach in the `parse_value` dispatcher based on token type

**Ground truth file**: `tests/benchmark/cjson/ground_truth.json`

```json
{
  "project": "cJSON",
  "version": "1.7.18",
  "source_files": ["cJSON.c"],
  "direct_edges": [
    {"caller": "cJSON_Parse", "callee": "parse_value"},
    {"caller": "parse_value", "callee": "parse_string"},
    {"caller": "parse_value", "callee": "parse_number"},
    {"caller": "parse_value", "callee": "parse_object"},
    {"caller": "parse_value", "callee": "parse_array"},
    ...
  ],
  "indirect_edges": [],
  "expected_direct_count": 35,
  "expected_indirect_count": 0
}
```

### 4.2 libuv (Medium Benchmark)

**Source**: https://github.com/libuv/libuv
**Target version**: v1.48.0 (stable release)
**Scope**: Core event loop module only (`src/unix/core.c`, `src/unix/loop.c`, `src/unix/fs.c` -- ~5K LOC subset)

**Download strategy**:
1. `git clone --depth 1 --branch v1.48.0 https://github.com/libuv/libuv.git` into `benchmarks/libuv/`
2. Extract the subset of files relevant to the core event loop:
   - `src/unix/core.c`, `src/unix/loop.c`, `src/unix/fs.c`
   - `include/uv.h`, `include/uv/unix.h`
   - `src/uv-common.c`
3. Store the subset in `tests/benchmark/libuv/`

**Ground truth analysis for libuv**:

libuv uses extensive callback registration and function pointer patterns:
- **Direct calls**: uv_run -> uv__run_timers, uv__run_idle, uv__run_prepare, etc.
- **Indirect calls**: Callback invocation via `handle->cb()`, `req->cb()`, function pointer arrays in `uv_loop_t`, callback registration patterns in `uv_timer_start()`, `uv_fs_open()`, etc.

**Ground truth file**: `tests/benchmark/libuv/ground_truth.json`

```json
{
  "project": "libuv",
  "version": "1.48.0",
  "source_files": ["core.c", "loop.c", "fs.c", "uv-common.c"],
  "direct_edges": [
    {"caller": "uv_run", "callee": "uv__run_timers"},
    {"caller": "uv_run", "callee": "uv__io_poll"},
    ...
  ],
  "indirect_edges": [
    {"caller": "uv__run_timers", "callee": "timer->cb", "kind": "callback_param"},
    ...
  ],
  "expected_direct_count": 80,
  "expected_indirect_count": 25
}
```

### 4.3 Ground truth generation approach

1. **Manual analysis**: Read the source code and document all call edges (direct and indirect) by hand
2. **Semi-automated verification**: Use the existing ethunter direct_call analyzer to get a baseline list of direct calls, then manually review for completeness
3. **Peer review**: Have the ground truth reviewed against the source code (two independent readings)
4. **JSON format**: Use the `CallEdge.to_dict()` schema for consistency

### 4.4 Benchmark integration test

**File**: `tests/test_benchmark.py` (new)

This test:
1. Runs the full ethunter pipeline (`run_all_analyses`) on each benchmark project
2. Compares output edges against ground truth JSON
3. Computes recall metrics:
   - Direct recall = (found direct edges that match ground truth) / (total ground truth direct edges)
   - Indirect recall = (found indirect edges that match ground truth) / (total ground truth indirect edges)
4. Asserts direct recall == 100%, indirect recall >= 80%

```python
def compute_recall(found_edges, expected_edges):
    """Compute recall: fraction of expected edges found in found set."""
    found_pairs = {(e.caller, e.callee) for e in found_edges}
    expected_pairs = {(e['caller'], e['callee']) for e in expected_edges}
    matched = found_pairs & expected_pairs
    return len(matched) / len(expected_pairs) if expected_pairs else 1.0
```

---

## Phase 5: Test File Organization

### Final directory structure

```
tests/
  fixtures/
    direct_call.c                    # existing
    fp_assign.c                      # existing
    callback_param.c                 # existing
    fp_return.c                      # existing
    fp_array.c                       # existing
    vtable.c                         # existing
    callback_reg.c                   # existing
    union_fp.c                       # existing
    typedef_fp.c                     # existing
    fp_alias.c                       # existing
    lazy_init.c                      # existing
    macro_fp.c                       # existing
    dlsym_fp.c                       # existing
    direct_call_complex.c            # NEW
    fp_assign_complex.c              # NEW
    callback_param_complex.c         # NEW
    fp_return_complex.c              # NEW
    fp_array_complex.c               # NEW
    vtable_complex.c                 # NEW
    callback_reg_complex.c           # NEW
    union_fp_complex.c               # NEW
    typedef_fp_complex.c             # NEW
    fp_alias_complex.c               # NEW
    lazy_init_complex.c              # NEW
    macro_fp_complex.c               # NEW
    dlsym_fp_complex.c               # NEW
    cross_file/                      # NEW directory
      direct_call/
        caller.c
        callee.c
        callee.h
      fp_assign/
        caller.c
        callee.c
      ... (12 more subdirectories)
  test_analyzers.py                  # MODIFIED: add 13 complex tests
  test_cross_file.py                 # NEW: 26 cross-file tests
  test_benchmark.py                  # NEW: benchmark integration tests
  conftest.py                        # NEW: shared fixtures
  benchmark/                         # NEW: populated
    cjson/
      cJSON.c
      cJSON.h
      ground_truth.json
    libuv/
      core.c
      loop.c
      fs.c
      uv-common.c
      uv.h
      ground_truth.json
```

### Test count summary

| Analyzer | Existing | Complex (NEW) | Cross-file (NEW) | Total |
|----------|----------|---------------|------------------|-------|
| direct_call | 1 | 1 | 2 | 4 |
| fp_assign | 1 | 1 | 2 | 4 |
| callback_param | 1 | 1 | 2 | 4 |
| fp_return | 1 | 1 | 2 | 4 |
| fp_array | 1 | 1 | 2 | 4 |
| vtable | 1 | 1 | 2 | 4 |
| callback_reg | 1 | 1 | 2 | 4 |
| union_fp | 1 | 1 | 2 | 4 |
| typedef_fp | 1 | 1 | 2 | 4 |
| fp_alias | 1 | 1 | 2 | 4 |
| lazy_init | 1 | 1 | 2 | 4 |
| macro_fp | 1 | 1 | 2 | 4 |
| dlsym_fp | 1 | 1 | 2 | 4 |
| Infrastructure | 3 | - | - | 3 |
| Benchmark | - | - | 2 | 2 |
| **Total** | **16** | **13** | **26** | **55+** |

---

## Phase 6: Implementation Sequence

### Step 1: Fix PYTHONPATH (30 min)
- Update `pyproject.toml` to add `pythonpath = ["src"]` to pytest config
- Verify all 16 existing tests still pass

### Step 2: Add complex scenario fixtures (2-3 hours)
- Create 13 `_complex.c` fixture files in `tests/fixtures/`
- Add 13 corresponding test functions to `tests/test_analyzers.py`
- Run tests incrementally as each fixture is created

### Step 3: Add cross-file fixtures (3-4 hours)
- Create `tests/fixtures/cross_file/` directory structure
- Create 26 cross-file fixture files (2 per analyzer)
- Create `tests/test_cross_file.py` with shared helper and 26 test functions
- Add `tests/conftest.py` with common fixtures

### Step 4: Download benchmark projects (1 hour)
- Clone cJSON v1.7.18 to `tests/benchmark/cjson/`
- Clone libuv v1.48.0 to `tests/benchmark/libuv/` (subset only)
- Verify files parse correctly with tree-sitter-c

### Step 5: Generate ground truth (4-6 hours)
- Manually analyze cJSON call graph, write `cjson/ground_truth.json`
- Manually analyze libuv core module call graph, write `libuv/ground_truth.json`
- Cross-verify with ethunter's existing direct call analyzer output

### Step 6: Write benchmark integration tests (2 hours)
- Create `tests/test_benchmark.py`
- Implement recall computation
- Add pytest assertions for 100% direct recall, >=80% indirect recall

### Step 7: Full test run and validation (30 min)
- Run `pytest -v` on entire test suite
- Verify all acceptance criteria are met

---

## Potential Challenges

1. **tree-sitter-c parsing of real projects**: cJSON and libuv may use C extensions or constructs that tree-sitter-c does not parse cleanly. May need to pre-process or skip problematic files.

2. **Cross-file symbol resolution**: The current `SymbolTable` does not automatically resolve `#include` directives. Cross-file tests must manually populate the symbol table from both files.

3. **Indirect call ground truth accuracy**: Manually determining all possible indirect call targets in libuv is error-prone. The conservative over-approximation nature of static analysis means the ground truth should list "definitely called" targets, not "possibly called."

4. **libuv scope selection**: libuv is a large project. Selecting the right subset of files is critical -- too few and the benchmark is trivial, too many and ground truth generation becomes infeasible.

5. **Macro expansion**: The `macro_fp` analyzer has partial support (reads raw macro text, does not expand). Complex macro scenarios may not be detectable, which is acceptable per the spec.

---

## Dependencies

- Python 3.11+ (confirmed available in `.venv`)
- tree-sitter-c >= 0.21 (confirmed in `pyproject.toml`)
- pytest >= 7.0 (confirmed)
- git (for benchmark project cloning)

No new package dependencies are required.

