/* ET-Bench fixture: fnptr-dynamic-call/example_3 */
/* fnptr: omx_context->ptr_Init, targets: OMX_Init */
/* Pattern: dlopen/dlsym, function pointer in context struct, called after resolution */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>

typedef int OMX_ERRORTYPE;
typedef void *OMX_HANDLETYPE;

#define AVERROR_ENCODER_NOT_FOUND (-1)

struct OMXContext {
    void *lib;
    void *host_init;
    OMX_ERRORTYPE (*ptr_Init)(void);
};

static void *dlsym_prefixed(void *handle, const char *symbol, const char *prefix)
{
    char buf[256];
    if (prefix)
        snprintf(buf, sizeof(buf), "%s%s", prefix, symbol);
    else
        snprintf(buf, sizeof(buf), "%s", symbol);
    return dlsym(handle, buf);
}

static int omx_try_load(struct OMXContext *s, void *logctx,
                        const char *libname, const char *prefix,
                        const char *libname2)
{
    (void)logctx; (void)libname2;
    s->lib = dlopen(libname, RTLD_NOW | RTLD_LOCAL);
    if (!s->lib)
        return AVERROR_ENCODER_NOT_FOUND;
    s->ptr_Init = (OMX_ERRORTYPE (*)(void))dlsym_prefixed(s->lib, "OMX_Init", prefix);
    if (!s->ptr_Init)
        return AVERROR_ENCODER_NOT_FOUND;
    return 0;
}

static struct OMXContext *omx_init(void *logctx, const char *libname, const char *prefix)
{
    static const char * const libnames[] = {
        "libOMX_Core.so", NULL,
        "libOmxCore.so", NULL,
        NULL
    };
    const char * const *nameptr;
    int ret = AVERROR_ENCODER_NOT_FOUND;
    struct OMXContext *omx_context;

    omx_context = (struct OMXContext *)calloc(1, sizeof(*omx_context));
    if (!omx_context)
        return NULL;

    if (libname) {
        ret = omx_try_load(omx_context, logctx, libname, prefix, NULL);
        if (ret < 0) {
            free(omx_context);
            return NULL;
        }
    } else {
        for (nameptr = libnames; *nameptr; nameptr += 2)
            if (!(ret = omx_try_load(omx_context, logctx, nameptr[0], prefix, nameptr[1])))
                break;
        if (!*nameptr) {
            free(omx_context);
            return NULL;
        }
    }

    if (omx_context->host_init)
        ((void (*)(void))omx_context->host_init)();
    omx_context->ptr_Init();
    return omx_context;
}

/* Target: resolved via dlsym */
OMX_ERRORTYPE OMX_Init(void) {
    return 0;
}
