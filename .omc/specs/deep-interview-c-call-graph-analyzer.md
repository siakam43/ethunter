# Deep Interview Spec: C Source Code Call Graph Analyzer

## Metadata
- Interview ID: di-c-callgraph-001
- Rounds: 10
- Final Ambiguity Score: 10%
- Type: greenfield
- Generated: 2026-05-07T11:12:30Z
- Threshold: 0.2
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.90 | 0.40 | 0.36 |
| Constraint Clarity | 0.90 | 0.30 | 0.27 |
| Success Criteria | 0.90 | 0.30 | 0.27 |
| **Total Clarity** | | | **0.90** |
| **Ambiguity** | | | **10%** |

## Goal

构建一个 Python 实现的 C 语言源码静态分析工具。第一个核心功能是**生成项目级别的全局调用图（Call Graph）**，支持直接调用和多种间接调用场景的精确解析。输出默认为 JSON 结构化数据，可选输出 DOT 格式供 Graphviz 渲染。提供查询接口：给定任意函数名，可查询其全部 callers（谁调用它）和 callees（它调用了谁）。

## Constraints

1. **实现语言**: Python
2. **输入**: 整个项目目录全自动扫描，递归发现所有 .c/.h 文件，自动处理 #include 依赖和跨文件引用。不需要用户提供 include path 或 compile_commands.json，不依赖任何构建系统配置
3. **输出**: 默认 JSON 结构化数据，可选 DOT 格式
4. **调用图范围**: 项目级别的全局调用图（所有函数间的调用关系），不关心调用频率
5. **直接调用**: 0 遗漏，必须 100% 覆盖
6. **间接调用必须支持**:
   - 函数指针直接赋值与调用
   - 回调函数（函数指针作为参数传递）
   - 函数指针作为返回值
   - 函数指针数组 / dispatch table
   - 结构体中的函数指针成员（vtable 风格）
   - 回调注册/注销模式（运行时注册，事件驱动调用）
   - 联合体中的函数指针
   - typedef 隐藏的函数指针类型
   - 函数指针别名/重定向（赋值链追踪）
   - 全局/静态函数指针的延迟初始化
7. **间接调用部分支持**:
   - 宏展开后产生的函数指针操作：尽可能支持静态分析可展开的宏
   - dlopen/dlsym 动态加载：仅解决硬编码字符串的场景（如 `dlsym(handle, "foo")` 中 "foo" 为字符串常量）
8. **性能**: 以小型项目（千~万行）为主，确保精确性；中大型项目（10-50万行）在精确性前提下优化性能；无需支持超大型项目（百万行以上）
9. **零编译依赖**: 工具不依赖目标项目的编译配置或编译过程。即使目标项目没有 Makefile、CMakeLists.txt 或无法编译通过，工具仍能正常分析

## Non-Goals

- 不支持 C++ 特性（模板、虚函数、lambda 等）
- 不关心调用频率或执行次数
- 不支持运行时动态加载（dlopen/dlsym）中非硬编码字符串的场景
- 不做数据流分析（变量生命周期、内存泄漏等）
- 不提供交互式 Web 可视化界面（第一期）

## Acceptance Criteria

- [ ] 对包含直接调用的 C 文件，生成 0 遗漏的调用图
- [ ] 对包含函数指针赋值+调用的 C 文件，正确识别间接调用边
- [ ] 对包含函数指针数组（dispatch table）的 C 文件，正确解析所有可能的调用目标
- [ ] 对包含结构体函数指针成员（vtable 风格）的 C 文件，正确解析调用关系
- [ ] 对包含回调注册模式的 C 文件，正确识别注册的回调函数
- [ ] 对包含宏展开后可解析的函数指针调用的 C 文件，正确解析
- [ ] 对包含 `dlsym(handle, "hardcoded_func_name")` 的 C 文件，能将硬编码字符串匹配到目标函数
- [ ] JSON 输出格式包含完整的函数节点和调用边信息
- [ ] 支持可选 DOT 格式输出，可被 Graphviz 渲染
- [ ] 支持查询：给定函数名 f，输出所有 callers 和所有 callees
- [ ] 在 Redis 或类似知名开源项目上运行，调用图结果经人工验证基本正确
- [ ] 提供一组小型手工测试用例，覆盖每种间接调用场景，逐一验证通过

## Assumptions Exposed & Resolved

| Assumption | Challenge | Resolution |
|------------|-----------|------------|
| "全部类型的间接调用"都支持 | C 中间接调用在理论上不可完全静态解析 | 明确列出 12 种场景，1-7/9/11-12 必须支持，8/10 部分支持 |
| 工具规模目标 | 大型项目性能需求不明确 | 小型项目为主，中大型项目优化性能，无需超大型 |
| 验证方式 | 仅靠人工审阅不够严格 | 同时采用已知项目 benchmark + 手工测试用例 |
| C 解析器选择 | 用户不希望依赖编译配置 | 零编译依赖，全自动项目扫描，不依赖 Makefile/CMake/clang |
| 跨文件分析 | 如何处理 #include 和项目范围 | 全自动扫描整个项目目录，递归发现 .c/.h 并处理 #include 依赖 |

