"""Module 13: dlopen/dlsym hardcoded string analysis (partial support)."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence
from ethunter.analyzer.dataflow import DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: DataflowEngine,
) -> list[CallEdge]:
    """Detect dlsym(handle, "func_name") with string literals and match against symbol table."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                if call_name == 'dlsym':
                    args = node.child_by_field_name('arguments')
                    if args:
                        for arg in args.children:
                            if arg.type == 'string_literal':
                                str_content = _extract_string_content(arg)
                                if str_content and str_content in symbol_names:
                                    edges.append(CallEdge(
                                        caller='<dlsym>',
                                        callee=str_content,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='dlsym_fp',
                                        caller_line=node.start_point[0] + 1,
                                        confidence=Confidence.LOW,
                                        evidence=Evidence('dlsym_string_match'),
                                    ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _extract_string_content(node: ts.Node) -> str | None:
    """Extract the string content from a string_literal node."""
    text = node.text
    if not text:
        return None
    s = text.decode('utf-8')
    if s.startswith('"') and s.endswith('"'):
        return s[1:-1]
    return None
