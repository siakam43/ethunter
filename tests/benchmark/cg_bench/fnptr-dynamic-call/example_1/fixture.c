/* CG-Bench fixture: fnptr-dynamic-call/example_1 */
/* fnptr: onload, targets: RedisModule_OnLoad */

handle = dlopen(path,RTLD_NOW|RTLD_LOCAL);
if (handle == NULL) {
    serverLog(LL_WARNING, "Module %s failed to load: %s", path, dlerror());
    return C_ERR;
}
onload = (int (*)(void *, void **, int))(unsigned long) dlsym(handle,"RedisModule_OnLoad");
if (onload == NULL) {
    dlclose(handle);
    serverLog(LL_WARNING,
        "Module %s does not export RedisModule_OnLoad() "
        "symbol. Module not loaded.",path);
    return C_ERR;
}
RedisModuleCtx ctx;
moduleCreateContext(&ctx, NULL, REDISMODULE_CTX_TEMP_CLIENT); /* We pass NULL since we don't have a module yet. */
if (onload((void*)&ctx,module_argv,module_argc) == REDISMODULE_ERR) {
    serverLog(LL_WARNING,
        "Module %s initialization failed. Module not loaded",path);
    if (ctx.module) {
        moduleUnregisterCommands(ctx.module);
        moduleUnregisterSharedAPI(ctx.module);
        moduleUnregisterUsedAPI(ctx.module);
        moduleRemoveConfigs(ctx.module);
        moduleUnregisterAuthCBs(ctx.module);
        moduleFreeModuleStructure(ctx.module);
    }
    moduleFreeContext(&ctx);
    dlclose(handle);
    return C_ERR;
}


/* Wrapper: calls through onload */
void onload_caller(void) {
    onload();
}



/* Stub implementation for RedisModule_OnLoad */
void RedisModule_OnLoad() {}
