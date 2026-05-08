/* CG-Bench fixture: fnptr-library/example_15 */
/* fnptr: s->fdsp->vector_fmul, targets: vector_fmul_c, ff_vector_fmul_neon, ff_vector_fmul_vfp */

static void apply_mdct(NellyMoserEncodeContext *s)
{
    float *in0 = s->buf;
    float *in1 = s->buf + NELLY_BUF_LEN;
    float *in2 = s->buf + 2 * NELLY_BUF_LEN;

    s->fdsp->vector_fmul        (s->in_buff,                 in0, ff_sine_128, NELLY_BUF_LEN);
    s->fdsp->vector_fmul_reverse(s->in_buff + NELLY_BUF_LEN, in1, ff_sine_128, NELLY_BUF_LEN);
    s->mdct_fn(s->mdct_ctx, s->mdct_out, s->in_buff, sizeof(float));

    s->fdsp->vector_fmul        (s->in_buff,                 in1, ff_sine_128, NELLY_BUF_LEN);
    s->fdsp->vector_fmul_reverse(s->in_buff + NELLY_BUF_LEN, in2, ff_sine_128, NELLY_BUF_LEN);
    s->mdct_fn(s->mdct_ctx, s->mdct_out + NELLY_BUF_LEN, s->in_buff, sizeof(float));
}

typedef struct NellyMoserEncodeContext {
    AVCodecContext  *avctx;
    int             last_frame;
    AVFloatDSPContext *fdsp;
    AVTXContext    *mdct_ctx;
    av_tx_fn        mdct_fn;
    AudioFrameQueue afq;
    DECLARE_ALIGNED(32, float, mdct_out)[NELLY_SAMPLES];
    DECLARE_ALIGNED(32, float, in_buff)[NELLY_SAMPLES];
    DECLARE_ALIGNED(32, float, buf)[3 * NELLY_BUF_LEN];     ///< sample buffer
    float           (*opt )[OPT_SIZE];
    uint8_t         (*path)[OPT_SIZE];
} NellyMoserEncodeContext;

av_cold AVFloatDSPContext *avpriv_float_dsp_alloc(int bit_exact)
{
    AVFloatDSPContext *fdsp = av_mallocz(sizeof(AVFloatDSPContext));
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

#if ARCH_AARCH64
    ff_float_dsp_init_aarch64(fdsp);
#elif ARCH_ARM
    ff_float_dsp_init_arm(fdsp);
#elif ARCH_PPC
    ff_float_dsp_init_ppc(fdsp, bit_exact);
#elif ARCH_RISCV
    ff_float_dsp_init_riscv(fdsp);
#elif ARCH_X86
    ff_float_dsp_init_x86(fdsp);
#elif ARCH_MIPS
    ff_float_dsp_init_mips(fdsp);
#endif
    return fdsp;
}

av_cold void ff_float_dsp_init_neon(AVFloatDSPContext *fdsp)
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

av_cold void ff_float_dsp_init_vfp(AVFloatDSPContext *fdsp, int cpu_flags)
{
    if (have_vfp_vm(cpu_flags)) {
        fdsp->vector_fmul = ff_vector_fmul_vfp;
        fdsp->vector_fmul_window = ff_vector_fmul_window_vfp;
    }
    fdsp->vector_fmul_reverse = ff_vector_fmul_reverse_vfp;
    if (have_vfp_vm(cpu_flags))
        fdsp->butterflies_float = ff_butterflies_float_vfp;
}

av_cold void ff_float_dsp_init_arm(AVFloatDSPContext *fdsp)
{
    int cpu_flags = av_get_cpu_flags();

    if (have_vfp(cpu_flags))
        ff_float_dsp_init_vfp(fdsp, cpu_flags);
    if (have_neon(cpu_flags))
        ff_float_dsp_init_neon(fdsp);
}


/* Stub implementation for vector_fmul_c */
void vector_fmul_c(void) {}



/* Stub implementation for ff_vector_fmul_neon */
void ff_vector_fmul_neon(void) {}



/* Stub implementation for ff_vector_fmul_vfp */
void ff_vector_fmul_vfp(void) {}
