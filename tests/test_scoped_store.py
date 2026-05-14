"""Unit tests for ScopedStore."""
import pytest
from ethunter.analyzer.scoped_store import ScopedStore


class TestFuncVars:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_func_var("my_func", "cb", "handler_a")
        store.assign_func_var("my_func", "cb", "handler_b")
        assert store.resolve_func_var("my_func", "cb") == {"handler_a", "handler_b"}

    def test_different_funcs_isolated(self):
        store = ScopedStore()
        store.assign_func_var("func1", "cb", "handler_a")
        store.assign_func_var("func2", "cb", "handler_b")
        assert store.resolve_func_var("func1", "cb") == {"handler_a"}
        assert store.resolve_func_var("func2", "cb") == {"handler_b"}

    def test_unresolved_returns_empty(self):
        store = ScopedStore()
        assert store.resolve_func_var("nonexistent", "x") == set()


class TestStructFields:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:my_type.cb", "handler_a")
        assert store.resolve_struct_field("gstruct:my_type.cb") == {"handler_a"}

    def test_unresolved_returns_empty(self):
        store = ScopedStore()
        assert store.resolve_struct_field("gstruct:nonexistent.field") == set()


class TestGlobalArrays:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_global_array("global_hooks", "hook_a")
        assert store.resolve_global_array("global_hooks") == {"hook_a"}


class TestVtableEntries:
    def test_assign_and_resolve(self):
        store = ScopedStore()
        store.assign_vtable_entry("my_vtable", "get_state", "state_impl")
        assert store.resolve_vtable_entry("my_vtable", "get_state") == {"state_impl"}


class TestFieldTail:
    def test_simple_field(self):
        store = ScopedStore()
        assert store.compute_field_tail("handler.cb") == "cb"

    def test_chained_field(self):
        store = ScopedStore()
        assert store.compute_field_tail("ctx.ext.alpn_select_cb") == "ext.alpn_select_cb"

    def test_no_dot(self):
        store = ScopedStore()
        assert store.compute_field_tail("cb") == "cb"

    def test_single_dot_at_end(self):
        store = ScopedStore()
        assert store.compute_field_tail("a.") == ""


class TestStructFieldFiles:
    def test_records_filepath(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:handler.cb", "func_a", "fixture.c")
        assert "fixture.c" in store.struct_field_files["gstruct:handler.cb"]

    def test_multiple_targets_same_file(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:h.cb", "func_a", "callee.c")
        store.assign_struct_field("gstruct:h.cb", "func_b", "callee.c")
        assert store.struct_field_files["gstruct:h.cb"] == {"callee.c"}

    def test_no_filepath(self):
        store = ScopedStore()
        store.assign_struct_field("gstruct:h.cb", "func_a")
        assert "gstruct:h.cb" not in store.struct_field_files
