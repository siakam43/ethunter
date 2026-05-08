/* CG-Bench fixture: fnptr-library/example_17 */
/* fnptr: ctx->celpf_ctx.celp_lp_synthesis_filterf, targets: ff_celp_lp_synthesis_filterf, ff_celp_lp_synthesis_filterf_mips */

static void synthesis(AMRWBContext *ctx, float *lpc, float *excitation,
                      float fixed_gain, const float *fixed_vector,
                      float *samples)
{
    ctx->acelpv_ctx.weighted_vector_sumf(excitation, ctx->pitch_vector, fixed_vector,
                            ctx->pitch_gain[0], fixed_gain, AMRWB_SFR_SIZE);

    /* emphasize pitch vector contribution in low bitrate modes */
    if (ctx->pitch_gain[0] > 0.5 && ctx->fr_cur_mode <= MODE_8k85) {
        int i;
        float energy = ctx->celpm_ctx.dot_productf(excitation, excitation,
                                                    AMRWB_SFR_SIZE);

        // XXX: Weird part in both ref code and spec. A unknown parameter
        // {beta} seems to be identical to the current pitch gain
        float pitch_factor = 0.25 * ctx->pitch_gain[0] * ctx->pitch_gain[0];

        for (i = 0; i < AMRWB_SFR_SIZE; i++)
            excitation[i] += pitch_factor * ctx->pitch_vector[i];

        ff_scale_vector_to_given_sum_of_squares(excitation, excitation,
                                                energy, AMRWB_SFR_SIZE);
    }

    ctx->celpf_ctx.celp_lp_synthesis_filterf(samples, lpc, excitation,
                                 AMRWB_SFR_SIZE, LP_ORDER);
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


/* Stub implementation for ff_celp_lp_synthesis_filterf */
void ff_celp_lp_synthesis_filterf(void) {}



/* Stub implementation for ff_celp_lp_synthesis_filterf_mips */
void ff_celp_lp_synthesis_filterf_mips(void) {}
