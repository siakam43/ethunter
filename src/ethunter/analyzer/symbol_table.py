"""Symbol table: extract all function declarations/definitions from parsed ASTs."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.graph.model import Function


def extract_functions(tree: ts.Tree, filepath: str) -> list[Function]:
    """Extract all function definitions and declarations from a tree-sitter AST."""
    functions: list[Function] = []
    root = tree.root_node

    def _visit(node: ts.Node) -> None:
        if node.type == 'function_definition':
            func = _parse_function_definition(node, filepath)
            if func:
                functions.append(func)
        elif node.type == 'declaration':
            # Only match real function declarations (not function pointer declarations)
            # Function pointer decls have parenthesized_declarator/pointer_declarator nesting
            declarator = _find_child_by_type(node, 'function_declarator')
            if declarator:
                # Skip if the function_declarator wraps a parenthesized_declarator
                # (that means it's a function pointer, not a real function decl)
                inner_wrappers = [c for c in declarator.children
                                  if c.type in ('parenthesized_declarator', 'pointer_declarator')]
                if not inner_wrappers:
                    ident = _find_child_by_type(declarator, 'identifier')
                    if ident and ident.text:
                        params_node = _find_child_by_type(declarator, 'parameter_list')
                        params = _extract_params(params_node) if params_node else []
                        ret_type = _get_decl_type(node)
                        functions.append(Function(
                            name=ident.text.decode('utf-8'),
                            file=filepath,
                            line=node.start_point[0] + 1,
                            return_type=ret_type,
                            parameters=params,
                            is_definition=False,
                        ))
        for child in node.children:
            _visit(child)

    _visit(root)
    return functions


def _parse_function_definition(node: ts.Node, filepath: str) -> Function | None:
    """Parse a function_definition node into a Function."""
    declarator = _find_child_by_type(node, 'function_declarator')
    if not declarator:
        return None

    ident = _find_child_by_type(declarator, 'identifier')
    if not ident:
        return None

    params_node = _find_child_by_type(declarator, 'parameter_list')
    params = _extract_params(params_node) if params_node else []
    ret_type = _get_decl_type(node)

    return Function(
        name=ident.text.decode('utf-8'),
        file=filepath,
        line=node.start_point[0] + 1,
        return_type=ret_type,
        parameters=params,
        is_definition=True,
    )


def _find_child_by_type(node: ts.Node, type_name: str) -> ts.Node | None:
    """Find a direct child of the given type, or recurse into pointer/parenthesized declarators."""
    for child in node.children:
        if child.type == type_name:
            return child
    # For pointer return types like `void *zmalloc(size_t)`,
    # the function_declarator is nested inside pointer_declarator
    for child in node.children:
        if child.type in ('pointer_declarator', 'parenthesized_declarator'):
            result = _find_child_by_type(child, type_name)
            if result:
                return result
    return None


def _extract_params(params_node: ts.Node) -> list[str]:
    params: list[str] = []
    for child in params_node.children:
        if child.type == 'parameter_declaration':
            params.append(child.text.decode('utf-8'))
    return params


def _get_decl_type(node: ts.Node) -> str:
    type_node = _find_child_by_type(node, 'primitive_type')
    if type_node:
        return type_node.text.decode('utf-8')
    specifier = _find_child_by_type(node, 'type_specifier')
    if specifier:
        return specifier.text.decode('utf-8')
    return ''


class SymbolTable:
    """Project-wide symbol table mapping function names to their declarations."""

    def __init__(self):
        self._functions: dict[str, list[Function]] = {}
        self._typedefs: dict[str, str] = {}
        self._structs: dict[str, list[tuple[str, str]]] = {}
        self._var_types: dict[str, str] = {}
        self._struct_fields: dict[str, list[str]] = {}

    def record_var_type(self, var_name: str, struct_type: str) -> None:
        """Record that a variable is declared as a struct type."""
        self._var_types[var_name] = struct_type

    def get_var_type(self, var_name: str) -> str | None:
        """Get the struct type of a variable, or None if unknown."""
        return self._var_types.get(var_name)

    def record_struct_fields(self, struct_type: str, fields: list[str]) -> None:
        """Record the field names for a struct type."""
        if struct_type not in self._struct_fields:
            self._struct_fields[struct_type] = list(fields)

    def get_struct_fields(self, struct_type: str) -> list[str]:
        """Get field names for a struct type."""
        return self._struct_fields.get(struct_type, [])

    def add_function(self, func: Function) -> None:
        if func.name not in self._functions:
            self._functions[func.name] = []
        self._functions[func.name].append(func)

    def add_typedef(self, name: str, target: str) -> None:
        self._typedefs[name] = target

    def add_struct(self, name: str, members: list[tuple[str, str]]) -> None:
        self._structs[name] = members

    @property
    def all_function_names(self) -> set[str]:
        return set(self._functions.keys())

    def lookup(self, name: str) -> list[Function]:
        return self._functions.get(name, [])

    def resolve_typedef(self, name: str) -> str | None:
        seen: set[str] = set()
        current = name
        while current in self._typedefs:
            if current in seen:
                return None  # circular typedef
            seen.add(current)
            current = self._typedefs[current]
        return current
