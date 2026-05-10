/* ET-Bench fixture: fnptr-struct/example_7 */
/* Scenario: Redis functions engine — engine memory overhead callback.
   fnptr: engine->get_engine_memory_overhead
   target: luaEngineMemoryOverhead
   caller: functionsRegisterEngine */

#include <stddef.h>
#include <stdlib.h>

#define C_OK 0
#define C_ERR 1
#define LL_WARNING 3

typedef char *sds;
typedef struct dict dict;

typedef struct client {
    int flags;
} client;

typedef struct engine {
    void *engine_ctx;
    size_t (*get_engine_memory_overhead)(void *ctx);
    size_t (*get_function_memory_overhead)(void *ctx, void *func);
    size_t (*get_used_memory)(void *ctx);
    void *(*create)(void *ctx, const char *name, const char *code);
    void (*call)(void *ctx, void *func, client *c);
    void (*free_function)(void *ctx, void *func);
} engine;

typedef struct engineInfo {
    sds name;
    engine *engine;
    client *c;
} engineInfo;

typedef struct luaEngineCtx {
    void *lua;
} luaEngineCtx;

static size_t engine_cache_memory = 0;
static dict *engines = NULL;

static void *dictFetchValue(dict *d, sds key) { (void)d; (void)key; return NULL; }
static sds sdsnew(const char *s) { return (sds)(void *)s; }
static void sdsfree(sds s) { (void)s; }
static void serverLog(int level, const char *fmt, ...) { (void)level; (void)fmt; }
static void dictAdd(dict *d, sds key, engineInfo *ei) { (void)d; (void)key; (void)ei; }
static size_t zmalloc_size(void *p) { (void)p; return 64; }
static size_t sdsZmallocSize(sds s) { (void)s; return 16; }
static client *createClient(void *fd) { (void)fd; return NULL; }
static void *zmalloc(size_t sz) { return malloc(sz); }
#define CLIENT_DENY_BLOCKING 0x1
#define CLIENT_SCRIPT 0x2

/* Target: lua engine memory overhead calculation */
static size_t luaEngineMemoryOverhead(void *ctx)
{
    luaEngineCtx *lctx = (luaEngineCtx *)ctx;
    if (lctx == NULL)
        return 0;
    return sizeof(luaEngineCtx);
}

static void *luaEngineCreate(void *ctx, const char *name, const char *code) {
    (void)ctx; (void)name; (void)code; return NULL;
}

static void luaEngineCall(void *ctx, void *func, client *c) {
    (void)ctx; (void)func; (void)c;
}

static size_t luaEngineGetUsedMemory(void *ctx) {
    (void)ctx; return 0;
}

static size_t luaEngineFunctionMemoryOverhead(void *ctx, void *func) {
    (void)ctx; (void)func; return 0;
}

static void luaEngineFreeFunction(void *ctx, void *func) {
    (void)ctx; (void)func;
}

/* Caller: invokes engine->get_engine_memory_overhead through the struct */
int functionsRegisterEngine(const char *engine_name, engine *engine_ptr) {
    sds engine_name_sds = sdsnew(engine_name);
    if (dictFetchValue(engines, engine_name_sds)) {
        serverLog(LL_WARNING, "Same engine was registered twice");
        sdsfree(engine_name_sds);
        return C_ERR;
    }

    client *c = createClient(NULL);
    c->flags |= (CLIENT_DENY_BLOCKING | CLIENT_SCRIPT);
    engineInfo *ei = zmalloc(sizeof(*ei));
    ei->name = engine_name_sds;
    ei->engine = engine_ptr;
    ei->c = c;

    dictAdd(engines, engine_name_sds, ei);

    engine_cache_memory += zmalloc_size(ei) + sdsZmallocSize(ei->name) +
            zmalloc_size(engine_ptr) +
            engine_ptr->get_engine_memory_overhead(engine_ptr->engine_ctx);

    return C_OK;
}

int luaEngineInitEngine(void) {
    luaEngineCtx *lua_engine_ctx = zmalloc(sizeof(*lua_engine_ctx));
    lua_engine_ctx->lua = NULL;

    engine *lua_engine = zmalloc(sizeof(*lua_engine));
    lua_engine->engine_ctx = lua_engine_ctx;
    lua_engine->create = luaEngineCreate;
    lua_engine->call = luaEngineCall;
    lua_engine->get_used_memory = luaEngineGetUsedMemory;
    lua_engine->get_function_memory_overhead = luaEngineFunctionMemoryOverhead;
    lua_engine->get_engine_memory_overhead = luaEngineMemoryOverhead;
    lua_engine->free_function = luaEngineFreeFunction;
    return functionsRegisterEngine("Lua", lua_engine);
}
