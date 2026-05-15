# Spec: P0/P2/P3 系统性缺陷修复

**日期**: 2026-05-15
**基于**: `docs/et_bench_architecture_analysis_part2.md`
**状态**: design
**目标**: FPR 31.33% → ~12%，召回率不劣化，高置信度 FPR → ~5%

## 实现顺序

```
P3 置信度形式化 → P2 类型系统扩展 → P0 Suffix 扫描精度 → P2 双轨迁移 → P3 Registration 替换
```

顺序逻辑：置信度形式化先做（数据模型基础，所有模块依赖）；类型系统扩展提升 Tier 1 命中率，为 Suffix 精度改动提供基础；双轨迁移在前几项稳定后清理技术债；Registration 替换最后做增量优化。

---

## Section 1: 置信度形式化（P3）

### 1.1 目标

定义正式 `Confidence` 枚举、结构化 `Evidence` dataclass，统一 12 个模块的置信度赋值标准，修复序列化丢失信息的问题。

### 1.2 `src/ethunter/graph/model.py` 改动

#### 1.2.1 新增 Confidence 枚举

```python
from enum import Enum

class Confidence(Enum):
    """边的置信度等级。ordinal 用于去重排序——高值优先。"""
    HIGH = 'high'       # AST 直接确认：call_expression、精确 key 匹配
    MEDIUM = 'medium'   # 证据丰富但非精确：作用域内 suffix、行为确认
    LOW = 'low'         # 启发式：跨文件 suffix、名称匹配、字符串匹配

    def ordinal(self) -> int:
        return _CONFIDENCE_RANK[self]

_CONFIDENCE_RANK = {
    Confidence.HIGH: 3,
    Confidence.MEDIUM: 2,
    Confidence.LOW: 1,
}
```

#### 1.2.2 新增 Evidence dataclass

```python
@dataclass(frozen=True)
class Evidence:
    """边发现的结构化证据。"""
    method: str                # 检测方法标识
    tier: int | None = None    # multi-tier 解析中的 tier 号
    source: str | None = None  # 数据源

    def __str__(self) -> str:
        parts = [self.method]
        if self.tier is not None:
            parts.append(f'tier={self.tier}')
        if self.source:
            parts.append(self.source)
        return ':'.join(parts)
```

#### 1.2.3 修改 CallEdge

```python
@dataclass(frozen=True)
class CallEdge:
    caller: str
    callee: str
    caller_file: str
    callee_file: str
    type: CallType
    indirect_kind: str | None = None
    caller_line: int | None = None
    confidence: Confidence = Confidence.MEDIUM   # 曾是 str
    evidence: Evidence | None = None             # 曾是 str

    def to_dict(self) -> dict:
        return {
            'caller': self.caller,
            'callee': self.callee,
            'caller_file': self.caller_file,
            'callee_file': self.callee_file,
            'type': self.type.value,
            'indirect_kind': self.indirect_kind or '',
            'caller_line': self.caller_line or 0,
            'confidence': self.confidence.value,       # 始终序列化
            'evidence': str(self.evidence) if self.evidence else '',
        }

    @classmethod
    def from_dict(cls, d: dict) -> 'CallEdge':
        conf_value = d.get('confidence', 'medium')
        evidence_str = d.get('evidence', '')
        return cls(
            caller=d['caller'],
            callee=d['callee'],
            caller_file=d.get('caller_file', ''),
            callee_file=d.get('callee_file', ''),
            type=CallType(d['type']),
            indirect_kind=d.get('indirect_kind') or None,
            caller_line=d.get('caller_line') or None,
            confidence=Confidence(conf_value) if conf_value in ('high', 'medium', 'low') else Confidence.MEDIUM,
            evidence=_parse_evidence(evidence_str) if evidence_str else None,
        )

def _parse_evidence(s: str) -> Evidence | None:
    """从字符串重建 Evidence。格式: method[:tier=N][:source]"""
    if not s:
        return None
    parts = s.split(':')
    method = parts[0]
    tier = None
    source = None
    for p in parts[1:]:
        if p.startswith('tier='):
            tier = int(p.split('=')[1])
        else:
            source = p
    return Evidence(method=method, tier=tier, source=source)
```

