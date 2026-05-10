/* ET-Bench fixture: fnptr-library/example_15 */
/* fnptr: s->fdsp->vector_fmul, targets: vector_fmul_c, ff_vector_fmul_neon, ff_vector_fmul_vfp */
/* Pattern: library float DSP context with architecture-optimized function pointer selection */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define av_cold
#define NELLY_BUF_LEN 128
#define NELLY_SAMPLES 256
#define OPT_SIZE 16
#define DECLARE_ALIGNED(n, type, name) type name
#define ARCH_AARCH64 0
#define ARCH_ARM 1
#define ARCH_PPC 0
#define ARCH_RISCV 0
#define ARCH_X86 0
#define ARCH_MIPS 0

typedef struct AVTXContext { int dummy; } AVTXContext;
typedef void (*av_tx_fn)(void *ctx, void *out, void *in, int size);

typedef struct AudioFrameQueue { int dummy; } AudioFrameQueue;

typedef struct AVFloatDSPContext {
    void (*vector_fmul)(float *dst, const float *src0, const float *src1, int len);
    void (*vector_dmul)(double *dst, const double *src0, const double *src1, int len);
    void (*vector_fmac_scalar)(float *dst, const float *src, float mul, int len);
    void (*vector_fmul_scalar)(float *dst, const float *src, float mul, int len);
    void (*vector_dmac_scalar)(double *dst, const double *src, double mul, int len);
    void (*vector_dmul_scalar)(double *dst, const double *src, double mul, int len);
    void (*vector_fmul_window)(float *dst, const float *src0, const float *src1, const float *win, int len);
    void (*vector_fmul_add)(float *dst, const float *src0, const float *src1, const float *src2, int len);
    void (*vector_fmul_reverse)(float *dst, const float *src0, const float *src1, int len);
    void (*butterflies_float)(float *v1, float *v2, int len);
    void (*scalarproduct_float)(float *v1, float *v2, int len);
} AVFloatDSPContext;

void *av_mallocz(size_t size) { void *p = calloc(1, size); return p; }
void av_free(void *p) { free(p); }
int av_get_cpu_flags(void) { return 0; }
int have_vfp(int flags) { (void)flags; return 0; }
int have_neon(int flags) { (void)flags; return 0; }
int have_vfp_vm(int flags) { (void)flags; return 0; }

typedef struct NellyMoserEncodeContext {
    void *avctx;
    int last_frame;
    AVFloatDSPContext *fdsp;
    AVTXContext *mdct_ctx;
    av_tx_fn mdct_fn;
    AudioFrameQueue afq;
    float mdct_out[NELLY_SAMPLES];
    float in_buff[NELLY_SAMPLES];
    float buf[3 * NELLY_BUF_LEN];
    float (*opt_)[OPT_SIZE];
    uint8_t (*path)[OPT_SIZE];
} NellyMoserEncodeContext;

/* Default C implementations */
static void vector_fmul_c(float *dst, const float *src0, const float *src1, int len) {
    int i;
    for (i = 0; i < len; i++) dst[i] = src0[i] * src1[i];
}
static void vector_dmul_c(double *d, const double *s0, const double *s1, int l) { (void)d; (void)s0; (void)s1; (void)l; }
static void vector_fmac_scalar_c(float *d, const float *s, float m, int l) { (void)d; (void)s; (void)m; (void)l; }
static void vector_fmul_scalar_c(float *d, const float *s, float m, int l) { (void)d; (void)s; (void)m; (void)l; }
static void vector_dmac_scalar_c(double *d, const double *s, double m, int l) { (void)d; (void)s; (void)m; (void)l; }
static void vector_dmul_scalar_c(double *d, const double *s, double m, int l) { (void)d; (void)s; (void)m; (void)l; }
static void vector_fmul_window_c(float *d, const float *s0, const float *s1, const float *w, int l) { (void)d; (void)s0; (void)s1; (void)w; (void)l; }
static void vector_fmul_add_c(float *d, const float *s0, const float *s1, const float *s2, int l) { (void)d; (void)s0; (void)s1; (void)s2; (void)l; }
static void vector_fmul_reverse_c(float *d, const float *s0, const float *s1, int l) { (void)d; (void)s0; (void)s1; (void)l; }
static void butterflies_float_c(float *v1, float *v2, int l) { (void)v1; (void)v2; (void)l; }
static void avpriv_scalarproduct_float_c(float *v1, float *v2, int l) { (void)v1; (void)v2; (void)l; }

/* NEON implementations */
static void ff_vector_fmul_neon(float *d, const float *s0, const float *s1, int l) { (void)d; (void)s0; (void)s1; (void)l; }
static void ff_vector_fmac_scalar_neon(float *d, const float *s, float m, int l) { (void)d; (void)s; (void)m; (void)l; }
static void ff_vector_fmul_scalar_neon(float *d, const float *s, float m, int l) { (void)d; (void)s; (void)m; (void)l; }
static void ff_vector_fmul_window_neon(float *d, const float *s0, const float *s1, const float *w, int l) { (void)d; (void)s0; (void)s1; (void)w; (void)l; }
static void ff_vector_fmul_add_neon(float *d, const float *s0, const float *s1, const float *s2, int l) { (void)d; (void)s0; (void)s1; (void)s2; (void)l; }
static void ff_vector_fmul_reverse_neon(float *d, const float *s0, const float *s1, int l) { (void)d; (void)s0; (void)s1; (void)l; }
static void ff_butterflies_float_neon(float *v1, float *v2, int l) { (void)v1; (void)v2; (void)l; }
static void ff_scalarproduct_float_neon(float *v1, float *v2, int l) { (void)v1; (void)v2; (void)l; }

