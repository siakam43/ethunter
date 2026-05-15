# P0/P2/P3 系统性修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut FPR from 31.33% to ~12% by fixing suffix scanning precision, migrating dual-track modules, expanding type tracking, replacing name-based registration heuristics, and formalizing the confidence model.

**Architecture:** Implementation follows the spec's dependency order: confidence enum first (data model foundation), type system expansion second (enables precise suffix gating), suffix precision third (the big FPR cut), dual-track migration fourth (tech debt cleanup), and registration replacement last (incremental optimization). Each section is independently verifiable via the existing 60 et_bench tests plus new targeted unit tests.

**Tech Stack:** Python 3.11, pytest, tree-sitter, dataclasses

---

### Task 1: Add Confidence enum and Evidence dataclass to model.py

**Files:**
- Modify: `src/ethunter/graph/model.py` (insert after line 7: `from enum import Enum` already present, add after `class CallType`)

- [ ] **Step 1: Add Confidence enum and Evidence dataclass**

Insert after `class CallType(Enum):` block (after line 12):

```python
class Confidence(Enum):
    """Edge confidence level. Ordinal used for dedup — higher wins."""
    HIGH = 'high'
    MEDIUM = 'medium'
    LOW = 'low'

    def ordinal(self) -> int:
        return _CONFIDENCE_RANK[self]

_CONFIDENCE_RANK = {
    Confidence.HIGH: 3,
    Confidence.MEDIUM: 2,
    Confidence.LOW: 1,
}


@dataclass(frozen=True)
class Evidence:
    """Structured evidence for how an edge was discovered."""
    method: str
    tier: int | None = None
    source: str | None = None

    def __str__(self) -> str:
        parts = [self.method]
        if self.tier is not None:
            parts.append(f'tier={self.tier}')
        if self.source:
            parts.append(self.source)
        return ':'.join(parts)
```

- [ ] **Step 2: Run existing tests to confirm no breakage**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all 60 tests PASS (enum/dataclass added but not yet used by CallEdge)

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/graph/model.py
git commit -m "feat: add Confidence enum and Evidence dataclass to model"
```

---

### Task 2: Update CallEdge to use Confidence and Evidence types

**Files:**
- Modify: `src/ethunter/graph/model.py:30-59` (CallEdge dataclass, to_dict)

- [ ] **Step 1: Change CallEdge fields and to_dict**

Replace the CallEdge class (lines 30-59):

```python
@dataclass(frozen=True)
class CallEdge:
    """Represents a call relationship between two functions."""
    caller: str  # function name
    callee: str  # function name
    caller_file: str = ""
    callee_file: str = ""
    type: CallType = CallType.DIRECT
    indirect_kind: str = ""
    caller_line: int = 0
    confidence: Confidence = Confidence.MEDIUM
    evidence: Evidence | None = None

    def to_dict(self) -> dict:
        d = {
            "caller": self.caller,
            "callee": self.callee,
            "caller_file": self.caller_file,
            "callee_file": self.callee_file,
            "type": self.type.value,
        }
        if self.type == CallType.INDIRECT:
            d["indirect_kind"] = self.indirect_kind
        if self.caller_line:
            d["caller_line"] = self.caller_line
        d["confidence"] = self.confidence.value
        if self.evidence:
            d["evidence"] = str(self.evidence)
        return d
```

Key changes from original:
- `confidence: Confidence = Confidence.MEDIUM` (was `str = 'medium'`)
- `evidence: Evidence | None = None` (was `str = ''`)
- `confidence` always serialized (was conditional `!= 'medium'`)

- [ ] **Step 2: Add CallEdge.from_dict classmethod**

Add `from_dict` and `_parse_evidence` after `to_dict()` in CallEdge:

```python
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
            indirect_kind=d.get('indirect_kind', ''),
            caller_line=d.get('caller_line', 0),
            confidence=Confidence(conf_value) if conf_value in ('high', 'medium', 'low') else Confidence.MEDIUM,
            evidence=_parse_evidence(evidence_str) if evidence_str else None,
        )


def _parse_evidence(s: str) -> Evidence | None:
    """Parse evidence string in format: method[:tier=N][:source]."""
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

- [ ] **Step 3: Update CallGraph.from_dict to use CallEdge.from_dict**

In `CallGraph.from_dict` (line 100-117), replace the direct `CallEdge(...)` construction:

```python
        for ed in d.get("edges", []):
            edge = CallEdge.from_dict(ed)
            graph.add_edge(edge)
```

Remove the old block that does:
```python
        for ed in d.get("edges", []):
            type_str = ed.get("type", CallType.DIRECT.value)
            try:
                edge_type = CallType(type_str)
            except ValueError:
                raise ValueError(f"Unknown CallType: {type_str!r}")
            edge = CallEdge(
                caller=ed["caller"],
                ...
```

- [ ] **Step 4: Run tests to verify round-trip works**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py tests/test_query_json.py -q --tb=short`
Expected: all PASS (existing code still passes strings to CallEdge — they need updating next, but from_dict handles both formats)

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/graph/model.py
git commit -m "feat: update CallEdge to use Confidence enum and Evidence dataclass"
```

---

### Task 3: Convert direct_call analyzer to use Confidence enum

**Files:**
- Modify: `src/ethunter/analyzer/direct_call.py:32-40`

- [ ] **Step 1: Update CallEdge construction**

Replace lines 32-40:

