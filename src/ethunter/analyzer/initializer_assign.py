"""Initializer-based function pointer assignment tracking.

Handles init_declarator with initializer_list patterns:
- Pure array: arr[] = { func_a, func_b } → key: <garray:arr>
- Designated initializer: s = { .field = func } → key: <gstruct:s.field>
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import extract_identifier_from_declarator


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list:
    """Track function pointer assignments via initializers."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _extract_field_name(pair_node: ts.Node) -> str | None:
        """Extract the field name from a pair node (.field = value)."""
        for c in pair_node.children:
            if c.type == 'field_designator' and c.text:
                return c.text.decode('utf-8').lstrip('.')
        # Fallback: look for field_identifier
        key = pair_node.child_by_field_name('key')
        if key and key.type == 'field_identifier' and key.text:
            return key.text.decode('utf-8')
        return None

    def _process_init_list(init_list: ts.Node, var_name: str) -> None:
        """Process an initializer_list node."""
        if not init_list:
            return
        # Check for designated initializers: initializer_pair with field_designator
        has_designated = any(c.type == 'initializer_pair' for c in init_list.children)
        if has_designated:
            for pair in init_list.children:
                if pair.type != 'initializer_pair':
                    continue
                field_name = _extract_field_name(pair)
                value = pair.children[-1] if pair.children else None
                if field_name and value and value.type == 'identifier' and value.text:
                    target = value.text.decode('utf-8')
                    if target in symbol_names:
                        dataflow.assign(f'<gstruct:{var_name}.{field_name}>', target)
            return
        # Pure identifier list: { func_a, func_b, ... }
        # Store both <garray:var> (for array calls) and <gstruct:var.N> (for field calls)
        index = 0
        for c in init_list.children:
            if c.type == 'identifier' and c.text:
                name = c.text.decode('utf-8')
                if name in symbol_names:
                    dataflow.assign(f'<garray:{var_name}>', name)
                    dataflow.assign(f'<gstruct:{var_name}.{index}>', name)
                    index += 1
            elif c.type == 'initializer_list':
                # Recurse into nested initializer lists (struct array elements)
                for inner in c.children:
                    if inner.type == 'identifier' and inner.text:
                        name = inner.text.decode('utf-8')
                        if name in symbol_names:
                            dataflow.assign(f'<garray:{var_name}>', name)

    def _visit(node: ts.Node) -> None:
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            init_list = node.child_by_field_name('value')
            if not init_list:
                for c in node.children:
                    if c.type == 'initializer_list':
                        init_list = c
                        break
            if declarator and init_list:
                var_name = extract_identifier_from_declarator(declarator)
                if var_name:
                    _process_init_list(init_list, var_name)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
