"""Module 11: Lazy initialization of function pointers analysis."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, find_child


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect lazy init patterns: if (!fp) fp = handler; then calls through fp."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        if node.type == 'if_statement':
            _handle_lazy_if(node, dataflow, symbol_names)
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
                            indirect_kind='lazy_init',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges


def _handle_lazy_if(
    node: ts.Node,
    dataflow: VariableState,
    symbol_names: set[str],
) -> None:
    """Look for if (!fp) fp = handler; patterns."""
    body = node.child_by_field_name('consequence')
    if not body:
        return

    def _collect_assignments(n: ts.Node) -> None:
        """Recursively find assignments within the if body."""
        if n.type == 'expression_statement':
            assign = find_child(n, 'assignment_expression')
            if assign:
                lhs = assign.child_by_field_name('left') or assign.children[0]
                rhs = assign.child_by_field_name('right') or assign.children[1]
                if lhs and rhs and lhs.type == 'identifier' and rhs.type == 'identifier':
                    var_name = lhs.text.decode('utf-8') if lhs.text else ''
                    target = rhs.text.decode('utf-8') if rhs.text else ''
                    if target in symbol_names:
                        dataflow.assign(var_name, target)
        for c in n.children:
            _collect_assignments(c)

    _collect_assignments(body)
