/* ET-Bench fixture: fnptr-dynamic-call/example_1 */
/* fnptr: onload, targets: RedisModule_OnLoad */
/* Pattern: dlopen/dlsym, function pointer resolved at runtime then called */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#define REDISMODULE_ERR -1
#define REDISMODULE_OK 0
#define LL_WARNING 4

void serverLog(int level, const char *fmt, ...);
typedef struct { int argc; } redisModuleCtx;
void moduleCreateContext(redisModuleCtx *ctx, void *mod, int flags);
void moduleFreeContext(redisModuleCtx *ctx);
void moduleUnregisterCommands(void *mod);
void moduleUnregisterSharedAPI(void *mod);
void moduleUnregisterUsedAPI(void *mod);
void moduleRemoveConfigs(void *mod);
void moduleFreeModuleStructure(void *mod);
void moduleUnregisterAuthCBs(void *mod);

#define C_ERR -1

int loadModule(const char *path, void **module_argv, int module_argc) {
    void *handle;
    int (*onload)(void *, void **, int);

    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        serverLog(LL_WARNING, "Module %s failed to load: %s", path, dlerror());
        return C_ERR;
    }

    onload = (int (*)(void *, void **, int))(unsigned long)dlsym(handle, "RedisModule_OnLoad");
    if (onload == NULL) {
        dlclose(handle);
        serverLog(LL_WARNING,
            "Module %s does not export RedisModule_OnLoad() symbol. Module not loaded.", path);
        return C_ERR;
    }

    redisModuleCtx ctx;
    moduleCreateContext(&ctx, NULL, 0);

    if (onload((void *)&ctx, module_argv, module_argc) == REDISMODULE_ERR) {
        serverLog(LL_WARNING,
            "Module %s initialization failed. Module not loaded", path);
        if (ctx.argc) {
            moduleUnregisterCommands(NULL);
            moduleUnregisterSharedAPI(NULL);
            moduleUnregisterUsedAPI(NULL);
            moduleRemoveConfigs(NULL);
            moduleFreeModuleStructure(NULL);
        }
        moduleFreeContext(&ctx);
        dlclose(handle);
        return C_ERR;
    }

    return 0;
}

void serverLog(int level, const char *fmt, ...) {
    (void)level;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void moduleCreateContext(redisModuleCtx *ctx, void *mod, int flags) {
    (void)mod; (void)flags;
    ctx->argc = 0;
}
void moduleFreeContext(redisModuleCtx *ctx) { (void)ctx; }
void moduleUnregisterCommands(void *mod) { (void)mod; }
void moduleUnregisterSharedAPI(void *mod) { (void)mod; }
void moduleUnregisterUsedAPI(void *mod) { (void)mod; }
void moduleRemoveConfigs(void *mod) { (void)mod; }
void moduleFreeModuleStructure(void *mod) { (void)mod; }
void moduleUnregisterAuthCBs(void *mod) { (void)mod; }

/* Wrapper: calls through onload */
int onload_caller(void *ctx, void **argv, int argc) {
    int (*onload)(void *, void **, int) = (int (*)(void *, void **, int))(unsigned long)dlsym(NULL, "RedisModule_OnLoad");
    if (onload) {
        return onload(ctx, argv, argc);
    }
    return -1;
}

/* Target: resolved via dlsym */
int RedisModule_OnLoad(void *ctx, void **argv, int argc) {
    (void)ctx; (void)argv; (void)argc;
    return 0;
}
