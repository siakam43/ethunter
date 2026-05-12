"""Field-expression-based function pointer call detection.

Detects calls through struct field access:
- obj.field()
- ptr->field()
- ptr->chain->field()  (chain access)

Also tracks field assignments (obj.field = func) for dataflow lookup.
Handles macro-expanded field calls: #define MACRO(...) obj.field(...)
"""

from __future__ import annotations

import re

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path, collect_field_assignments


def _collect_macros(tree: ts.Tree) -> dict[str, str]:
    """Collect preproc_def/preproc_function_def macros and their bodies.
    Returns mapping: macro_name -> macro_body_text
    """
    macros: dict[str, str] = {}

    def _scan(n: ts.Node) -> None:
        if n.type in ('preproc_def', 'preproc_function_def'):
            name_node = None
            body_text = None
            for child in n.children:
                if child.type == 'identifier' and child.text:
                    name_node = child
                elif child.type == 'preproc_arg' and child.text:
                    body_text = child.text.decode('utf-8')
            if name_node and name_node.text and body_text:
                macros[name_node.text.decode('utf-8')] = body_text
        for child in n.children:
            _scan(child)

    _scan(tree.root_node)
    return macros


def _extract_field_path_from_macro_body(body: str) -> str | None:
    """Extract struct_var.field pattern from macro body text.
    e.g., 'streamer_hooks.read_tree(IB, DATA_IN)' -> 'streamer_hooks.read_tree'
    """
    match = re.search(r'(\w+)\s*(?:\.|->)\s*(\w+)', body)
    if match:
        return f'{match.group(1)}.{match.group(2)}'
    return None


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through struct field expressions."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names
    macro_map = _collect_macros(tree)

    def _extract_field_expression(node: ts.Node | None) -> ts.Node | None:
        """Extract a field_expression, unwrapping parentheses and pointer expressions."""
        if not node:
            return None
        if node.type == 'field_expression':
            return node
        # (*ptr->field) → parenthesized_expression → pointer_expression → field_expression
        if node.type == 'parenthesized_expression':
            for c in node.children:
                if c.type == 'pointer_expression':
                    for cc in c.children:
                        if cc.type == 'field_expression':
                            return cc
        return node if node.type == 'field_expression' else None

    # Pass 1: collect all field assignments across the entire file
    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.resolved_value is not None and fa.resolved_value in symbol_names:
            dataflow.assign(f'<gstruct:{fa.field_path}>', fa.resolved_value)

    # Pass 2: detect call sites (existing logic, minus the assignment block)
    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            field_expr = _extract_field_expression(func_node)
            if field_expr:
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(field_expr)
                if field_path:
                    targets = set()
                    # Try <gstruct:path> first (from initializer_assign or this module)
                    targets = dataflow.resolve(f'<gstruct:{field_path}>')
                    # Try <struct:path> (from param_assign)
                    if not targets:
                        targets = dataflow.resolve(f'<struct:{field_path}>')
                    # Try <chain:path> for complex chain
                    if not targets:
                        targets = dataflow.resolve(f'<chain:{field_path}>')
                    # Fallback: global array initializer (e.g., <garray:global_hooks>)
                    if not targets:
                        base_name = field_path.split('.')[0]
                        garray_targets = dataflow.resolve(f'<garray:{base_name}>')
                        if garray_targets:
                            targets = garray_targets
                            # Also merge struct-specific targets for the same base
                            for key, vals in dataflow.targets.items():
                                if key.startswith(f'<gstruct:{base_name}.') and vals:
                                    targets.update(vals)
                    elif '.' in field_path:
                        # Also merge <garray:base> targets when field-name match exists
                        base_name = field_path.split('.')[0]
                        garray_targets = dataflow.resolve(f'<garray:{base_name}>')
                        if garray_targets:
                            targets.update(garray_targets)
                    # Always merge suffix-matched targets (even when <gstruct:> had partial hits)
                    if '.' in field_path:
                        parts = field_path.split('.')
                        for i in range(1, len(parts)):
                            suffix = '.'.join(parts[i:])
                            for key, vals in dataflow.targets.items():
                                if key.endswith(f'.{suffix}>') and vals:
                                    targets.update(vals)
                    # Fallback: resolve struct alias (e.g., Curl_ssl -> Curl_ssl_openssl)
                    if not targets and '.' in field_path:
                        parts = field_path.split('.')
                        alias_targets = dataflow.resolve(parts[0])
                        if alias_targets:
                            for resolved in alias_targets:
                                resolved_path = resolved + '.' + '.'.join(parts[1:])
                                targets = dataflow.resolve(f'<gstruct:{resolved_path}>')
                                if targets:
                                    break
                    # Fallback: suffix match on <struct:*.field> (e.g., input_buffer.hooks.allocate -> <struct:hooks.allocate> or <struct:hooks>)
                    if not targets and '.' in field_path:
                        parts = field_path.split('.')
                        # Try progressively shorter suffixes
                        for i in range(1, len(parts)):
                            suffix = '.'.join(parts[i:])
                            targets = dataflow.resolve(f'<struct:{suffix}>')
                            if targets:
                                break
                            targets = dataflow.resolve(f'<gstruct:{suffix}>')
                            if targets:
                                break
                        # If no match yet, try middle components as struct keys
                        if not targets and len(parts) > 1:
                            for part in parts[1:-1]:
                                targets = dataflow.resolve(f'<struct:{part}>')
                                if targets:
                                    break
                                targets = dataflow.resolve(f'<gstruct:{part}>')
                                if targets:
                                    break
                    # Fallback: try last component alone
                    if not targets:
                        last_part = field_path.split('.')[-1]
                        # Try <struct:field> and <gstruct:var.field> for any var
                        targets = dataflow.resolve(last_part)
                        if not targets:
                            # Scan for keys ending with .{last_part}> in dataflow
                            for key, vals in dataflow.targets.items():
                                if key.endswith(f'.{last_part}>') and vals:
                                    targets.update(vals)
                    # Fallback: try <vtable:path> (old key format)
                    if not targets:
                        targets = dataflow.resolve(f'<vtable:{field_path}>')
                    # Fallback: global initializer list
                    if not targets:
                        targets = dataflow.resolve('<vtable_init>')

                    for target in targets:
                        edges.append(CallEdge(
                            caller=caller or '<unknown>',
                            callee=target,
                            caller_file=filepath,
                            callee_file='',
                            type=CallType.INDIRECT,
                            indirect_kind='field_call',
                            caller_line=node.start_point[0] + 1,
                        ))
            elif func_node.type == 'identifier' and func_node.text:
                # Fallback: macro-expanded field call
                call_name = func_node.text.decode('utf-8')
                if call_name in macro_map:
                    body = macro_map[call_name]
                    resolved_path = _extract_field_path_from_macro_body(body)
                    if resolved_path:
                        targets = dataflow.resolve(f'<gstruct:{resolved_path}>')
                        if targets:
                            caller = find_enclosing_function(node, tree.root_node)
                            for target in targets:
                                edges.append(CallEdge(
                                    caller=caller or '<unknown>',
                                    callee=target,
                                    caller_file=filepath,
                                    callee_file='',
                                    type=CallType.INDIRECT,
                                    indirect_kind='field_call',
                                    caller_line=node.start_point[0] + 1,
                                ))
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
