"""Cross-function dataflow engine for function pointer tracking."""

from __future__ import annotations

from dataclasses import dataclass, field
from ethunter.analyzer.scoped_store import ScopedStore


class _NilSymbolTableType:
    """Null object for symbol_table when not available."""
    def get_func_var_type(self, *args, **kwargs): return None
    def get_var_type(self, *args, **kwargs): return None
    def get_struct_fields(self, *args, **kwargs): return []
    def record_struct_fields(self, *args, **kwargs): pass
    def record_var_type(self, *args, **kwargs): pass
    def record_func_var_type(self, *args, **kwargs): pass


_NilSymbolTable = _NilSymbolTableType()


@dataclass
class DataflowEngine:
    """Cross-function dataflow engine for function pointer tracking.

    Stores:
    - ScopedStore: function-scoped variable → targets
    - func_params: function → parameter name list
    - func_fp_params: function → set of fnptr parameter positions
    - param_usage: (function, position) → usage role (caller/forwarder/storage/unknown)
    - param_fields: (function, position) → field paths (param→struct field bridging)
    - ret_fields: function → field paths (return value bridging)
    """
    # Scoped dataflow store
    store: ScopedStore = field(default_factory=ScopedStore)

    # Parameter propagation: (func_name, param_position) -> {field_path}
    param_fields: dict[tuple[str, int], set[str]] = field(default_factory=dict)

    # Return value tracking: func_name -> set of field paths returned
    ret_fields: dict[str, set[str]] = field(default_factory=dict)

    # Alias tracking: reserved for future use
    aliases: dict[str, str] = field(default_factory=dict)

    # Parameter alias map: (enclosing_func, local_var) -> global_struct_name
    param_alias_map: dict[tuple[str, str], str] = field(default_factory=dict)

    # Cross-file function metadata (populated by param_helpers.prepare)
    func_params: dict[str, list[str]] = field(default_factory=dict)

    # Function pointer parameter positions: func_name -> {fnptr_param_positions}
    func_fp_params: dict[str, set[int]] = field(default_factory=dict)

    # Parameter usage classification: (func_name, position) -> role
    param_usage: dict[tuple[str, int], str] = field(default_factory=dict)

    # Phase 3 registration tracking
    registration_sites: list = field(default_factory=list)
    covered_callees: set[str] = field(default_factory=set)

    # Per-call-site targets (Phase 1 → Phase 2 handoff)
    call_site_targets: dict = field(default_factory=dict)

    # Param binding storage: (call_name, param_name) -> {targets}
    _param_bindings: dict[tuple[str, str], set[str]] = field(default_factory=dict)

    def add_param_binding(self, call_name: str, param_name: str, target: str) -> None:
        """Register a call-site param binding."""
        key = (call_name, param_name)
        if key not in self._param_bindings:
            self._param_bindings[key] = set()
        self._param_bindings[key].add(target)

    def assign(self, var_name: str, target: str) -> None:
        """Assign a target to a key, routing to ScopedStore for structured prefixes."""
        if var_name.startswith('<gstruct:'):
            key = var_name[len('<gstruct:'):-1]
            self.store.assign_struct_field(f'gstruct:{key}', target)
        elif var_name.startswith('<struct:'):
            key = var_name[len('<struct:'):-1]
            self.store.assign_struct_field(f'gstruct:{key}', target)
        elif var_name.startswith('<garray:'):
            name = var_name[len('<garray:'):-1]
            self.store.assign_global_array(name, target)
        elif var_name.startswith('<var>:'):
            parts = var_name[len('<var>:'):].split(':', 1)
            if len(parts) == 2:
                self.store.assign_func_var(parts[0], parts[1], target)
        else:
            self.store.assign_func_var('<global>', var_name, target)

    def resolve(self, var_name: str) -> set[str]:
        """Resolve a key, checking ScopedStore first for structured prefixes."""
        if var_name.startswith('<gstruct:'):
            key = var_name[len('<gstruct:'):-1]
            return self.store.resolve_struct_field(f'gstruct:{key}')
        if var_name.startswith('<struct:'):
            key = var_name[len('<struct:'):-1]
            return self.store.resolve_struct_field(f'gstruct:{key}')
        if var_name.startswith('<garray:'):
            name = var_name[len('<garray:'):-1]
            return self.store.resolve_global_array(name)
        if var_name.startswith('<var>:'):
            parts = var_name[len('<var>:'):].split(':', 1)
            if len(parts) == 2:
                return self.resolve_variable(parts[1], parts[0])
            return self.resolve_variable(var_name)
        if var_name == '<initializer>':
            return set()
        if ':' in var_name and not var_name.startswith('<'):
            func, param = var_name.split(':', 1)
            results = self.resolve_variable(param, func)
            for (call, pname), vals in self._param_bindings.items():
                if call == func and pname == param:
                    results.update(vals)
            return results
        results: set[str] = set()
        for (func, var), vals in self.store.func_vars.items():
            if var == var_name:
                results.update(vals)
        return results

    def merge(self, src_var: str, dst_var: str) -> None:
        """Merge src_var targets into dst_var using func_vars."""
        to_merge = [(func, dst_var, v) for (func, var), vals
                    in list(self.store.func_vars.items())
                    if var == src_var for v in vals]
        for func, dvar, v in to_merge:
            self.store.assign_func_var(func, dvar, v)

    def resolve_variable(self, var_name: str, caller_func: str | None = None,
                         local_fp_mapping: dict | None = None) -> set[str]:
        """Resolve a variable name to function targets.

        Priority: func-scoped > global > any-scope > local_fp_mapping.
        """
        if caller_func:
            targets = self.store.resolve_func_var(caller_func, var_name)
            if targets:
                return targets
            targets = self.store.resolve_func_var('<global>', var_name)
            if targets:
                return targets
        targets = self.store.resolve_func_var('<global>', var_name)
        if targets:
            return targets
        for (func, var), vals in self.store.func_vars.items():
            if var == var_name and vals:
                return vals
        if local_fp_mapping:
            targets = local_fp_mapping.get(var_name, set())
            if targets:
                return targets
        return set()

    def resolve_global_array(self, name: str) -> set[str]:
        """Resolve a global function pointer array name to targets."""
        return self.store.resolve_global_array(name)

    def resolve_struct_field_call(self, field_path: str, base_var: str,
                                  caller_func: str | None, filepath: str,
                                  symbol_table, local_fp_mapping: dict | None = None,
                                  pointer_resolutions: dict | None = None) \
            -> tuple[set[str], 'Confidence | None', 'Evidence | None']:
        """Resolve a struct field function pointer call.

        Uses FieldResolver 4-tier chain + garray fallback.
        """
        from ethunter.analyzer.field_resolver import FieldResolver
        from ethunter.graph.model import Confidence, Evidence

        if symbol_table is None:
            symbol_table = _NilSymbolTable

        resolver = FieldResolver(
            store=self.store,
            dataflow=self,
            symbol_table=symbol_table,
            local_fp_mapping=local_fp_mapping or {},
            pointer_resolutions=pointer_resolutions or {},
        )
        targets, confidence, evidence = resolver.resolve_field_call(
            field_path, base_var, caller_func, filepath)
        garray_targets = self.store.resolve_global_array(base_var)
        if garray_targets:
            targets.update(garray_targets)
            if confidence is None:
                confidence, evidence = Confidence.LOW, Evidence('garray_fallback')
        return targets, confidence, evidence

    def rebuild_param_mappings(self) -> dict[str, set[str]]:
        """Rebuild param_name -> {targets} mapping from _param_bindings."""
        result: dict[str, set[str]] = {}
        for (call_name, param_name), vals in self._param_bindings.items():
            result.setdefault(param_name, set()).update(vals)
        return result

    @property
    def targets(self) -> dict[str, set[str]]:
        """Aggregate targets from all stores for backward compat."""
        result: dict[str, set[str]] = {}
        for (func, var), vals in self.store.func_vars.items():
            key = f'<var>:{func}:{var}'
            result.setdefault(key, set()).update(vals)
            result.setdefault(var, set()).update(vals)
        for key, vals in self.store.struct_fields.items():
            result.setdefault(f'<{key}>', set()).update(vals)
        for key, vals in self.store.global_arrays.items():
            result.setdefault(f'<{key}>', set()).update(vals)
        return result

    def register_callback(self, func_name: str) -> None:
        """No-op: registered_callbacks was dead code."""

    def register_param_mapping(
        self,
        func_name: str,
        param_idx: int,
        field_path: str,
        struct_param_idx: int = 0,
    ) -> None:
        """Register that param_idx of func_name stores into a struct field."""
        key = (func_name, param_idx)
        if key not in self.param_fields:
            self.param_fields[key] = set()
        self.param_fields[key].add(f"<gstruct:{field_path}>")

    def resolve_call_site_param(
        self,
        func_name: str,
        param_idx: int,
        arg_name: str,
        symbol_names: set[str] | None = None,
        filepath: str = '',
    ) -> set[str]:
        """Resolve what targets the call-site argument has, and propagate to field paths."""
        key = (func_name, param_idx)
        if key not in self.param_fields:
            return set()

        arg_targets = self.resolve_variable(arg_name)
        if symbol_names and arg_name in symbol_names:
            arg_targets.add(arg_name)

        if not arg_targets:
            return set()

        for target in arg_targets:
            for field_key in self.param_fields[key]:
                if field_key.startswith('<gstruct:') and field_key.endswith('>'):
                    self.store.assign_struct_field(field_key[1:-1], target, filepath)

        return arg_targets

    def register_return(self, func_name: str, field_path: str) -> None:
        """Register that a function returns a struct field function pointer."""
        if func_name not in self.ret_fields:
            self.ret_fields[func_name] = set()
        self.ret_fields[func_name].add(field_path)

    def resolve_returned_field(self, func_name: str) -> set[str]:
        """Resolve the targets of the field path that func_name returns."""
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            results.update(self.store.resolve_struct_field(f'gstruct:{field_path}'))

            parts = field_path.split('.')
            for i in range(1, len(parts)):
                suffix = '.'.join(parts[i:])
                before = len(results)
                for key, vals in self.store.struct_fields.items():
                    if key.endswith(f'.{suffix}') and vals:
                        results.update(vals)
                if len(results) > before:
                    break

        return results

    def unwrap_cast(self, node) -> str | None:
        """Recursively unwrap nested cast expressions.

        (T1)(T2)func  ->  "func"
        """
        if node.type == 'identifier' and node.text:
            return node.text.decode('utf-8')

        if node.type == 'cast_expression':
            value = node.child_by_field_name('value')
            if value:
                return self.unwrap_cast(value)
            for child in reversed(node.children):
                result = self.unwrap_cast(child)
                if result:
                    return result
            return None

        if node.type == 'pointer_expression':
            operand = node.child_by_field_name('argument')
            if operand:
                return self.unwrap_cast(operand)

        if node.type == 'parenthesized_expression':
            inner = node.child_by_field_name('expression')
            if inner is None and len(node.children) >= 2:
                inner = node.children[1]
            if inner:
                return self.unwrap_cast(inner)

        return None