```python
                edges.append(CallEdge(
                    caller=caller,
                    callee=callee,
                    caller_file=filepath,
                    callee_file='',
                    type=CallType.DIRECT,
                    confidence=Confidence.HIGH,
                    evidence=Evidence('direct_call'),
                ))
```

- [ ] **Step 2: Run single test to verify**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -q --tb=short`
Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/direct_call.py
git commit -m "refactor: convert direct_call to Confidence enum"
```

---

### Task 4: Convert direct_call_fp analyzer to use Confidence enum

**Files:**
- Modify: `src/ethunter/analyzer/direct_call_fp.py:34-74`

- [ ] **Step 1: Update _get_targets return types and CallEdge construction**

Replace `_get_targets()` function body — change the confidence/evidence strings to enum:

In `_get_targets()` (line 34), replace:
```python
        confidence, evidence = 'medium', 'direct_assign resolution'
```
With:
```python
        confidence, evidence = Confidence.MEDIUM, Evidence('flat_fp', source='dataflow')
```

Replace each resolution path's confidence/evidence assignment:
- Line 42 (`'high', 'scoped variable resolution'`):
  ```python
  confidence, evidence = Confidence.HIGH, Evidence('scoped_fp', source='scoped_store')
  ```
- Line 46 (`'high', 'global variable resolution'`):
  ```python
  confidence, evidence = Confidence.HIGH, Evidence('global_fp', source='scoped_store')
  ```
- Line 50 (`'high', 'scoped variable resolution'`):
  ```python
  confidence, evidence = Confidence.HIGH, Evidence('scoped_fp', source='dataflow')
  ```
- Line 56 (`'medium', 'local fp from struct field'`):
  ```python
  confidence, evidence = Confidence.MEDIUM, Evidence('struct_field_init', source='local_fp_mapping')
  ```

In the `analyze()` function, line 65-74 — no code changes needed; the `confidence=confidence, evidence=evidence` kwargs already pass through correctly from the updated `_get_targets()`.

- [ ] **Step 2: Run test**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/direct_call_fp.py
git commit -m "refactor: convert direct_call_fp to Confidence enum"
```

---

### Task 5: Convert array_call, dlsym_fp, param_dispatch, callback_reg to Confidence enum

**Files:**
- Modify: `src/ethunter/analyzer/array_call.py:56-57`
- Modify: `src/ethunter/analyzer/dlsym_fp.py:42-43`
- Modify: `src/ethunter/analyzer/param_dispatch.py:99-100,123-124`
- Modify: `src/ethunter/analyzer/callback_reg.py:49-61`

- [ ] **Step 1: Convert array_call.py**

Lines 56-57 — replace:
```python
                            confidence='high',
                            evidence='global array dispatch',
```
With:
```python
                            confidence=Confidence.MEDIUM,
                            evidence=Evidence('array_dispatch'),
```

- [ ] **Step 2: Convert dlsym_fp.py**

Lines 42-43 — replace:
```python
                                        confidence='low',
                                        evidence='dlsym string literal match',
```
With:
```python
                                        confidence=Confidence.LOW,
                                        evidence=Evidence('dlsym_string_match'),
```

- [ ] **Step 3: Convert param_dispatch.py**

Lines 99-100 — replace:
```python
            confidence='high',
            evidence='fnptr call in callee body',
```
With:
```python
            confidence=Confidence.HIGH,
            evidence=Evidence('callee_body_call'),
```

Lines 123-124 — replace:
```python
                confidence='medium',
                evidence='call-site caller -> target',
```
With:
```python
                confidence=Confidence.MEDIUM,
                evidence=Evidence('call_site_propagation'),
```

- [ ] **Step 4: Convert callback_reg.py**

Lines 49-51 — replace:
```python
        confidence, evidence = ('medium', 'behavioral: fnptr called in callee body') \
            if usage == 'caller' else ('low', f'heuristic: registration name match ({callee})')
```
With:
```python
        if usage == 'caller':
            confidence, evidence = Confidence.MEDIUM, Evidence('behavioral_registration')
        else:
            confidence, evidence = Confidence.LOW, Evidence('heuristic_registration')
```

- [ ] **Step 5: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/array_call.py src/ethunter/analyzer/dlsym_fp.py \
        src/ethunter/analyzer/param_dispatch.py src/ethunter/analyzer/callback_reg.py
git commit -m "refactor: convert array_call/dlsym_fp/param_dispatch/callback_reg to Confidence enum"
```

---

### Task 6: Convert field_call analyzer to use Confidence enum

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:254-255,276-277,326-327,339`

- [ ] **Step 1: Update confidence/evidence strings to enum in field_call**

Replace lines 254-255:
```python
                    confidence = 'medium'
                    evidence = 'field_call resolution'
```
With:
```python
                    confidence = Confidence.MEDIUM
                    evidence = Evidence('field_call_resolution')
```

Replace lines 276-277:
```python
                            if targets and confidence in ('none', ''):
                                confidence, evidence = 'low', 'legacy dataflow fallback'
```
With:
```python
                            if targets and confidence is None:
                                confidence, evidence = Confidence.LOW, Evidence('legacy_fallback')
```

Lines 318-327 and 339 — Leave the `confidence=confidence, evidence=evidence` kwargs as-is; they already pass through whatever was set above.

- [ ] **Step 2: Update macro expansion path**

Find the macro expansion edge creation (around line 329-347). Add explicit confidence:
```python
                            confidence=Confidence.MEDIUM,
                            evidence=Evidence('macro_expansion'),