### 1.3 `src/ethunter/graph/model.py` — `CallGraph.from_dict` 适配

`CallGraph.from_dict()`（当前 line 100-117）直接构造 `CallEdge` 并传字符串 `confidence`/`evidence`。改为使用 `CallEdge.from_dict()` 委派：

```python
# CallGraph.from_dict — 替换 CallEdge 构造部分
for ed in d.get("edges", []):
    edge = CallEdge.from_dict(ed)
    graph.add_edge(edge)
```

这样 confidence/evidence 的类型转换逻辑集中在 `CallEdge.from_dict` 一处，避免重复。

### 1.4 `src/ethunter/analyzer/orchestrator.py` 改动

去重逻辑改为使用 `Confidence.ordinal()`：

```python
def _dedup_edges(edges: list[CallEdge]) -> list[CallEdge]:
    best: dict[tuple[str, str], CallEdge] = {}
    for edge in edges:
        key = (edge.caller, edge.callee)
        if key not in best:
            best[key] = edge
            continue
        if edge.confidence.ordinal() > best[key].confidence.ordinal():
            best[key] = edge
        elif (edge.confidence == best[key].confidence
              and edge.type == CallType.DIRECT
              and best[key].type != CallType.DIRECT):
            best[key] = edge
    return list(best.values())
```

移除旧常量 `_confidence_rank`。

### 1.5 各模块置信度赋值表

| 模块 | 检测路径 | confidence | evidence |
|------|---------|-----------|----------|
| direct_call | AST call_expression | HIGH | `Evidence('direct_call')` |
| direct_call_fp | ScopedStore 解析 | HIGH | `Evidence('scoped_fp', source='scoped_store')` |
| direct_call_fp | VariableState 回退 | MEDIUM | `Evidence('flat_fp', source='dataflow')` |
| direct_call_fp | struct field init | MEDIUM | `Evidence('struct_field_init', source='local_fp_mapping')` |
| field_resolver | Tier 1 类型感知精确 | HIGH | `Evidence('type_aware', tier=1)` |
| field_resolver | Tier 2 精确路径 | HIGH | `Evidence('exact_path', tier=2)` |
| field_resolver | Tier 3 同文件 suffix | MEDIUM | `Evidence('same_file_suffix', tier=3)` |
| field_resolver | Tier 4 跨文件 suffix | LOW | `Evidence('cross_file_suffix', tier=4)` |
| field_call | legacy fallback | LOW | `Evidence('legacy_fallback')` |
| field_call | macro expansion | MEDIUM | `Evidence('macro_expansion')` |
| field_call | callback-of-callback | MEDIUM | `Evidence('callback_of_callback')` |
| array_call | 全局数组分发 | MEDIUM | `Evidence('array_dispatch')` |
| param_dispatch | Pass A callee 体调用 | HIGH | `Evidence('callee_body_call')` |
| param_dispatch | Pass B 调用点传播 | MEDIUM | `Evidence('call_site_propagation')` |
| callback_reg | behavioral caller | MEDIUM | `Evidence('behavioral_registration')` |
| callback_reg | heuristic name match | LOW | `Evidence('heuristic_registration')` |
| dlsym_fp | string literal match | LOW | `Evidence('dlsym_string_match')` |
| param_assign (legacy) | all passes | MEDIUM | `Evidence('legacy_param_assign')` |

注：`array_call` 从当前 `'high'` 降为 `MEDIUM`——它解析的是间接 dataflow key，非 AST 直接确认，与 `field_resolver` same-file suffix 属于同一可靠性级别。

### 1.6 验证标准

- 所有 et_bench 测试通过（60 tests）
- 新增 `test_confidence_round_trip`: `to_dict()` → `from_dict()` 无信息丢失
- 新增 `test_confidence_ordinals`: 验证排序关系
- 高置信度 FPR 统计使用枚举值精确过滤（`confidence == Confidence.HIGH`）

---

## Section 2: 类型系统扩展（P2）

### 2.1 目标

扩大 `SymbolTable` 的类型信息覆盖，提升 `FieldResolver` Tier 1 命中率，减少 suffix 扫描触发。

