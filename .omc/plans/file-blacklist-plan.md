# Plan: .ethunterignore File Blacklist

## Requirements Summary

Add `.ethunterignore` support to `scan_files()` so users can exclude C source files from analysis using gitignore-style glob patterns. The feature auto-discovers a `.ethunterignore` file in the project root directory.

## Acceptance Criteria

- [ ] `scan_files()` reads `.ethunterignore` from project root when it exists
- [ ] `*v300*` matches files whose relative path contains "v300" (e.g., `src/v300/foo.c`)
- [ ] `**/build/*` matches files in `build/` dirs at any depth
- [ ] `#` comment lines and blank lines are skipped
- [ ] Invalid glob patterns produce a stderr Warning, analysis continues
- [ ] After analysis, stderr prints "Ignored N files matching .ethunterignore"
- [ ] No `.ethunterignore` → behavior is identical to current (backward compatible)
- [ ] Unit tests cover `parse_ignore_file()` and `is_ignored()` logic

## Implementation Steps

### Step 1: Add `parse_ignore_file()` to `scanner.py`

**File:** `src/ethunter/parser/scanner.py`

Add a function that reads `.ethunterignore` from the project root and returns a list of glob patterns.

- Try to open `{project_dir}/.ethunterignore`
- Strip each line, skip empty lines and `#`-prefixed lines
- If file doesn't exist, return empty list
- No validation of patterns at parse time (invalid patterns caught in matching)

### Step 2: Add `is_ignored()` to `scanner.py`

**File:** `src/ethunter/parser/scanner.py`

Add a function that checks if a file path matches any of the glob patterns.

- Convert the file's absolute path to a relative path against `project_dir`
- Use a glob-to-regex converter function for pattern matching
- `fnmatch` is insufficient: it treats `*` as matching `/` (good for `*v300*` on paths) but `**` doesn't properly handle the zero-directory case (e.g., `**/build/*` doesn't match `build/foo.c`)
- `PurePath.match()` is insufficient: it does component-level matching, so `*v300*` fails on `src/v300/foo.c` (it treats `*v300*` as a filename pattern, not a substring match on the path)
- **Implementation choice:** A small `glob_to_regex()` helper that converts gitignore-style patterns to compiled regex:
  - `*` → `.*` (matches anything including `/`)
  - `**/` → `(.*/)?` (matches zero or more directories)
  - `**` (trailing) → `.*`
  - `?` → `.`
  - `.` → `\.`
  - Other chars → `re.escape()`
- Catch `re.error` for invalid patterns, warn and skip

### Step 3: Integrate into `scan_files()`

**File:** `src/ethunter/parser/scanner.py`

- Call `parse_ignore_file(project_dir)` once at the start
- Track ignored file count
- In the file loop, after existing `_EXCLUDE_DIRS` check, also call `is_ignored()`
- Print warning for invalid patterns (catch `ValueError` from `PurePath.match()`)
- Return filtered file list

### Step 4: Add ignored count to stderr output

**File:** `src/ethunter/cli.py`

- `scan_files()` returns a tuple: `(files: list[Path], ignored_count: int)`
- Or add an optional callback/parameter. Simpler: just return the tuple
- In `cli.py`, after Phase 1, print to stderr if `ignored_count > 0`

### Step 5: Add unit tests

**File:** `tests/test_scanner.py` (new file)

Test cases:
- `test_no_ignore_file_returns_empty_list`
- `test_parse_ignore_file_skips_comments_and_blanks`
- `test_is_ignored_star_pattern` — `*v300*` matches `src/v300/foo.c`
- `test_is_ignored_doublestar_pattern` — `**/build/*` matches `a/b/build/foo.c`
- `test_is_ignored_no_match`
- `test_is_ignored_invalid_pattern_warned`
- `test_scan_files_respects_ignore_file`
- `test_scan_files_without_ignore_file_backward_compatible`

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Glob-to-regex edge cases | The converter is small (~15 lines); comprehensive unit tests cover all pattern forms |
| Performance with many patterns | Patterns list is small in practice; pre-compile regexes once in `scan_files()` |
| Invalid glob syntax crashes | `re.error` caught per-pattern, warn and skip that pattern |

## Verification Steps

1. Run `PYTHONPATH=src .venv/bin/python -m ethunter.cli` on a project with `.ethunterignore`
2. Verify excluded files don't appear in JSON output
3. Verify stderr shows ignored count
4. Run `tests/test_scanner.py` — all pass
5. Run full test suite — no regressions

## RALPLAN-DR Summary

### Principles
1. **Minimal surface area** — only add what the spec requires (file-based, no CLI)
2. **Backward compatible** — no `.ethunterignore` = zero behavior change
3. **Fail gracefully** — bad patterns warn, don't crash

### Decision Drivers
1. `**` matching capability (core user requirement)
2. Python 3.11 availability (project constraint)
3. Keep `scan_files()` API simple (only caller is `cli.py`)

### Options

**Option A: Custom glob-to-regex converter** (chosen)
- Pros: Correct handling of both `*v300*` and `**/build/*`, no external deps, ~15 lines
- Cons: Must handle edge cases ourselves (but testable)

**Option B: `fnmatch` + `PurePath.match()` hybrid**
- Pros: Both stdlib
- Cons: `fnmatch` fails `**/build/*` on `build/foo.c`; `PurePath.match()` fails `*v300*` on `src/v300/foo.c`. Neither handles the full requirement set alone.

## ADR

**Decision:** Use a small `glob_to_regex()` helper (~15 lines) that converts gitignore-style glob patterns to compiled Python regex.

**Drivers:** Neither `fnmatch` nor `PurePath.match()` alone handles both `*v300*` (substring on path) and `**/build/*` (zero-dir `**` case). The converter is trivial to implement and fully testable.

**Alternatives considered:** `fnmatch` alone (rejected — `**/build/*` doesn't match `build/foo.c`), `PurePath.match()` alone (rejected — `*v300*` doesn't match `src/v300/foo.c`), `pathspec` library (rejected — new dependency, negation not in scope).

**Why chosen:** Minimal code (~15 lines), correct for all required pattern forms, zero dependencies, easy to replace with `pathspec` later if full gitignore compatibility is requested.

**Consequences:** If users later request full gitignore semantics (negation `!`, `/` anchoring, trailing `/` for dirs), switching to `pathspec` is easy since the `is_ignored()` API already abstracts the matching logic.

**Follow-ups:** Add `pathspec` dependency if full `.gitignore` compatibility is requested.
