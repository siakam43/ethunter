/* CG-Bench fixture: fnptr-library/example_16 */
/* fnptr: context->bbdsp.bswap16_buf, targets: bswap16_buf, ff_bswap16_buf_rvv */

static int raw_decode(AVCodecContext *avctx, AVFrame *frame,
                      int *got_frame, AVPacket *avpkt)
{
  if (packed && swap) {
    av_fast_padded_malloc(&context->bitstream_buf, &context->bitstream_buf_size, buf_size);
    if (!context->bitstream_buf)
        return AVERROR(ENOMEM);
    if (swap == 16)
        context->bbdsp.bswap16_buf(context->bitstream_buf, (const uint16_t*)buf, buf_size / 2);
    else if (swap == 32)
        context->bbdsp.bswap_buf(context->bitstream_buf, (const uint32_t*)buf, buf_size / 4);
    else
        return AVERROR_INVALIDDATA;
    buf = context->bitstream_buf;
  }
}

av_cold void ff_bswapdsp_init(BswapDSPContext *c)
{
    c->bswap_buf   = bswap_buf;
    c->bswap16_buf = bswap16_buf;

#if ARCH_RISCV
    ff_bswapdsp_init_riscv(c);
#elif ARCH_X86
    ff_bswapdsp_init_x86(c);
#endif
}

av_cold void ff_bswapdsp_init_riscv(BswapDSPContext *c)
{
    int flags = av_get_cpu_flags();

    if (flags & AV_CPU_FLAG_RVB_ADDR) {
#if (__riscv_xlen >= 64)
        if (flags & AV_CPU_FLAG_RVB_BASIC)
            c->bswap_buf = ff_bswap32_buf_rvb;
#endif
#if HAVE_RVV
        if (flags & AV_CPU_FLAG_RVV_I32)
            c->bswap16_buf = ff_bswap16_buf_rvv;
#endif
    }
}


/* Stub implementation for bswap16_buf */
void bswap16_buf(void) {}



/* Stub implementation for ff_bswap16_buf_rvv */
void ff_bswap16_buf_rvv(void) {}