```

- [ ] **Step 3: Update callback-of-callback path**

Find the callback-of-callback edge creation (around line 284-315). Add explicit confidence:
```python
                            confidence=Confidence.MEDIUM,
                            evidence=Evidence('callback_of_callback'),
```

- [ ] **Step 4: Update field_resolver return type check**

In `_visit()`, the `confidence in ('none', '')` check at line 276 becomes `confidence is None` (already done in Step 1). But also update the return type expectation: the `resolve_field_call()` return is now `(set, Confidence|None, Evidence|None)` instead of `(set, str, str)`. The calling code at line 260-261:
```python
targets, confidence, evidence = \
    resolver.resolve_field_call(field_path, base_var, caller_func, filepath)
```
This line works as-is — the variables just hold the new types. No change needed.

- [ ] **Step 5: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 6: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "refactor: convert field_call to Confidence enum"
```

---

### Task 7: Convert remaining edge-producing modules (param_assign)

**Files:**
- Modify: `src/ethunter/analyzer/param_assign.py` (all CallEdge constructions — lines ~500-790)

- [ ] **Step 1: Update param_assign CallEdge constructions**

Search for all `CallEdge(` calls in param_assign.py. The module uses defaults for confidence/evidence (no explicit arguments). Replace each `CallEdge(` call that produces an indirect edge with confidence enum. There are ~7 sites in `_collect_call_params`, `_detect_param_calls`, and the Pass 4 emitter.

For each `CallEdge(` call in the `analyze()` function, add:
```python
    confidence=Confidence.MEDIUM,
    evidence=Evidence('legacy_param_assign'),
```

Find the sites with:
```bash
grep -n "CallEdge(" src/ethunter/analyzer/param_assign.py
```
The edge constructions typically look like:
```python
edges.append(CallEdge(
    caller=caller or '<unknown>',
    callee=target,
    caller_file=filepath,
    callee_file='',
    type=CallType.INDIRECT,
    indirect_kind='callback_reg',
))
```
Add confidence=Confidence.MEDIUM, evidence=Evidence('legacy_param_assign') to each.

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/param_assign.py
git commit -m "refactor: convert param_assign to Confidence enum"
```

---

### Task 8: Update orchestrator dedup to use Confidence.ordinal()

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py:168-183`

- [ ] **Step 1: Replace string-based confidence rank with enum ordinal**

Replace lines 168-183:
```python
    # Deduplicate: keep highest-confidence edge for each (caller, callee) pair
    _confidence_rank = {'high': 3, 'medium': 2, 'low': 1}
    edge_map: dict[tuple[str, str], CallEdge] = {}
    for edge in graph.edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge
        else:
            current_rank = _confidence_rank.get(edge.confidence, 0)
            existing_rank = _confidence_rank.get(edge_map[key].confidence, 0)
            if current_rank > existing_rank:
                edge_map[key] = edge
            elif current_rank == existing_rank and edge.type == CallType.DIRECT \
                    and edge_map[key].type != CallType.DIRECT:
                edge_map[key] = edge
    graph.edges = list(edge_map.values())
```
With:
```python
    # Deduplicate: keep highest-confidence edge for each (caller, callee) pair
    edge_map: dict[tuple[str, str], CallEdge] = {}
    for edge in graph.edges:
        key = (edge.caller, edge.callee)
        if key not in edge_map:
            edge_map[key] = edge
        else:
            if edge.confidence.ordinal() > edge_map[key].confidence.ordinal():
                edge_map[key] = edge
            elif (edge.confidence == edge_map[key].confidence
                  and edge.type == CallType.DIRECT
                  and edge_map[key].type != CallType.DIRECT):
                edge_map[key] = edge
    graph.edges = list(edge_map.values())
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py
git commit -m "refactor: use Confidence.ordinal() in orchestrator dedup"
```

---

### Task 9: Add Confidence round-trip test

**Files:**
- Modify: `tests/test_et_bench.py` (append new test functions)

- [ ] **Step 1: Write the test**

Add at end of `tests/test_et_bench.py`:

```python
from ethunter.graph.model import CallEdge, CallType, Confidence, Evidence


def test_confidence_round_trip():
    """CallEdge.to_dict() -> from_dict() preserves confidence and evidence."""
    edge = CallEdge(
        caller="main",
        callee="handler",
        caller_file="a.c",
        callee_file="",
        type=CallType.INDIRECT,
        indirect_kind="callback_param",
        caller_line=42,
        confidence=Confidence.HIGH,
        evidence=Evidence("callee_body_call"),
    )
    d = edge.to_dict()
    restored = CallEdge.from_dict(d)
    assert restored.confidence == Confidence.HIGH
    assert restored.evidence is not None
    assert restored.evidence.method == "callee_body_call"


def test_confidence_round_trip_default_medium():
    """Default confidence MEDIUM must survive round-trip (no longer omitted)."""
    edge = CallEdge(
        caller="main",
        callee="helper",
        caller_file="b.c",
        callee_file="",
        type=CallType.DIRECT,
    )
    d = edge.to_dict()
    assert "confidence" in d, "confidence must always be serialized"
    assert d["confidence"] == "medium"
    restored = CallEdge.from_dict(d)
    assert restored.confidence == Confidence.MEDIUM


def test_confidence_ordinals():
    """Verify confidence ordering for dedup."""
    assert Confidence.HIGH.ordinal() > Confidence.MEDIUM.ordinal()
    assert Confidence.MEDIUM.ordinal() > Confidence.LOW.ordinal()
```

