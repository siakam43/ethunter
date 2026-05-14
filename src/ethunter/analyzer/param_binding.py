"""Phase 1: Parameter binding — writes dataflow + registration_sites, no edges."""

from __future__ import annotations

import tree_sitter as ts

from ethunter.analyzer.helpers import find_enclosing_function, extract_field_path, collect_field_assignments
from ethunter.analyzer.param_helpers import (
    _is_registration,
    _find_child,
    _extract_field_operand,
    _collect_simple_macros,
)


def _propagate_call_site(
    call_name: str, arg_idx: int, target: str,
    dataflow, symbol_names: set[str],
) -> None:
    """Propagate a call-site argument target to registered field paths."""
    dataflow.resolve_call_site_param(
        call_name, arg_idx, target, symbol_names=symbol_names
    )


def analyze(
    tree: ts.Tree,
    filepath: str,
    symbol_table,
    dataflow,
) -> list:
    """Phase 1: Bind call-site arguments to function targets. Writes dataflow
    and registration_sites. Returns empty list (no edges).

    Reads: engine.func_params, engine.state.func_fp_params (from prepare),
           symbol_table.all_function_names
    Writes: dataflow.targets (param->target mappings), engine.registration_sites,
            engine.call_site_targets (per-call-site targets for param_dispatch)
    """
    func_params = dataflow.func_params
    func_fp_params = getattr(dataflow.state, 'func_fp_params', {})
    symbol_names = symbol_table.all_function_names
    macros = _collect_simple_macros(tree)

    param_mappings: dict[str, set[str]] = {}
    call_site_targets: dict[tuple[str, str, int], set[str]] = {}

    def _collect_call_params(node: ts.Node) -> None:
        if node.type == 'call_expression':
            func_node = node.child_by_field_name('function') or node.children[0]
            if func_node and func_node.text:
                call_name = func_node.text.decode('utf-8')
                args = node.child_by_field_name('arguments')
                if args:
                    caller = find_enclosing_function(node, tree.root_node)

                    if call_name not in func_params and call_name in macros:
                        real_func, _ = macros[call_name]
                        if real_func in func_params:
                            call_name = real_func

                    param_names = func_params.get(call_name, [])
                    comma_count = 0
                    for c in args.children:
                        if c.type == ',':
                            comma_count += 1
                        elif c.type == 'identifier' and c.text:
                            arg_idx = comma_count
                            target = c.text.decode('utf-8')
                            if target in symbol_names:
                                fp_params_positions = func_fp_params.get(call_name, None)
                                is_reg = False
                                if fp_params_positions is not None:
                                    if arg_idx in fp_params_positions:
                                        is_reg = True
                                else:
                                    if _is_registration(call_name):
                                        is_reg = True
                                if is_reg:
                                    dataflow.registration_sites.append({
                                        "caller": caller or '<unknown>',
                                        "callee": call_name,
                                        "arg_idx": arg_idx,
                                        "target": target,
                                        "file": filepath,
                                        "line": node.start_point[0] + 1,
                                    })
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        dataflow.store.assign_func_var(call_name, pname, target)
                                else:
                                    if arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        dataflow.store.assign_func_var(call_name, pname, target)
                                        cs_key = (caller or '<unknown>', call_name, arg_idx)
                                        if cs_key not in call_site_targets:
                                            call_site_targets[cs_key] = set()
                                        call_site_targets[cs_key].add(target)
                                _propagate_call_site(call_name, arg_idx, target, dataflow, symbol_names)
                            else:
                                df_targets = dataflow.resolve(f'{caller}:{target}')
                                if not df_targets:
                                    df_targets = dataflow.resolve(target)
                                if df_targets and arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].update(df_targets)
                                    for t in df_targets:
                                        dataflow.assign(f'{call_name}:{pname}', t)
                                        dataflow.assign(pname, t)
                                        dataflow.store.assign_func_var(call_name, pname, t)
                                    cs_key = (caller or '<unknown>', call_name, arg_idx)
                                    if cs_key not in call_site_targets:
                                        call_site_targets[cs_key] = set()
                                    call_site_targets[cs_key].update(df_targets)
                        elif c.type == 'cast_expression':
                            extracted = None
                            if hasattr(dataflow, 'unwrap_cast'):
                                extracted = dataflow.unwrap_cast(c)
                            if not extracted:
                                for cc in reversed(c.children):
                                    if cc.type == 'identifier' and cc.text:
                                        extracted = cc.text.decode('utf-8')
                                        break
                            if extracted and extracted in symbol_names:
                                arg_idx = comma_count
                                target = extracted
                                fp_params_positions = func_fp_params.get(call_name, None)
                                is_reg = False
                                if fp_params_positions is not None:
                                    if arg_idx in fp_params_positions:
                                        is_reg = True
                                else:
                                    if _is_registration(call_name):
                                        is_reg = True
                                if is_reg:
                                    dataflow.registration_sites.append({
                                        "caller": caller or '<unknown>',
                                        "callee": call_name,
                                        "arg_idx": arg_idx,
                                        "target": target,
                                        "file": filepath,
                                        "line": node.start_point[0] + 1,
                                    })
                                elif arg_idx < len(param_names):
                                    pname = param_names[arg_idx]
                                    if pname not in param_mappings:
                                        param_mappings[pname] = set()
                                    param_mappings[pname].add(target)
                                _propagate_call_site(call_name, arg_idx, target, dataflow, symbol_names)
                        elif c.type == 'pointer_expression' and c.children:
                            inner = c.children[-1]
                            if inner.type == 'identifier' and inner.text:
                                target = inner.text.decode('utf-8')
                                if target in symbol_names:
                                    arg_idx = comma_count
                                    fp_params_positions = func_fp_params.get(call_name, None)
                                    is_reg = False
                                    if fp_params_positions is not None:
                                        if arg_idx in fp_params_positions:
                                            is_reg = True
                                    else:
                                        if _is_registration(call_name):
                                            is_reg = True
                                    if is_reg:
                                        dataflow.registration_sites.append({
                                            "caller": caller or '<unknown>',
                                            "callee": call_name,
                                            "arg_idx": arg_idx,
                                            "target": target,
                                            "file": filepath,
                                            "line": node.start_point[0] + 1,
                                        })
                                    elif arg_idx < len(param_names):
                                        pname = param_names[arg_idx]
                                        if pname not in param_mappings:
                                            param_mappings[pname] = set()
                                        param_mappings[pname].add(target)
                                        dataflow.assign(f'{call_name}:{pname}', target)
                                        dataflow.assign(pname, target)
                                        dataflow.store.assign_func_var(call_name, pname, target)
                                        cs_key = (caller or '<unknown>', call_name, arg_idx)
                                        if cs_key not in call_site_targets:
                                            call_site_targets[cs_key] = set()
                                        call_site_targets[cs_key].add(target)
                                    _propagate_call_site(call_name, arg_idx, target, dataflow, symbol_names)
        for child in node.children:
            _collect_call_params(child)

    _collect_call_params(tree.root_node)

    # Store call_site_targets on engine for param_dispatch (Phase 2)
    dataflow.call_site_targets.update(call_site_targets)

    return []  # Phase 1 returns NO edges


