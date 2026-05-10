/* ET-Bench fixture: fnptr-library/example_16 */
/* fnptr: context->bbdsp.bswap16_buf, targets: bswap16_buf, ff_bswap16_buf_rvv */
/* Pattern: library bitstream DSP context with endian-swap function pointer selection */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define av_cold
#define AVERROR(e) (-(e))
#define AVERROR_INVALIDDATA (-2)
#define AVERROR_NOMEM (-1)
#define ARCH_RISCV 0
#define ARCH_X86 0

typedef struct BswapDSPContext {
    void (*bswap_buf)(uint32_t *dst, const uint32_t *src, int w);
    void (*bswap16_buf)(uint16_t *dst, const uint16_t *src, int w);
} BswapDSPContext;

void av_fast_padded_malloc(uint8_t **buf, size_t *size, size_t min_size) {
    (void)min_size;
    if (*buf == NULL || *size < min_size) {
        free(*buf);
        *size = min_size + 64;
        *buf = (uint8_t *)malloc(*size);
    }
}

int av_get_cpu_flags(void) { return 0; }
#define AV_CPU_FLAG_RVB_ADDR 0x01
#define AV_CPU_FLAG_RVB_BASIC 0x02
#define AV_CPU_FLAG_RVV_I32 0x04

/* Default C implementations */
static void bswap_buf(uint32_t *dst, const uint32_t *src, int w) {
    int i;
    for (i = 0; i < w; i++) {
        uint32_t v = src[i];
        dst[i] = ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) |
                 ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
    }
}

/* Target 1: default bswap16 */
static void bswap16_buf(uint16_t *dst, const uint16_t *src, int w) {
    int i;
    for (i = 0; i < w; i++) {
        uint16_t v = src[i];
        dst[i] = (v >> 8) | (v << 8);
    }
}

/* Target 2: RISC-V Vector implementation */
static void ff_bswap16_buf_rvv(uint16_t *dst, const uint16_t *src, int w) {
    int i;
    for (i = 0; i < w; i++) {
        uint16_t v = src[i];
        dst[i] = (v >> 8) | (v << 8);
    }
}

static void ff_bswap32_buf_rvb(uint32_t *dst, const uint32_t *src, int w) {
    (void)dst; (void)src; (void)w;
}

static void ff_bswapdsp_init_riscv(BswapDSPContext *c)
{
    int flags = av_get_cpu_flags();
    if (flags & AV_CPU_FLAG_RVB_ADDR) {
        if (flags & AV_CPU_FLAG_RVB_BASIC)
            c->bswap_buf = ff_bswap32_buf_rvb;
        if (flags & AV_CPU_FLAG_RVV_I32)
            c->bswap16_buf = ff_bswap16_buf_rvv;
    }
}

static void ff_bswapdsp_init(BswapDSPContext *c)
{
    c->bswap_buf   = bswap_buf;
    c->bswap16_buf = bswap16_buf;

#if ARCH_RISCV
    ff_bswapdsp_init_riscv(c);
#elif ARCH_X86
    /* ff_bswapdsp_init_x86(c); */
#endif
}

typedef struct RawVideoContext {
    BswapDSPContext bbdsp;
    uint8_t *bitstream_buf;
    size_t bitstream_buf_size;
} RawVideoContext;

static int raw_decode(AVCodecContext *avctx, AVFrame *frame,
                      int *got_frame, AVPacket *avpkt)
{
    RawVideoContext *context = (RawVideoContext *)avctx->priv_data;
    const uint8_t *buf = avpkt->data;
    int buf_size = avpkt->size;
    int packed = 1, swap = 16;

    (void)frame; (void)got_frame;

    if (packed && swap) {
        av_fast_padded_malloc(&context->bitstream_buf, &context->bitstream_buf_size, buf_size);
        if (!context->bitstream_buf)
            return AVERROR_NOMEM;
        if (swap == 16)
            context->bbdsp.bswap16_buf((uint16_t *)context->bitstream_buf,
                                       (const uint16_t *)buf, buf_size / 2);
        else if (swap == 32)
            context->bbdsp.bswap_buf((uint32_t *)context->bitstream_buf,
                                     (const uint32_t *)buf, buf_size / 4);
        else
            return AVERROR_INVALIDDATA;
        buf = context->bitstream_buf;
    }
    return 0;
}
