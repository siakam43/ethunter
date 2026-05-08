/* CG-Bench fixture: fnptr-struct/example_7 */
/* fnptr: engine->get_engine_memory_overhead, targets: luaEngineMemoryOverhead */

int functionsRegisterEngine(const char *engine_name, engine *engine) {
    sds engine_name_sds = sdsnew(engine_name);
    if (dictFetchValue(engines, engine_name_sds)) {
        serverLog(LL_WARNING, "Same engine was registered twice");
        sdsfree(engine_name_sds);
        return C_ERR;
    }

    client *c = createClient(NULL);
    c->flags |= (CLIENT_DENY_BLOCKING | CLIENT_SCRIPT);
    engineInfo *ei = zmalloc(sizeof(*ei));
    *ei = (engineInfo ) { .name = engine_name_sds, .engine = engine, .c = c,};

    dictAdd(engines, engine_name_sds, ei);

    engine_cache_memory += zmalloc_size(ei) + sdsZmallocSize(ei->name) +
            zmalloc_size(engine) +
            engine->get_engine_memory_overhead(engine->engine_ctx);

    return C_OK;
}

int luaEngineInitEngine(void) {
    luaEngineCtx *lua_engine_ctx = zmalloc(sizeof(*lua_engine_ctx));
    lua_engine_ctx->lua = lua_open();

    ...


    engine *lua_engine = zmalloc(sizeof(*lua_engine));
    *lua_engine = (engine) {
        .engine_ctx = lua_engine_ctx,
        .create = luaEngineCreate,
        .call = luaEngineCall,
        .get_used_memory = luaEngineGetUsedMemoy,
        .get_function_memory_overhead = luaEngineFunctionMemoryOverhead,
        .get_engine_memory_overhead = luaEngineMemoryOverhead,
        .free_function = luaEngineFreeFunction,
    };
    return functionsRegisterEngine(LUA_ENGINE_NAME, lua_engine);
}


/* Stub implementation for luaEngineMemoryOverhead */
void luaEngineMemoryOverhead(void) {}