### 2.2 `src/ethunter/analyzer/field_call.py` 改动

#### 2.2.1 扩展 `_collect_local_var_types()` 覆盖非指针声明

`_collect_local_var_types()`（`field_call.py:84-129`）当前只匹配 `pointer_declarator` 子节点。扩展现有的递归 `_scan(node, current_func)` 模式，在 `declaration` 分支中增加 non-pointer 变量名提取：

当前逻辑（仅指针路径）：
```python
if node.type == 'declaration' and current_func:
    type_name = None
    var_name = None
    for c in node.children:
        if c.type == 'type_identifier' and c.text:
            type_name = c.text.decode('utf-8')
        elif c.type == 'struct_specifier':
            for sc in c.children:
                if sc.type == 'type_identifier' and sc.text:
                    type_name = sc.text.decode('utf-8'); break
        elif c.type == 'pointer_declarator':
            for pc in c.children:
                if pc.type == 'identifier' and pc.text:
                    var_name = pc.text.decode('utf-8'); break
    if type_name and var_name:
        symbol_table.record_func_var_type(current_func, var_name, type_name)
```

扩展后：在已有的 `pointer_declarator` 路径之后，新增对非指针声明和 `init_declarator` 的变量名提取。使用已有 helper `extract_identifier_from_declarator` 复用声明器解析逻辑：

在 `declaration` 处理的 `for c in node.children` 循环中，于已有 `pointer_declarator` 分支的 `elif` 后追加：

```python
# NEW：处理 declarator 为 init_declarator 的情况（如 "struct foo var = {...}"）
# init_declarator 本身作为 declaration 的 declarator 字段出现
elif c.type == 'init_declarator':
    inner_decl = c.child_by_field_name('declarator')
    if inner_decl:
        from ethunter.analyzer.helpers import extract_identifier_from_declarator
        var_name = extract_identifier_from_declarator(inner_decl)
# NEW：处理 field_identifier / identifier 作为声明直接子节点（非指针声明）
elif c.type in ('field_identifier', 'identifier') and c.text:
    var_name = c.text.decode('utf-8')
```

注意：当声明包含 `init_declarator` 时（如 `foo_t var = expr`），`declaration` 节点的 `declarator` 字段直接指向 `init_declarator`，因此需要在 `declaration.children` 中匹配 `init_declarator` 类型。从 `init_declarator` 中通过 `child_by_field_name('declarator')` 获取内部的 identifier/pointer_declarator，再用 `extract_identifier_from_declarator` 提取变量名。

类型名提取逻辑（`type_identifier` / `struct_specifier`）不需要改动——已在循环中覆盖。

### 2.3 `src/ethunter/analyzer/param_helpers.py` 改动

#### 2.3.1 扩展 `_collect_param_types()` 覆盖函数声明

`_collect_param_types()`（`param_helpers.py:357-406`）是模块级函数，当前通过递归 `_scan()` 仅遍历 `function_definition` 节点。新增对 `declaration` 节点中带 `function_declarator` 的函数声明的处理：

在现有 `_scan()` 内部（`param_helpers.py:363`），于 `if node.type == 'function_definition':` 块之后新增 `elif` 分支：

```python
elif node.type == 'declaration':
    decl = _find_child(node, 'function_declarator')
    if decl:
        fname, inner_decl = _find_func_name_from_decl(decl)
        if fname:
            plist = _find_child(inner_decl, 'parameter_list')
            if plist:
                for p in plist.children:
                    if p.type == 'parameter_declaration':
                        pname = _extract_param_name(p)
                        if not pname:
                            continue
                        for tc in p.children:
                            if tc.type == 'type_identifier' and tc.text:
                                type_name = tc.text.decode('utf-8')
                                symbol_table.record_func_var_type(fname, pname, type_name)
                                break
                            if tc.type == 'struct_specifier':
                                for sc in tc.children:
                                    if sc.type == 'type_identifier' and sc.text:
                                        type_name = sc.text.decode('utf-8')
                                        symbol_table.record_func_var_type(fname, pname, type_name)
                                        break
```