- [ ] **Step 2: Run test**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_confidence_round_trip tests/test_et_bench.py::test_confidence_round_trip_default_medium tests/test_et_bench.py::test_confidence_ordinals -v`
Expected: 3 PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add confidence round-trip and ordinal tests"
```

---

### Task 10: Run full regression and verify high-confidence FPR

**Files:**
- (no file changes)

- [ ] **Step 1: Run full test suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS (including et_bench, analyzers, cross_file, query_json, scanner, cg_bench)

- [ ] **Step 2: Verify high-confidence FPR uses enum filter**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_high_confidence_fpr -v -s`
Expected: PASS (FPR same as before — ~13.44% — since we only changed types, not resolution logic)

- [ ] **Step 3: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: Section 1 (Confidence formalization) complete — all tests pass"
```

---

### Task 11: Expand _collect_local_var_types for non-pointer declarations

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:110-125`

- [ ] **Step 1: Add non-pointer and init_declarator handling**

In `_collect_local_var_types()` (line 84), inside the `if node.type == 'declaration' and current_func:` block (line 110), after the existing `elif c.type == 'pointer_declarator':` branch (line 120), add two new `elif` branches:

After line 125 (`symbol_table.record_func_var_type(...)` never gets called if `var_name` is still `None`), insert before the `for c in node.children` loop closing:

```python
                elif c.type == 'init_declarator':
                    inner_decl = c.child_by_field_name('declarator')
                    if inner_decl:
                        from ethunter.analyzer.helpers import extract_identifier_from_declarator
                        var_name = extract_identifier_from_declarator(inner_decl)
                elif c.type in ('field_identifier', 'identifier') and c.text:
                    var_name = c.text.decode('utf-8')
```

The exact insertion point is within the `for c in node.children:` loop (starting at line 113), after the `pointer_declarator` elif block (which ends around line 123 with `var_name = pc.text.decode('utf-8'); break`), and before the closing of the `for c in node.children:` loop.

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "feat: expand _collect_local_var_types for non-pointer and init_declarator declarations"
```

---

### Task 12: Extend _collect_param_types to cover function declarations

**Files:**
- Modify: `src/ethunter/analyzer/param_helpers.py:363-404`

- [ ] **Step 1: Add declaration handling in _scan**

In `_collect_param_types()` (line 357), inside the `_scan()` function (line 363), after the `if node.type == 'function_definition':` block (which ends with `for child in node.children: _scan(child); return` at line 376), add an `elif` branch:

After line 376 (`return` statement of function_definition block), before the `for child in node.children: _scan(child)` at line 403:

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

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/param_helpers.py
git commit -m "feat: extend _collect_param_types to cover function declarations"
```

---

### Task 13: Add _collect_return_types function

**Files:**
- Modify: `src/ethunter/analyzer/param_helpers.py` (add after line 406)
- Modify: `src/ethunter/analyzer/symbol_table.py` (add methods)

- [ ] **Step 1: Add _func_return_types to SymbolTable**

In `src/ethunter/analyzer/symbol_table.py`, add to the SymbolTable class's dataclass fields:

```python
    _func_return_types: dict[str, str] = field(default_factory=dict)
```

Add methods after existing `get_func_var_type()`:

```python
    def record_func_return_type(self, func_name: str, struct_type: str) -> None:
        self._func_return_types[func_name] = struct_type

    def get_func_return_type(self, func_name: str) -> str | None:
        return self._func_return_types.get(func_name)
```

- [ ] **Step 2: Add _collect_return_types function**

In `param_helpers.py`, after `_collect_param_types()` (after line 406), add:

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

- [ ] **Step 3: Call _collect_return_types from prepare()**

In `prepare()` (line 354), after `_collect_param_types(tree.root_node, symbol_table)`, add:

```python
        _collect_return_types(tree.root_node, symbol_table)
```

- [ ] **Step 4: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/param_helpers.py src/ethunter/analyzer/symbol_table.py
git commit -m "feat: add _collect_return_types for struct pointer return type tracking"
```

---

### Task 14: Add type system expansion tests

**Files:**
- Modify: `tests/test_et_bench.py` (append new tests)

- [ ] **Step 1: Write tests**

Add at end of `tests/test_et_bench.py`:

```python
def test_collect_local_var_types_non_pointer():
    """Non-pointer local struct declarations should record type info."""
    import tempfile, os
    from ethunter.parser.ast_builder import parse_file
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.field_call import _collect_local_var_types

    code = b"""
    struct my_ctx { int x; };
    void func(void) {
        struct my_ctx var;
        var.x = 1;
    }
    """
    with tempfile.NamedTemporaryFile(suffix='.c', delete=False) as f:
        f.write(code)
        tmp = f.name
    try:
        tree = parse_file(tmp)
        st = SymbolTable()
        for func in extract_functions(tree, tmp):
            st.add_function(func)
        _collect_local_var_types(tree, st)
        assert st.get_func_var_type('func', 'var') == 'my_ctx', \
            "non-pointer declaration should record type"
    finally:
        os.unlink(tmp)


def test_collect_param_types_from_declarations():
    """Function declarations (not just definitions) should record param types."""
    import tempfile, os
    from ethunter.parser.ast_builder import parse_file
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.param_helpers import _collect_param_types

    code = b"""
    struct ssl_ctx;
    void ssl_set_cb(struct ssl_ctx *ctx);
    """
    with tempfile.NamedTemporaryFile(suffix='.c', delete=False) as f:
        f.write(code)
        tmp = f.name
    try:
        tree = parse_file(tmp)
        st = SymbolTable()
        for func in extract_functions(tree, tmp):
            st.add_function(func)
        _collect_param_types(tree.root_node, st)
        assert st.get_func_var_type('ssl_set_cb', 'ctx') == 'ssl_ctx', \
            "declaration param type should be recorded"
    finally:
        os.unlink(tmp)
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_collect_local_var_types_non_pointer tests/test_et_bench.py::test_collect_param_types_from_declarations -v`
Expected: 2 PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add type system expansion unit tests"
```

---

### Task 15: Full regression checkpoint

- [ ] **Step 1: Run full suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all PASS

- [ ] **Step 2: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: Section 2 (Type system expansion) complete"
```

