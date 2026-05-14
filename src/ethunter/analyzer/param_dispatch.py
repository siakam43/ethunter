"""Phase 2: Fnptr parameter call detection — produces callback_param edges.

Pass A: Detect calls through fnptr params in function body (cb() / (*cb)())
Pass B: Emit call-site edges from caller -> target
Pass A/B dedup: when Pass A produces (inner_func, target), Pass B skips
  (outer_caller, target) for the same (target, arg_idx) pair.
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType
from ethunter.analyzer.helpers import find_enclosing_function


def analyze(
    tree: ts.Tree,
    filepath: str,
    dataflow,
) -> list[CallEdge]:
    """Detect indirect calls through fnptr params and produce callback_param edges."""
    edges: list[CallEdge] = []
    func_params = dataflow.func_params
    func_fp_params = getattr(dataflow.state, 'func_fp_params', {})

    # Read per-call-site targets from engine (populated by param_binding Phase 1)
    call_site_targets = dataflow.call_site_targets

    # Reconstruct param_mappings from old dataflow keys only in Phase A.
    # func_vars would aggregate direct_assign local vars — deferred to Phase C
    # where param_dispatch uses call_site_targets exclusively.
    param_mappings: dict[str, set[str]] = {}
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            param_name = key.split(':')[-1]
            if param_name not in param_mappings:
                param_mappings[param_name] = set()
            param_mappings[param_name].update(vals)

    # === Pass A: detect calls through fnptr params ===
    pass_a_edges: set[tuple[str, str, str, int]] = set()

    def _detect_param_calls(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            call_target_name = None
            if func_node and func_node.type == 'identifier' and func_node.text:
                call_target_name = func_node.text.decode('utf-8')
            elif func_node and func_node.type == 'parenthesized_expression':
                for c in func_node.children:
                    if c.type == 'pointer_expression' and c.children:
                        inner = c.children[-1]
                        if inner.type == 'identifier' and inner.text:
                            call_target_name = inner.text.decode('utf-8')
                            break
            elif func_node and func_node.type == 'pointer_expression' and func_node.children:
                inner = func_node.children[-1]
                if inner.type == 'identifier' and inner.text:
                    call_target_name = inner.text.decode('utf-8')

            if call_target_name:
                enclosing_func = find_enclosing_function(node, tree.root_node)
                targets = set()

                if enclosing_func and enclosing_func in func_params:
                    params = func_params[enclosing_func]
                    if call_target_name in params:
                        arg_idx = params.index(call_target_name)
                        for (clr, cn, ai), tgs in call_site_targets.items():
                            if cn == enclosing_func and ai == arg_idx:
                                targets.update(tgs)

                pm_targets = param_mappings.get(call_target_name, set())
                if pm_targets:
                    targets = targets | pm_targets

                if targets:
                    for target in targets:
                        pass_a_edges.add(
                            (enclosing_func or '<unknown>', target, filepath,
                             node.start_point[0] + 1))

        for child in node.children:
            _detect_param_calls(child)

    _detect_param_calls(tree.root_node)

    # Emit Pass A edges
    for (caller, target, fp, line) in pass_a_edges:
        edges.append(CallEdge(
            caller=caller,
            callee=target,
            caller_file=fp,
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_param',
            caller_line=line,
        ))

    # === Pass B: call-site caller edges (dedup against Pass A) ===
    pass_a_targets = {(tgt, caller) for (caller, tgt, _, _) in pass_a_edges}
    seen_pass4: set[tuple[str, str]] = set()

    for (caller, callee, arg_idx), targets in call_site_targets.items():
        for target in targets:
            key = (caller, target)
            if key in seen_pass4:
                continue
            if (target, callee) in pass_a_targets:
                continue
            seen_pass4.add(key)
            edges.append(CallEdge(
                caller=caller,
                callee=target,
                caller_file=filepath,
                callee_file='',
                type=CallType.INDIRECT,
                indirect_kind='callback_param',
                caller_line=0,
            ))

    return edges
