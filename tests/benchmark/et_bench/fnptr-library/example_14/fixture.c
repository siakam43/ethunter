/* ET-Bench fixture: fnptr-library/example_14 */
/* fnptr: ctx->dsp.upsample_plane, targets: upsample_plane_c */
/* Pattern: library codec context with DSP function pointer initialized during codec setup */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define av_cold

typedef struct AVCodecContext {
    void *priv_data;
} AVCodecContext;

typedef struct MSS2DSPContext {
    void (*mss2_blit_wmv9)(void);
    void (*mss2_blit_wmv9_masked)(void);
    void (*mss2_gray_fill_masked)(void);
    void (*upsample_plane)(uint8_t *data, int linesize, int w, int h);
} MSS2DSPContext;

typedef struct VC1Context {
    int respic;
    struct {
        AVCodecContext *avctx;
    } s;
} VC1Context;

typedef struct MSS12Context { int dummy; } MSS12Context;
typedef struct SliceContext { int dummy; } SliceContext;

typedef struct AVFrame {
    uint8_t *data[4];
    int linesize[4];
} AVFrame;

typedef struct MSS2Context {
    VC1Context v;
    int split_position;
    AVFrame *last_pic;
    MSS12Context c;
    MSS2DSPContext dsp;
    SliceContext sc[2];
} MSS2Context;

void mss2_blit_wmv9_c(void) {}
void mss2_blit_wmv9_masked_c(void) {}
void mss2_gray_fill_masked_c(void) {}

/* Target: set in ff_mss2dsp_init */
void upsample_plane_c(uint8_t *data, int linesize, int w, int h) {
    (void)data; (void)linesize; (void)w; (void)h;
}

void ff_mss2dsp_init(MSS2DSPContext *dsp)
{
    dsp->mss2_blit_wmv9        = mss2_blit_wmv9_c;
    dsp->mss2_blit_wmv9_masked = mss2_blit_wmv9_masked_c;
    dsp->mss2_gray_fill_masked = mss2_gray_fill_masked_c;
    dsp->upsample_plane        = upsample_plane_c;
}

static int decode_wmv9(AVCodecContext *avctx, const uint8_t *buf, int buf_size,
                       int x, int y, int w, int h, int wmv9_mask)
{
    MSS2Context *ctx = (MSS2Context *)avctx->priv_data;
    VC1Context *v = &ctx->v;
    AVFrame *f = ctx->last_pic;

    (void)buf; (void)buf_size; (void)x; (void)y; (void)wmv9_mask;

    if (v->respic == 3) {
        ctx->dsp.upsample_plane(f->data[0], f->linesize[0], w, h);
        ctx->dsp.upsample_plane(f->data[1], f->linesize[1], (w+1) >> 1, (h+1) >> 1);
        ctx->dsp.upsample_plane(f->data[2], f->linesize[2], (w+1) >> 1, (h+1) >> 1);
    } else if (v->respic) {
        /* Asymmetric WMV9 rectangle subsampling */
    }
    return 0;
}
