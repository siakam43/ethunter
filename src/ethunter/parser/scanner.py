"""Recursively discover .c/.h files in a project directory."""

import re
import sys
from pathlib import Path

_IGNORE_FILE = '.ethunterignore'
_EXCLUDE_DIRS = {'.git', 'build', 'dist', '.build', 'CMakeFiles', 'cmake-build*', '__pycache__', '.svn'}


def _glob_to_regex(pattern: str) -> re.Pattern:
    """Convert a gitignore-style glob pattern to a compiled regex."""
    i = 0
    n = len(pattern)
    regex = ''

    while i < n:
        c = pattern[i]
        if c == '*':
            if i + 1 < n and pattern[i + 1] == '*':
                # ** - match zero or more directories
                i += 2
                if i < n and pattern[i] == '/':
                    regex += '(.*/)?'
                    i += 1
                else:
                    regex += '.*'
            else:
                regex += '.*'
                i += 1
        elif c == '?':
            regex += '.'
            i += 1
        elif c == '.':
            regex += r'\.'
            i += 1
        else:
            regex += re.escape(c)
            i += 1

    return re.compile(f'^{regex}$')


def parse_ignore_file(project_dir: Path) -> list[str]:
    """Read .ethunterignore from project root, return list of glob patterns."""
    ignore_path = project_dir / _IGNORE_FILE
    if not ignore_path.is_file():
        return []

    patterns = []
    for line in ignore_path.read_text().splitlines():
        stripped = line.strip()
        if stripped and not stripped.startswith('#'):
            patterns.append(stripped)
    return patterns


def _is_ignored(rel_path: str, compiled: list[re.Pattern]) -> bool:
    """Check if a relative path matches any compiled ignore pattern."""
    for regex in compiled:
        if regex.match(rel_path):
            return True
    return False


def scan_files(project_dir: str | Path) -> tuple[list[Path], int]:
    """Return all .c and .h files under project_dir, excluding common non-source dirs
    and patterns from .ethunterignore.

    Returns:
        (files, ignored_count): sorted list of discovered files and the number
        of files filtered out by .ethunterignore.
    """
    root = Path(project_dir).resolve()
    ignore_patterns = parse_ignore_file(root)

    # Pre-compile patterns; warn about invalid ones
    compiled: list[re.Pattern] = []
    for pat in ignore_patterns:
        try:
            compiled.append(_glob_to_regex(pat))
        except re.error:
            print(f'Warning: invalid pattern in {_IGNORE_FILE}: {pat}', file=sys.stderr)

    files: list[Path] = []
    ignored = 0
    for path in root.rglob('*'):
        if not path.is_file() or path.suffix not in ('.c', '.h'):
            continue
        # Exclude files in build-like directories
        parts = set(path.parts)
        if parts & _EXCLUDE_DIRS:
            continue
        # Check .ethunterignore patterns
        if compiled:
            rel = str(path.relative_to(root))
            if _is_ignored(rel, compiled):
                ignored += 1
                continue
        files.append(path)
    return sorted(files), ignored
