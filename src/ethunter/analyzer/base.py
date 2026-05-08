"""Base analyzer interface."""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any

import tree_sitter as ts

from ethunter.graph.model import CallEdge
from ethunter.analyzer.dataflow import VariableState
from ethunter.analyzer.symbol_table import SymbolTable


class BaseAnalyzer(ABC):
    """Base class for all call graph analyzers."""

    def __init__(self, name: str, indirect_kind: str = ''):
        self.name = name
        self.indirect_kind = indirect_kind

    @abstractmethod
    def analyze(
        self,
        tree: ts.Tree,
        filepath: str,
        symbol_table: SymbolTable,
        dataflow: VariableState,
    ) -> list[CallEdge]:
        ...
