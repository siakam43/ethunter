"""Phase 3: Callback registration detection — produces callback_reg edges.

Three-stage determination:
  Stage 1: Behavior check (param_usage): forwarder/storage -> skip
  Stage 2: Coverage check (covered_callees): target already dispatched by field_call -> skip
  Stage 3: Heuristic fallback: usage == 'unknown' and _is_registration(callee) -> emit
"""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence
from ethunter.analyzer.param_helpers import _is_registration


def analyze(
    tree: ts.Tree,
    filepath: str,
    dataflow,
) -> list[CallEdge]:
    """Produce callback_reg edges for registration sites, applying behavioral
    and coverage checks.
    """
    edges: list[CallEdge] = []
    param_usage = dataflow.param_usage
    covered_callees = dataflow.covered_callees

    for site in dataflow.registration_sites:
        target = site["target"]
        callee = site["callee"]
        arg_idx = site["arg_idx"]

        # Stage 1: Behavior check
        usage = param_usage.get((callee, arg_idx), 'unknown')
        if usage in ('forwarder', 'storage'):
            continue
        if usage == 'caller':
            pass  # proceed to Stage 2

        # Stage 2: Coverage check
        if target in covered_callees:
            continue

        # Stage 3: Heuristic fallback for unknown usage
        if usage == 'unknown' and not _is_registration(callee):
            continue

        if usage == 'caller':
            confidence, evidence = Confidence.MEDIUM, Evidence('behavioral_registration')
        else:
            confidence, evidence = Confidence.LOW, Evidence('heuristic_registration')

        edges.append(CallEdge(
            caller=site["caller"],
            callee=target,
            caller_file=site["file"],
            callee_file='',
            type=CallType.INDIRECT,
            indirect_kind='callback_reg',
            caller_line=site["line"],
            confidence=confidence,
            evidence=evidence,
        ))

    return edges
