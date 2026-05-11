"""Initializer-based function pointer assignment tracking.

Handles init_declarator with initializer_list patterns:
- Pure array: arr[] = { func_a, func_b } → key: <garray:arr>
- Designated initializer: s = { .field = func } → key: <gstruct:s.field>
- Cast in init: { (type)func } → key: <gstruct:var.field> (positional mapped to field name)
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

    def _extract_cast_target(node: ts.Node) -> str | None:
        """Extract function name from inside a cast_expression."""
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
                if name in symbol_names:
                    return name
        return None

    def _extract_function_from_value(value_node: ts.Node) -> str | None:
        """Extract a function name from an initializer value node."""
        if value_node.type == 'identifier' and value_node.text:
            name = value_node.text.decode('utf-8')
            if name in symbol_names:
                return name
        cast_target = _extract_cast_target(value_node)
        if cast_target:
            return cast_target
        # Handle macro wrapper call: MACRO(type, func_name)
        if value_node.type == 'call_expression':
            args = value_node.child_by_field_name('arguments')
            if args:
                for c in reversed(args.children):
                    if c.type == 'identifier' and c.text:
                        name = c.text.decode('utf-8')
                        if name in symbol_names:
                            return name
        return None

    def _extract_field_name(pair_node: ts.Node) -> str | None:
        """Extract the field name from a pair node (.field = value)."""
        for c in pair_node.children:
            if c.type == 'field_designator' and c.text:
                return c.text.decode('utf-8').lstrip('.')
        key = pair_node.child_by_field_name('key')
        if key and key.type == 'field_identifier' and key.text:
            return key.text.decode('utf-8')
        return None

    def _collect_struct_field_names() -> dict[str, list[str]]:
        """Build a map from struct type name to ordered list of field identifiers."""
        struct_fields: dict[str, list[str]] = {}

        def _scan(n: ts.Node) -> None:
            if n.type == 'struct_specifier':
                type_id = None
                field_list = []
                for cc in n.children:
                    if cc.type == 'type_identifier' and cc.text:
                        type_id = cc.text.decode('utf-8')
                    if cc.type == 'field_declaration_list':
                        for fc in cc.children:
                            if fc.type == 'field_declaration':
                                # Direct field_identifier
                                for fcc in fc.children:
                                    if fcc.type == 'field_identifier' and fcc.text:
                                        field_list.append(fcc.text.decode('utf-8'))
                                    # Function pointer: void (*field_name)(...) — name is in pointer_declarator
                                    if fcc.type in ('function_declarator', 'pointer_declarator', 'parenthesized_declarator'):
                                        name = _extract_declarator_id(fcc)
                                        if name:
                                            field_list.append(name)
                if type_id and field_list:
                    struct_fields[type_id] = field_list
            for child in n.children:
                _scan(child)

        def _extract_declarator_id(node: ts.Node) -> str | None:
            """Extract identifier from a declarator that may contain pointer/function_declarator nesting."""
            if node.type in ('identifier', 'field_identifier') and node.text:
                return node.text.decode('utf-8')
            for c in node.children:
                if c.type in ('pointer_declarator', 'parenthesized_declarator', 'function_declarator', 'array_declarator'):
                    result = _extract_declarator_id(c)
                    if result:
                        return result
                if c.type in ('identifier', 'field_identifier') and c.text:
                    return c.text.decode('utf-8')
            return None

        _scan(tree.root_node)
        return struct_fields

    struct_field_map = _collect_struct_field_names()

    # Build typedef alias → struct type mapping
    typedef_map: dict[str, str] = {}

    def _collect_typedefs(n: ts.Node) -> None:
        if n.type == 'type_definition':
            struct_type = None
            for c in n.children:
                if c.type == 'struct_specifier':
                    for cc in c.children:
                        if cc.type == 'type_identifier' and cc.text:
                            struct_type = cc.text.decode('utf-8')
                if c.type == 'type_identifier' and c.text and struct_type:
                    typedef_map[c.text.decode('utf-8')] = struct_type
        for child in n.children:
            _collect_typedefs(child)

    _collect_typedefs(tree.root_node)

    def _get_parent(target: ts.Node) -> ts.Node | None:
        """Find the parent of a node by walking from root."""
        result = [None]

        def _search(n: ts.Node) -> None:
            if result[0]:
                return
            if target in n.children:
                result[0] = n
                return
            for c in n.children:
                if c.start_point[0] <= target.start_point[0] and target.end_point[0] <= c.end_point[0]:
                    _search(c)

        _search(tree.root_node)
        return result[0]

    def _resolve_struct_type(node: ts.Node) -> str | None:
        """Try to extract struct type name from a declaration's type specifiers."""
        parent = _get_parent(node)
        if parent:
            for c in parent.children:
                if c.type == 'struct_specifier':
                    for cc in c.children:
                        if cc.type == 'type_identifier' and cc.text:
                            return cc.text.decode('utf-8')
                # Handle typedef alias type (e.g., fmd_hdl_ops_t → fmd_hdl_ops)
                if c.type == 'type_identifier' and c.text:
                    type_name = c.text.decode('utf-8')
                    resolved = typedef_map.get(type_name)
                    if resolved:
                        return resolved
        return None

    def _process_init_list(init_list: ts.Node, var_name: str, struct_type: str | None = None) -> None:
        """Process an initializer_list node."""
        if not init_list:
            return

        # Designated initializers: initializer_pair with field_designator
        has_designated = any(c.type == 'initializer_pair' for c in init_list.children)
        if has_designated:
            for pair in init_list.children:
                if pair.type != 'initializer_pair':
                    continue
                field_name = _extract_field_name(pair)
                value = pair.children[-1] if pair.children else None
                if value:
                    target = _extract_function_from_value(value)
                    if target and field_name:
                        dataflow.assign(f'<gstruct:{var_name}.{field_name}>', target)
            return

        # Positional (pure identifier/cast list): { func_a, (type)func_b, ... }
        # For structs, map positional index → field name from struct_field_map
        field_names = struct_field_map.get(struct_type, []) if struct_type else []
        index = 0
        for c in init_list.children:
            if c.type in ('identifier', 'cast_expression', 'call_expression'):
                target = _extract_function_from_value(c)
                if target:
                    dataflow.assign(f'<garray:{var_name}>', target)
                    # Store with numeric index
                    dataflow.assign(f'<gstruct:{var_name}.{index}>', target)
                    # Also store with field name if we can map the index
                    if index < len(field_names):
                        field_name = field_names[index]
                        dataflow.assign(f'<gstruct:{var_name}.{field_name}>', target)
                    index += 1
            elif c.type == 'initializer_list':
                for inner in c.children:
                    if inner.type in ('identifier', 'cast_expression'):
                        target = _extract_function_from_value(inner)
                        if target:
                            dataflow.assign(f'<garray:{var_name}>', target)

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
                    struct_type = _resolve_struct_type(node)
                    _process_init_list(init_list, var_name, struct_type)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
