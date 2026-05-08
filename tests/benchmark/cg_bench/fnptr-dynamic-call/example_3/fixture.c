/* CG-Bench fixture: fnptr-dynamic-call/example_3 */
/* fnptr: omx_context->ptr_Init, targets: OMX_Init */

static av_cold void *dlsym_prefixed(void *handle, const char *symbol, const char *prefix)
{
    char buf[50];
    snprintf(buf, sizeof(buf), "%s%s", prefix ? prefix : "", symbol);
    return dlsym(handle, buf);
}

static av_cold int omx_try_load(OMXContext *s, void *logctx,
                                   const char *libname, const char *prefix,
                                   const char *libname2)
   {
        ...
        s->ptr_Init                = dlsym_prefixed(s->lib, "OMX_Init", prefix);
       ...
   }

static av_cold OMXContext *omx_init(void *logctx, const char *libname, const char *prefix)
{
    static const char * const libnames[] = {
#if CONFIG_OMX_RPI
        "/opt/vc/lib/libopenmaxil.so", "/opt/vc/lib/libbcm_host.so",
#else
        "libOMX_Core.so", NULL,
        "libOmxCore.so", NULL,
#endif
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

static av_cold int omx_encode_init(AVCodecContext *avctx)
{
    OMXCodecContext *s = avctx->priv_data;
    ...

    /* cleanup relies on the mutexes/conditions being initialized first. */
    ret = ff_pthread_init(s, omx_codec_context_offsets);
    if (ret < 0)
        return ret;
    s->omx_context = omx_init(avctx, s->libname, s->libprefix);
    ...
}

typedef struct OMXCodecContext {
    const AVClass *class;
    char *libname;
    char *libprefix;
    ...
} OMXCodecContext;

const FFCodec ff_h264_omx_encoder = {
    .p.name           = "h264_omx",
    CODEC_LONG_NAME("OpenMAX IL H.264 video encoder"),
    .p.type           = AVMEDIA_TYPE_VIDEO,
    .p.id             = AV_CODEC_ID_H264,
    .priv_data_size   = sizeof(OMXCodecContext),
    .init             = omx_encode_init,
    FF_CODEC_ENCODE_CB(omx_encode_frame),
    .close            = omx_encode_end,
    .p.pix_fmts       = omx_encoder_pix_fmts,
    .p.capabilities   = AV_CODEC_CAP_DELAY,
    .caps_internal    = FF_CODEC_CAP_INIT_CLEANUP,
    .p.priv_class     = &omx_h264enc_class,
};

static const AVClass omx_h264enc_class = {
    .class_name = "h264_omx",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVOption options[] = {
    { "omx_libname", "OpenMAX library name", OFFSET(libname), AV_OPT_TYPE_STRING, { 0 }, 0, 0, VDE },
    { "omx_libprefix", "OpenMAX library prefix", OFFSET(libprefix), AV_OPT_TYPE_STRING, { 0 }, 0, 0, VDE },
    { "zerocopy", "Try to avoid copying input frames if possible", OFFSET(input_zerocopy), AV_OPT_TYPE_INT, { .i64 = CONFIG_OMX_RPI }, 0, 1, VE },
    { "profile",  "Set the encoding profile", OFFSET(profile), AV_OPT_TYPE_INT,   { .i64 = AV_PROFILE_UNKNOWN },       AV_PROFILE_UNKNOWN, AV_PROFILE_H264_HIGH, VE, "profile" },
    { "baseline", "",                         0,               AV_OPT_TYPE_CONST, { .i64 = AV_PROFILE_H264_BASELINE }, 0, 0, VE, "profile" },
    { "main",     "",                         0,               AV_OPT_TYPE_CONST, { .i64 = AV_PROFILE_H264_MAIN },     0, 0, VE, "profile" },
    { "high",     "",                         0,               AV_OPT_TYPE_CONST, { .i64 = AV_PROFILE_H264_HIGH },     0, 0, VE, "profile" },
    { NULL }
};


/* Stub implementation for OMX_Init */
void OMX_Init(void) {}
