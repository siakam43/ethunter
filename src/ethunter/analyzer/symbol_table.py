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
            # Check for function pointer declarations (not function declarations)
            decl = node
            declarator = _find_child_by_type(decl, 'function_declarator')
            if declarator:
                ident = _find_child_by_type(declarator, 'identifier')
                if ident and ident.text:
                    params_node = _find_child_by_type(declarator, 'parameter_list')
                    params = _extract_params(params_node) if params_node else []
                    ret_type = _get_decl_type(decl)
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
    for child in node.children:
        if child.type == type_name:
            return child
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
