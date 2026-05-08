"""Serialize CallGraph to Graphviz DOT format."""

from ethunter.graph.model import CallEdge, CallGraph


def to_dot(graph: CallGraph) -> str:
    lines = ['digraph CallGraph {', '  rankdir=LR;', '  node [shape=box];', '']

    # Add nodes
    for func in graph.functions.values():
        label = f'{func.name}\\n({func.file})'
        safe_name = _safe_id(func.key)
        lines.append(f'  {safe_name} [label="{label}"];')

    lines.append('')

    # Add edges
    for edge in graph.edges:
        caller_func = _find_func_by_name(graph, edge.caller, edge.caller_file)
        callee_func = _find_func_by_name(graph, edge.callee, edge.callee_file)
        caller_id = _safe_id(caller_func.key if caller_func else f'<unknown>:{edge.caller}:0')
        callee_id = _safe_id(callee_func.key if callee_func else f'<unknown>:{edge.callee}:0')
        style = 'style=dashed' if edge.type.value == 'indirect' else ''
        lines.append(f'  {caller_id} -> {callee_id} [{style}];')

    lines.append('}')
    return '\n'.join(lines)


def _safe_id(s: str) -> str:
    return s.replace('/', '_').replace(':', '_').replace('.', '_').replace('-', '_')


def _find_func_by_name(graph: CallGraph, name: str, filepath: str):
    if name == '<unknown>':
        return None
    for func in graph.functions.values():
        if func.name == name and (not filepath or func.file == filepath):
            return func
    # Fallback: match by name only
    for func in graph.functions.values():
        if func.name == name:
            return func
    return None
