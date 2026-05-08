"""Parse C files using tree-sitter."""

from pathlib import Path

import tree_sitter_c as tsc
import tree_sitter as ts


def _make_parser() -> ts.Parser:
    """Create a tree-sitter parser for C."""
    lang = ts.Language(tsc.language())
    return ts.Parser(lang)


def parse_file(filepath: str | Path) -> ts.Tree:
    """Parse a single .c or .h file and return the AST tree."""
    parser = _make_parser()
    source = Path(filepath).read_bytes()
    return parser.parse(source)


def parse_source(source: bytes) -> ts.Tree:
    """Parse C source from bytes and return the AST tree."""
    parser = _make_parser()
    return parser.parse(source)
