# Deep Interview Spec: Default Output Files

## Metadata
- Interview ID: default-output-files-003
- Rounds: 4
- Final Ambiguity Score: 5%
- Type: brownfield
- Generated: 2026-05-08
- Threshold: 20%
- Status: PASSED

## Goal

Add default output file paths for all CLI modes when `-o` is not specified. Existing files should be overwritten without warning.

## Default File Mapping

| Mode | Default Output |
|---|---|
| `--analyze DIR` | `callgraph.json` |
| `--from-json FILE --query NAME` | `query.json` |
| `--from-json FILE --to-dot` | `output.dot` |
| `--from-json FILE --find-entry` | `entry.json` |

## Constraints

- `-o` flag always overrides the default filename
- If default output file already exists, overwrite it without prompting
- `--from-json` without `--query`, `--to-dot`, or `--find-entry` should produce an error + exit(1)

## Non-Goals

- No interactive overwrite confirmation
- No append mode — always replace

## Acceptance Criteria

- [ ] `--analyze DIR` without `-o` writes to `callgraph.json` in current directory
- [ ] `--analyze DIR -o custom.json` writes to `custom.json`
- [ ] `--from-json FILE --query NAME` without `-o` writes to `query.json`
- [ ] `--from-json FILE --to-dot` without `-o` writes to `output.dot`
- [ ] `--from-json FILE --find-entry` without `-o` writes to `entry.json`
- [ ] `--from-json FILE` alone (no query/to-dot/find-entry) produces error + exit(1)
- [ ] Existing default files are overwritten without confirmation
- [ ] All existing tests updated and passing
- [ ] CLAUDE.md updated with new default file behavior

## Technical Context

### Current CLI (`src/ethunter/cli.py`)

Lines 149-154: current output logic — if `-o` specified, write to file; otherwise print to stdout.

### Change Required

Replace the current output block with:
1. Determine default filename based on active mode (`--analyze`, `--query`, `--to-dot`, `--find-entry`)
2. If `-o` specified, use that; otherwise use the default
3. Write to the resolved path, overwrite if exists
4. Print "Output written to {path}" confirmation
5. Add error check: if `--from-json` without any query operation, error + exit(1)

## Interview Transcript
<details>
<summary>Full Q&A (4 rounds)</summary>

### Round 1
**Q:** 只有--analyze和--find-entry有默认输出文件，还是其他模式也需要默认文件？
**A:** All modes should have defaults
**Ambiguity:** 50%

### Round 2
**Q:** --query、--to-dot、--from-json alone 的默认输出文件名分别是什么？
**A:** --query默认query.json（固定），如果结果文件已存在则删除原文件
**Ambiguity:** 30%

### Round 3
**Q:** --to-dot和--from-json alone的默认文件名？
**A:** --from-json不允许单独使用，单独使用应该报错
**Ambiguity:** 20%

### Round 4
**Q:** --to-dot模式的默认输出文件名是什么？
**A:** output.dot
**Ambiguity:** 5%
</details>
