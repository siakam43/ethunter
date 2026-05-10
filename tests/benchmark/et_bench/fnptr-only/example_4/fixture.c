/* ET-Bench fixture: fnptr-only/example_4 */
/* fnptr: conv, targets: gray8aToPacked32, gray8aToPacked32_1, gray8aToPacked24, sws_convertPalette8ToPacked32, sws_convertPalette8ToPacked24 */
/* Source: FFmpeg-style swscale pixel converter */

#include <stdlib.h>
#include <string.h>

typedef enum {
    AV_PIX_FMT_NONE = -1,
    AV_PIX_FMT_YA8,
    AV_PIX_FMT_RGB32,
    AV_PIX_FMT_BGR32,
    AV_PIX_FMT_BGR32_1,
    AV_PIX_FMT_RGB32_1,
    AV_PIX_FMT_RGB24,
    AV_PIX_FMT_BGR24,
    AV_PIX_FMT_PAL8,
    AV_PIX_FMT_NB
} AVPixelFormat;

typedef struct SwsContext {
    AVPixelFormat srcFormat;
    AVPixelFormat dstFormat;
    int srcW;
    int chrSrcW;
} SwsContext;

#define AV_LOG_ERROR 16

static int usePal(AVPixelFormat fmt) {
    return fmt == AV_PIX_FMT_PAL8;
}

static void gray8aToPacked32(const uint8_t *src, uint8_t *dst, int num_pixels,
                              const uint8_t *palette) {
    for (int i = 0; i < num_pixels; i++) {
        uint8_t gray = src[i * 2];
        uint8_t alpha = src[i * 2 + 1];
        dst[i * 4]     = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = alpha;
    }
}

static void gray8aToPacked32_1(const uint8_t *src, uint8_t *dst, int num_pixels,
                                const uint8_t *palette) {
    for (int i = 0; i < num_pixels; i++) {
        uint8_t gray = src[i * 2];
        uint8_t alpha = src[i * 2 + 1];
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = gray;
        dst[i * 4]     = alpha;
    }
}

static void gray8aToPacked24(const uint8_t *src, uint8_t *dst, int num_pixels,
                              const uint8_t *palette) {
    for (int i = 0; i < num_pixels; i++) {
        uint8_t gray = src[i * 2];
        dst[i * 3]     = gray;
        dst[i * 3 + 1] = gray;
        dst[i * 3 + 2] = gray;
    }
}

static void sws_convertPalette8ToPacked32(const uint8_t *src, uint8_t *dst,
                                           int num_pixels,
                                           const uint8_t *palette) {
    for (int i = 0; i < num_pixels; i++) {
        int idx = src[i] * 4;
        dst[i * 4]     = palette[idx];
        dst[i * 4 + 1] = palette[idx + 1];
        dst[i * 4 + 2] = palette[idx + 2];
        dst[i * 4 + 3] = 255;
    }
}

static void sws_convertPalette8ToPacked24(const uint8_t *src, uint8_t *dst,
                                           int num_pixels,
                                           const uint8_t *palette) {
    for (int i = 0; i < num_pixels; i++) {
        int idx = src[i] * 4;
        dst[i * 3]     = palette[idx];
        dst[i * 3 + 1] = palette[idx + 1];
        dst[i * 3 + 2] = palette[idx + 2];
    }
}

static void av_log(SwsContext *c, int level, const char *fmt, ...) {}
static const char *av_get_pix_fmt_name(AVPixelFormat fmt) { return "unknown"; }

static int palToRgbWrapper(SwsContext *c, const uint8_t *src[], int srcStride[],
                           int srcSliceY, int srcSliceH, uint8_t *dst[],
                           int dstStride[])
{
    const AVPixelFormat srcFormat = c->srcFormat;
    const AVPixelFormat dstFormat = c->dstFormat;
    void (*conv)(const uint8_t *src, uint8_t *dst, int num_pixels,
                 const uint8_t *palette) = NULL;
    int i;
    uint8_t *dstPtr = dst[0] + dstStride[0] * srcSliceY;
    const uint8_t *srcPtr = src[0];
    const uint8_t pal_rgb[1024];

    if (srcFormat == AV_PIX_FMT_YA8) {
        switch (dstFormat) {
        case AV_PIX_FMT_RGB32:   conv = gray8aToPacked32; break;
        case AV_PIX_FMT_BGR32:   conv = gray8aToPacked32; break;
        case AV_PIX_FMT_BGR32_1: conv = gray8aToPacked32_1; break;
        case AV_PIX_FMT_RGB32_1: conv = gray8aToPacked32_1; break;
        case AV_PIX_FMT_RGB24:   conv = gray8aToPacked24; break;
        case AV_PIX_FMT_BGR24:   conv = gray8aToPacked24; break;
        }
    } else if (usePal(srcFormat)) {
        switch (dstFormat) {
        case AV_PIX_FMT_RGB32:   conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_BGR32:   conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_BGR32_1: conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_RGB32_1: conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_RGB24:   conv = sws_convertPalette8ToPacked24; break;
        case AV_PIX_FMT_BGR24:   conv = sws_convertPalette8ToPacked24; break;
        }
    }

    if (!conv) {
        av_log(c, AV_LOG_ERROR, "internal error %s -> %s converter\n",
               av_get_pix_fmt_name(srcFormat), av_get_pix_fmt_name(dstFormat));
        return -1;
    }

    for (i = 0; i < srcSliceH; i++) {
        conv(srcPtr, dstPtr, c->srcW, pal_rgb);
        srcPtr += srcStride[0];
        dstPtr += dstStride[0];
    }

    return srcSliceH;
}
