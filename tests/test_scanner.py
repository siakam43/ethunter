"""Tests for scanner.py — file discovery and .ethunterignore filtering."""

from pathlib import Path

import pytest

from ethunter.parser.scanner import (
    _glob_to_regex,
    _is_ignored,
    parse_ignore_file,
    scan_files,
)


# --- _glob_to_regex ---

class TestGlobToRegex:
    def test_star_matches_path_components(self):
        regex = _glob_to_regex('*v300*')
        assert regex.match('src/v300/foo.c')
        assert regex.match('v300/bar.c')
        assert not regex.match('src/foo.c')

    def test_doublestar_matches_any_depth(self):
        regex = _glob_to_regex('**/build/*')
        assert regex.match('build/foo.c')
        assert regex.match('a/build/foo.c')
        assert regex.match('a/b/build/foo.c')
        assert not regex.match('src/main.c')

    def test_doublestar_middle(self):
        regex = _glob_to_regex('src/**/*')
        assert regex.match('src/foo.c')
        assert regex.match('src/a/b/foo.c')
        assert not regex.match('other/foo.c')

    def test_question_mark(self):
        regex = _glob_to_regex('foo.?')
        assert regex.match('foo.c')
        assert regex.match('foo.h')
        assert not regex.match('foo.cc')

    def test_dot_escaped(self):
        regex = _glob_to_regex('*.c')
        assert regex.match('foo.c')
        assert not regex.match('fooXc')

    def test_doublestar_only(self):
        regex = _glob_to_regex('**/*')
        assert regex.match('foo.c')
        assert regex.match('a/b/foo.c')


# --- parse_ignore_file ---

class TestParseIgnoreFile:
    def test_no_file_returns_empty(self, tmp_path):
        assert parse_ignore_file(tmp_path) == []

    def test_skips_comments_and_blanks(self, tmp_path):
        (tmp_path / '.ethunterignore').write_text(
            '# This is a comment\n'
            '\n'
            '*v300*\n'
            '  \n'
            '**/build/*\n'
            '# Another comment\n'
        )
        patterns = parse_ignore_file(tmp_path)
        assert patterns == ['*v300*', '**/build/*']

    def test_strips_whitespace(self, tmp_path):
        (tmp_path / '.ethunterignore').write_text('  *test*  \n')
        assert parse_ignore_file(tmp_path) == ['*test*']


# --- _is_ignored ---

class TestIsIgnored:
    def test_star_pattern(self):
        compiled = [_glob_to_regex('*v300*')]
        assert _is_ignored('src/v300/foo.c', compiled)
        assert not _is_ignored('src/foo.c', compiled)

    def test_doublestar_pattern(self):
        compiled = [_glob_to_regex('**/build/*')]
        assert _is_ignored('build/foo.c', compiled)
        assert _is_ignored('a/b/build/foo.c', compiled)
        assert not _is_ignored('src/main.c', compiled)

    def test_no_match(self):
        compiled = [_glob_to_regex('*.h')]
        assert _is_ignored('foo.h', compiled)
        assert not _is_ignored('foo.c', compiled)

    def test_multiple_patterns(self):
        compiled = [_glob_to_regex('*v300*'), _glob_to_regex('**/test/*')]
        assert _is_ignored('src/v300/foo.c', compiled)
        assert _is_ignored('a/test/bar.c', compiled)
        assert not _is_ignored('src/real/foo.c', compiled)


# --- scan_files ---

class TestScanFiles:
    def _setup_project(self, tmp_path, ignore_content=None):
        """Create a fake C project structure."""
        (tmp_path / 'src').mkdir()
        (tmp_path / 'src' / 'main.c').write_text('int main() {}')
        (tmp_path / 'src' / 'v300').mkdir()
        (tmp_path / 'src' / 'v300' / 'old.c').write_text('void old() {}')
        (tmp_path / 'build').mkdir()
        (tmp_path / 'build' / 'gen.c').write_text('void gen() {}')
        if ignore_content is not None:
            (tmp_path / '.ethunterignore').write_text(ignore_content)

    def test_without_ignore_file_backward_compatible(self, tmp_path):
        self._setup_project(tmp_path)
        files, ignored = scan_files(tmp_path)
        assert ignored == 0
        names = {f.name for f in files}
        # build/ is excluded by _EXCLUDE_DIRS, so only main.c and old.c
        assert 'main.c' in names
        assert 'old.c' in names

    def test_respects_ignore_file(self, tmp_path):
        self._setup_project(tmp_path, '*v300*')
        files, ignored = scan_files(tmp_path)
        assert ignored >= 1
        names = {f.name for f in files}
        assert 'main.c' in names
        # v300 files should be filtered out
        assert 'old.c' not in names

    def test_scan_files_returns_ignored_count(self, tmp_path):
        """Verify ignored count is returned even when patterns match files
        that would also be excluded by _EXCLUDE_DIRS (build/ in this case)."""
        self._setup_project(tmp_path, '**/build/*')
        files, ignored = scan_files(tmp_path)
        # build/ is already excluded by _EXCLUDE_DIRS before .ethunterignore check,
        # so count stays 0. The pattern logic is verified via _is_ignored tests.
        assert ignored == 0
        assert any(f.name == 'main.c' for f in files)

    def test_special_chars_in_pattern(self, tmp_path):
        """Patterns with regex-special chars like [ are safely escaped."""
        self._setup_project(tmp_path, '[special]')
        files, ignored = scan_files(tmp_path)
        # Pattern is escaped, so it just looks for literal '[special]' in paths
        # No files match, so nothing is ignored
        assert ignored == 0