逻辑与现有 `function_definition` 分支完全相同——通过 `_find_func_name_from_decl`（已在 `param_helpers.py` 中定义）提取函数名，通过 `_find_child` 和 `_extract_param_name`（已有 helper）获取参数名，从 `type_identifier` / `struct_specifier` 提取类型名。

#### 2.3.2 新增 `_collect_return_types()`

新增模块级函数，使用与现有 `_collect_param_types()` 相同的递归 `_scan()` 模式，扫描函数定义和声明，记录返回 struct 指针的函数名：

在 `param_helpers.py` 中新增：

```python
def _collect_return_types(root_node, symbol_table) -> None:
    """Record struct pointer return types for functions.
    
    For 'struct type *func(...)', records func_name -> 'type' in symbol_table.
    """
    def _scan(node):
        if node.type in ('function_definition', 'declaration'):
            type_node = _find_child(node, 'type')
            decl = _find_child(node, 'function_declarator')
            if type_node and decl:
                fname, _ = _find_func_name_from_decl(decl)
                if fname:
                    # Check for struct_specifier or type_identifier in type_node
                    for tc in type_node.children:
                        if tc.type == 'type_identifier' and tc.text:
                            symbol_table.record_func_return_type(fname, tc.text.decode('utf-8'))
                            break
                        if tc.type == 'struct_specifier':
                            for sc in tc.children:
                                if sc.type == 'type_identifier' and sc.text:
                                    symbol_table.record_func_return_type(fname, sc.text.decode('utf-8'))
                                    break
        for child in node.children:
            _scan(child)
    _scan(root_node)
```

调用点：在 `prepare()` 末尾（`param_helpers.py:354`），`_collect_param_types()` 调用后新增：
```python
if symbol_table is not None:
    _collect_return_types(tree.root_node, symbol_table)
```

### 2.4 `src/ethunter/analyzer/symbol_table.py` 改动

新增方法：

```python
# 新 dict
_func_return_types: dict[str, str] = field(default_factory=dict)

def record_func_return_type(self, func_name: str, struct_type: str) -> None:
    self._func_return_types[func_name] = struct_type

def get_func_return_type(self, func_name: str) -> str | None:
    return self._func_return_types.get(func_name)
```

### 2.5 验证标准

- 所有 et_bench 测试通过
- 新增 `test_collect_local_var_types_non_pointer`: 验证非指针声明类型被记录
- 新增 `test_collect_param_types_from_declarations`: 验证函数声明的参数类型被记录
- 新增 `test_collect_return_types`: 验证返回类型被记录
- 指标：Tier 1 命中率（添加日志统计，改进前后对比）

---

## Section 3: Suffix 扫描精度（P0）

### 3.1 目标

削减 field_call 误报约 140 条。核心原则：当 struct 类型已知时，Tier 1 精确 key 是最终答案——不存在则无数据，不回退到 suffix 扫描。

### 3.2 `src/ethunter/analyzer/field_resolver.py` 改动

`resolve_field_call()` 的 Tier 3/4 增加类型门控：

```python
def resolve_field_call(self, field_path, base_var, caller_func, filepath):
    field_tail = self._store.compute_field_tail(field_path)
    targets = set()

    # Tier 1: type-aware exact match
    struct_type = None
    if caller_func:
        struct_type = self._symbol_table.get_func_var_type(caller_func, base_var)
    if not struct_type:
        struct_type = self._symbol_table.get_var_type(base_var)
    if struct_type:
        targets = self._store.resolve_struct_field(f'gstruct:{struct_type}.{field_tail}')
        if targets:
            return targets, Confidence.HIGH, Evidence('type_aware', tier=1)

    # Tier 2: exact path match
    targets = self._store.resolve_struct_field(f'gstruct:{base_var}.{field_tail}')
    if targets:
        return targets, Confidence.HIGH, Evidence('exact_path', tier=2)

    # === 核心改动：类型已知 → 不 fallback 到 suffix ===
    if struct_type:
        # Tier 1 已在类型精确 key 上未命中 → 该类型确实无此字段数据
        # suffix 扫描只会带回不相关类型的同名字段 → 直接返回空
        return set(), None, None

    # 类型未知时才 fall through 到 suffix 扫描（保证召回率）
    suffix = f'.{field_tail}'

    # Tier 3: same-file scoped suffix
    for key, vals in self._store.struct_fields.items():
        if not key.endswith(suffix):
            continue
        files = self._store.struct_field_files.get(key)
        if files and filepath not in files:
            continue
        targets.update(vals)
    if targets:
        return targets, Confidence.MEDIUM, Evidence('same_file_suffix', tier=3)

    # Tier 4: cross-file suffix
    for key, vals in self._store.struct_fields.items():
        if key.endswith(suffix):
            targets.update(vals)
    if targets:
        return targets, Confidence.LOW, Evidence('cross_file_suffix', tier=4)

    return set(), None, None
```

