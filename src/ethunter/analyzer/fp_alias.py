"""Module 10: Function pointer aliasing/redirection analysis."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import (
    find_enclosing_function,
    find_child,
    handle_init_declarator,
)


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Track fp2 = fp1 alias chains and propagate possible targets."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'identifier' and rhs.type == 'identifier':
                dst = lhs.text.decode('utf-8') if lhs.text else ''
                src = rhs.text.decode('utf-8') if rhs.text else ''
                if src and dst:
                    if src in symbol_names:
                        dataflow.assign(dst, src)
                    else:
                        targets = dataflow.resolve(src)
                        if targets:
                            for t in targets:
                                dataflow.assign(dst, t)
        if node.type == 'init_declarator':
            handle_init_declarator(node, dataflow, symbol_names)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
                var_name = func_node.text.decode('utf-8')
                targets = dataflow.resolve(var_name)
                if targets:
                    caller = find_enclosing_function(node, tree.root_node)
                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='fp_alias',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges

