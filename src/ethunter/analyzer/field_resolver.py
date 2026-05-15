"""Strategy-chain field resolver for struct field function pointer calls.

Replaces the 15-layer fallback stack in field_call._visit() with a chain of
ResolutionStrategy classes, each doing exact key lookups only.
"""

from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol

from ethunter.graph.model import Confidence, Evidence


@dataclass
class ResolutionContext:
    """Immutable context passed to each strategy."""
    field_path: str       # e.g., "ctx.ext.alpn_select_cb"
    base_var: str         # e.g., "ctx"
    caller_func: str | None = None  # enclosing function


class ResolutionStrategy(Protocol):
    """Protocol for field resolution strategies.

    Each strategy does exact key lookups only. No suffix scans,
    no iteration over all dataflow entries.
    """

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        """Resolve targets. Returns empty set if unresolvable."""
        ...


class TypeAwareStructLookup:
    """Query: struct_fields['gstruct:<type>.<field_tail>']"""

    def __init__(self, store, symbol_table):
        self._store = store
        self._symbol_table = symbol_table

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        struct_type = self._symbol_table.get_func_var_type(ctx.caller_func, ctx.base_var)
        if not struct_type:
            return set()
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{struct_type}.{field_tail}')


class ExactPathStructLookup:
    """Query: struct_fields['gstruct:<base_var>.<field_tail>']"""

    def __init__(self, store):
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{ctx.base_var}.{field_tail}')


class TypeAwareVtableLookup:
    """Query: vtable_entries['vtable:<type>.<field_name>']"""

    def __init__(self, store, symbol_table):
        self._store = store
        self._symbol_table = symbol_table

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        struct_type = self._symbol_table.get_func_var_type(ctx.caller_func, ctx.base_var)
        if not struct_type:
            return set()
        field_name = ctx.field_path.split('.')[-1]
        return self._store.resolve_vtable_entry(struct_type, field_name)


class GlobalArrayLookup:
    """Query: global_arrays['garray:<base_var>']"""

    def __init__(self, store):
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        return self._store.resolve_global_array(ctx.base_var)


class StructAliasLookup:
    """Resolve base_var via alias map, then query struct_fields."""

    def __init__(self, store):
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        alias = self._store.aliases.get(ctx.base_var)
        if not alias:
            return set()
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{alias}.{field_tail}')


class ParamAliasLookup:
    """Query: param_alias_map[(caller_func, base_var)] -> field_path -> struct_fields"""

    def __init__(self, dataflow):
        self._dataflow = dataflow

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        if not ctx.caller_func:
            return set()
        alias_key = (ctx.caller_func, ctx.base_var)
        if alias_key not in self._dataflow.param_alias_map:
            return set()
        global_name = self._dataflow.param_alias_map[alias_key]
        field_tail = self._dataflow.store.compute_field_tail(ctx.field_path)
        return self._dataflow.store.resolve_struct_field(f'gstruct:{global_name}.{field_tail}')


class LocalFpLookup:
    """Query: local_fp_mapping[base_var] -> targets"""

    def __init__(self, local_fp_mapping):
        self._mapping = local_fp_mapping

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        return self._mapping.get(ctx.base_var, set()).copy()


class PointerAliasLookup:
    """Query: pointer_resolutions[base_var] -> resolved_base -> struct_fields"""

    def __init__(self, pointer_resolutions, store):
        self._resolutions = pointer_resolutions
        self._store = store

    def resolve(self, ctx: ResolutionContext) -> set[str]:
        if ctx.base_var not in self._resolutions:
            return set()
        resolved_base = self._resolutions[ctx.base_var]
        field_tail = self._store.compute_field_tail(ctx.field_path)
        return self._store.resolve_struct_field(f'gstruct:{resolved_base}.{field_tail}')