**返回值语义**：`(targets_set, Confidence | None, Evidence | None)`。当 confidence 为 `None` 时表示未找到任何匹配——调用方应将此视为无结果，不使用 legacy fallback。

#### 3.2.1 caller 适配：`field_call.py` 中的 `_visit()`

`_visit()` 当前调用 `resolver.resolve_field_call()` 后检查 `confidence == 'none'` 来决定是否使用 legacy fallback。适配为：

```python
targets, confidence, evidence = resolver.resolve_field_call(...)
if confidence is not None:
    # FieldResolver 给出了答案（targets 可能为空集——类型已知但 key 未命中）
    # 直接使用返回的 targets，不进入 legacy fallback
    pass  # 跳过 legacy 回退
else:
    # FieldResolver 所有 tier 均未匹配（类型未知 + suffix 也失败）
    # 使用 legacy fallback（<gstruct:>, <struct:>, dataflow 后缀扫描）
    targets = _resolve_via_legacy_fallback(field_path, ...)
```

注意：当 `confidence is not None` 但 targets 为空时（类型已知 + Tier 1 未命中），应直接生成**零条边**——这是 field_resolver 的明确结论，不应交由 legacy fallback 补充。

### 3.3 修复未追踪写入

#### 3.3.1 `field_call.py` `analyze()` 中赋给 struct_fields

`analyze()` 在 line 227-228 调用 `store.assign_struct_field(key, targets)` 时不传 `filepath`。`filepath` 已是 `analyze()` 的可用参数，直接传入：

```python
store.assign_struct_field(key, targets, filepath=filepath)
```

#### 3.3.2 `dataflow.py` `resolve_call_site_param()` 扩展签名

新增 `filepath` 参数，传递给 `assign_struct_field()`：

```python
def resolve_call_site_param(self, func_name, param_idx, arg_name,
                            symbol_names, filepath=''):
    ...
    self.store.assign_struct_field(key, targets, filepath=filepath)
```

调用方 `_propagate_call_site()`（`param_binding.py:16-23`）从 `analyze()` 接收 `filepath` 并传递给 `resolve_call_site_param()`：
```python
# _propagate_call_site 扩展签名
def _propagate_call_site(call_name, arg_idx, target, dataflow, symbol_names, filepath=''):
    dataflow.resolve_call_site_param(
        call_name, arg_idx, target, symbol_names=symbol_names, filepath=filepath
    )
```
在 `analyze()` 的 4 个参数处理分支中，`_propagate_call_site(...)` 调用均增加 `filepath=filepath` 参数。

### 3.4 验证标准

- 所有 et_bench 60 测试通过（重点：recall 不低于当前值）
- 新增 `test_tier3_skipped_when_type_known`: 类型已知但 key 不存在时 Tier 3 不执行
- 新增 `test_unresolvable_struct_type_proceeds_to_suffix`: 类型未知时仍执行 suffix 扫描
- 新增 `test_struct_field_file_tracking`: 所有 `assign_struct_field` 调用传入有效 filepath
- 指标：field_call 误报从 ~158 → ~20
- `resolve_with_evidence()`（`field_resolver.py:220-228`）返回的是 `(targets, strategy_class_name)`——其中 strategy_class_name 是匹配策略的类名（如 `"TypeAwareStructLookup"`），用于调试而非置信度判定。**不修改此方法**——它与 `resolve_field_call()` 的语义不同

---

## Section 4: 双轨迁移完成（P2）

### 4.1 目标

移除已废弃的 `param_assign.analyze()` 调用，清理双轨并行。新模块功能上完全覆盖旧模块。

