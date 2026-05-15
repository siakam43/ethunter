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

    def test_engine_has_all_methods(self):
        """DataflowEngine has all expected methods."""
        eng = DataflowEngine()
        assert hasattr(eng, 'unwrap_cast')
        assert hasattr(eng, 'register_param_mapping')
        assert hasattr(eng, 'resolve_call_site_param')
        assert hasattr(eng, 'register_return')
        assert hasattr(eng, 'resolve_returned_field')


class TestInitializerAssignUnwrapCast:
    """initializer_assign with nested cast in designated initializer."""

    def test_nested_cast_in_designated_initializer(self):
        """.field = (T1)(T2)func should extract func as target (truly nested cast)."""
        from ethunter.analyzer import initializer_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void my_handler(void) {}
typedef struct { void (*cb)(void); } ops_t;
void init(void) {
    ops_t o = { .cb = (void (*)(void))(unsigned long)my_handler };
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        initializer_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<gstruct:o.cb>')
        assert 'my_handler' in targets

    def test_variable_state_still_works(self):
        """When VariableState is passed (not DataflowEngine), existing behavior is preserved."""
        from ethunter.analyzer import initializer_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void my_handler(void) {}
void init(void) {
    ops_t o = { .cb = my_handler };
}
typedef struct { void (*cb)(void); } ops_t;
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        vs = DataflowEngine(state=VariableState())
        initializer_assign.analyze(tree, 'test.c', st, vs)

        targets = vs.resolve('<gstruct:o.cb>')
        assert 'my_handler' in targets


class TestParamAssignRegistration:
    """param_assign registers param->field mappings and return value tracking."""

    def test_register_param_to_field_mapping(self):
        """ctx->ext.alpn_select_cb = cb -> register_param_mapping called."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void alpn_cb(void *ctx) {}
void SSL_CTX_set_alpn_select_cb(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        assert ("SSL_CTX_set_alpn_select_cb", 1) in df.param_fields

    def test_register_return_from_field_expression(self):
        """return ctx->cert->sec_cb -> register_return called."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void *ssl_security_default_callback(void) { return NULL; }
void *SSL_CTX_get_security_callback(void *ctx) {
    return ctx->cert->sec_cb;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        assert "SSL_CTX_get_security_callback" in df.ret_fields



class TestParamAssignCallSitePropagation:
    """param_assign propagates targets at call sites and handles cast args."""

    def test_call_site_propagates_bare_function_to_field(self):
        """custom_assign_alpn(ctx, alpn_cb) -> field gets alpn_cb target.

        Uses the single-file param_mappings chain: call-site arg → param_name → struct field.
        The cross-file param_fields chain (resolve_call_site_param) is tested in
        test_cross_file_param_registration (Task 7).

        NOTE: Function name must NOT match any REG_PATTERNS (set_, register_, etc.)
        or it will be treated as a registration call instead of populating param_mappings.
        """
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void alpn_cb(void *ctx) {}
void custom_assign_alpn(void *ctx, void (*cb)(void)) {
    ctx->ext.alpn_select_cb = cb;
}
void s_server_main(void) {
    custom_assign_alpn(ctx, alpn_cb);
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<struct:ctx.ext.alpn_select_cb>')
        assert 'alpn_cb' in targets

    def test_call_site_cast_wrapped_arg(self):
        """CRYPTO_gcm128_init(..., (block128_f)aesni_encrypt) -> extracts aesni_encrypt."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void aesni_encrypt(void *ctx) {}
void CRYPTO_gcm128_init(void *ctx, void *key, void (*block)(void *k)) {
    ctx->block = block;
}
void aesni_gcm_init_key(void) {
    CRYPTO_gcm128_init(&gctx, &ks, (void (*)(void *))aesni_encrypt);
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<struct:ctx.block>')
        assert 'aesni_encrypt' in targets

    def test_rhs_call_expression_assignment(self):
        """sdb.old_cb = SSL_CTX_get_security_callback(ctx) -> resolves via ret_fields."""
        from ethunter.analyzer import param_assign

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void ssl_security_default_callback(void) {}
void *SSL_CTX_get_security_callback(void *ctx) {
    return ctx->cert->sec_cb;
}
void ssl_ctx_security_debug(void *ctx) {
    struct { void (*old_cb)(void); } sdb;
    sdb.old_cb = SSL_CTX_get_security_callback(ctx);
}
void setup(void *ctx) {
    ctx->cert->sec_cb = ssl_security_default_callback;
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        # Pre-populate dataflow as field_call would:
        # setup() writes ctx->cert->sec_cb = ssl_security_default_callback
        df.assign('<gstruct:ctx.cert.sec_cb>', 'ssl_security_default_callback')
        param_assign.analyze(tree, 'test.c', st, df)

        targets = df.resolve('<gstruct:sdb.old_cb>')
        assert 'ssl_security_default_callback' in targets

    def test_example_13_chain_through_local_fp(self):
        """End-to-end: param_assign -> dataflow -> local_fp_tracker -> direct_call_fp.

        Verifies the full chain for example_13:
        1. param_assign extracts aesni_encrypt from cast arg
        2. param_assign registers ctx->block = aesni_encrypt
        3. local_fp_tracker reads <struct:ctx->block> -> {aesni_encrypt}
        """
        from ethunter.analyzer import param_assign
        from ethunter.analyzer.local_fp_tracker import collect_local_fp_assignments

        lang = Language(tsc.language())
        parser = Parser(lang)

        source = b'''
void aesni_encrypt(void *ctx) {}
void CRYPTO_gcm128_init(void *ctx, void *key, void (*block)(void *k)) {
    ctx->block = block;
}
void CRYPTO_gcm128_encrypt(void *ctx) {
    void (*block)(void *k) = ctx->block;
    (*block)(ctx);
}
void aesni_gcm_init_key(void) {
    CRYPTO_gcm128_init(&gctx, &ks, (void (*)(void *))aesni_encrypt);
}
'''
        tree = parser.parse(source)

        st = SymbolTable()
        for func in extract_functions(tree, 'test.c'):
            st.add_function(func)

        df = DataflowEngine()
        param_assign.analyze(tree, 'test.c', st, df)

        # Step 1+2: dataflow has ctx.block mapped
        assert 'aesni_encrypt' in df.resolve('<struct:ctx.block>')

        # Step 3: local_fp_tracker can read it
        symbol_names = st.all_function_names
        local_mapping = collect_local_fp_assignments(tree, df, symbol_names)
        assert 'block' in local_mapping
        assert 'aesni_encrypt' in local_mapping['block']
