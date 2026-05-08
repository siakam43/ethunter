"""Module 8: Union function pointer analysis."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect union function pointer members and track calls through them."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # Track union member assignments: a.sa = act_simple
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                target = rhs.text.decode('utf-8')
                if target in symbol_names:
                    dataflow.assign('<union_init>', target)
        if node.type == 'initializer_list':
            for child in node.children:
                if child.type == 'identifier' and child.text:
                    name = child.text.decode('utf-8')
                    if name in symbol_names:
                        dataflow.assign('<union_init>', name)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'field_expression':
                caller = _find_enclosing_function(node, tree.root_node)
                targets = dataflow.resolve('<union_init>')
                for target in targets:
                    edges.append(CallEdge(
                        caller=caller or '<unknown>',
                        callee=target,
                        caller_file=filepath,
                        callee_file='',
                        type=CallType.INDIRECT,
                        indirect_kind='union_fp',
                        caller_line=node.start_point[0] + 1,
                    ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _find_enclosing_function(node: ts.Node, root: ts.Node) -> str | None:
    result = [None]
    def _search(n: ts.Node, line: int) -> None:
        if result[0] is not None: return
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if decl:
                ident = _find_child(decl, 'identifier')
                if ident and ident.text:
                    result[0] = ident.text.decode('utf-8')
        for c in n.children:
            if c.start_point[0] <= line <= c.end_point[0]:
                _search(c, line)
    _search(root, node.start_point[0])
    return result[0]

def _find_child(node: ts.Node, type_name: str) -> ts.Node | None:
    for c in node.children:
        if c.type == type_name: return c
    return None
