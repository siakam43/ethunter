"""Core data model for the call graph."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum


class CallType(Enum):
    DIRECT = "direct"
    INDIRECT = "indirect"


@dataclass(frozen=True)
class Function:
    """Represents a C function definition or declaration."""
    name: str
    file: str
    line: int
    signature: str = ""
    is_definition: bool = False
    return_type: str = ""
    parameters: list[str] = field(default_factory=list)

    @property
    def key(self) -> str:
        return f"{self.file}:{self.name}:{self.line}"


@dataclass(frozen=True)
class CallEdge:
    """Represents a call relationship between two functions."""
    caller: str  # function name
    callee: str  # function name
    caller_file: str = ""
    callee_file: str = ""
    type: CallType = CallType.DIRECT
    indirect_kind: str = ""
    caller_line: int = 0
    confidence: str = 'medium'   # 'high' | 'medium' | 'low'
    evidence: str = ''           # human-readable evidence description

    def to_dict(self) -> dict:
        d = {
            "caller": self.caller,
            "callee": self.callee,
            "caller_file": self.caller_file,
            "callee_file": self.callee_file,
            "type": self.type.value,
        }
        if self.type == CallType.INDIRECT:
            d["indirect_kind"] = self.indirect_kind
        if self.caller_line:
            d["caller_line"] = self.caller_line
        if self.confidence != 'medium':
            d["confidence"] = self.confidence
        if self.evidence:
            d["evidence"] = self.evidence
        return d


@dataclass
class CallGraph:
    """A project-level call graph containing functions and edges."""
    functions: dict[str, Function] = field(default_factory=dict)  # key -> Function
    edges: list[CallEdge] = field(default_factory=list)
    source_files: list[str] = field(default_factory=list)

    def add_function(self, func: Function) -> None:
        self.functions[func.key] = func

    def add_edge(self, edge: CallEdge) -> None:
        self.edges.append(edge)

    def query_callers(self, func_name: str) -> list[CallEdge]:
        return [e for e in self.edges if e.callee == func_name]

    def query_callees(self, func_name: str) -> list[CallEdge]:
        return [e for e in self.edges if e.caller == func_name]

    @classmethod
    def from_dict(cls, d: dict) -> CallGraph:
        """Deserialize a CallGraph from the dict produced by to_dict().

        Note: The 'summary' key from to_dict() is intentionally not deserialized
        as it is a computed field, not stored state.
        """
        graph = cls()
        for fd in d.get("functions", []):
            func = Function(
                name=fd["name"],
                file=fd["file"],
                line=fd["line"],
                signature=fd.get("signature", ""),
                is_definition=fd.get("is_definition", False),
                return_type=fd.get("return_type", ""),
                parameters=fd.get("parameters", []),
            )
            graph.add_function(func)
        for ed in d.get("edges", []):
            type_str = ed.get("type", CallType.DIRECT.value)
            try:
                edge_type = CallType(type_str)
            except ValueError:
                raise ValueError(f"Unknown CallType: {type_str!r}")
            edge = CallEdge(
                caller=ed["caller"],
                callee=ed["callee"],
                caller_file=ed.get("caller_file", ""),
                callee_file=ed.get("callee_file", ""),
                type=edge_type,
                indirect_kind=ed.get("indirect_kind", ""),
                caller_line=ed.get("caller_line", 0),
                confidence=ed.get("confidence", "medium"),
                evidence=ed.get("evidence", ""),
            )
            graph.add_edge(edge)
        graph.source_files = d.get("source_files", [])
        return graph

    def to_dict(self) -> dict:
        return {
            "functions": [
                {
                    "name": f.name,
                    "file": f.file,
                    "line": f.line,
                    "signature": f.signature,
                    "is_definition": f.is_definition,
                    "return_type": f.return_type,
                    "parameters": f.parameters,
                }
                for f in self.functions.values()
            ],
            "edges": [e.to_dict() for e in self.edges],
            "source_files": self.source_files,
            "summary": {
                "function_count": len(self.functions),
                "edge_count": len(self.edges),
                "direct_count": sum(1 for e in self.edges if e.type == CallType.DIRECT),
                "indirect_count": sum(1 for e in self.edges if e.type == CallType.INDIRECT),
            },
        }
