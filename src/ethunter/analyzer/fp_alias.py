"""Module 10: Function pointer aliasing/redirection analysis."""

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
            _handle_init_declarator(node, dataflow, symbol_names)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
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
                            indirect_kind='fp_alias',
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