class FieldResolver:
    """Chain of resolution strategies for struct field function pointer calls."""

    def __init__(self, store, dataflow, symbol_table,
                 local_fp_mapping, pointer_resolutions):
        self._store = store
        self._symbol_table = symbol_table
        self._strategies = [
            TypeAwareStructLookup(store, symbol_table),
            ExactPathStructLookup(store),
            TypeAwareVtableLookup(store, symbol_table),
            GlobalArrayLookup(store),
            StructAliasLookup(store),
            ParamAliasLookup(dataflow),
            LocalFpLookup(local_fp_mapping),
            PointerAliasLookup(pointer_resolutions, store),
        ]

    def resolve(self, field_path: str, base_var: str,
                caller_func: str | None = None) -> set[str]:
        ctx = ResolutionContext(
            field_path=field_path,
            base_var=base_var,
            caller_func=caller_func,
        )
        for strategy in self._strategies:
            targets = strategy.resolve(ctx)
            if targets:
                return targets
        return set()

    def resolve_field_call(self, field_path: str, base_var: str,
                           caller_func: str | None, filepath: str
                           ) -> tuple[set[str], Confidence | None, Evidence | None]:
        """Resolve a struct field function pointer call via 4-tier chain.

        Returns (targets, Confidence|None, Evidence|None).
        None confidence means no match — caller should use legacy fallback.
        """
        field_tail = self._store.compute_field_tail(field_path)
        targets = set()

        # === Tier 1: Type-aware exact match ===
        struct_type = None
        if caller_func:
            struct_type = self._symbol_table.get_func_var_type(caller_func, base_var)
        if not struct_type:
            struct_type = self._symbol_table.get_var_type(base_var)
        if struct_type:
            targets = self._store.resolve_struct_field(f'gstruct:{struct_type}.{field_tail}')
            if targets:
                return targets, Confidence.HIGH, Evidence('type_aware', tier=1)

        # === Tier 2: Exact path match ===
        targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

        # === Chain decomposition ===
        # Handle s.method.put_cb where s.method resolves to a concrete struct
        parts = field_path.split('.')
        if len(parts) >= 3:
            for cut in range(2, len(parts)):
                prefix = '.'.join(parts[:cut])
                suffix = '.'.join(parts[cut:])

                resolved_vars = self._store.resolve_struct_field(f'gstruct:{prefix}')
                if not resolved_vars:
                    continue

                for var_name in resolved_vars:
                    var_type = self._symbol_table.get_var_type(var_name)
                    if var_type:
                        targets = self._store.resolve_struct_field(
                            f'gstruct:{var_type}.{suffix}')
                        if targets:
                            return targets, Confidence.HIGH, Evidence('chain_resolve', tier=1)
                    targets = self._store.resolve_struct_field(
                        f'gstruct:{var_name}.{suffix}')
                    if targets:
                        return targets, Confidence.HIGH, Evidence('chain_resolve_exact', tier=2)

        # === Type gate: known type + Tier 1 miss + no chain success → skip suffix ===
        if struct_type:
            return set(), None, None

        # === Tier 3: Same-file scoped suffix ===
        suffix = f'.{field_tail}'
        for key, vals in self._store.struct_fields.items():
            if not key.endswith(suffix):
                continue
            files = self._store.struct_field_files.get(key)
            if files and filepath not in files:
                continue
            targets.update(vals)
        if targets:
            return targets, Confidence.MEDIUM, Evidence('same_file_suffix', tier=3)

        # === Tier 4: Cross-file suffix ===
        for key, vals in self._store.struct_fields.items():
            if key.endswith(suffix):
                targets.update(vals)
        if targets:
            return targets, Confidence.LOW, Evidence('cross_file_suffix', tier=4)

        return set(), None, None

    def resolve_with_evidence(self, field_path, base_var, caller_func=None):
        """Resolve targets and return (targets, strategy_name)."""
        ctx = ResolutionContext(field_path=field_path, base_var=base_var,
                                caller_func=caller_func)
        for strategy in self._strategies:
            targets = strategy.resolve(ctx)
            if targets:
                return targets, type(strategy).__name__
        return set(), 'none'