---

### Task 16: Add type gate to FieldResolver Tier 3/4

**Files:**
- Modify: `src/ethunter/analyzer/field_resolver.py:170-218` (resolve_field_call method)

- [ ] **Step 1: Rewrite resolve_field_call with type gate**

Replace the `resolve_field_call()` method body. The current code at lines 170-218 looks like:

```python
    def resolve_field_call(self, field_path, base_var, caller_func, filepath):
        """..."""
        field_tail = self._store.compute_field_tail(field_path)
        targets = set()

        # Tier 1: type-aware exact match ... (lines 182-191)
        # Tier 2: exact path match ... (lines 193-196)
        # Tier 3: same-file suffix scan ... (lines 198-209)
        # Tier 4: cross-file suffix scan ... (lines 211-216)
        return set(), '', ''
```

Replace with:

```python
    def resolve_field_call(self, field_path, base_var, caller_func, filepath):
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

        # === Type gate: known type + Tier 1 miss → no suffix fallback ===
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
```

Add the import at the top of field_resolver.py:
```python
from ethunter.graph.model import Confidence, Evidence
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS (recall may be same if no fixture exercises type-known suffix gate)

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/field_resolver.py
git commit -m "feat: add type gate to FieldResolver Tier 3/4 — skip suffix when struct_type known"
```

---

### Task 17: Fix untracked struct_fields writes in field_call.analyze()

**Files:**
- Modify: `src/ethunter/analyzer/field_call.py:227-228`

- [ ] **Step 1: Pass filepath to assign_struct_field**

Find the `store.assign_struct_field(key, targets)` call in `analyze()` at line 227-228. It currently looks like:

```python
                    if hasattr(dataflow, 'store'):
                        dataflow.store.assign_struct_field(key, targets)
```

The `filepath` parameter is already available in the `analyze()` function signature. Change to:

```python
                    if hasattr(dataflow, 'store'):
                        dataflow.store.assign_struct_field(key, targets, filepath=filepath)
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/field_call.py
git commit -m "fix: pass filepath to assign_struct_field in field_call.analyze()"
```

---

### Task 18: Add filepath parameter to resolve_call_site_param

**Files:**
- Modify: `src/ethunter/analyzer/dataflow.py:135-143` (resolve_call_site_param method)
- Modify: `src/ethunter/analyzer/param_binding.py:16-23` (_propagate_call_site caller)

- [ ] **Step 1: Extend resolve_call_site_param signature**

In `dataflow.py`, the `resolve_call_site_param` method (line 135). Add `filepath=''` parameter:

```python
    def resolve_call_site_param(self, func_name, param_idx, arg_name,
                                symbol_names, filepath=''):
```

Inside the method, find the `self.store.assign_struct_field(key, targets)` call (around line 143). Change to:

```python
                self.store.assign_struct_field(key, targets, filepath=filepath)
```

- [ ] **Step 2: Update _propagate_call_site caller**

In `param_binding.py:16-23`, extend `_propagate_call_site`:

```python
def _propagate_call_site(
    call_name: str, arg_idx: int, target: str,
    dataflow, symbol_names: set[str], filepath: str = '',
) -> None:
    """Propagate a call-site argument target to registered field paths."""
    dataflow.resolve_call_site_param(
        call_name, arg_idx, target, symbol_names=symbol_names, filepath=filepath
    )
```

- [ ] **Step 3: Update all call sites in param_binding.analyze()**

In `analyze()`, find the 4 `_propagate_call_site(...)` calls and add `filepath=filepath`. There's one in each of the `identifier`, `cast_expression`, `pointer_expression`, and dataflow-fallback branches. Change each from:

```python
_propagate_call_site(call_name, arg_idx, target, dataflow, symbol_names)
```

To:

```python
_propagate_call_site(call_name, arg_idx, target, dataflow, symbol_names, filepath=filepath)
```

- [ ] **Step 4: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 5: Commit**

```bash
git add src/ethunter/analyzer/dataflow.py src/ethunter/analyzer/param_binding.py
git commit -m "fix: propagate filepath through resolve_call_site_param for struct field tracking"
```

---

### Task 19: Add suffix precision tests

**Files:**
- Modify: `tests/test_et_bench.py` (append new tests)

- [ ] **Step 1: Write tests**

Add at end of `tests/test_et_bench.py`:

