"""Unit tests for DataflowEngine."""

import pytest
import tree_sitter_c as tsc
from tree_sitter import Language, Parser
from ethunter.analyzer.dataflow import VariableState, DataflowEngine
from ethunter.analyzer.symbol_table import SymbolTable, extract_functions


def _find_node(node, target_type):
    """Helper to find a node of a given type in the tree."""
    if node.type == target_type:
        return node
    for child in node.children:
        result = _find_node(child, target_type)
        if result:
            return result
    return None


class TestDataflowEngineBasic:
    """Backward compatibility: DataflowEngine proxies VariableState."""

    def setup_method(self):
        self.state = VariableState()
        self.engine = DataflowEngine(state=self.state)

    def test_assign_and_resolve(self):
        self.engine.assign('<gstruct:obj.cb>', 'my_handler')
        assert self.engine.resolve('<gstruct:obj.cb>') == {'my_handler'}

    def test_merge(self):
        self.engine.assign('src', 'func_a')
        self.engine.merge('src', 'dst')
        assert self.engine.resolve('dst') == {'func_a'}

    def test_targets_property(self):
        self.engine.assign('<gstruct:x>', 'fn')
        assert '<gstruct:x>' in self.engine.targets


class TestDataflowEngineParamTracker:
    """ParamTracker: register and resolve call-site param mappings."""

    def setup_method(self):
        self.engine = DataflowEngine()

    def test_register_param_mapping(self):
        self.engine.register_param_mapping(
            "SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb"
        )
        assert ("SSL_CTX_set_alpn_select_cb", 1) in self.engine.param_fields

    def test_resolve_call_site_propagates_targets(self):
        self.engine.register_param_mapping(
            "SSL_CTX_set_alpn_select_cb", 1, "ctx->ext.alpn_select_cb"
        )
        result = self.engine.resolve_call_site_param(
            "SSL_CTX_set_alpn_select_cb", 1, "alpn_cb",
            symbol_names={"alpn_cb"}
        )
        assert "alpn_cb" in result
        assert self.engine.resolve('<gstruct:ctx->ext.alpn_select_cb>') == {'alpn_cb'}

    def test_resolve_call_site_bare_function(self):
        """resolve_call_site_param recognizes bare function names not in dataflow."""
        self.engine.register_param_mapping(
            "register_callback", 0, "ctx->on_event"
        )
        result = self.engine.resolve_call_site_param(
            "register_callback", 0, "on_init",
            symbol_names={"on_init", "other_fn"}
        )
        assert "on_init" in result

    def test_resolve_call_site_no_mapping(self):
        result = self.engine.resolve_call_site_param("unknown_fn", 0, "arg")
        assert result == set()


class TestDataflowEngineRetTracker:
    """RetTracker: register and resolve return value tracking."""

    def setup_method(self):
        self.engine = DataflowEngine()

    def test_register_return(self):
        self.engine.register_return(
            "SSL_CTX_get_security_callback", "cert->sec_cb"
        )
        assert "SSL_CTX_get_security_callback" in self.engine.ret_fields

    def test_resolve_returned_field(self):
        self.engine.register_return(
            "SSL_CTX_get_security_callback", "cert->sec_cb"
        )
        self.engine.assign('<gstruct:cert->sec_cb>', 'ssl_security_default')
        result = self.engine.resolve_returned_field("SSL_CTX_get_security_callback")
        assert 'ssl_security_default' in result

    def test_resolve_returned_field_no_register(self):
        result = self.engine.resolve_returned_field("unknown_fn")
        assert result == set()


class TestDataflowEngineCastResolver:
    """CastResolver: unwrap nested cast expressions."""

    def setup_method(self):
        self.engine = DataflowEngine()

    def test_unwrap_cast_simple_identifier(self):
        lang = Language(tsc.language())
        parser = Parser(lang)
        tree = parser.parse(b'void fn() { (block128_f)aesni_encrypt; }')
        cast_node = _find_node(tree.root_node, 'cast_expression')
        assert cast_node is not None
        result = self.engine.unwrap_cast(cast_node)
        assert result == 'aesni_encrypt'

    def test_unwrap_cast_nested(self):
        """(T1)(T2)func -> func."""
        lang = Language(tsc.language())
        parser = Parser(lang)
        tree = parser.parse(b'void fn() { (unflushed_iter_fn_t *)(uintptr_t)cb; }')
        cast_node = _find_node(tree.root_node, 'cast_expression')
        assert cast_node is not None
        result = self.engine.unwrap_cast(cast_node)
        assert result == 'cb'

    def test_unwrap_cast_returns_none_for_non_cast(self):
        """Non-cast node -> None."""
        result = self.engine.unwrap_cast(type('FakeNode', (), {'type': 'binary_expression'})())
        assert result is None


class TestFieldCallTwoPass:
    """field_call two-pass: assignments collected before call detection."""

    def test_assignment_after_call_still_detected(self):
        """When field assignment appears after call site in source, edge is still found."""
        from ethunter.analyzer import field_call

        lang = Language(tsc.language())
        parser = Parser(lang)

        # Call site BEFORE assignment in source order
        source = b'''
void handler(void) {}
void caller(void) {
    obj.cb();
}
void init(void) {
    obj.cb = handler;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        edges = field_call.analyze(tree, 'test.c', st, df)

        callers_callees = {(e.caller, e.callee) for e in edges}
        assert ('caller', 'handler') in callers_callees


class TestHasattrDowngrade:
    """Verify analyzers work correctly when passed VariableState instead of DataflowEngine."""

    def test_variable_state_has_no_new_methods(self):
        """VariableState does not have DataflowEngine methods — hasattr checks should return False."""
        vs = VariableState()
        assert not hasattr(vs, 'unwrap_cast')
        assert not hasattr(vs, 'register_param_mapping')
        assert not hasattr(vs, 'resolve_call_site_param')
        assert not hasattr(vs, 'register_return')
        assert not hasattr(vs, 'resolve_returned_field')

    def test_engine_has_all_methods(self):
        """DataflowEngine has all expected methods."""
        eng = DataflowEngine()
        assert hasattr(eng, 'unwrap_cast')
        assert hasattr(eng, 'register_param_mapping')
        assert hasattr(eng, 'resolve_call_site_param')
        assert hasattr(eng, 'register_return')
        assert hasattr(eng, 'resolve_returned_field')
