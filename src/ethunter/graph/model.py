"""Core data model for the call graph."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum


class CallType(Enum):
    DIRECT = "direct"
    INDIRECT = "indirect"


class Confidence(Enum):
    """Edge confidence level. Ordinal used for dedup — higher wins."""
    HIGH = 'high'
    MEDIUM = 'medium'
    LOW = 'low'

    def ordinal(self) -> int:
        return _CONFIDENCE_RANK[self]


_CONFIDENCE_RANK = {
    Confidence.HIGH: 3,
    Confidence.MEDIUM: 2,
    Confidence.LOW: 1,
}


@dataclass(frozen=True)
class Evidence:
    """Structured evidence for how an edge was discovered."""
    method: str
    tier: int | None = None
    source: str | None = None

    def __str__(self) -> str:
        parts = [self.method]
        if self.tier is not None:
            parts.append(f'tier={self.tier}')
        if self.source:
            parts.append(self.source)
        return ':'.join(parts)


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
    confidence: Confidence = Confidence.MEDIUM
    evidence: Evidence | None = None

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
        d["confidence"] = self.confidence.value
        if self.evidence:
            d["evidence"] = str(self.evidence)
        return d

    @classmethod
    def from_dict(cls, d: dict) -> 'CallEdge':
        conf_value = d.get('confidence', 'medium')
        evidence_str = d.get('evidence', '')
        return cls(
            caller=d['caller'],
            callee=d['callee'],
            caller_file=d.get('caller_file', ''),
            callee_file=d.get('callee_file', ''),
            type=CallType(d['type']),
            indirect_kind=d.get('indirect_kind', ''),
            caller_line=d.get('caller_line', 0),
            confidence=Confidence(conf_value) if conf_value in ('high', 'medium', 'low') else Confidence.MEDIUM,
            evidence=_parse_evidence(evidence_str) if evidence_str else None,
        )


def _parse_evidence(s: str) -> Evidence | None:
    """Parse evidence string in format: method[:tier=N][:source]."""
    if not s:
        return None
    parts = s.split(':')
    method = parts[0]
    tier = None
    source = None
    for p in parts[1:]:
        if p.startswith('tier='):
            tier = int(p.split('=')[1])
        else:
            source = p
    return Evidence(method=method, tier=tier, source=source)


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
            edge = CallEdge.from_dict(ed)
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
