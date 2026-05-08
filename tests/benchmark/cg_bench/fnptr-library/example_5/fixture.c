/* CG-Bench fixture: fnptr-library/example_5 */
/* fnptr: s->decode_mb, targets: ff_h263_decode_mb */

av_cold int ff_h263_decode_init(AVCodecContext *avctx)
{
    MpegEncContext *s = avctx->priv_data;
    int ret;

    s->out_format      = FMT_H263;

    // set defaults
    ff_mpv_decode_init(s, avctx);

    s->quant_precision = 5;
    s->decode_mb       = ff_h263_decode_mb;
    s->low_delay       = 1;
    ...
}

static int decode_slice(MpegEncContext *s)
{
    const int part_mask = s->partitioned_frame
                          ? (ER_AC_END | ER_AC_ERROR) : 0x7F;
    const int mb_size   = 16 >> s->avctx->lowres;
    int ret;

...

        ff_init_block_index(s);
        for (; s->mb_x < s->mb_width; s->mb_x++) {
            int ret;

            ...
            ret = s->decode_mb(s, s->block);
        }
}

int ff_h263_decode_frame(AVCodecContext *avctx, AVFrame *pict,
                         int *got_frame, AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;
    MpegEncContext *s  = avctx->priv_data;
    int ret;
    int slice_ret = 0;

    ...

    /* decode each macroblock */
    s->mb_x = 0;
    s->mb_y = 0;

    slice_ret = decode_slice(s);
    ...
}

const FFCodec ff_h263_decoder = {
    .p.name         = "h263",
    CODEC_LONG_NAME("H.263 / H.263-1996, H.263+ / H.263-1998 / H.263 version 2"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_H263,
    .priv_data_size = sizeof(MpegEncContext),
    .init           = ff_h263_decode_init,
    .close          = ff_h263_decode_end,
    FF_CODEC_DECODE_CB(ff_h263_decode_frame),
    .p.capabilities = AV_CODEC_CAP_DRAW_HORIZ_BAND | AV_CODEC_CAP_DR1 |
                      AV_CODEC_CAP_DELAY,
    .caps_internal  = FF_CODEC_CAP_SKIP_FRAME_FILL_PARAM,
    .flush          = ff_mpeg_flush,
    .p.max_lowres   = 3,
    .p.pix_fmts     = ff_h263_hwaccel_pixfmt_list_420,
    .hw_configs     = h263_hw_config_list,
};


/* Stub implementation for ff_h263_decode_mb */
void ff_h263_decode_mb(void) {}
