"""Serialize CallGraph to JSON."""

import json
from ethunter.graph.model import CallGraph


def to_json(graph: CallGraph, indent: int = 2) -> str:
    return json.dumps(graph.to_dict(), indent=indent, ensure_ascii=False)
