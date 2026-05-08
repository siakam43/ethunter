"""Module 2: Function pointer assignment + call analysis."""

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
    """Track function pointer assignments and subsequent calls through them."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # Track assignments: fp = func_name
        if node.type == 'assignment_expression':
            _handle_assignment(node, dataflow, symbol_names)
        # Track init_declarators: void (*fp)(void) = func_name
        elif node.type == 'init_declarator':
            handle_init_declarator(node, dataflow, symbol_names)
        # Track calls through function pointers: fp()
        elif node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier':
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
                            indirect_kind='fp_assign',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _handle_assignment(
    node: ts.Node,
    dataflow: VariableState,
    symbol_names: set[str],
) -> None:
    """Handle assignments like fp = func_name or fp2 = fp1."""
    lhs = node.child_by_field_name('left') or node.children[0]
    rhs = node.child_by_field_name('right') or node.children[1]
    if not lhs or not rhs:
        return

    if lhs.type == 'identifier' and lhs.text:
        var_name = lhs.text.decode('utf-8')
        # Direct function reference: fp = func_name
        if rhs.type == 'identifier' and rhs.text:
            target = rhs.text.decode('utf-8')
            if target in symbol_names:
                dataflow.assign(var_name, target)
            else:
                # fp2 = fp1 (alias chain)
                targets = dataflow.resolve(target)
                if targets:
                    for t in targets:
                        dataflow.assign(var_name, t)

