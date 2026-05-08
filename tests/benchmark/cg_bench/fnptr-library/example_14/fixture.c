/* CG-Bench fixture: fnptr-library/example_14 */
/* fnptr: ctx->dsp.upsample_plane, targets: upsample_plane_c */

static int decode_wmv9(AVCodecContext *avctx, const uint8_t *buf, int buf_size,
                       int x, int y, int w, int h, int wmv9_mask)
{
    MSS2Context *ctx  = avctx->priv_data;
    if (v->respic == 3) {
        ctx->dsp.upsample_plane(f->data[0], f->linesize[0], w,      h);
        ctx->dsp.upsample_plane(f->data[1], f->linesize[1], w+1 >> 1, h+1 >> 1);
        ctx->dsp.upsample_plane(f->data[2], f->linesize[2], w+1 >> 1, h+1 >> 1);
    } else if (v->respic)
        avpriv_request_sample(v->s.avctx,
                              "Asymmetric WMV9 rectangle subsampling");
}

av_cold void ff_mss2dsp_init(MSS2DSPContext* dsp)
{
    dsp->mss2_blit_wmv9        = mss2_blit_wmv9_c;
    dsp->mss2_blit_wmv9_masked = mss2_blit_wmv9_masked_c;
    dsp->mss2_gray_fill_masked = mss2_gray_fill_masked_c;
    dsp->upsample_plane        = upsample_plane_c;
}

typedef struct MSS2Context {
    VC1Context     v;
    int            split_position;
    AVFrame       *last_pic;
    MSS12Context   c;
    MSS2DSPContext dsp;
    SliceContext   sc[2];
} MSS2Context;


/* Stub implementation for upsample_plane_c */
void upsample_plane_c(void) {}
