"""Module 2: Function pointer assignment + call analysis."""

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
    """Track function pointer assignments and subsequent calls through them."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # Track assignments: fp = func_name
        if node.type == 'assignment_expression':
            _handle_assignment(node, dataflow, symbol_names)
        # Track init_declarators: void (*fp)(void) = func_name
        elif node.type == 'init_declarator':
            _handle_init_declarator(node, dataflow, symbol_names)
        # Track calls through function pointers: fp()
        elif node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier':
                var_name = func_node.text.decode('utf-8')
                targets = dataflow.resolve(var_name)
                if targets:
                    caller = _find_enclosing_function(node, tree.root_node)
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


def _handle_init_declarator(
    node: ts.Node,
    dataflow: VariableState,
    symbol_names: set[str],
) -> None:
    """Handle init_declarators like void (*fp)(void) = func_name."""
    declarator = node.child_by_field_name('declarator')
    value = node.child_by_field_name('value')
    if not declarator or not value:
        return

    if value.type == 'identifier' and value.text:
        target = value.text.decode('utf-8')
        var_name = _extract_identifier_from_declarator(declarator)
        if var_name and target in symbol_names:
            dataflow.assign(var_name, target)


def _extract_identifier_from_declarator(declarator: ts.Node) -> str | None:
    """Extract the variable name from a pointer/function-pointer declarator."""
    if declarator.type == 'identifier' and declarator.text:
        return declarator.text.decode('utf-8')
    if declarator.type in ('parenthesized_declarator', 'function_declarator'):
        for c in declarator.children:
            if c.type not in ('(', ')', ';'):
                result = _extract_identifier_from_declarator(c)
                if result:
                    return result
    if declarator.type == 'pointer_declarator':
        return _extract_identifier_from_declarator(declarator.children[-1])
    return None


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


def _find_enclosing_function(node: ts.Node, root: ts.Node) -> str | None:
    result = [None]

    def _search(n: ts.Node, target_line: int) -> None:
        if result[0] is not None:
            return
        if n.type == 'function_definition':
            decl = _find_child(n, 'function_declarator')
            if decl:
                ident = _find_child(decl, 'identifier')
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