### 4.2 `src/ethunter/analyzer/param_binding.py` 改动

在 `analyze()` 的 `_collect_call_params()` 中，有 4 个参数处理分支会将 target 添加到 `registration_sites[]`：`identifier` 实参（line 79）、`cast_expression` 实参（line 144）、`pointer_expression` 实参（line 173）、以及 dataflow 回退路径。在每个分支的 `dataflow.registration_sites.append(...)` 之前增加 `param_usage` 预过滤：

```python
# 在每个分支的 is_reg 判断为 True 后、registration_sites.append 之前：
usage = dataflow.state.param_usage.get((call_name, arg_idx), 'unknown')
if usage in ('forwarder', 'storage'):
    continue  # 与旧模块行为对齐：不注册 forwarder/storage 参数
```

### 4.3 `src/ethunter/analyzer/orchestrator.py` 改动

移除 deprecated Phase 1c：

```python
# 移除以下代码块：
# # Phase 1c: param_assign.analyze() (DEPRECATED — kept for backward compat)
# edges = param_assign.analyze(tree, filepath, symbol_table, engine)
# graph.edges.extend(edges)
```

保留 `param_assign.register_phase()` 调用——作为安全网（`param_helpers.prepare()` 已运行后为实际 noop），一个版本后移除。

### 4.4 等价性验证

新增测试 `test_new_modules_equivalent_to_old`：

```python
def test_new_modules_equivalent_to_old():
    """新模块 (param_binding + param_dispatch + callback_reg) 至少与旧模块 (param_assign) 等价。"""
    for category in get_categories():
        for example in get_examples(category):
            # 模式 A: 仅旧模块
            graph_old = run_with_old_only(fixture)
            # 模式 B: 仅新模块
            graph_new = run_with_new_only(fixture)
            # 断言
            assert recall_new >= recall_old, f"recall regression in {category}/{example}"
            assert fpr_new <= fpr_old, f"FPR regression in {category}/{example}"
```

### 4.5 验证标准

- 等价性测试通过：新模块召回率 >= 旧模块，FPR <= 旧模块
- 所有 et_bench 测试通过
- `param_assign.py` 保留但 `analyze()` 不再被 orchestrator 调用

---

## Section 5: Registration 名称启发式替换（P3）

### 5.1 目标

将 registration 检测从名称猜测转向签名分析。绝大多数场景由 `func_fp_params`（扩展后）覆盖，边缘场景保守化——不注册而非猜测。

### 5.2 `src/ethunter/analyzer/param_helpers.py` 改动

`_collect_func_params()`（`param_helpers.py:156-188`）是模块级递归函数。当前仅在 `node.type == 'function_definition'` 时处理。在现有递归逻辑中新增对 `declaration` 节点的处理：

在 `_collect_func_params()` 的函数体中，于 `if node.type == 'function_definition':` 块之后新增：

```python
elif node.type == 'declaration':
    decl = _find_child(node, 'function_declarator')
    if decl:
        fname, inner_decl = _find_func_name_from_decl(decl)
        if fname:
            params = []
            fp_positions = set()
            plist = _find_child(inner_decl, 'parameter_list')
            if plist:
                pos = 0
                for p in plist.children:
                    if p.type == 'parameter_declaration':
                        pname = _extract_param_name(p)
                        if pname:
                            params.append(pname)
                            if func_fp_params is not None and _has_fnptr_declarator(p, fnptr_typedefs):
                                fp_positions.add(pos)
                            pos += 1
            func_params[fname] = params
            if func_fp_params is not None and fp_positions:
                func_fp_params[fname] = fp_positions
```

此分支逻辑与现有 `function_definition` 分支完全相同——通过 `_find_func_name_from_decl` 提取函数名，通过 `_extract_param_name` 和 `_has_fnptr_declarator` 检测 fnptr 参数。

### 5.3 `src/ethunter/analyzer/callback_reg.py` 改动

Stage 3 保守化：

```python
# Stage 3: heuristic fallback — was registration name match, now skipped
if usage == 'unknown':
    # callee 在所有分析文件中既无定义也无声明 → 无法确认注册身份
    # 默认不注册（保守化）
    continue
```

