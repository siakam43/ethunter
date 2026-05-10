/* ET-Bench fixture: fnptr-library/example_11 */
/* fnptr: synth->synth_filter_float, targets: synth_filter_sse2, synth_filter_avx, synth_filter_fma3 */
/* Pattern: library DSP context with architecture-optimized function pointer selection */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define av_cold
#define LOCAL_ALIGNED_32(type, name, size) type name[size]

typedef void (*av_tx_fn)(void *ctx, void *out, void *in, int size);

typedef struct AVTXContext { int dummy; } AVTXContext;

#define AV_TX_FLOAT_MDCT 0

typedef struct SynthFilterContext {
    void (*synth_filter_float)(AVTXContext *imdct, float *hist1, int *offset,
                               float *hist2, const float *filter_coeff,
                               float *pcm_samples, const float *input,
                               float scale, av_tx_fn imdct_fn);
    void (*synth_filter_float_64)(void);
    void (*synth_filter_fixed)(void);
    void (*synth_filter_fixed_64)(void);
} SynthFilterContext;

static void synth_filter_float(AVTXContext *imdct, float *hist1, int *offset,
                               float *hist2, const float *filter_coeff,
                               float *pcm_samples, const float *input,
                               float scale, av_tx_fn imdct_fn) {
    (void)imdct; (void)hist1; (void)offset; (void)hist2;
    (void)filter_coeff; (void)pcm_samples; (void)input;
    (void)scale; (void)imdct_fn;
}
static void synth_filter_float_64(void) {}
static void synth_filter_fixed(void) {}
static void synth_filter_fixed_64(void) {}

/* Architecture-optimized implementations */
static void synth_filter_sse2(AVTXContext *imdct, float *hist1, int *offset,
                              float *hist2, const float *filter_coeff,
                              float *pcm_samples, const float *input,
                              float scale, av_tx_fn imdct_fn) {
    (void)imdct; (void)hist1; (void)offset; (void)hist2;
    (void)filter_coeff; (void)pcm_samples; (void)input;
    (void)scale; (void)imdct_fn;
}

static void synth_filter_avx(AVTXContext *imdct, float *hist1, int *offset,
                             float *hist2, const float *filter_coeff,
                             float *pcm_samples, const float *input,
                             float scale, av_tx_fn imdct_fn) {
    (void)imdct; (void)hist1; (void)offset; (void)hist2;
    (void)filter_coeff; (void)pcm_samples; (void)input;
    (void)scale; (void)imdct_fn;
}

static void synth_filter_fma3(AVTXContext *imdct, float *hist1, int *offset,
                              float *hist2, const float *filter_coeff,
                              float *pcm_samples, const float *input,
                              float scale, av_tx_fn imdct_fn) {
    (void)imdct; (void)hist1; (void)offset; (void)hist2;
    (void)filter_coeff; (void)pcm_samples; (void)input;
    (void)scale; (void)imdct_fn;
}

#define HAVE_X86ASM 1
#define EXTERNAL_SSE2(f) ((f) & 0x1)
#define EXTERNAL_AVX_FAST(f) ((f) & 0x2)
#define EXTERNAL_FMA3_FAST(f) ((f) & 0x4)

static void ff_synth_filter_init_x86(SynthFilterContext *s)
{
    int cpu_flags = 0x7; /* SSE2 | AVX | FMA3 */
    if (EXTERNAL_SSE2(cpu_flags)) {
        s->synth_filter_float = synth_filter_sse2;
    }
    if (EXTERNAL_AVX_FAST(cpu_flags)) {
        s->synth_filter_float = synth_filter_avx;
    }
    if (EXTERNAL_FMA3_FAST(cpu_flags)) {
        s->synth_filter_float = synth_filter_fma3;
    }
}

static void ff_synth_filter_init(SynthFilterContext *c)
{
    c->synth_filter_float    = synth_filter_float;
    c->synth_filter_float_64 = synth_filter_float_64;
    c->synth_filter_fixed    = synth_filter_fixed;
    c->synth_filter_fixed_64 = synth_filter_fixed_64;
    ff_synth_filter_init_x86(c);
}

static void sub_qmf32_float_c(SynthFilterContext *synth,
                              AVTXContext *imdct, av_tx_fn imdct_fn,
                              float *pcm_samples,
                              int32_t **subband_samples_lo,
                              int32_t **subband_samples_hi,
                              float *hist1, int *offset, float *hist2,
                              const float *filter_coeff, int npcmblocks,
                              float scale)
{
    float input[32];
    int i, j;

    for (j = 0; j < npcmblocks; j++) {
        for (i = 0; i < 32; i++) {
            if ((i - 1) & 2)
                input[i] = -subband_samples_lo[i][j];
            else
                input[i] = subband_samples_lo[i][j];
        }
        synth->synth_filter_float(imdct, hist1, offset, hist2, filter_coeff,
                                  pcm_samples, input, scale, imdct_fn);
        pcm_samples += 32;
    }
}
