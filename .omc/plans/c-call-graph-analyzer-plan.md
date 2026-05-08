# Plan: C Source Code Call Graph Analyzer (ethunter)

## RALPLAN-DR Summary

### Principles (3-5)
1. **Zero compilation dependency** — pure source-level analysis, no build system required
2. **One analyzer per pattern** — each indirect call scenario has an isolated, testable module
3. **Direct calls = 100% precision** — no false negatives for direct function calls
4. **Conservative over-approximation for indirect calls** — when uncertain, include all possible targets rather than miss a valid one
5. **Modular, composable output** — JSON is the canonical output; DOT is a serialization format derived from the same data model

### Decision Drivers
1. **C parser selection** — must handle full C syntax + cross-file #include without compilation config
2. **Indirect call resolution strategy** — how to track function pointer data flow across the AST
3. **Project architecture** — monolithic vs. modular analyzer pipeline

### Viable Options

#### Option A: tree-sitter as parser (Recommended)
**Approach:** Use tree-sitter-c grammar for parsing, build a custom semantic layer on top for function pointer tracking and cross-file resolution.
**Pros:**
- No compilation dependency — pure syntax parsing
- High fault tolerance (handles incomplete/buggy C code)
- Excellent Python binding (`tree-sitter` + `py-tree-sitter`)
- Incremental parsing support (useful for large projects)
- Active community, well-maintained C grammar
**Cons:**
- Syntax-only (no built-in semantic analysis) — we must build our own symbol resolution, type inference, and #include handling
- More work to implement cross-file analysis (#include tracking, symbol table)
- No built-in macro expansion (must handle preprocessor separately or skip)

#### Option B: pycparser as parser
**Approach:** Use pycparser to generate full AST with type information.
**Pros:**
- Produces typed AST — function signatures, parameter types available
- Pure Python, no system dependencies beyond pycparser itself
- Supports C99 well
**Cons:**
- Requires fake libc headers (ships with ~200 lines of pre-declarations)
- Fails on GNU C extensions (`__attribute__`, `__builtin_*`, etc.) without preprocessing
- #include resolution requires providing all header search paths
- Slower on large codebases (single-threaded)
- Known issues with variadic macros and complex preprocessor usage

#### Option C: libclang as parser
**Approach:** Use clang's Python binding for full semantic analysis.
**Pros:**
- Strongest semantic analysis out of the box
- Handles macros, typedefs, and complex C extensions natively
- Cross-file resolution is built-in
**Cons:**
- Requires LLVM system library installation
- User explicitly rejected compilation-dependent approaches
- Needs compile_commands.json or explicit include paths for correct results
- Fails on projects that cannot compile

### Decision: Option A (tree-sitter)
**Rationale for rejection of B and C:**
- **Option B (pycparser):** Fails on projects using GNU C extensions (extremely common in embedded/kernel code), requires manual fake header management. The user's requirement for "zero compilation dependency" and "automatic project scanning" conflicts with pycparser's need for preprocessor setup.
- **Option C (libclang):** Directly contradicts the user's explicit requirement of no compilation dependency. LLVM dependency is system-level and non-trivial to install.

### Pre-mortem (3 failure scenarios)
1. **tree-sitter's syntax-only nature misses critical semantic information**: We build a symbol table and type inference layer, but edge cases in C's type system (e.g., complex typedef chains, struct/union disambiguation) could cause false negatives in indirect call detection. Mitigation: build extensive test fixtures for each edge case pattern.
2. **Cross-file #include resolution produces incomplete symbol table**: tree-sitter doesn't resolve #includes automatically. We use a union-of-all-declarations strategy — collect all top-level declarations from every .c/.h file and treat them as globally visible. This is conservative but safe.
3. **Function pointer data flow analysis produces too many false positives**: Conservative over-approximation could produce call graphs with too many spurious edges, making the output less useful. Mitigation: refine data flow precision incrementally — start conservative, then add narrowing passes (e.g., type-based filtering of function pointer targets).

## Requirements Summary

- **Input**: C project directory, automatic scanning of all .c/.h files
- **Output**: JSON (default) and DOT (optional) call graph
- **Query**: Callers/callees lookup by function name
- **Coverage**: 100% direct calls, high-coverage indirect calls across 12 distinct patterns
- **Constraint**: No compilation dependency, no build system config required
- **Scale**: Small projects (thousands of lines) primary; medium (100-500K lines) secondary

## Acceptance Criteria

- [ ] Direct calls: 0 false negatives for standard function calls (`foo()`, `obj->method()`)
- [ ] Indirect calls: Each of the 10 mandatory patterns has a dedicated analyzer module with ≥1 passing test fixture
- [ ] JSON output: Valid JSON containing `functions` array and `edges` array with caller/callee/type fields
- [ ] DOT output: Valid Graphviz DOT syntax, renderable to image
- [ ] Query: `query_callers(func_name)` and `query_callees(func_name)` return correct results
- [ ] Test coverage: ≥1 fixture per indirect call pattern, all passing
- [ ] Benchmark: Runs on cJSON (≈800 LOC, single file), direct call edges match manual analysis with ≥95% recall

## Implementation Steps

### Phase 0: Project Setup

1. Initialize Python project with `pyproject.toml`
   - Dependencies: `tree-sitter` (≥0.21), `tree-sitter-c` (latest), `pytest`
   - Structure as defined in spec
2. Create project scaffolding with empty modules

### Phase 1: Core Data Model & Parser

3. Implement `graph/model.py` — `Function`, `CallEdge`, `CallGraph` dataclasses
4. Implement `parser/scanner.py` — recursive file discovery (glob `.c`/`.h`), exclude common non-source dirs (`.git`, `build/`)
5. Implement `parser/preprocessor.py` — union-of-all-declarations strategy: parse `#include` directives for dependency tracking, but for symbol resolution collect ALL top-level declarations from every .c/.h file regardless of `#ifdef` guards. Functions inside dead `#ifdef` branches will appear in the output — document as known limitation
6. Implement `parser/ast_builder.py` — tree-sitter integration: parse each .c/.h file, build AST trees, cache results. Strip/ignore `__attribute__` and compiler-specific builtin nodes during traversal

### Phase 2: Shared Data Flow Engine

7. Implement `analyzer/dataflow.py` — shared variable state tracker for function pointer data flow
   - `VariableState`: maps each variable to the set of possible function targets
   - Processes statements in order within a function body
   - Handles assignment propagation (`fp2 = fp1` merges target sets)
   - Handles conditional assignment (over-approximation: union of all branches)
   - Provides cross-function analysis via the global symbol table

### Phase 3: Symbol Table Construction

8. Implement `analyzer/symbol_table.py` — extract all function declarations/definitions from the AST, build a project-wide symbol table mapping function names to their declarations (file, line, signature)
9. Implement type tracking for function pointers: record typedef definitions, struct/union member types, variable types from declarations

### Phase 4: Direct Call Analyzer

10. Implement `analyzer/direct_call.py` — walk each function body's AST, find all function call nodes, look up in symbol table, emit `CallEdge` entries
11. Write test fixtures for direct calls: simple calls, nested calls, cross-file calls

### Phase 5: Indirect Call Analyzers (Modules 2-13)

Each analyzer module follows the same interface: `analyze(ast_root, symbol_table, dataflow) -> list[CallEdge]`. Analyzers are separate modules for testability but share the `dataflow.VariableState` for cross-pattern function pointer tracking.

11. `analyzer/fp_assign.py` — Module 2: function pointer assignment + call
    - Track `Identifier = FunctionRef` assignments
    - Track `Identifier(args)` calls where Identifier is a function pointer
12. `analyzer/callback_param.py` — Module 3: callbacks as parameters
    - Detect function pointer parameters in function signatures
    - Track call sites where functions are passed as arguments to such parameters
13. `analyzer/fp_return.py` — Module 4: function pointer return values
    - Detect functions that return function pointer types
    - Track `func_returning_fp()(args)` call patterns
14. `analyzer/fp_array.py` — Module 5: function pointer arrays / dispatch tables
    - Detect array initializers containing function references
    - Track `array[index](args)` call patterns
15. `analyzer/vtable.py` — Module 6: struct function pointer members (vtable)
    - Detect struct type definitions with function pointer members
    - Track struct initialization with function references
    - Track `struct_ptr->member(args)` call patterns
16. `analyzer/callback_reg.py` — Module 7: callback registration
    - Detect `register_callback(func_ptr)` style calls
    - Maintain a global registry of registered callbacks
    - Emit edges from all registration sites to all registered callbacks
17. `analyzer/union_fp.py` — Module 8: union function pointers
    - Detect union types with function pointer members
    - Track union initialization and member access patterns
18. `analyzer/typedef_fp.py` — Module 9: typedef-hidden function pointers
    - Unwrap typedef chains to detect underlying function pointer types
    - Apply same analysis as fp_assign but with typedef-aware type resolution
19. `analyzer/fp_alias.py` — Module 10: function pointer aliasing / redirection
    - Track `fp2 = fp1` assignments (pointer-to-pointer assignment chains)
    - Propagate possible targets along the alias chain
20. `analyzer/lazy_init.py` — Module 11: lazy initialization of function pointers
    - Detect static/global function pointers with NULL initial value
    - Track conditional assignment (`if (!fp) fp = default_handler`)
    - Track subsequent calls through the lazily-initialized pointer
21. `analyzer/macro_fp.py` — Module 12: macro-generated function pointer operations (partial support)
    - Parse `#define` directives that expand to function pointer operations
    - Handle macro concatenation patterns (e.g., `CONCAT(handler_, TYPE)`)
    - Limitation: only handles static, non-recursive macros
22. `analyzer/dlsym_fp.py` — Module 13: dlopen/dlsym hardcoded strings (partial support)
    - Detect `dlsym(handle, "function_name")` calls with string literal arguments
    - Match `"function_name"` against the global function symbol table
    - Emit indirect edges from the dlsym call site to the matched function

### Phase 6: Query Interface & Output

23. Implement `analyzer/orchestrator.py` — runs all analyzer modules, merges results into a single CallGraph
24. Implement `query/engine.py` — callers/callees lookup by function name
25. Implement `output/json.py` — serialize CallGraph to JSON
26. Implement `output/dot.py` — serialize CallGraph to Graphviz DOT format

### Phase 7: CLI & Integration

27. Implement `cli.py` — command-line entry point: `ethunter <project_dir> [--format json|dot] [--query <func_name>]`
28. Wire together: scan → parse → symbol table → dataflow → all analyzers → merge → output

### Phase 8: Testing

29. Create `tests/fixtures/` — one C file per indirect call pattern
30. Write `tests/test_*.py` — unit tests for each analyzer module
31. Write `tests/test_integration.py` — end-to-end test: parse a multi-file C project, verify call graph
32. Benchmark test on an open-source C project (e.g., cJSON, klib)

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                     CLI Entry                         │
│  ethunter <project_dir> [--format json|dot]           │
│  ethunter <project_dir> --query <func_name>           │
├──────────────────────────────────────────────────────┤
│                   Scanner                              │
│  Recursive .c/.h discovery, exclude build dirs        │
├──────────────────────────────────────────────────────┤
│                   Parser                               │
│  tree-sitter AST + conservative #include union        │
├──────────────────────────────────────────────────────┤
│                 Symbol Table                           │
│  All functions, typedefs, structs, function pointers  │
├──────────────────────────────────────────────────────┤
│              Data Flow Engine                          │
│  Shared VariableState for fp target tracking          │
├────────────┬────────────┬────────────┬───────────────┤
│ Direct Call│ 12 Indirect Call Analyzer Modules        │
│ Analyzer   │ fp_assign, callback_param, fp_return,    │
│            │ fp_array, vtable, callback_reg,          │
│            │ union_fp, typedef_fp, fp_alias,          │
│            │ lazy_init, macro_fp, dlsym_fp            │
│            │ (all share dataflow.VariableState)       │
├────────────┴────────────┴────────────┴───────────────┤
│                   Orchestrator                         │
│  Merge all CallEdge lists, deduplicate                │
├───────────────────────┬──────────────────────────────┤
│   Query Engine        │       Output                  │
│   callers/callees     │   JSON / DOT                  │
└───────────────────────┴──────────────────────────────┘
```

## Key Technical Decisions

### tree-sitter AST Traversal Strategy
- Use tree-sitter's `walk()` API for depth-first AST traversal
- Node types of interest: `function_definition`, `call_expression`, `assignment_expression`, `declaration`, `init_declarator`, `struct_specifier`, `type_definition`
- Build a visitor pattern: each analyzer module is a visitor that extracts relevant nodes

### Cross-File Symbol Resolution
1. First pass: scan ALL .c/.h files, extract all top-level function declarations and definitions into a global symbol table
2. Second pass: for each function body, resolve call targets against the global symbol table
3. For function pointers: build a project-wide assignment graph of function pointer variables, then propagate possible targets along the graph

### Function Pointer Data Flow Analysis
- Build a `VariableState` map: for each variable, track the set of possible function targets
- Process statements in order (within a function body) for sequential analysis
- For cross-function analysis: use the global symbol table and function signatures
- Handle conservative over-approximation: if a function pointer's possible targets cannot be determined, include all functions with compatible signature types

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| tree-sitter C grammar doesn't cover all GNU C extensions | Medium | Use grammar overrides; fall back to treating unknown constructs as opaque |
| Cross-file symbol resolution produces false positives (same function name in different files) | Medium | Use full path+name for disambiguation; warn on duplicates |
| Function pointer alias chains across multiple files are too complex to resolve | Low | Conservative over-approximation: include all possible targets; document limitation |
| Macro expansion for macro_fp is incomplete | Low | Document as "partial support"; focus on common patterns |
| Large project performance (500K+ lines) | Low | Not a primary target; optimize hot paths after correctness is verified |

## Verification Steps

1. Unit tests for each analyzer module with dedicated C fixtures
2. Integration test: multi-file C project with direct + indirect calls
3. Manual benchmark: run on cJSON (small, well-known), verify direct calls match source
4. Output validation: verify JSON schema and DOT syntax correctness
5. Query validation: known function names → verify callers/callees lists

## Risks and Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| tree-sitter's syntax-only nature misses semantic info needed for indirect call analysis | High | Medium | Build robust symbol table and type inference layer; extensive test fixtures |
| #include resolution without compilation config is incomplete | Medium | High | Conservative union of all project declarations; over-approximate |
| Function pointer over-approximation produces too many spurious edges | Medium | Medium | Add type-based narrowing pass; iterate on precision |
| `__attribute__` and compiler-specific extensions interfere with AST traversal | Medium | High | Strip/ignore opaque nodes during parser traversal |

## Expanded Test Plan

### Unit Tests
- Each analyzer module: ≥3 test fixtures covering happy path, edge cases, and negative cases
- Symbol table: test function declaration extraction, typedef resolution, struct member types
- Scanner: test file discovery with various directory structures

### Integration Tests
- Multi-file C project with cross-file calls (direct and indirect)
- Project with macros, typedefs, and mixed C99/GNU C extensions
- End-to-end: CLI → JSON output → validate schema and content

### Performance Tests
- Run on projects of increasing size (1K, 10K, 100K lines)
- Measure parsing time, analysis time, memory usage

## ADR

**Decision:** Use tree-sitter as the C parser with a modular analyzer architecture featuring a shared data flow engine.

**Drivers:**
1. Zero compilation dependency (user requirement)
2. Full automatic project scanning without build config (user requirement)
3. Need to support 12 distinct indirect call patterns with dedicated modules
4. Python implementation for development speed

**Alternatives considered:**
- pycparser: Rejected — fails on GNU C extensions, requires fake header management, needs preprocessor setup
- libclang: Rejected — requires LLVM system library, contradicts zero-compilation-dependency requirement

**Why chosen:** tree-sitter is the only parser that satisfies the zero-compilation-dependency constraint while having a well-maintained C grammar and Python bindings. The shared data flow engine resolves the Architect's concern that isolated analyzer modules cannot share function pointer state across patterns.

**Consequences:**
- Must build custom semantic analysis layer (symbol table, type inference, data flow) on top of syntax-only parsing
- Conservative over-approximation may produce spurious edges in indirect call analysis
- `#ifdef` dead code will appear in the output (documented limitation)

**Follow-ups:**
- After correctness is verified, add a type-based narrowing pass to reduce false positives in indirect call analysis
- Consider adding compile_commands.json as an optional input for users who want higher precision (without making it required)

## Applied Improvements (Architect + Critic feedback)

1. Added `analyzer/dataflow.py` as a shared variable state engine (Architect: cross-pattern data flow sharing)
2. Changed analyzer interface to `analyze(ast_root, symbol_table, dataflow)` (Architect: shared state access)
3. Clarified preprocessor strategy as union-of-all-declarations (Architect: aligns with zero-compilation-dependency)
4. Added `__attribute__` / compiler extension handling to parser phase (Critic: missing consideration)
5. Tightened benchmark acceptance criteria to "cJSON, ≥95% recall" (Critic: vague acceptance criteria)
6. Pinned tree-sitter dependency versions (Critic: API breaking change risk)
