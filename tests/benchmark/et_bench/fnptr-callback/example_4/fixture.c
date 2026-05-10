/* ET-Bench fixture: fnptr-callback/example_4 */
/* Based on ffmpeg's find_codec pattern */
/* fnptr: x, targets: av_codec_is_decoder, av_codec_is_encoder */

#include <stddef.h>

enum AVCodecID {
    AV_CODEC_ID_NONE = 0,
    AV_CODEC_ID_H264 = 27,
    AV_CODEC_ID_AAC  = 86051,
};

#define AV_CODEC_CAP_EXPERIMENTAL 0x0020

typedef struct AVCodec {
    enum AVCodecID id;
    int capabilities;
    const char *name;
} AVCodec;

static enum AVCodecID remap_deprecated_codec_id(enum AVCodecID id) {
    return id;
}

static const AVCodec *av_codec_iterate(void **iter) {
    static const AVCodec codecs[] = {
        { AV_CODEC_ID_H264, AV_CODEC_CAP_EXPERIMENTAL, "h264" },
        { AV_CODEC_ID_AAC,  0, "aac" },
        { AV_CODEC_ID_NONE, 0, NULL }
    };
    uintptr_t i = (uintptr_t)*iter;
    if (codecs[i].id == AV_CODEC_ID_NONE)
        return NULL;
    (*iter) = (void *)(i + 1);
    return &codecs[i];
}

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

int av_codec_is_encoder(const AVCodec *codec) {
    return codec != NULL && codec->name != NULL;
}

int av_codec_is_decoder(const AVCodec *codec) {
    return codec != NULL && codec->name != NULL;
}
