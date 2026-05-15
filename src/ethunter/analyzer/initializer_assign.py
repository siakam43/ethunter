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
from ethunter.analyzer.helpers import extract_identifier_from_declarator, collect_pointer_resolutions


def collect_var_types(tree: ts.Tree, filepath: str,
                      symbol_table, dataflow) -> None:
    """Phase 1a: collect struct variable types from init_declarators.
    Must run BEFORE field_call.collect() so var_types are available.
    Returns no edges — metadata only.
    """
    # Build typedef map from symbol_table
    typedef_map = getattr(symbol_table, '_typedefs', {})

    def _resolve_type(decl_node):
        """Extract struct type name from a declaration node."""
        for c in decl_node.children:
            if c.type == 'struct_specifier':
                for cc in c.children:
                    if cc.type == 'type_identifier' and cc.text:
                        return cc.text.decode('utf-8')
            if c.type == 'type_identifier' and c.text:
                type_name = c.text.decode('utf-8')
                resolved = typedef_map.get(type_name)
                if resolved:
                    return resolved
                return type_name
        return None

    def _scan(node):
        if node.type == 'declaration':
            type_name = _resolve_type(node)
            if type_name:
                for c in node.children:
                    if c.type == 'init_declarator':
                        declarator = c.child_by_field_name('declarator')
                        if declarator:
                            var_name = extract_identifier_from_declarator(declarator)
                            if var_name:
                                symbol_table.record_var_type(var_name, type_name)
        for child in node.children:
            _scan(child)
    _scan(tree.root_node)


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list:
    """Track function pointer assignments via initializers."""
    edges: list = []
    symbol_names = symbol_table.all_function_names

    def _assign_gstruct(field_path: str, target: str) -> None:
        """Write gstruct dataflow key in old + new (field_tail) formats."""
        dataflow.assign(f'<gstruct:{field_path}>', target)  # backward compat
        base_var = field_path.split('.')[0]
        field_tail = dataflow.store.compute_field_tail(field_path) if hasattr(dataflow, 'store') else field_path
        if hasattr(dataflow, 'store'):
            dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', target, filepath)
        struct_type = symbol_table.get_var_type(base_var)
        if struct_type:
            dataflow.assign(f'<gstruct>:{struct_type}.{field_path}>', target)
            if hasattr(dataflow, 'store'):
                dataflow.store.assign_struct_field(f'gstruct:{struct_type}.{field_tail}', target, filepath)

    def _extract_cast_target(node: ts.Node) -> str | None:
        """Extract function name from inside a cast_expression."""
        # Try unwrap_cast if dataflow has it (DataflowEngine)
        if hasattr(dataflow, 'unwrap_cast'):
            result = dataflow.unwrap_cast(node)
            if result:
                return result
        # Fallback: original single-level logic
        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value and value.type == 'identifier' and value.text:
                name = value.text.decode('utf-8')
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
                                    if fcc.type in ('function_declarator', 'pointer_declarator', 'parenthesized_declarator', 'array_declarator'):
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
    # Register struct fields with SymbolTable for type-aware lookup
    for stype, fields in struct_field_map.items():
        symbol_table.record_struct_fields(stype, fields)

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
                        _assign_gstruct(f'{var_name}.{field_name}', target)
            return

        # Positional (pure identifier/cast list): { func_a, (type)func_b, ... }
        # For structs, map positional index → field name from struct_field_map
        field_names = struct_field_map.get(struct_type, []) if struct_type else []
        # Node types that represent value positions (increment index)
        _VALUE_TYPES = {
            'identifier', 'cast_expression', 'call_expression',
            'string_literal', 'number_literal', 'null',
            'pointer_expression', 'field_expression',
            'parenthesized_expression', 'char_literal', 'concatenated_string',
            'sizeof_expression', 'conditional_expression',
            'binary_expression', 'unary_expression', 'subscript_expression',
        }
        # Node types that carry function targets (store to dataflow)
        _STORE_TYPES = {'identifier', 'cast_expression', 'call_expression'}
        # pointer_expression stores struct names (for array-of-struct-pointers)
        _STRUCT_REF_TYPES = {'pointer_expression'}

        index = 0
        for c in init_list.children:
            if c.type in _VALUE_TYPES:
                if c.type in _STORE_TYPES:
                    target = _extract_function_from_value(c)
                    if target:
                        dataflow.assign(f'<garray:{var_name}>', target)
                        if hasattr(dataflow, 'store'):
                            dataflow.store.assign_global_array(var_name, target)
                        _assign_gstruct(f'{var_name}.{index}', target)
                        if index < len(field_names):
                            field_name = field_names[index]
                            _assign_gstruct(f'{var_name}.{field_name}', target)
                elif c.type in _STRUCT_REF_TYPES:
                    # &struct_name -> store struct name for downstream resolution
                    inner = c.children[-1] if c.children else None
                    if inner and inner.type == 'identifier' and inner.text:
                        dataflow.assign(f'<garray:{var_name}>', inner.text.decode('utf-8'))
                        if hasattr(dataflow, 'store'):
                            dataflow.store.assign_global_array(var_name, inner.text.decode('utf-8'))
                index += 1
            elif c.type == 'initializer_list':
                inner_index = 0
                for inner in c.children:
                    if inner.type in _VALUE_TYPES:
                        if inner.type in _STORE_TYPES:
                            target = _extract_function_from_value(inner)
                            if target:
                                dataflow.assign(f'<garray:{var_name}>', target)
                                if hasattr(dataflow, 'store'):
                                    dataflow.store.assign_global_array(var_name, target)
                                if inner_index < len(field_names):
                                    field_name = field_names[inner_index]
                                    _assign_gstruct(f'{var_name}.{field_name}', target)
                        elif inner.type in _STRUCT_REF_TYPES:
                            ref = inner.children[-1] if inner.children else None
                            if ref and ref.type == 'identifier' and ref.text:
                                dataflow.assign(f'<garray:{var_name}>', ref.text.decode('utf-8'))
                                if hasattr(dataflow, 'store'):
                                    dataflow.store.assign_global_array(var_name, ref.text.decode('utf-8'))
                        inner_index += 1

    def _track_pointer_field_assignments(
        tree: ts.Tree,
        filepath: str,
        dataflow: VariableState,
        symbol_names: set[str],
    ) -> None:
        """Track vec->field = func assignments, resolving vec to global array name.

        Handles two cases:
        1. vec->field = literal_func (direct literal function name)
        2. vec->field = param_func (parameter, needs call-site tracing)
        """
        resolutions = collect_pointer_resolutions(tree)

        # Collect function parameters (func_name -> [param_names])
        func_params: dict[str, list[str]] = {}

        def _extract_param_id(node: ts.Node) -> str | None:
            """Extract identifier from parameter_declaration."""
            if node.type == 'identifier' and node.text:
                return node.text.decode('utf-8')
            for c in node.children:
                if c.type in ('parenthesized_declarator', 'pointer_declarator',
                              'array_declarator', 'function_declarator'):
                    result = _extract_param_id(c)
                    if result:
                        return result
                if c.type == 'identifier' and c.text:
                    return c.text.decode('utf-8')
            return None

        def _collect_func_params(node: ts.Node) -> None:
            if node.type == 'function_definition':
                decl = None
                for c in node.children:
                    if c.type == 'function_declarator':
                        decl = c
                        break
                    if c.type in ('pointer_declarator', 'parenthesized_declarator'):
                        for cc in c.children:
                            if cc.type == 'function_declarator':
                                decl = cc
                                break
                if decl:
                    func_name_node = None
                    for c in decl.children:
                        if c.type == 'identifier' and c.text:
                            func_name_node = c
                            break
                    if func_name_node:
                        fname = func_name_node.text.decode('utf-8')
                        params = []
                        plist = None
                        for c in decl.children:
                            if c.type == 'parameter_list':
                                plist = c
                                break
                        if plist:
                            for p in plist.children:
                                if p.type == 'parameter_declaration':
                                    pname = _extract_param_id(p)
                                    if pname:
                                        params.append(pname)
                        func_params[fname] = params
            for child in node.children:
                _collect_func_params(child)

        _collect_func_params(tree.root_node)

        # Collect param mappings from call sites: param_name -> {actual_func, ...}
        param_mappings: dict[str, set[str]] = {}

        def _collect_call_params(node: ts.Node) -> None:
            if node.type == 'call_expression':
                func_node = node.child_by_field_name('function') or node.children[0]
                if func_node and func_node.text:
                    call_name = func_node.text.decode('utf-8')
                    args = node.child_by_field_name('arguments')
                    if args:
                        param_names = func_params.get(call_name, [])
                        arg_idx = 0
                        for c in args.children:
                            if c.type == '(' or c.type == ')' or c.type == ',':
                                continue
                            if c.type == 'identifier' and c.text:
                                target = c.text.decode('utf-8')
                                if target in symbol_names and arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].add(target)
                                arg_idx += 1
            for child in node.children:
                _collect_call_params(child)

        _collect_call_params(tree.root_node)

        # Fix B: register param->global array bindings
        def _register_param_global_aliases(node: ts.Node) -> None:
            if node.type == 'call_expression':
                func_node = node.child_by_field_name('function') or node.children[0]
                if func_node and func_node.text:
                    callee = func_node.text.decode('utf-8')
                    if callee in func_params:
                        args = node.child_by_field_name('arguments')
                        if args:
                            param_names = func_params[callee]
                            arg_idx = 0
                            for c in args.children:
                                if c.type in ('(', ')', ','):
                                    continue
                                if c.type == 'identifier' and c.text:
                                    arg_name = c.text.decode('utf-8')
                                    has_gstruct = any(
                                        k.startswith(f'<gstruct:{arg_name}.') and bool(v)
                                        for k, v in dataflow.targets.items()
                                    )
                                    has_garray = bool(dataflow.resolve(f'<garray:{arg_name}>'))
                                    if (has_gstruct or has_garray) and arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if not hasattr(dataflow, 'param_alias_map'):
                                            dataflow.param_alias_map = {}
                                        dataflow.param_alias_map[(callee, pname)] = arg_name
                                arg_idx += 1
            for child in node.children:
                _register_param_global_aliases(child)

        _register_param_global_aliases(tree.root_node)

        def _visit(n: ts.Node) -> None:
            if n.type == 'assignment_expression':
                lhs = n.child_by_field_name('left') or (n.children[0] if n.children else None)
                rhs = n.child_by_field_name('right') or (n.children[-1] if n.children else None)
                if lhs and lhs.type == 'field_expression' and rhs:
                    var_name = None
                    field_name = None
                    for child in lhs.children:
                        if child.type == 'identifier' and child.text:
                            var_name = child.text.decode('utf-8')
                        elif child.type == 'field_identifier' and child.text:
                            field_name = child.text.decode('utf-8')

                    if var_name and field_name and rhs.type == 'identifier' and rhs.text:
                        raw_name = rhs.text.decode('utf-8')
                        resolved = resolutions.get(var_name, var_name)
                        # Check if RHS is a literal function name
                        if raw_name in symbol_names:
                            dataflow.assign(f'<gstruct:{resolved}.{field_name}>', raw_name)
                            if hasattr(dataflow, 'store'):
                                dataflow.store.assign_struct_field(
                                    f'gstruct:{resolved}.{field_name}', raw_name, filepath)
                        # Check if RHS is a parameter — resolve to actual functions
                        elif raw_name in param_mappings:
                            for t in param_mappings[raw_name]:
                                dataflow.assign(f'<gstruct:{resolved}.{field_name}>', t)
                                if hasattr(dataflow, 'store'):
                                    dataflow.store.assign_struct_field(
                                        f'gstruct:{resolved}.{field_name}', t, filepath)
            for child in n.children:
                _visit(child)

        _visit(tree.root_node)

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
                    if struct_type:
                        symbol_table.record_var_type(var_name, struct_type)
                        if hasattr(dataflow, 'store'):
                            dataflow.store.aliases[var_name] = struct_type
                    _process_init_list(init_list, var_name, struct_type)
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)

    # Pass 2: Track vec->field = func (runtime struct pointer field assignments)
    _track_pointer_field_assignments(tree, filepath, dataflow, symbol_names)

    return edges