def _resolve_fields(tree: ts.Tree, filepath: str, symbol_table, dataflow) -> None:
    """Pass 2: resolve struct member assignments (field=param + return value tracking).
    Must run AFTER all other TARGET_RESOLVERS.
    Reconstructs param_mappings from dataflow (consistent with param_dispatch)."""
    func_params = dataflow.func_params

    # Reconstruct param_mappings from dataflow
    param_mappings: dict[str, set[str]] = {}
    for key, vals in dataflow.targets.items():
        if ':' in key and not key.startswith('<'):
            p = key.split(':')[-1]
            if p not in param_mappings:
                param_mappings[p] = set()
            param_mappings[p].update(vals)

    for fa in collect_field_assignments(tree, unwrap_fn=getattr(dataflow, 'unwrap_cast', None)):
        if fa.enclosing_func is None:
            continue
        field_path = fa.field_path
        field_name = field_path.split('.')[-1]
        base_var = field_path.split('.')[0]
        field_tail = dataflow.store.compute_field_tail(field_path) if hasattr(dataflow, 'store') else field_path

        if fa.value_node and fa.value_node.type == 'call_expression':
            call_func = fa.value_node.child_by_field_name('function') or fa.value_node.children[0]
            if call_func and call_func.type == 'identifier' and call_func.text:
                func_name = call_func.text.decode('utf-8')
                ret_targets = dataflow.resolve_returned_field(func_name)
                for t in ret_targets:
                    dataflow.assign(f'<gstruct:{field_path}>', t)
                    if hasattr(dataflow, 'store'):
                        dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', t, filepath)
        elif fa.resolved_value is not None:
            param_name = fa.resolved_value
            targets = param_mappings.get(param_name, set())
            for t in targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                if hasattr(dataflow, 'store'):
                    dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', t)
            df_targets = dataflow.resolve(f'{fa.enclosing_func}:{param_name}')
            if not df_targets:
                df_targets = dataflow.resolve(param_name)
            if not df_targets:
                df_targets = dataflow.resolve(f'<garray:{param_name}>')
            for t in df_targets:
                dataflow.assign(f'<struct:{field_path}>', t)
                dataflow.assign(f'<struct:{field_name}>', t)
                if hasattr(dataflow, 'store'):
                    dataflow.store.assign_struct_field(f'gstruct:{base_var}.{field_tail}', t)
            if fa.enclosing_func in func_params:
                params = func_params[fa.enclosing_func]
                if param_name in params:
                    param_idx = params.index(param_name)
                    dataflow.register_param_mapping(fa.enclosing_func, param_idx, field_path)