## Technical Context

### Technology Choices
- **C Parser**: 需支持完整 C 语法解析（预处理、宏展开、AST 构建），且不依赖目标项目的编译配置。推荐候选：
  - `pycparser`：纯 Python，支持 C99，可访问 AST，但需要内置 fake libc headers 来处理系统头文件
  - `tree-sitter` + Python binding：高性能增量解析，不依赖编译配置，对语法错误容忍度高
  - `libclang` + Python binding（`clang.cindex`）：语义分析能力最强（内置类型推断、宏展开），但依赖 LLVM 系统库
- **选型指导**: 优先考虑不依赖编译配置 + 能处理跨文件 #include + 语义分析精度高的方案。tree-sitter 和 pycparser 是主要候选，libclang 语义最强但需要系统级 LLVM 依赖。

### Indirect Call Resolution Strategy
间接调用的核心难点在于数据流分析——需要追踪函数指针的赋值链以确定可能的目标集合。策略：
1. **直接赋值**（`fp = foo`）：直接解析为目标
2. **条件赋值**（`if (x) fp = foo; else fp = bar;`）：保守过近似，fp 可能指向 foo 或 bar
3. **赋值链**（`fp2 = fp1; fp2()`）：沿链传递可能的目标集合
4. **回调注册**：维护全局注册表，所有注册调用都是潜在目标
5. **dlsym 硬编码**：提取字符串常量参数，与全局函数符号表匹配

### Project Structure (Proposed)
```
ethunter/
├── src/
│   ├── parser/              # C 源码解析（AST 构建）
│   ├── analyzer/
│   │   ├── base.py          # 分析器基类，定义统一接口
│   │   ├── direct_call.py   # 模块1：直接调用分析
│   │   ├── fp_assign.py     # 模块2：函数指针赋值+调用
│   │   ├── callback_param.py # 模块3：回调函数（参数传递）
│   │   ├── fp_return.py     # 模块4：函数指针返回值
│   │   ├── fp_array.py      # 模块5：函数指针数组/dispatch table
│   │   ├── vtable.py        # 模块6：结构体函数指针成员
│   │   ├── callback_reg.py  # 模块7：回调注册/注销模式
│   │   ├── union_fp.py      # 模块8：联合体中的函数指针
│   │   ├── typedef_fp.py    # 模块9：typedef隐藏的函数指针
│   │   ├── fp_alias.py      # 模块10：函数指针别名/重定向
│   │   ├── lazy_init.py     # 模块11：全局/静态函数指针延迟初始化
│   │   ├── macro_fp.py      # 模块12：宏展开后的函数指针（部分支持）
│   │   └── dlsym_fp.py     # 模块13：dlopen/dlsym硬编码（部分支持）
│   ├── graph/               # 调用图数据结构与操作
│   ├── query/               # 查询接口（callers/callees）
│   └── output/              # 输出格式化（JSON/DOT）
├── tests/
│   ├── fixtures/            # 手工测试用例 C 文件（每种间接调用场景一个文件）
│   └── test_*.py            # 单元测试
├── benchmarks/              # 开源项目验证集
└── pyproject.toml
```

### Design Principle: One Analyzer Per Indirect Call Pattern
每种间接调用场景对应一个**独立分析器模块**。每个模块：
- 继承统一的基础分析器接口
- 专注识别一种特定的间接调用模式
- 输出标准化的调用边（CallEdge）
- 有独立的测试用例（对应的 C fixture 文件）
- 所有模块的分析结果合并为最终的全局调用图

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| CallGraph | core domain | nodes: Function[], edges: CallEdge[], source_files: str[] | contains Function nodes, connected by CallEdge |
| Function | core domain | name: str, file: str, line: int, is_indirect_target: bool | is caller of Function, is callee of Function |
| CallEdge | core domain | caller: Function, callee: Function, type: str (direct/indirect), indirect_kind: str | links caller Function to callee Function |
| QueryInterface | supporting | function_name: str, callers: Function[], callees: Function[] | queries CallGraph for a Function |
| JSONOutput | supporting | graph: CallGraph (serialized), functions: dict, edges: list | serializes CallGraph |
| DOTOutput | supporting | graph: CallGraph (serialized) | serializes CallGraph |
| ValidationSuite | supporting | test_files: C source files, expected_edges: CallEdge[], actual_edges: CallEdge[] | validates CallGraph correctness |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 1 | 2 | 2 | - | - | N/A |
| 2 | 3 | 1 | 0 | 2 | ~67% |
| 3 | 5 | 2 | 0 | 3 | ~60% |
| 4 | 6 | 1 | 0 | 5 | ~83% |
| 5 | 7 | 1 | 0 | 6 | ~86% |
| 6 | 7 | 0 | 0 | 7 | 100% |
| 7 | 7 | 0 | 0 | 7 | 100% |