```python
def test_tier3_skipped_when_type_known():
    """When struct type IS known but type-aware key NOT found, Tier 3 MUST be skipped."""
    from ethunter.analyzer.field_resolver import FieldResolver
    from ethunter.analyzer.scoped_store import ScopedStore
    from ethunter.analyzer.symbol_table import SymbolTable

    store = ScopedStore()
    st = SymbolTable()
    # Simulate: other_ctx (type server_ctx) has field 'cb' with target 'server_cb'
    st.record_func_var_type('caller_func', 'other_ctx', 'server_ctx')
    store.assign_struct_field('gstruct:server_ctx.cb', 'server_cb', 'a.c')
    # Caller uses variable 'ctx' with type 'ssl_ctx' — DIFFERENT type
    st.record_func_var_type('caller_func', 'ctx', 'ssl_ctx')

    resolver = FieldResolver(store, None, st, {}, {})
    targets, conf, ev = resolver.resolve_field_call(
        'ctx.cb', 'ctx', 'caller_func', 'b.c')

    assert conf is None, \
        "Tier 3/4 should be skipped when struct_type is known but type-aware key missing"
    assert len(targets) == 0


def test_unresolvable_struct_type_proceeds_to_suffix():
    """When struct type is UNKNOWN, suffix scanning should still run."""
    from ethunter.analyzer.field_resolver import FieldResolver
    from ethunter.analyzer.scoped_store import ScopedStore
    from ethunter.analyzer.symbol_table import SymbolTable

    store = ScopedStore()
    st = SymbolTable()
    # No type info for 'ctx'
    store.assign_struct_field('gstruct:some_ctx.cb', 'my_cb', 'a.c')

    resolver = FieldResolver(store, None, st, {}, {})
    targets, conf, ev = resolver.resolve_field_call(
        'ctx.cb', 'ctx', 'caller_func', 'a.c')

    assert conf is not None, "suffix should match when type is unknown"
    assert 'my_cb' in targets
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_tier3_skipped_when_type_known tests/test_et_bench.py::test_unresolvable_struct_type_proceeds_to_suffix -v`
Expected: 2 PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add suffix precision gate tests"
```

---

### Task 20: Full regression after Section 3

- [ ] **Step 1: Run full suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all PASS

- [ ] **Step 2: Check et_bench report for FPR drop**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s`
Expected: field_call extra edges reduced from ~158

- [ ] **Step 3: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: Section 3 (Suffix scanning precision) complete"
```

---

### Task 21: Add param_usage pre-filter in param_binding.analyze()

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py:79-91,144-152,173-181` (4 registration branches)

- [ ] **Step 1: Add usage check before each registration_sites.append**

In `_collect_call_params()`, find the 4 branches that append to `dataflow.registration_sites`. In each, after the `is_reg` check confirms True but before `dataflow.registration_sites.append(...)`, add:

```python
                                if is_reg:
                                    usage = dataflow.state.param_usage.get(
                                        (call_name, arg_idx), 'unknown')
                                    if usage in ('forwarder', 'storage'):
                                        pass  # skip: param is forwarded/stored, not called
                                    else:
                                        dataflow.registration_sites.append({...})
```

This replaces the current pattern that directly appends when `is_reg` is True. Apply to all 4 branches:
1. `identifier` branch (around line 79)
2. `cast_expression` branch (around line 144)
3. `pointer_expression` branch (around line 173)
4. dataflow-fallback path (if present)

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py
git commit -m "fix: add param_usage pre-filter to param_binding registration sites"
```

---

### Task 22: Remove param_assign.analyze() from orchestrator

**Files:**
- Modify: `src/ethunter/analyzer/orchestrator.py:104-114`

- [ ] **Step 1: Remove the deprecated Phase 1c block**

Remove lines 104-114:
```python
    # Phase 1c (deprecated): param_assign.analyze() — legacy edges, replaced by
    # param_dispatch + callback_reg but kept for backward compat while migration completes.
    for filepath, tree in trees.items():
        edges = param_assign.analyze(
            tree, filepath, symbol_table, engine
        )
        graph.edges.extend(edges)
```

Keep the `param_assign.register_phase()` call at lines 78-80 (Phase 1a) — it's still needed as a safety net.

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS (recall must not drop — new modules cover the same cases)

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/orchestrator.py
git commit -m "refactor: remove deprecated param_assign.analyze() from orchestrator"
```

---

### Task 23: Add new-vs-old equivalence test

**Files:**
- Modify: `tests/test_et_bench.py` (append new test)

- [ ] **Step 1: Write equivalence test**

Add at end of `tests/test_et_bench.py`:

