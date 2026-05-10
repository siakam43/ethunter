/* ET-Bench fixture: fnptr-library/example_17 */
/* fnptr: ctx->celpf_ctx.celp_lp_synthesis_filterf, targets: ff_celp_lp_synthesis_filterf, ff_celp_lp_synthesis_filterf_mips */
/* Pattern: library CELP filter context with architecture-optimized function pointer selection */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AMRWB_SFR_SIZE 40
#define LP_ORDER 16
#define HAVE_MIPSFPU 0
#define HAVE_INLINE_ASM 0

typedef struct CELPFContext {
    void (*celp_lp_synthesis_filterf)(float *out, const float *lpc,
                                       const float *in, int len, int order);
    void (*celp_lp_zero_synthesis_filterf)(float *out, const float *lpc,
                                            const float *in, int len, int order);
} CELPFContext;

typedef struct CELPMContext {
    float (*dot_productf)(const float *a, const float *b, int len);
} CELPMContext;

typedef struct {
    void (*weighted_vector_sumf)(float *dst, const float *a, const float *b,
                                  float a_weight, float b_weight, int len);
} ACELPVContext;

typedef struct AMRWBContext {
    CELPFContext celpf_ctx;
    CELPMContext celpm_ctx;
    ACELPVContext acelpv_ctx;
    float pitch_gain[4];
    float *pitch_vector;
    int fr_cur_mode;
} AMRWBContext;

#define MODE_8k85 2

/* C implementation */
static void ff_celp_lp_synthesis_filterf(float *out, const float *lpc,
                                          const float *in, int len, int order) {
    int i, j;
    float sum;
    for (i = 0; i < len; i++) {
        sum = in[i];
        for (j = 0; j < order; j++)
            sum -= lpc[j] * out[i - j - 1];
        out[i] = sum;
    }
}

static void ff_celp_lp_zero_synthesis_filterf(float *out, const float *lpc,
                                               const float *in, int len, int order) {
    (void)out; (void)lpc; (void)in; (void)len; (void)order;
}

/* MIPS implementation */
static void ff_celp_lp_synthesis_filterf_mips(float *out, const float *lpc,
                                               const float *in, int len, int order) {
    int i, j;
    float sum;
    for (i = 0; i < len; i++) {
        sum = in[i];
        for (j = 0; j < order; j++)
            sum -= lpc[j] * out[i - j - 1];
        out[i] = sum;
    }
}

static void ff_celp_lp_zero_synthesis_filterf_mips(float *out, const float *lpc,
                                                    const float *in, int len, int order) {
    (void)out; (void)lpc; (void)in; (void)len; (void)order;
}

void ff_celp_filter_init(CELPFContext *c)
{
    c->celp_lp_synthesis_filterf        = ff_celp_lp_synthesis_filterf;
    c->celp_lp_zero_synthesis_filterf   = ff_celp_lp_zero_synthesis_filterf;

#if HAVE_MIPSFPU
    ff_celp_filter_init_mips(c);
#endif
}

void ff_celp_filter_init_mips(CELPFContext *c)
{
#if HAVE_INLINE_ASM
#if !HAVE_MIPS32R6 && !HAVE_MIPS64R6
    c->celp_lp_synthesis_filterf        = ff_celp_lp_synthesis_filterf_mips;
    c->celp_lp_zero_synthesis_filterf   = ff_celp_lp_zero_synthesis_filterf_mips;
#endif
#endif
}

void ff_scale_vector_to_given_sum_of_squares(float *dst, const float *src,
                                              float sum, int len) {
    (void)dst; (void)src; (void)sum; (void)len;
}

static void synthesis(AMRWBContext *ctx, float *lpc, float *excitation,
                      float fixed_gain, const float *fixed_vector,
                      float *samples)
{
    ctx->celpm_ctx.dot_productf(excitation, excitation, AMRWB_SFR_SIZE);
    ctx->acelpv_ctx.weighted_vector_sumf(excitation, ctx->pitch_vector, fixed_vector,
                            ctx->pitch_gain[0], fixed_gain, AMRWB_SFR_SIZE);

    /* emphasize pitch vector contribution in low bitrate modes */
    if (ctx->pitch_gain[0] > 0.5 && ctx->fr_cur_mode <= MODE_8k85) {
        int i;
        float pitch_factor = 0.25f * ctx->pitch_gain[0] * ctx->pitch_gain[0];
        for (i = 0; i < AMRWB_SFR_SIZE; i++)
            excitation[i] += pitch_factor * ctx->pitch_vector[i];
        ff_scale_vector_to_given_sum_of_squares(excitation, excitation, 1.0f, AMRWB_SFR_SIZE);
    }

    ctx->celpf_ctx.celp_lp_synthesis_filterf(samples, lpc, excitation,
                                 AMRWB_SFR_SIZE, LP_ORDER);
}