注意：`_is_registration()` 函数保留但不被 callback_reg 调用。Remaining call site in `param_binding`（已由改动 5.2 大幅减少了 `func_fp_params` 缺失的情况）。

**风险控制**：添加日志记录所有被跳过的 registration（callee 不在 func_params 且不在 func_fp_params 中）。若 et_bench 召回率回落，说明有 callee 仅在系统头文件中声明——此时对特定缺失模式，应在头文件中也扫描声明，而非回退到名称匹配。

### 5.4 `src/ethunter/analyzer/param_binding.py` 改动

对 `func_fp_params` 无记录且 callee 不在 `func_params` 中的调用点——不注册：

```python
if callee_name not in func_fp_params and callee_name not in func_params:
    continue  # 无法确认注册身份，不注册
```

### 5.5 验证标准

- 所有 et_bench 测试通过，callback_reg 误报减少
- 新增 `test_fp_params_collected_from_declarations`: 验证声明中的 fnptr 参数被收集
- 新增 `test_unknown_callee_not_registered`: 不可见 callee 不产生 callback_reg 边
- `_is_registration()` 标记为 deprecated（保留但无调用点）

---

## 综合验证

### 回归测试

所有改动完成后运行：

```bash
PYTHONPATH=src .venv/bin/python -m pytest tests/ -q
```

必须全部通过，包括：
- 60 个 et_bench 测试
- 所有 test_analyzers.py、test_cross_file.py、test_query_json.py、test_scanner.py、test_cg_bench.py

### 指标目标

| 指标 | 当前 | 目标 |
|------|------|------|
| 总体 FPR | 31.33% | ≤15% |
| 高置信度 FPR | 13.44% | ≤5% |
| 总体召回率 | 98.86% | ≥98.86%（不劣化） |
| field_call 误报 | 158 | ≤30 |
| callback_reg 误报 | 15 | ≤5 |
| callback_param 误报 | 90 | ≤70（内生限制） |

### 文件改动清单

| 文件 | Section | 改动性质 |
|------|---------|---------|
| `src/ethunter/graph/model.py` | 1 | 新增 Confidence 枚举、Evidence dataclass，修改 CallEdge |
| `src/ethunter/analyzer/orchestrator.py` | 1, 4 | 去重使用 ordinal；移除 param_assign.analyze() |
| `src/ethunter/analyzer/field_call.py` | 2, 3 | 扩展类型收集；analyze() 传入 filepath |
| `src/ethunter/analyzer/param_helpers.py` | 2, 5 | 扩展参数类型；新增返回类型；扩展 func_fp_params |
| `src/ethunter/analyzer/symbol_table.py` | 2 | 新增 _func_return_types + 方法 |
| `src/ethunter/analyzer/field_resolver.py` | 3 | Tier 3/4 增加类型门控 |
| `src/ethunter/analyzer/dataflow.py` | 3 | resolve_call_site_param 新增 filepath 参数 |
| `src/ethunter/analyzer/param_binding.py` | 4, 5 | 补齐 usage 预过滤；faillback 保守化 |
| `src/ethunter/analyzer/callback_reg.py` | 5 | Stage 3 保守化 |
| `src/ethunter/analyzer/direct_call.py` | 1 | 置信度枚举替换 |
| `src/ethunter/analyzer/direct_call_fp.py` | 1 | 置信度枚举替换 |
| `src/ethunter/analyzer/direct_assign.py` | 1 | (确认：不产边，无需改) |
| `src/ethunter/analyzer/initializer_assign.py` | 1 | (确认：不产边，无需改) |
| `src/ethunter/analyzer/cast_assign.py` | 1 | (确认：不产边，无需改) |
| `src/ethunter/analyzer/array_call.py` | 1 | 置信度枚举替换 + 调整 |
| `src/ethunter/analyzer/dlsym_fp.py` | 1 | 置信度枚举替换 |
| `src/ethunter/analyzer/param_assign.py` | 1 | 置信度枚举替换 |
| `src/ethunter/analyzer/param_dispatch.py` | 1 | 置信度枚举替换（已有） |
| `tests/test_et_bench.py` | 1, 4 | 新增 round-trip、等价性测试 |
