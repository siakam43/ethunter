/* CG-Bench fixture: fnptr-struct/example_3 */
/* fnptr: s->abs_pow34, targets: ff_abs_pow34_sse, abs_pow34_v */

av_cold void ff_aac_dsp_init_x86(AACEncContext *s)
{
    int cpu_flags = av_get_cpu_flags();

    if (EXTERNAL_SSE(cpu_flags))
        s->abs_pow34   = ff_abs_pow34_sse;

    if (EXTERNAL_SSE2(cpu_flags))
        s->quant_bands = ff_aac_quantize_bands_sse2;
}

static av_cold int aac_encode_init(AVCodecContext *avctx)
{
    AACEncContext *s = avctx->priv_data;
    ...
    s->random_state = 0x1f2e3d4c;
    
    s->abs_pow34   = abs_pow34_v;
    s->quant_bands = quantize_bands;
    ...
}

static void search_for_quantizers_fast(AVCodecContext *avctx, AACEncContext *s,
                                       SingleChannelElement *sce,
                                       const float lambda)
{
    int start = 0, i, w, w2, g;
    ...
    if (!allz)
        return;
    s->abs_pow34(s->scoefs, sce->coeffs, 1024);
    ff_quantize_band_cost_cache_init(s);
    ...
}


/* Stub implementation for ff_abs_pow34_sse */
void ff_abs_pow34_sse(void) {}



/* Stub implementation for abs_pow34_v */
void abs_pow34_v(void) {}
