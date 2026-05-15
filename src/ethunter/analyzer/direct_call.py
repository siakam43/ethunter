"""Direct call analyzer: find all foo() style function calls in function bodies."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence
from ethunter.analyzer.symbol_table import extract_functions


def analyze(tree: ts.Tree, filepath: str, symbol_names: set[str], **kwargs) -> list[CallEdge]:
    """Find all direct function calls in the AST.

    Args:
        tree: tree-sitter AST
        filepath: source file path
        symbol_names: set of known function names from the symbol table

    Returns:
        List of CallEdge entries for direct calls.
    """
    edges: list[CallEdge] = []

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = _find_function_in_call(node)
            if func_node and func_node.text:
                callee = func_node.text.decode('utf-8')
                # Determine caller: find enclosing function definition
                caller = _find_enclosing_function_name(node, tree.root_node)
                if caller and callee in symbol_names:
                    edges.append(CallEdge(
                        caller=caller,
                        callee=callee,
                        caller_file=filepath,
                        callee_file='',
                        type=CallType.DIRECT,
                        caller_line=node.start_point[0] + 1,
                        confidence=Confidence.HIGH,
                        evidence=Evidence('direct_call'),
                    ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _find_function_in_call(call_node: ts.Node) -> ts.Node | None:
    """Find the function identifier node within a call_expression."""
    for child in call_node.children:
        if child.type == 'identifier':
            return child
        if child.type == 'field_expression':
            return child
        if child.type == 'subscript_expression':
            return child
    return None


def _find_enclosing_function_name(node: ts.Node, root: ts.Node) -> str | None:
    """Walk up the tree to find the enclosing function definition's name."""
    # Use a simpler approach: walk from root to find which function contains this node
    current = root
    result = [None]

    def _search(n: ts.Node, target_line: int) -> None:
        if result[0] is not None:
            return
        if n.type == 'function_definition':
            declarator = _find_child(n, 'function_declarator')
            if declarator:
                ident = _find_child(declarator, 'identifier')
                if ident and ident.text:
                    result[0] = ident.text.decode('utf-8')
        for child in n.children:
            if child.start_point[0] <= target_line <= child.end_point[0]:
                _search(child, target_line)

    _search(root, node.start_point[0])
    return result[0]


def _find_child(node: ts.Node, type_name: str) -> ts.Node | None:
    for child in node.children:
        if child.type == type_name:
            return child
    return None
