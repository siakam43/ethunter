"""Unit tests for FieldResolver and ResolutionStrategy classes."""
import pytest
from ethunter.analyzer.scoped_store import ScopedStore
from ethunter.analyzer.field_resolver import (
    ResolutionContext, TypeAwareStructLookup, ExactPathStructLookup,
    GlobalArrayLookup, FieldResolver,
)


class FakeSymbolTable:
    def __init__(self, types=None):
        self._types = types or {}

    def get_func_var_type(self, func, var):
        return self._types.get((func, var))

    def get_var_type(self, var):
        return None


class FakeDataflowEngine:
    def __init__(self, store, param_alias_map=None):
        self.store = store
        self.param_alias_map = param_alias_map or {}


class TestTypeAwareStructLookup:
    def test_matches_type_aware_key(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        sym = FakeSymbolTable({("caller", "h"): "my_type"})
        strategy = TypeAwareStructLookup(store, sym)
        ctx = ResolutionContext(field_path="h.cb", base_var="h", caller_func="caller")
        assert strategy.resolve(ctx) == {"handler_a"}

    def test_no_type_info_returns_empty(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        sym = FakeSymbolTable()
        strategy = TypeAwareStructLookup(store, sym)
        ctx = ResolutionContext(field_path="h.cb", base_var="h", caller_func="caller")
        assert strategy.resolve(ctx) == set()

    def test_wrong_type_returns_empty(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:other_type.cb", "handler_a")
        sym = FakeSymbolTable({("caller", "h"): "my_type"})
        strategy = TypeAwareStructLookup(store, sym)
        ctx = ResolutionContext(field_path="h.cb", base_var="h", caller_func="caller")
        assert strategy.resolve(ctx) == set()


class TestExactPathStructLookup:
    def test_matches_exact_var_name(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a")
        strategy = ExactPathStructLookup(store)
        ctx = ResolutionContext(field_path="handler.cb", base_var="handler")
        assert strategy.resolve(ctx) == {"handler_a"}

    def test_different_var_name_returns_empty(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a")
        strategy = ExactPathStructLookup(store)
        ctx = ResolutionContext(field_path="h.cb", base_var="h")
        assert strategy.resolve(ctx) == set()

    def test_chained_field(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:ctx.ext.alpn_select_cb", "cb_func")
        strategy = ExactPathStructLookup(store)
        ctx = ResolutionContext(field_path="ctx.ext.alpn_select_cb", base_var="ctx")
        assert strategy.resolve(ctx) == {"cb_func"}


class TestGlobalArrayLookup:
    def test_matches_global_array(self):
        store = ScopedStore()
        store.assign_global_array("hooks", "hook_a")
        strategy = GlobalArrayLookup(store)
        ctx = ResolutionContext(field_path="hooks.dispatch", base_var="hooks")
        assert strategy.resolve(ctx) == {"hook_a"}

    def test_no_match_returns_empty(self):
        store = ScopedStore()
        strategy = GlobalArrayLookup(store)
        ctx = ResolutionContext(field_path="hooks.dispatch", base_var="hooks")
        assert strategy.resolve(ctx) == set()


class TestFieldResolver:
    def test_resolves_via_first_matching_strategy(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        store.assign_struct_field("gstruct:h.cb", "handler_b")
        sym = FakeSymbolTable({("caller", "h"): "my_type"})
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        # TypeAwareStructLookup should match first
        targets = resolver.resolve("h.cb", "h", "caller")
        assert targets == {"handler_a"}

    def test_falls_back_to_exact_when_no_type(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:h.cb", "handler_b")
        sym = FakeSymbolTable()  # no type info
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets = resolver.resolve("h.cb", "h", "caller")
        assert targets == {"handler_b"}

    def test_returns_empty_when_no_match(self):
        store = ScopedStore()
        sym = FakeSymbolTable()
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets = resolver.resolve("unknown.field", "unknown", "caller")
        assert targets == set()


class TestResolveFieldCall:
    """Tests for the 4-tier resolve_field_call method."""

    def test_tier1_type_aware_match(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        sym = FakeSymbolTable({("caller", "obj"): "my_type"})
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets, conf, ev = resolver.resolve_field_call(
            "obj.cb", "obj", "caller", "fixture.c")
        assert targets == {"handler_a"}
        assert conf == 'high'
        assert 'type-aware' in ev

    def test_tier2_exact_path_when_no_type(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a")
        sym = FakeSymbolTable()  # no type
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets, conf, ev = resolver.resolve_field_call(
            "handler.cb", "handler", "caller", "fixture.c")
        assert targets == {"handler_a"}
        assert conf == 'high'
        assert 'exact path' in ev

    def test_tier3_same_file_suffix(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a", "fixture.c")
        store.assign_struct_field("gstruct:other_type.cb", "handler_b", "other.c")
        sym = FakeSymbolTable()
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets, conf, ev = resolver.resolve_field_call(
            "obj.cb", "obj", "caller", "fixture.c")
        assert targets == {"handler_a"}
        assert "handler_b" not in targets
        assert conf == 'medium'
        assert 'same-file' in ev

    def test_tier4_cross_file_suffix(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "handler_a", "other.c")
        sym = FakeSymbolTable()
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets, conf, ev = resolver.resolve_field_call(
            "obj.cb", "obj", "caller", "fixture.c")
        assert targets == {"handler_a"}
        assert conf == 'low'
        assert 'cross-file' in ev

    def test_returns_empty_when_no_match(self):
        store = ScopedStore()
        sym = FakeSymbolTable()
        resolver = FieldResolver(store, FakeDataflowEngine(store), sym, {}, {})
        targets, conf, ev = resolver.resolve_field_call(
            "x.y", "x", "func", "f.c")
        assert targets == set()
        assert conf == 'none'
