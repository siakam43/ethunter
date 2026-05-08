# Deep Interview Spec: File Blacklist (.ethunterignore)

## Metadata
- Interview ID: di-file-blacklist-001
- Rounds: 6
- Final Ambiguity Score: 16.5%
- Type: brownfield
- Generated: 2026-05-08
- Threshold: 20%
- Status: PASSED

## Clarity Breakdown
| Dimension | Score | Weight | Weighted |
|-----------|-------|--------|----------|
| Goal Clarity | 0.9 | 0.35 | 0.315 |
| Constraint Clarity | 0.9 | 0.25 | 0.225 |
| Success Criteria | 0.7 | 0.25 | 0.175 |
| Context Clarity | 0.8 | 0.15 | 0.120 |
| **Total Clarity** | | | **0.835** |
| **Ambiguity** | | | **16.5%** |

## Goal

在 `scan_files()` 中增加 `.ethunterignore` 文件的读取和过滤能力，让用户可以通过类似 gitignore 的 glob 模式排除不想被分析的 C 源文件。

## Constraints

- `.ethunterignore` 放在被分析项目的根目录下，自动发现（无需 CLI 参数）
- 使用 gitignore glob 语法，支持 `**` 跨多层目录匹配
- 模式匹配的是文件相对于项目根目录的相对路径
- 文件格式：每行一个 glob 模式，空行跳过，`#` 开头的行为注释
- 无效模式：打印 Warning 到 stderr，跳过该规则，继续分析
- 被忽略的文件不出现在 JSON 输出的 `source_files` 中
- 被忽略的文件的函数也不出现在调用图中
- 分析完成后打印被忽略的文件数量到 stderr

## Non-Goals

- 不改变现有的硬编码 `_EXCLUDE_DIRS` 行为（保留，作为默认过滤）
- 不支持 CLI 参数传入额外规则（后续可扩展）
- 不支持 negation（`!` 前缀取消忽略）

## Acceptance Criteria

- [ ] 项目根目录有 `.ethunterignore` 时，匹配的文件不被 `scan_files()` 返回
- [ ] 模式 `*v300*` 能匹配相对路径中包含 v300 的文件（如 `src/v300/foo.c`）
- [ ] 模式 `**/build/*` 能匹配任意深度的 build 目录下的文件
- [ ] `.ethunterignore` 中的 `#` 注释行和空行被正确跳过
- [ ] 无效 glob 模式产生 stderr Warning，不中断分析
- [ ] 分析结束后，stderr 打印 "Ignored N files matching .ethunterignore"
- [ ] 无 `.ethunterignore` 文件时行为与当前完全一致（向后兼容）
- [ ] 新增单元测试覆盖 `parse_ignore_file()` 和过滤逻辑

## Assumptions Exposed & Resolved
| Assumption | Challenge | Resolution |
|-----------|-----------|-----------|
| 规则来源 | 文件 vs CLI vs 配置 | 只用 .ethunterignore 文件 |
| 匹配对象 | 文件名 vs 相对路径 vs 绝对路径 | 相对路径匹配 |
| 语法 | glob vs 正则 | gitignore glob + ** 扩展 |
| 错误处理 | 终止 vs 警告 vs 静默 | 警告并跳过 |
| 注释格式 | 支持/不支持 | 支持 # 注释和空行 |

## Technical Context

### 现有代码

- `src/ethunter/parser/scanner.py` — `scan_files()` 使用 `Path.rglob('*')` 发现文件，当前只有 `_EXCLUDE_DIRS` 硬编码集合
- `src/ethunter/cli.py:91` — `files = scan_files(project_dir)` 调用入口
- Python 标准库 `fnmatch` 可用于 glob 模式匹配

### 实现方向

1. 新增 `parse_ignore_file(project_dir: Path) -> list[str]` 函数
2. 新增 `is_ignored(filepath: Path, project_dir: Path, patterns: list[str]) -> bool` 函数
3. 在 `scan_files()` 中：先读 `.ethunterignore`，过滤文件时应用 pattern
4. CLI 打印忽略数量到 stderr

## Ontology (Key Entities)

| Entity | Type | Fields | Relationships |
|--------|------|--------|---------------|
| .ethunterignore | core domain | glob patterns, comments, blank lines | read by scan_files() |
| scan_files() | core domain | project_dir, exclude_dirs, returns list[Path] | reads .ethunterignore |
| glob_pattern | supporting | pattern, match_type (relative_path) | used to filter in scan_files() |
| CallGraph | core domain | source_files | excludes ignored files |

## Ontology Convergence

| Round | Entity Count | New | Changed | Stable | Stability Ratio |
|-------|-------------|-----|---------|--------|----------------|
| 1 | 4 | 4 | - | - | - |
| 2-6 | 4 | 0 | 0 | 4 | 100% |

## Interview Transcript
<details>
<summary>Full Q&A (6 rounds)</summary>

### Round 1
**Q:** 黑名单规则应该从哪里读取？
**A:** 自动读取项目根目录下的 .ethunterignore 文件
**Ambiguity:** 100%

### Round 2
**Q:** .ethunterignore 里的规则用什么语法？
**A:** Gitignore glob 语法并配合支持**拓展
**Ambiguity:** 59.5%

### Round 3
**Q:** 规则是匹配文件路径还是文件名？
**A:** 相对路径匹配
**Ambiguity:** 42.5%

### Round 4 (Contrarian)
**Q:** 如果 .ethunterignore 中有无效模式或错误，怎么处理？
**A:** 警告并跳过
**Ambiguity:** 36.5%

### Round 5
**Q:** .ethunterignore 文件格式支持哪些约定？
**A:** 支持 # 注释和空行
**Ambiguity:** 31.5%

### Round 6
**Q:** 黑名单生效的直观表现是什么？
**A:** 两者都需要：被忽略的文件不出现在输出中，且控制台打印被忽略的文件数
**Ambiguity:** 16.5%
</details>
