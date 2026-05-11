"""Field-expression-based function pointer call detection.

Detects calls through struct field access:
- obj.field()
- ptr->field()
- ptr->chain->field()  (chain access)

Also tracks field assignments (obj.field = func) for dataflow lookup.
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through struct field expressions."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names

    def _visit(node: ts.Node) -> None:
        # Track field assignments: obj.field = func_name
        if node.type == 'assignment_expression':
            lhs = node.child_by_field_name('left') or node.children[0]
            rhs = node.child_by_field_name('right') or node.children[1]
            if lhs and rhs and lhs.type == 'field_expression' and rhs.type == 'identifier' and rhs.text:
                target = rhs.text.decode('utf-8')
                if target in symbol_names:
                    field_path = extract_field_path(lhs)
                    if field_path:
                        dataflow.assign(f'<gstruct:{field_path}>', target)

        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'field_expression':
                caller = find_enclosing_function(node, tree.root_node)
                field_path = extract_field_path(func_node)
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
                        targets = dataflow.resolve(f'<garray:{base_name}>')
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
                        targets = dataflow.resolve(last_part)
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
        for child in node.children:
            _visit(child)

    _visit(tree.root_node)
    return edges
