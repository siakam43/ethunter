"""Local variable function pointer tracking.

Tracks local variables that inherit function pointer types from struct field access:
- Type local = struct_ptr->field;
- Type local = struct_var.field;
- local = struct_ptr->field;
- local = struct_var.field;

Returns a mapping from local variable name to resolved function targets.
Not stored in VariableState — local variables are function-scoped.
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.helpers import extract_field_path


def collect_local_fp_assignments(
    tree: ts.Tree,
    dataflow: VariableState,
    symbol_names: set[str],
    symbol_table=None,
) -> dict[str, set[str]]:
    """Collect local variable assignments from struct field function pointers.

    Returns mapping from local variable name to set of resolved function targets.
    """
    mapping: dict[str, set[str]] = {}

    def _visit(node: ts.Node) -> None:
        # init_declarator: Type local = struct.field or Type local = struct_ptr->field
        if node.type == 'init_declarator':
            declarator = node.child_by_field_name('declarator')
            value = node.child_by_field_name('value')
            if declarator and value and value.type == 'field_expression':
                var_name = _extract_identifier(declarator)
                if var_name:
                    _resolve_and_store(var_name, value, mapping, dataflow, symbol_table)

        # assignment_expression: local = struct.field or local = struct_ptr->field
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left')
            rhs = node.child_by_field_name('right')
            if lhs and rhs and lhs.type == 'identifier' and rhs.type == 'field_expression':
                var_name = lhs.text.decode('utf-8')
                _resolve_and_store(var_name, rhs, mapping, dataflow, symbol_table)

        for child in node.children:
            _visit(child)

    def _extract_identifier(declarator: ts.Node) -> str | None:
        """Extract identifier from a declarator (handles pointer_declarator nesting)."""
        if declarator.type in ('identifier', 'field_identifier') and declarator.text:
            return declarator.text.decode('utf-8')
        if declarator.type == 'pointer_declarator':
            return _extract_identifier(declarator.children[-1])
        if declarator.type in ('parenthesized_declarator', 'function_declarator', 'array_declarator'):
            for c in declarator.children:
                if c.type not in ('(', ')'):
                    result = _extract_identifier(c)
                    if result:
                        return result
        return None

    def _resolve_and_store(
        var_name: str,
        field_expr: ts.Node,
        mapping: dict[str, set[str]],
        dataflow: VariableState,
        symbol_table=None,
    ) -> None:
        """Build dataflow key from field expression and resolve targets."""
        field_path = extract_field_path(field_expr)
        if not field_path:
            return
        base_var = field_path.split('.')[0]
        targets, _, _ = dataflow.resolve_struct_field_call(
            field_path, base_var, None, '',
            symbol_table=symbol_table,
        )
        if targets:
            if var_name not in mapping:
                mapping[var_name] = set()
            mapping[var_name].update(targets)

    _visit(tree.root_node)
    return mapping
