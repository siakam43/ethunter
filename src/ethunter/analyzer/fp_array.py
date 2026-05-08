"""Module 5: Function pointer array / dispatch table analysis."""

from __future__ import annotations

import re
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
    """Detect function pointer arrays and dispatch table calls."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # Detect array declarations: void (*arr[])(void) = { func_a, func_b }
        # AST: declaration -> init_declarator -> initializer_list
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            init_list = node.child_by_field_name('value')
            if not init_list:
                # fallback: find initializer_list among children
                for c in node.children:
                    if c.type == 'initializer_list':
                        init_list = c
                        break
            if declarator and init_list and declarator.text:
                # Extract array name from declarator text like "(*dispatch[])(void)"
                arr_name = '<initializer>'
                decl_text = declarator.text.decode('utf-8')
                m = re.search(r'(\w+)\s*\[', decl_text)
                if m:
                    arr_name = m.group(1)
                for child in init_list.children:
                    if child.type == 'identifier' and child.text:
                        name = child.text.decode('utf-8')
                        if name in symbol_names:
                            dataflow.assign(arr_name, name)
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'subscript_expression':
                caller = find_enclosing_function(node, tree.root_node)
                arr_node = func_node.children[0] if func_node.children else None
                if arr_node and arr_node.text:
                    arr_name = arr_node.text.decode('utf-8')
                    targets = dataflow.resolve(arr_name)
                    if not targets:
                        targets = dataflow.resolve('<initializer>')
                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='fp_array',
                            caller_line=node.start_point[0] + 1,
                        ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges

