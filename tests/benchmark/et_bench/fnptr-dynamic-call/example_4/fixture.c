/* ET-Bench fixture: fnptr-dynamic-call/example_4 */
/* fnptr: ctx->bind_engine, targets: bind_engine */
/* Pattern: dlopen/dlsym, function pointer in struct context, resolved then called */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef int dynamic_bind_engine_fn(void *e, const char *id, void *fns);

typedef struct {
    void *dynamic_dso;
    char *DYNAMIC_LIBNAME;
    const char *engine_id;
    const char *DYNAMIC_F1;
    const char *DYNAMIC_F2;
    int no_vcheck;
    int dir_load;
    void *dirs;
    int list_add_value;
    /* Function pointer resolved via dlsym */
    dynamic_bind_engine_fn *bind_engine;
    unsigned long (*v_check)(unsigned long);
} dynamic_data_ctx;

typedef struct {
    void *meth;
} DSO;

/* Internal load helper */
static int int_load(dynamic_data_ctx *ctx)
{
    if (!ctx->DYNAMIC_LIBNAME || !ctx->dynamic_dso)
        return 0;
    return 1; /* simplified */
}

static int dynamic_load(dynamic_data_ctx *ctx, DSO *dso)
{
    if (ctx->dynamic_dso == NULL)
        ctx->dynamic_dso = (DSO *)0x1; /* placeholder */
    if (ctx->dynamic_dso == NULL)
        return 0;

    if (!ctx->DYNAMIC_LIBNAME) {
        if (!ctx->engine_id)
            return 0;
        ctx->DYNAMIC_LIBNAME = (char *)ctx->engine_id;
    }

    if (!int_load(ctx)) {
        ctx->dynamic_dso = NULL;
        return 0;
    }

    /* Resolve bind_engine via dlsym */
    if (!(ctx->bind_engine =
         (dynamic_bind_engine_fn *)dlsym(ctx->dynamic_dso,
                                         ctx->DYNAMIC_F2))) {
        ctx->bind_engine = NULL;
        ctx->dynamic_dso = NULL;
        return 0;
    }

    /* Version check if enabled */
    if (!ctx->no_vcheck) {
        unsigned long vcheck_res = 0;
        ctx->v_check = (unsigned long (*)(unsigned long))
            dlsym(ctx->dynamic_dso, ctx->DYNAMIC_F1);
        if (ctx->v_check)
            vcheck_res = ctx->v_check(100);
        if (vcheck_res < 10) {
            ctx->bind_engine = NULL;
            ctx->v_check = NULL;
            ctx->dynamic_dso = NULL;
            return 0;
        }
    }

    /* Try to bind the engine onto our structure */
    if (!ctx->bind_engine(NULL, ctx->engine_id, NULL)) {
        ctx->bind_engine = NULL;
        ctx->v_check = NULL;
        ctx->dynamic_dso = NULL;
        return 0;
    }

    return 1;
}

void engine_load_dynamic_int(void)
{
    dynamic_data_ctx ctx;
    DSO dso;
    memset(&ctx, 0, sizeof(ctx));
    memset(&dso, 0, sizeof(dso));

    ctx.DYNAMIC_F1 = "v_check";
    ctx.DYNAMIC_F2 = "bind_engine";
    ctx.dir_load = 1;

    dynamic_load(&ctx, &dso);
}

/* Target: resolved via dlsym */
int bind_engine(void *e, const char *id, void *fns) {
    (void)e; (void)id; (void)fns;
    return 1;
}