/* VFP implementations */
static void ff_vector_fmul_vfp(float *d, const float *s0, const float *s1, int l) { (void)d; (void)s0; (void)s1; (void)l; }
static void ff_vector_fmul_window_vfp(float *d, const float *s0, const float *s1, const float *w, int l) { (void)d; (void)s0; (void)s1; (void)w; (void)l; }
static void ff_vector_fmul_reverse_vfp(float *d, const float *s0, const float *s1, int l) { (void)d; (void)s0; (void)s1; (void)l; }
static void ff_butterflies_float_vfp(float *v1, float *v2, int l) { (void)v1; (void)v2; (void)l; }

/* Init functions */
static void ff_float_dsp_init_vfp(AVFloatDSPContext *fdsp, int cpu_flags)
{
    if (have_vfp_vm(cpu_flags)) {
        fdsp->vector_fmul = ff_vector_fmul_vfp;
        fdsp->vector_fmul_window = ff_vector_fmul_window_vfp;
    }
    fdsp->vector_fmul_reverse = ff_vector_fmul_reverse_vfp;
    if (have_vfp_vm(cpu_flags))
        fdsp->butterflies_float = ff_butterflies_float_vfp;
}

static void ff_float_dsp_init_neon(AVFloatDSPContext *fdsp)
{
    fdsp->vector_fmul = ff_vector_fmul_neon;
    fdsp->vector_fmac_scalar = ff_vector_fmac_scalar_neon;
    fdsp->vector_fmul_scalar = ff_vector_fmul_scalar_neon;
    fdsp->vector_fmul_window = ff_vector_fmul_window_neon;
    fdsp->vector_fmul_add    = ff_vector_fmul_add_neon;
    fdsp->vector_fmul_reverse = ff_vector_fmul_reverse_neon;
    fdsp->butterflies_float = ff_butterflies_float_neon;
    fdsp->scalarproduct_float = ff_scalarproduct_float_neon;
}

static void ff_float_dsp_init_arm(AVFloatDSPContext *fdsp)
{
    int cpu_flags = av_get_cpu_flags();
    if (have_vfp(cpu_flags))
        ff_float_dsp_init_vfp(fdsp, cpu_flags);
    if (have_neon(cpu_flags))
        ff_float_dsp_init_neon(fdsp);
}

static AVFloatDSPContext *avpriv_float_dsp_alloc(int bit_exact)
{
    AVFloatDSPContext *fdsp = (AVFloatDSPContext *)av_mallocz(sizeof(AVFloatDSPContext));
    if (!fdsp)
        return NULL;

    fdsp->vector_fmul = vector_fmul_c;
    fdsp->vector_dmul = vector_dmul_c;
    fdsp->vector_fmac_scalar = vector_fmac_scalar_c;
    fdsp->vector_fmul_scalar = vector_fmul_scalar_c;
    fdsp->vector_dmac_scalar = vector_dmac_scalar_c;
    fdsp->vector_dmul_scalar = vector_dmul_scalar_c;
    fdsp->vector_fmul_window = vector_fmul_window_c;
    fdsp->vector_fmul_add = vector_fmul_add_c;
    fdsp->vector_fmul_reverse = vector_fmul_reverse_c;
    fdsp->butterflies_float = butterflies_float_c;
    fdsp->scalarproduct_float = avpriv_scalarproduct_float_c;

    if (ARCH_ARM) {
        ff_float_dsp_init_arm(fdsp);
    }
    return fdsp;
}

static void apply_mdct(NellyMoserEncodeContext *s)
{
    float *in0 = s->buf;
    float *in1 = s->buf + NELLY_BUF_LEN;
    float *in2 = s->buf + 2 * NELLY_BUF_LEN;

    static const float ff_sine_128[NELLY_BUF_LEN] = {0};

    s->fdsp->vector_fmul(s->in_buff, in0, ff_sine_128, NELLY_BUF_LEN);
    s->fdsp->vector_fmul_reverse(s->in_buff + NELLY_BUF_LEN, in1, ff_sine_128, NELLY_BUF_LEN);
    s->mdct_fn(s->mdct_ctx, s->mdct_out, s->in_buff, sizeof(float));

    s->fdsp->vector_fmul(s->in_buff, in1, ff_sine_128, NELLY_BUF_LEN);
    s->fdsp->vector_fmul_reverse(s->in_buff + NELLY_BUF_LEN, in2, ff_sine_128, NELLY_BUF_LEN);
    s->mdct_fn(s->mdct_ctx, s->mdct_out + NELLY_BUF_LEN, s->in_buff, sizeof(float));
}