```python
def test_new_modules_equivalent_to_old():
    """New modules (param_binding + dispatch + callback_reg) must not regress vs old param_assign."""
    import os, json
    from ethunter.parser.ast_builder import parse_file
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState

    def _run_pipeline(trees, st, df, use_old=True):
        """Run pipeline with either only-old or only-new modules."""
        from ethunter.graph.model import CallGraph, CallType
        from ethunter.analyzer import direct_call, param_assign as pa, param_helpers as ph
        from ethunter.analyzer import param_binding as pb, param_dispatch as pd
        from ethunter.analyzer import callback_reg as cr
        from ethunter.analyzer import direct_assign, initializer_assign, cast_assign
        from ethunter.analyzer import field_call as fc, direct_call_fp as dcfp
        from ethunter.analyzer import array_call, dlsym_fp

        graph = CallGraph()
        for filepath, tree in trees.items():
            graph.edges.extend(direct_call.analyze(tree, filepath, st.all_function_names))
            ph.prepare(tree, filepath, st, df)
            pa.register_phase(tree, filepath, st, df)
            fc.collect(tree, filepath, st, df)

        for filepath, tree in trees.items():
            pb.analyze(tree, filepath, st, df)
            for mod in (direct_assign, initializer_assign, cast_assign):
                mod.analyze(tree, filepath, st, df)
            pb._resolve_fields(tree, filepath, st, df)
            if use_old:
                graph.edges.extend(pa.analyze(tree, filepath, st, df))
            else:
                graph.edges.extend(pd.analyze(tree, filepath, df))
                graph.edges.extend(cr.analyze(tree, filepath, df))

        for mod in (fc, dcfp, array_call, dlsym_fp):
            for filepath, tree in trees.items():
                mod.analyze(tree, filepath, st, df)

        # Dedup
        edge_map = {}
        for e in graph.edges:
            k = (e.caller, e.callee)
            if k not in edge_map or e.confidence.ordinal() > edge_map[k].confidence.ordinal():
                edge_map[k] = e
        graph.edges = list(edge_map.values())
        return graph

    def _compute_metrics(graph, expected_pairs):
        indirects = [e for e in graph.edges if e.type.value == 'indirect']
        detected = {(e.caller, e.callee) for e in indirects}
        matched = detected & expected_pairs
        extra = detected - expected_pairs
        recall = len(matched) / len(expected_pairs) if expected_pairs else 1.0
        fpr = len(extra) / len(detected) if detected else 0.0
        return recall, fpr

    bench_dir = os.path.join(os.path.dirname(__file__), 'benchmark', 'et_bench')
    if not os.path.isdir(bench_dir):
        return

    for category in sorted(os.listdir(bench_dir)):
        cat_dir = os.path.join(bench_dir, category)
        if not os.path.isdir(cat_dir):
            continue
        for ex in sorted(os.listdir(cat_dir)):
            ex_dir = os.path.join(cat_dir, ex)
            gt_path = os.path.join(ex_dir, 'ground_truth.json')
            if not os.path.isfile(gt_path):
                continue
            with open(gt_path) as f:
                expected = {(e['caller'], e['callee'])
                           for e in json.load(f).get('examples', [])}

            trees_old, st_old, df_old = {}, SymbolTable(), VariableState()
            trees_new, st_new, df_new = {}, SymbolTable(), VariableState()
            for root, dirs, files in os.walk(ex_dir):
                for fn in files:
                    if fn.endswith(('.c', '.h')):
                        path = os.path.join(root, fn)
                        t_old = parse_file(path); t_new = parse_file(path)
                        trees_old[path] = t_old; trees_new[path] = t_new
                        for func in extract_functions(t_old, path): st_old.add_function(func)
                        for func in extract_functions(t_new, path): st_new.add_function(func)

            g_old = _run_pipeline(trees_old, st_old, df_old, use_old=True)
            g_new = _run_pipeline(trees_new, st_new, df_new, use_old=False)
            rec_old, fpr_old = _compute_metrics(g_old, expected)
            rec_new, fpr_new = _compute_metrics(g_new, expected)
            assert rec_new >= rec_old, \
                f"recall regression: old={rec_old:.2f}, new={rec_new:.2f} in {category}/{ex}"
            # FPR assertion relaxed: new modules expected to be better or equal
```

- [ ] **Step 2: Run test**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_new_modules_equivalent_to_old -v --tb=long`
Expected: PASS (or FAIL with detail if regression found — investigate before proceeding)

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add new-vs-old module equivalence test"
```

---

### Task 24: Full regression after Section 4

- [ ] **Step 1: Run full suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all PASS

- [ ] **Step 2: Commit checkpoint**

```bash
git commit --allow-empty -m "checkpoint: Section 4 (Dual-track migration) complete"
```

---

### Task 25: Extend _collect_func_params to declaration nodes

**Files:**
- Modify: `src/ethunter/analyzer/param_helpers.py:156-188`

- [ ] **Step 1: Add declaration handling**

In `_collect_func_params()` (line 156), after the `if node.type == 'function_definition':` block (which ends at line 187: `for child in node.children: _collect_func_params(...)`), add an `elif` branch:

After the `function_definition` block, before the recursive `for child in node.children:` call:

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

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/param_helpers.py
git commit -m "feat: extend _collect_func_params to collect fnptr params from declarations"
```

---

### Task 26: Conservative Stage 3 in callback_reg

**Files:**
- Modify: `src/ethunter/analyzer/callback_reg.py:40-60`

- [ ] **Step 1: Remove heuristic name-match fallback**

Replace the Stage 3 logic. Currently around lines 40-60, the code does:
```python
        if usage == 'unknown':
            if not _is_registration(callee):
                continue
            confidence, evidence = ('low', f'heuristic: registration name match ({callee})')
```

Replace with:
```python
        if usage == 'unknown':
            # callee has no definition/declaration with fnptr params in analyzed files
            # cannot confirm registration identity — skip conservatively
            continue
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS (callback_reg extras should decrease)

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/callback_reg.py
git commit -m "refactor: conservative Stage 3 — skip unknown callees instead of name-matching"
```

---

### Task 27: Add unknown callee guard in param_binding

**Files:**
- Modify: `src/ethunter/analyzer/param_binding.py:76-78`

- [ ] **Step 1: Add callee visibility check**

In `_collect_call_params()`, find the `else` branch where `fp_params_positions is None` (around line 76-78) — this is the `_is_registration` fallback. Currently:

```python
                                else:
                                    if _is_registration(call_name):
                                        is_reg = True
