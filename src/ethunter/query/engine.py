"""Query interface: callers/callees lookup by function name."""

from __future__ import annotations

from ethunter.graph.model import CallEdge, CallGraph


def query_callers(graph: CallGraph, func_name: str) -> list[CallEdge]:
    """Return all edges where func_name is the callee (who calls this function)."""
    return graph.query_callers(func_name)


def query_callees(graph: CallGraph, func_name: str) -> list[CallEdge]:
    """Return all edges where func_name is the caller (what this function calls)."""
    return graph.query_callees(func_name)
