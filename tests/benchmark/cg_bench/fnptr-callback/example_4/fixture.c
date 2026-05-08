/* CG-Bench fixture: fnptr-callback/example_4 */
/* fnptr: x, targets: av_codec_is_decoder, av_codec_is_encoder */

static const AVCodec *find_codec(enum AVCodecID id, int (*x)(const AVCodec *))
{
    const AVCodec *p, *experimental = NULL;
    void *i = 0;

    id = remap_deprecated_codec_id(id);

    while ((p = av_codec_iterate(&i))) {
        if (!x(p))
            continue;
        if (p->id == id) {
            if (p->capabilities & AV_CODEC_CAP_EXPERIMENTAL && !experimental) {
                experimental = p;
            } else
                return p;
        }
    }

    return experimental;
}

const AVCodec *avcodec_find_encoder(enum AVCodecID id)
{
    return find_codec(id, av_codec_is_encoder);
}

const AVCodec *avcodec_find_decoder(enum AVCodecID id)
{
    return find_codec(id, av_codec_is_decoder);
}


/* Wrapper: calls through x */
void x_caller(void) {
    x();
}



/* Stub implementation for av_codec_is_decoder */
void av_codec_is_decoder(void) {}



/* Stub implementation for av_codec_is_encoder */
void av_codec_is_encoder(void) {}
