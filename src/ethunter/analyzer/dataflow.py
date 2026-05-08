"""Shared variable state tracker for function pointer data flow."""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class VariableState:
    """Tracks possible function targets for each variable across the codebase."""
    # Maps variable name -> set of possible function target names
    targets: dict[str, set[str]] = field(default_factory=dict)
    # Maps variable name -> type info (e.g., 'fp', 'fp[]', 'struct.member')
    var_types: dict[str, str] = field(default_factory=dict)
    # Callback registry: registered function names
    registered_callbacks: set[str] = field(default_factory=set)

    def assign(self, var_name: str, target: str) -> None:
        if var_name not in self.targets:
            self.targets[var_name] = set()
        self.targets[var_name].add(target)

    def merge(self, src_var: str, dst_var: str) -> None:
        if src_var in self.targets:
            if dst_var not in self.targets:
                self.targets[dst_var] = set()
            self.targets[dst_var].update(self.targets[src_var])

    def resolve(self, var_name: str) -> set[str]:
        return self.targets.get(var_name, set()).copy()

    def register_callback(self, func_name: str) -> None:
        self.registered_callbacks.add(func_name)
