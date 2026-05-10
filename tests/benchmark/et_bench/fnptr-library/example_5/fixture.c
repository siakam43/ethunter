/* ET-Bench fixture: fnptr-library/example_5 */
/* fnptr: s->decode_mb, targets: ff_h263_decode_mb */
/* Pattern: library context with decode function pointer set during init, called in decode loop */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FMT_H263 1

typedef struct AVCodecContext {
    void *priv_data;
    int lowres;
} AVCodecContext;

typedef struct AVFrame {
    uint8_t **data;
} AVFrame;

typedef struct AVPacket {
    const uint8_t *data;
    int size;
} AVPacket;

#define av_cold
#define AVERROR(e) (-(e))
#define AVERROR_BUG (-1)

typedef struct MpegEncContext {
    int out_format;
    int quant_precision;
    int low_delay;
    int mb_x, mb_y, mb_width, mb_height;
    int partitioned_frame;
    int (*decode_mb)(struct MpegEncContext *s, int16_t block[6][64]);
    int16_t block[6][64];
} MpegEncContext;

static void ff_mpv_decode_init(MpegEncContext *s, AVCodecContext *avctx) {
    (void)s; (void)avctx;
}

int ff_h263_decode_mb(MpegEncContext *s, int16_t block[6][64]) {
    (void)s; (void)block;
    return 0;
}

static int decode_slice(MpegEncContext *s)
{
    const int mb_size = 16;
    int ret;

    for (; s->mb_x < s->mb_width; s->mb_x++) {
        ret = s->decode_mb(s, s->block);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int ff_h263_decode_init(AVCodecContext *avctx)
{
    MpegEncContext *s = (MpegEncContext *)avctx->priv_data;

    s->out_format = FMT_H263;
    ff_mpv_decode_init(s, avctx);

    s->quant_precision = 5;
    s->decode_mb = ff_h263_decode_mb;
    s->low_delay = 1;

    return 0;
}
