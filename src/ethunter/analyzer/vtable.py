"""Module 6: Struct function pointer members (vtable style) analysis."""

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
    """Detect struct vtable-style calls via function pointer members."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _extract_field_path(node: ts.Node) -> str | None:
        """Extract 'struct_name.field_name' from a field_expression."""
        if node.type == 'field_expression':
            parts = []
            for child in node.children:
                if child.type in ('identifier', 'field_identifier') and child.text:
                    parts.append(child.text.decode('utf-8'))
            return '.'.join(parts) if parts else None
        return None

    def _visit(node: ts.Node) -> None:
        # Track struct member assignments: d.init = fs_init
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                target = rhs.text.decode('utf-8')
                if target in symbol_names:
                    field_path = _extract_field_path(lhs)
                    if field_path:
                        dataflow.assign(f'<vtable:{field_path}>', target)
                    else:
                        # Fallback: use just the field name
                        field_name = lhs.children[-1].text.decode('utf-8') if lhs.children else 'unknown'
                        dataflow.assign(f'<vtable:{field_name}>', target)
        # Track initializer_list: { fs_init, fs_read, ... }
        if node.type == 'initializer_list':
            for child in node.children:
                if child.type == 'identifier' and child.text:
                    name = child.text.decode('utf-8')
                    if name in symbol_names:
                        dataflow.assign('<vtable_init>', name)
        # Track field_expression calls: d.init()
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'field_expression':
                caller = find_enclosing_function(node, tree.root_node)
                # Try struct_name.field lookup first
                field_path = _extract_field_path(func_node)
                if field_path:
                    targets = dataflow.resolve(f'<vtable:{field_path}>')
                else:
                    targets = set()
                # Fallback to global initializer list
                if not targets:
                    targets = dataflow.resolve('<vtable_init>')
                for target in targets:
                    edges.append(CallEdge(
                        caller=caller or '<unknown>',
                        callee=target,
                        caller_file=filepath,
                        callee_file='',
                        type=CallType.INDIRECT,
                        indirect_kind='vtable',
                        caller_line=node.start_point[0] + 1,
                    ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges

