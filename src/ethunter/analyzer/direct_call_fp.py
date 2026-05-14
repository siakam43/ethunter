"""Direct identifier-based function pointer call detection.

Detects calls through function pointers identified by simple identifiers:
- fp() where fp has been assigned via dataflow
- fp() where fp is a local variable from a struct field assignment
- (*fp)() pointer dereference calls with the same resolution
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable
from ethunter.analyzer.helpers import find_enclosing_function
from ethunter.analyzer.local_fp_tracker import collect_local_fp_assignments


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table: SymbolTable,
    dataflow: VariableState,
) -> list[CallEdge]:
    """Detect indirect calls through function pointer identifiers."""
    edges: list[CallEdge] = []
    symbol_names = symbol_table.all_function_names
    local_mapping = collect_local_fp_assignments(tree, dataflow, symbol_names)

    def _get_targets(var_name: str, caller_func: str | None = None) -> set[str]:
        """Resolve function targets for a variable name.

        Checks in order:
        1. Scoped key <var>:<caller_func>:<var_name>
        2. Bare variable name (fallback)
        3. Local variable from struct field
        """
        targets = set()
        if caller_func:
            if hasattr(dataflow, 'store'):
                targets = dataflow.store.resolve_func_var(caller_func, var_name)
            if not targets:
                targets = dataflow.resolve(f'<var>:{caller_func}:{var_name}')
        if not targets:
            targets = dataflow.resolve(var_name)
        if not targets:
            targets = local_mapping.get(var_name, set()).copy()
        return targets

    def _add_edges(func_name: str, call_node: ts.Node) -> None:
        """Add call edges for resolved targets."""
        caller = find_enclosing_function(call_node, tree.root_node)
        targets = _get_targets(func_name, caller)
        if targets:
            for target in targets:
                edges.append(CallEdge(
                    caller=caller or '<unknown>',
                    callee=target,
                    caller_file=filepath,
                    callee_file='',
                    type=CallType.INDIRECT,
                    indirect_kind='direct_assign',
                    caller_line=call_node.start_point[0] + 1,
                ))

    def _visit(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.type == 'identifier' and func_node.text:
                var_name = func_node.text.decode('utf-8')
                _add_edges(var_name, node)
            elif func_node and func_node.type == 'parenthesized_expression':
                # Handle (*fp)(args) pattern
                inner = _unwrap_pointer(func_node)
                if inner and inner.type == 'identifier' and inner.text:
                    var_name = inner.text.decode('utf-8')
                    _add_edges(var_name, node)
        for child in node.children:
            _visit(child)

    def _unwrap_pointer(node: ts.Node) -> ts.Node | None:
        """Unwrap parenthesized_expression → pointer_expression to get inner identifier."""
        for c in node.children:
            if c.type == 'pointer_expression':
                for cc in c.children:
                    if cc.type == 'identifier':
                        return cc
        return None

    _visit(tree.root_node)
    return edges
