"""Module 12: Macro-generated function pointer operations (partial support)."""

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
    """Parse #define directives and macro call sites to detect function references."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    # Collect macro definitions: name -> body text
    macro_defs: dict[str, str] = {}

    def _collect_macros(node: ts.Node) -> None:
        if node.type in ('preproc_def', 'preproc_function_def'):
            # Extract the macro name and body from the text
            text = node.text.decode('utf-8')
            if text.startswith('#define'):
                parts = text[len('#define'):].strip().split(None, 1)
                if len(parts) >= 2:
                    macro_name = parts[0].split('(')[0]  # Strip parameters
                    macro_body = parts[1]
                    macro_defs[macro_name] = macro_body
        for child in node.children:
            _collect_macros(child)

    _collect_macros(tree.root_node)

    # Find call sites where macro names are used
    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                if call_name in macro_defs:
                    # This is a macro call - check if the macro body references known functions
                    macro_body = macro_defs[call_name]
                    caller = _find_enclosing_function(node, tree.root_node)
                    for sym in symbol_names:
                        if sym in macro_body:
                            edges.append(CallEdge(
                                caller=caller or f'<macro:{call_name}>',
                                callee=sym,
                                caller_file=filepath,
                                callee_file='',
                                type=CallType.INDIRECT,
                                indirect_kind='macro_fp',
                                caller_line=node.start_point[0] + 1,
                            ))
                    # Also check arguments passed to the macro
                    args = node.child_by_field_name('arguments')
                    if args:
                        for arg in args.children:
                            if arg.type == 'identifier' and arg.text:
                                arg_name = arg.text.decode('utf-8')
                                if arg_name in symbol_names:
                                    edges.append(CallEdge(
                                        caller=caller or f'<macro:{call_name}>',
                                        callee=arg_name,
                                        caller_file=filepath,
                                        callee_file='',
                                        type=CallType.INDIRECT,
                                        indirect_kind='macro_fp',
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
