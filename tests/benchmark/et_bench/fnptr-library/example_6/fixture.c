/* ET-Bench fixture: fnptr-library/example_6 */
/* fnptr: mt->mem_usage2, targets: NULL */
/* Pattern: library context with optional function pointer, NULL check before call */

#include <stdio.h>
#include <stdlib.h>

typedef struct redisObject {
    int type;
    void *ptr;
} robj;

typedef struct moduleValue {
    void *type;
    void *value;
} moduleValue;

typedef struct RedisModuleKeyOptCtx {
    void *key;
    void *val;
    int dbid;
    int padding;
} RedisModuleKeyOptCtx;

typedef struct moduleType {
    size_t (*mem_usage)(void *value);
    size_t (*mem_usage2)(void *ctx, void *value, size_t sample_size);
} moduleType;

/* Concrete memory usage implementations */
size_t moduleMemUsageDefault(void *value) {
    (void)value;
    return 64;
}

size_t moduleMemUsageEnhanced(void *ctx, void *value, size_t sample_size) {
    (void)ctx; (void)value; (void)sample_size;
    return 128;
}

/* Module type with real function pointers */
moduleType moduleTypeDefault = {
    .mem_usage = moduleMemUsageDefault,
    .mem_usage2 = moduleMemUsageEnhanced,
};

size_t moduleGetMemUsage(robj *key, robj *val, size_t sample_size, int dbid) {
    moduleValue *mv = (moduleValue *)val->ptr;
    moduleType *mt = (moduleType *)mv->type;
    size_t size = 0;

    /* We prefer to use the enhanced version. */
    if (mt->mem_usage2 != NULL) {
        RedisModuleKeyOptCtx ctx = { key, NULL, dbid, -1 };
        size = mt->mem_usage2(&ctx, mv->value, sample_size);
    } else if (mt->mem_usage != NULL) {
        size = mt->mem_usage(mv->value);
    }

    return size;
}

size_t objectComputeSize(robj *key, robj *o, size_t sample_size, int dbid) {
    size_t asize = 0;

    if (o->type == 5) { /* OBJ_MODULE */
        asize = moduleGetMemUsage(key, o, sample_size, dbid);
    } else {
        fprintf(stderr, "Unknown object type\n");
    }
    return asize;
}

void memoryCommand(void *c) {
    robj key;
    robj val;
    moduleValue mv;
    mv.type = &moduleTypeDefault;
    mv.value = NULL;
    val.ptr = &mv;
    size_t usage = objectComputeSize(&key, &val, 16, 0);
    (void)c;
    (void)usage;
}