## Interview Transcript
<details>
<summary>Full Q&A (9 rounds)</summary>

### Round 1
**Targeting:** Goal Clarity | Ambiguity: 100%
**Q:** 你说的 'call graph' 具体指什么？比如：A函数调用了B、C，B又调用了D，输出类似 A→B→D 和 A→C 的关系图。还是说你希望包含更具体的信息，比如调用顺序、调用次数、间接调用关系（通过函数指针）等？
**A:** 项目级别的全局调用图，不需要关心调用频率，但需要仅可能支持全部类型的间接调用
**Scores:** Goal: 0.55, Constraints: 0.1, Criteria: 0.1

### Round 2
**Targeting:** Constraint Clarity | Ambiguity: 74%
**Q:** 这个工具你打算用什么编程语言来实现？考虑到需要处理C源码的AST解析和间接调用分析，技术栈选择会影响开发效率和最终性能。
**A:** Python
**Scores:** Goal: 0.55, Constraints: 0.4, Criteria: 0.1

### Round 3
**Targeting:** Constraint Clarity | Ambiguity: 59%
**Q:** Call graph生成出来后，你希望输出什么格式？另外输入是单个.c文件还是整个项目目录（需要自动处理#include和跨文件引用）？
**A:** 默认输出json结构化的数据。但需要补充转化为DOT格式的额外功能选项，同时需要补充查询功能。例如查询函数f,输出调用f的全部callers，同时再输出被f调用的全部callees
**Scores:** Goal: 0.75, Constraints: 0.7, Criteria: 0.1

### Round 4
**Targeting:** Success Criteria | Ambiguity: 43%
**Q:** 如果工具生成了一份call graph，你怎么判断它是正确的？
**A:** 同时采用1和2两种方式验证（已知项目benchmark + 手工测试用例）
**Scores:** Goal: 0.75, Constraints: 0.7, Criteria: 0.7

### Round 5 (Contrarian Mode)
**Targeting:** Goal Clarity | Ambiguity: 32%
**Q:** 以下这些间接调用场景，哪些是这个工具必须支持的？
**A:** 1-7，9，11-12必须准确支持，8尽可能解决可展开的宏，10解决硬编码字符串场景
**Scores:** Goal: 0.85, Constraints: 0.7, Criteria: 0.7

### Round 6
**Targeting:** Constraint Clarity | Ambiguity: 24%
**Q:** 这个工具需要能处理多大体量的C项目？
**A:** 以小型项目为主，力求精确性。但也存在少量中大型项目的分析需求，保证精确性的前提下，再优化性能。没有超大型项目的分析需求
**Scores:** Goal: 0.85, Constraints: 0.85, Criteria: 0.7

### Round 7
**Targeting:** Success Criteria | Ambiguity: 15%
**Q:** call graph 生成工具的验收标准中，你对间接调用的解析精度有什么期望？
**A:** 直接调用0遗漏，间接调用高覆盖
**Scores:** Goal: 0.85, Constraints: 0.85, Criteria: 0.9

### Round 8
**Targeting:** Constraint Clarity | Ambiguity: 10%
**Q:** C 源码解析是这个工具的基础。你倾向于用哪个 C 解析库？这个选择会影响间接调用分析的精度。
**A:** 我不希望工具依赖源码编译，因为有些项目的编译方式是不确定的，甚至clang无法对齐编译。同时我分析的目标是整个项目，我关注跨文件的函数调用。而不是逐一分析单个文件。基于以上事实，尽可能选择分析更精确的方案
**Scores:** Goal: 0.88, Constraints: 0.88, Criteria: 0.90

### Round 9
**Targeting:** Constraint Clarity | Ambiguity: ~10%
**Q:** 对于跨文件分析，工具如何发现项目中的所有源文件并处理 #include 依赖？
**A:** 全自动项目扫描
**Scores:** Goal: 0.90, Constraints: 0.90, Criteria: 0.90

### Round 10
**Targeting:** Goal Clarity | Ambiguity: 10%
**Q:** 继续 refine 的方向
**A:** 间接调用的分析是难点，我希望针对之前列举的每一个间接调用场景，都有一个专门的模块进行分析，力求每一种场景都精准识别
**Scores:** Goal: 0.92, Constraints: 0.90, Criteria: 0.90

</details>
