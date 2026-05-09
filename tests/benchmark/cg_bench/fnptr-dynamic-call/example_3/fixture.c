/* CG-Bench fixture: fnptr-dynamic-call/example_3 */
/* fnptr: omx_context->ptr_Init, targets: OMX_Init */

static void *dlsym_prefixed(void *handle, const char *symbol, const char *prefix)
{
    char buf[50];
    snprintf(buf, sizeof(buf), "%s%s", prefix ? prefix : "", symbol);
    return dlsym(handle, buf);
}

static int omx_try_load(OMXContext *s, void *logctx,
                                   const char *libname, const char *prefix,
                                   const char *libname2)
{
        s->ptr_Init = dlsym_prefixed(s->lib, "OMX_Init", prefix);
        return 0;
}

static OMXContext *omx_init(void *logctx, const char *libname, const char *prefix)
{
    static const char * const libnames[] = {
        "libOMX_Core.so", NULL,
        "libOmxCore.so", NULL,
        NULL
    };
    const char* const* nameptr;
    int ret = AVERROR_ENCODER_NOT_FOUND;
    OMXContext *omx_context;

    omx_context = av_mallocz(sizeof(*omx_context));
    if (!omx_context)
        return NULL;
    if (libname) {
        ret = omx_try_load(omx_context, logctx, libname, prefix, NULL);
        if (ret < 0) {
            av_free(omx_context);
            return NULL;
        }
    } else {
        for (nameptr = libnames; *nameptr; nameptr += 2)
            if (!(ret = omx_try_load(omx_context, logctx, nameptr[0], prefix, nameptr[1])))
                break;
        if (!*nameptr) {
            av_free(omx_context);
            return NULL;
        }
    }

    if (omx_context->host_init)
        omx_context->host_init();
    omx_context->ptr_Init();
    return omx_context;
}

static int omx_encode_init(AVCodecContext *avctx)
{
    OMXCodecContext *s = avctx->priv_data;
    int ret;

    ret = ff_pthread_init(s, omx_codec_context_offsets);
    if (ret < 0)
        return ret;
    s->omx_context = omx_init(avctx, s->libname, s->libprefix);
    return ret;
}

typedef struct OMXCodecContext {
    const AVClass *class;
    char *libname;
    char *libprefix;
    OMXContext *omx_context;
} OMXCodecContext;

/* Stub implementation for OMX_Init */
void OMX_Init(void) {}
