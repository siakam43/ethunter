/* ET-Bench fixture: fnptr-struct/example_3 */
/* Scenario: AAC encoder DSP init — abs_pow34 function pointer dispatch.
   fnptr: s->abs_pow34
   targets: ff_abs_pow34_sse, abs_pow34_v
   caller: search_for_quantizers_fast */

#include <stddef.h>

#define av_cold __attribute__((cold))

typedef struct AACEncContext AACEncContext;

struct AACEncContext {
    float scoefs[1024];
    float coeffs[1024];
    unsigned int random_state;
    void (*abs_pow34)(float *out, const float *in, const int size);
    void (*quant_bands)(float *out, const float *in, const int size);
};

typedef struct AVCodecContext {
    AACEncContext *priv_data;
} AVCodecContext;

typedef struct SingleChannelElement {
    float coeffs[1024];
} SingleChannelElement;

static int av_get_cpu_flags(void) { return 0; }
#define EXTERNAL_SSE(flags) ((flags) & 0x0002)
#define EXTERNAL_SSE2(flags) ((flags) & 0x0004)

/* Target: SSE-optimized absolute power */
void ff_abs_pow34_sse(float *out, const float *in, const int size)
{
    for (int i = 0; i < size; i++) {
        float v = in[i];
        if (v < 0) v = -v;
        out[i] = v * v * v;
    }
}

/* Target: generic vector absolute power */
void abs_pow34_v(float *out, const float *in, const int size)
{
    for (int i = 0; i < size; i++) {
        float v = in[i];
        if (v < 0) v = -v;
        out[i] = v * v * v;
    }
}

static void quantize_bands(float *out, const float *in, const int size)
{
    (void)out; (void)in; (void)size;
}

static void ff_aac_quantize_bands_sse2(float *out, const float *in, const int size)
{
    (void)out; (void)in; (void)size;
}

void ff_aac_dsp_init_x86(AACEncContext *s)
{
    int cpu_flags = av_get_cpu_flags();

    if (EXTERNAL_SSE(cpu_flags))
        s->abs_pow34 = ff_abs_pow34_sse;

    if (EXTERNAL_SSE2(cpu_flags))
        s->quant_bands = ff_aac_quantize_bands_sse2;
}

static int aac_encode_init(AVCodecContext *avctx)
{
    AACEncContext *s = avctx->priv_data;
    s->random_state = 0x1f2e3d4c;

    s->abs_pow34 = abs_pow34_v;
    s->quant_bands = quantize_bands;
    return 0;
}

static void ff_quantize_band_cost_cache_init(AACEncContext *s)
{
    (void)s;
}

/* Caller: invokes s->abs_pow34 through the struct */
static void search_for_quantizers_fast(AVCodecContext *avctx, AACEncContext *s,
                                       SingleChannelElement *sce,
                                       const float lambda)
{
    int start = 0, i, w, w2, g;
    int allz = 0;
    (void)avctx; (void)start; (void)i; (void)w; (void)w2; (void)g;
    (void)lambda;

    if (!allz)
        return;
    s->abs_pow34(s->scoefs, sce->coeffs, 1024);
    ff_quantize_band_cost_cache_init(s);
}