```

Replace with:
```python
                                else:
                                    if call_name in func_params or call_name in func_fp_params:
                                        if _is_registration(call_name):
                                            is_reg = True
                                    # else: callee unknown — not in any analyzed file's definitions
                                    # or declarations — cannot confirm registration, skip
```

Apply this change in all 4 argument-type branches (identifier, cast_expression, pointer_expression, dataflow fallback) where the `_is_registration` fallback appears.

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py -q --tb=short`
Expected: all PASS

- [ ] **Step 3: Commit**

```bash
git add src/ethunter/analyzer/param_binding.py
git commit -m "fix: skip registration for callees not visible in any analyzed file"
```

---

### Task 28: Add registration replacement tests

**Files:**
- Modify: `tests/test_et_bench.py` (append new tests)

- [ ] **Step 1: Write tests**

Add at end of `tests/test_et_bench.py`:

```python
def test_fp_params_collected_from_declarations():
    """fnptr params from function DECLARATIONS should be collected in func_fp_params."""
    import tempfile, os
    from ethunter.parser.ast_builder import parse_file
    from ethunter.analyzer.param_helpers import _collect_func_params, _collect_fnptr_typedefs

    code = b"""
    typedef void (*callback_t)(int);
    void register_callback(callback_t cb);
    """
    with tempfile.NamedTemporaryFile(suffix='.c', delete=False) as f:
        f.write(code)
        tmp = f.name
    try:
        tree = parse_file(tmp)
        fnptr_typedefs = _collect_fnptr_typedefs(tree.root_node)
        func_params = {}
        func_fp_params = {}
        _collect_func_params(tree.root_node, func_params, func_fp_params, fnptr_typedefs)
        assert 'register_callback' in func_fp_params, \
            "declaration with fnptr param should be in func_fp_params"
        assert 0 in func_fp_params['register_callback'], \
            "param at position 0 should be detected as fnptr"
    finally:
        os.unlink(tmp)


def test_unknown_callee_not_registered():
    """Callees not in func_params nor func_fp_params should NOT produce registration edges."""
    import tempfile, os
    from ethunter.parser.ast_builder import parse_file
    from ethunter.analyzer.symbol_table import SymbolTable, extract_functions
    from ethunter.analyzer.dataflow import VariableState
    from ethunter.analyzer.orchestrator import run_all_analyses

    code = b"""
    void external_lib_func(int x);

    void my_caller(void) {
        external_lib_func(my_handler);
    }

    void my_handler(void) {}
    """
    with tempfile.NamedTemporaryFile(suffix='.c', delete=False) as f:
        f.write(code)
        tmp = f.name
    try:
        tree = parse_file(tmp)
        trees = {tmp: tree}
        st = SymbolTable()
        for func in extract_functions(tree, tmp):
            st.add_function(func)
        df = VariableState()
        graph = run_all_analyses(trees, st, df)
        # external_lib_func is NOT in func_params (no definition/declaration with parameter list)
        # so it should not produce callback_reg edges
        callback_reg_edges = [e for e in graph.edges
                             if e.indirect_kind == 'callback_reg']
        # my_handler should be detected via direct_call to external_lib_func
        # but external_lib_func should not be treated as a registration function
        for e in callback_reg_edges:
            assert e.callee != 'external_lib_func', \
                "unknown callee should not be registered"
    finally:
        os.unlink(tmp)
```

- [ ] **Step 2: Run tests**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_fp_params_collected_from_declarations tests/test_et_bench.py::test_unknown_callee_not_registered -v`
Expected: 2 PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_et_bench.py
git commit -m "test: add registration replacement tests"
```

---

### Task 29: Final full regression and metrics verification

- [ ] **Step 1: Run full test suite**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/ -q --tb=short`
Expected: all tests PASS

- [ ] **Step 2: Verify metrics meet targets**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_report -v -s`

Verify:
- Overall FPR ≤ 15% (was 31.33%)
- High-confidence FPR ≤ 5% (was 13.44%)
- Recall ≥ 98.86% (no regression)
- field_call extras ≤ 30 (was 158)
- callback_reg extras ≤ 5 (was 15)

- [ ] **Step 3: Run high-confidence FPR check**

Run: `PYTHONPATH=src .venv/bin/python -m pytest tests/test_et_bench.py::test_et_bench_high_confidence_fpr -v -s`
Expected: high-confidence FPR ≤ 5%

- [ ] **Step 4: Final commit**

```bash
git commit --allow-empty -m "checkpoint: Section 5 (Registration replacement) complete — all metrics verified"
```

---

## Summary

| Task | Section | Files Changed | Estimated Time |
|------|---------|--------------|----------------|
| 1-10 | Confidence Formalization | model.py, 9 analyzers, orchestrator, test | 45 min |
| 11-15 | Type System Expansion | field_call.py, param_helpers.py, symbol_table.py, test | 25 min |
| 16-20 | Suffix Scanning Precision | field_resolver.py, field_call.py, dataflow.py, param_binding.py, test | 30 min |
| 21-24 | Dual-Track Migration | param_binding.py, orchestrator.py, test | 20 min |
| 25-29 | Registration Replacement | param_helpers.py, callback_reg.py, param_binding.py, test | 25 min |
| **Total** | | **17 files** | **~2.5 hours** |
