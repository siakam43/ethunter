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


@dataclass
class DataflowEngine:
    """Cross-function dataflow engine for function pointer tracking.

    Wraps VariableState (backward compatible) and adds:
    - ParamTracker: parameter-to-field propagation across function calls
    - RetTracker: return value tracking for struct field function pointers
    - CastResolver: nested cast expression unwrapping
    """
    state: VariableState = field(default_factory=VariableState)

    # Parameter propagation: (func_name, param_position) -> {field_path}
    param_fields: dict[tuple[str, int], set[str]] = field(default_factory=dict)

    # Return value tracking: func_name -> set of field paths returned
    ret_fields: dict[str, set[str]] = field(default_factory=dict)

    # Alias tracking: reserved for future use
    aliases: dict[str, str] = field(default_factory=dict)

    # === Backward compatible interface ===

    def assign(self, var_name: str, target: str) -> None:
        self.state.assign(var_name, target)

    def resolve(self, var_name: str) -> set[str]:
        return self.state.resolve(var_name)

    def merge(self, src_var: str, dst_var: str) -> None:
        self.state.merge(src_var, dst_var)

    @property
    def targets(self) -> dict[str, set[str]]:
        return self.state.targets

    def register_callback(self, func_name: str) -> None:
        self.state.register_callback(func_name)

    @property
    def registered_callbacks(self) -> set[str]:
        return self.state.registered_callbacks

    # === New: ParamTracker ===

    def register_param_mapping(
        self,
        func_name: str,
        param_idx: int,
        field_path: str,
        struct_param_idx: int = 0,
    ) -> None:
        """Register that param_idx of func_name stores into a struct field.

        Example: SSL_CTX_set_alpn_select_cb(ctx, cb) stores cb into ctx->ext.alpn_select_cb
        -> register_param_mapping("SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb")
        """
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
    ) -> set[str]:
        """Resolve what targets the call-site argument has, and propagate to field paths.

        Returns the set of function names that arg_name resolves to.
        Also writes those targets into the registered field paths in dataflow.
        """
        key = (func_name, param_idx)
        if key not in self.param_fields:
            return set()

        # Step 1: Try dataflow resolve (for variables that were assigned)
        arg_targets = self.state.resolve(arg_name)

        # Step 2: If arg_name itself is a known function name, add it directly
        if symbol_names and arg_name in symbol_names:
            arg_targets.add(arg_name)

        if not arg_targets:
            return set()

        for target in arg_targets:
            for field_key in self.param_fields[key]:
                self.state.assign(field_key, target)

        return arg_targets

    # === New: RetTracker ===

    def register_return(self, func_name: str, field_path: str) -> None:
        """Register that a function returns a struct field function pointer.

        Example: SSL_CTX_get_security_callback returns ctx->cert->sec_cb
        -> register_return("SSL_CTX_get_security_callback", "cert->sec_cb")
        """
        if func_name not in self.ret_fields:
            self.ret_fields[func_name] = set()
        self.ret_fields[func_name].add(field_path)

    def resolve_returned_field(self, func_name: str) -> set[str]:
        """Resolve the targets of the field path that func_name returns."""
        if func_name not in self.ret_fields:
            return set()

        results = set()
        for field_path in self.ret_fields[func_name]:
            targets = self.state.resolve(f"<gstruct:{field_path}>")
            results.update(targets)
        return results

    # === New: CastResolver ===

    def unwrap_cast(self, node) -> str | None:
        """Recursively unwrap nested cast expressions.

        (T1)(T2)func  ->  "func"
        (T1)(uintptr_t)cb  ->  "cb"

        Uses child_by_field_name('value') for cast_expression (robust across tree-sitter versions).
        Returns None if the node is not a cast/pointer/paren expression.
        """
        if node.type == 'identifier' and node.text:
            return node.text.decode('utf-8')

        if node.type == 'cast_expression':
            # Prefer child_by_field_name for robustness, fallback to iteration
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
