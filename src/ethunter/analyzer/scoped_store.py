"""Function-scoped dataflow storage.

Function-scoped dataflow storage with four separate stores:
  - func_vars: function-scoped variable -> targets
  - struct_fields: struct field -> targets (global, inherently cross-function)
  - global_arrays: global array -> targets
  - vtable_entries: vtable field -> targets (reserved for future vtable support)
"""

from __future__ import annotations
from dataclasses import dataclass, field


@dataclass
class ScopedStore:
    """Function-scoped variable -> targets mapping.

    Keys are ALWAYS (func_name, var_name) tuples for func_vars.
    Cross-function information flows through explicit bridges
    (call_site_targets, param_fields, ret_fields), not through
    shared variable names.
    """
    # (func_name, var_name) -> {target_functions}
    func_vars: dict[tuple[str, str], set[str]] = field(default_factory=dict)

    # Struct field targets: "gstruct:<path>" -> {target_functions}
    # Path is either "<base_var>.<field_tail>" or "<struct_type>.<field_tail>"
    # where field_tail is the field path WITHOUT the base variable name
    struct_fields: dict[str, set[str]] = field(default_factory=dict)

    # Global array targets: "garray:<var_name>" -> {target_functions}
    global_arrays: dict[str, set[str]] = field(default_factory=dict)

    # Vtable entries: "vtable:<struct_type>.<field_name>" -> {target_functions}
    vtable_entries: dict[str, set[str]] = field(default_factory=dict)

    # Struct variable aliases: var_name -> struct_type_or_resolved_name
    # Populated by initializer_assign during global struct initialization
    aliases: dict[str, str] = field(default_factory=dict)

    # Per-file index: key -> set of source filepaths
    # Used by Tier 3 for same-file scoped suffix matching
    struct_field_files: dict[str, set[str]] = field(default_factory=dict)

    # --- func_vars helpers ---

    def assign_func_var(self, func: str, var: str, target: str) -> None:
        """Assign a target to a function-scoped variable."""
        key = (func, var)
        if key not in self.func_vars:
            self.func_vars[key] = set()
        self.func_vars[key].add(target)

    def resolve_func_var(self, func: str, var: str) -> set[str]:
        """Resolve targets for a function-scoped variable."""
        return self.func_vars.get((func, var), set()).copy()

    # --- struct_fields helpers ---

    def assign_struct_field(self, key: str, target: str, filepath: str = '') -> None:
        """Assign a target to a struct field. Key format: 'gstruct:<path>'."""
        if key not in self.struct_fields:
            self.struct_fields[key] = set()
        self.struct_fields[key].add(target)
        if filepath:
            if key not in self.struct_field_files:
                self.struct_field_files[key] = set()
            self.struct_field_files[key].add(filepath)

    def resolve_struct_field(self, key: str) -> set[str]:
        """Resolve targets for a struct field key."""
        return self.struct_fields.get(key, set()).copy()

    # --- global_arrays helpers ---

    def assign_global_array(self, name: str, target: str) -> None:
        """Assign a target to a global array. Key format: 'garray:<name>'."""
        key = f'garray:{name}'
        if key not in self.global_arrays:
            self.global_arrays[key] = set()
        self.global_arrays[key].add(target)

    def resolve_global_array(self, name: str) -> set[str]:
        """Resolve targets for a global array name."""
        return self.global_arrays.get(f'garray:{name}', set()).copy()

    # --- vtable_entries helpers ---

    def assign_vtable_entry(self, struct_type: str, field: str, target: str) -> None:
        """Assign a target to a vtable entry."""
        key = f'vtable:{struct_type}.{field}'
        if key not in self.vtable_entries:
            self.vtable_entries[key] = set()
        self.vtable_entries[key].add(target)

    def resolve_vtable_entry(self, struct_type: str, field: str) -> set[str]:
        """Resolve targets for a vtable field."""
        return self.vtable_entries.get(f'vtable:{struct_type}.{field}', set()).copy()

    # --- utility ---

    def compute_field_tail(self, field_path: str) -> str:
        """Extract field_tail from a full field path.

        'handler.cb' -> 'cb'
        'ctx.ext.alpn_select_cb' -> 'ext.alpn_select_cb'
        """
        if '.' in field_path:
            return field_path.split('.', 1)[1]
        return field_path
