# CG-Bench Fixture 修复计划

## 问题概述

CG-Bench 共 11 个 category，104 个 example，616 条期望的间接调用边。当前总召回率 **8.77%**。

核心问题：**54/104 个 fixture.c 存在 tree-sitter 解析错误**，导致 ethunter 无法正确分析这些文件。

## 修复约束（用户指定）

1. **最小量修改**：只修让 tree-sitter 能正确解析所必需的部分
2. **不改变代码框架**：保持 fixture.c 原始结构和风格
3. **保护间接调用三要素**：caller、fnptr、callee 及其调用关系绝不破坏
4. **不删除代码**：冗余代码保留
5. **不修改 ground_truth.json**：发现问题只做记录
6. **不修改 ethunter 分析器**
7. **严禁使用脚本批量修复**：用 LLM review 逐个文件修复
8. **使用多 sub-agent 并行处理**：每个 sub-agent 保持干净上下文
9. **每个 category 修完立即验证**：验证语法 + 语义（对照 CG-Bench md 文档）

## 验证标准

对每个 category 修复完成后：
1. **语法验证**：tree-sitter 解析该 category 所有 example 无 error
2. **语义验证**：对照 `tests/benchmark/CG-Bench/fnptr-*.md` 中对应的原始代码片段，确认修复后的 fixture.c 保留了所有关键元素（函数签名、回调参数、结构体定义、函数指针数组等）和 caller→fnptr→callee 的调用链
3. **测试验证**：运行 `pytest tests/test_cg_bench.py -v -s` 确认 recall 有改善

## 实施顺序

按 category 顺序逐个修复。每个 category 修复流程：
1. 启动 N 个 sub-agent（N = example 数量，分批），每个 agent 独立修复 1-2 个 example
2. 主进程合并后验证
3. 验证通过后进入下一个 category

## Category 修复详情

### 已解析通过（0 个 parse error，无需修复）
| Category | Examples | 当前 Recall |
|---|---|---|
| fnptr-callback (部分) | ex1-5,7-13,15 | 69.44% |
| fnptr-cast (部分) | ex2-7 | 10.00% |
| fnptr-dynamic-call | ex1-5 | 0.00% |
| fnptr-only (部分) | ex1,4 | 66.67% |
| fnptr-global-array (部分) | ex1-2 | 0.98% |
| fnptr-global-struct (部分) | ex3,10-11 | 0.00% |
| fnptr-struct (部分) | ex1,8-13 | 9.52% |

### 需要修复（54 个 fixture 有 parse error）

按 category 排序：

| # | Category | Parse Errors | Total Examples | Notes |
|---|---|---|---|---|
| 1 | fnptr-callback | 2 (ex6,14) | 15 | 缺失类型定义/函数声明 |
| 2 | fnptr-cast | 1 (ex1) | 7 | JET_MUTABLE 宏未定义 |
| 3 | fnptr-dynamic-call | 0 | 5 | 解析通过但 0% recall，需语义分析 |
| 4 | fnptr-global-array | 4 (ex3-6) | 6 | 缺失 struct 定义/typedef |
| 5 | fnptr-global-struct-array | 8 (ex2-7,10-11) | 12 | 大量缺失的类型/结构体定义 |
| 6 | fnptr-global-struct | 9 (ex1-2,4-9) | 11 | 缺失结构体/函数指针类型定义 |
| 7 | fnptr-library | 10 (ex2-9,11,14-16,18) | 20 | 缺失类型/宏/函数声明 |
| 8 | fnptr-only | 8 (ex2-3,5-12) | 12 | 缺失宏定义/类型声明 |
| 9 | fnptr-struct | 6 (ex2-7,14) | 14 | 缺失结构体/类型定义 |
| 10 | fnptr-varargs | 1 (ex1) | 1 | 缺失 va_list/类型定义 |
| 11 | fnptr-virtual | 1 (ex1) | 1 | C++ 代码用 C 解析器 |

## 常见修复模式

根据已阅读的 fixture.c 文件，主要修复类型为：

1. **补充宏定义**：如 `JET_MULAble`、`unlikely`、`DEBUGASSERT`、`ATTRIBUte_UNUSED` 等
2. **补充 typedef/struct 前向声明**：如 `struct Curl_easy`、`quicklist`、`client` 等
3. **补充函数原型声明**：让 tree-sitter 能识别未定义的函数调用
4. **修复语法错误**：缺失的括号、分号、不完整的语句
5. **补充 `#include` 等价声明**：如 `va_list` 需要 `<stdarg.h>` 的等价物

## 风险与注意事项

- **ground_truth.json 可能有误的情况**：如果 ground_truth.json 中的 caller 在 fixture.c 中不是真正的调用发起者，记录下来告知用户
- **fnptr-virtual example_1**：包含 C++ 代码（region_model_context 等），C 解析器无法处理，可能需要特殊处理
- **fnptr-dynamic-call 0% recall**：无 parse error 但全漏，说明是分析器能力问题而非 fixture 问题，本轮不处理
