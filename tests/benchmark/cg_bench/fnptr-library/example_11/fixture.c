/* CG-Bench fixture: fnptr-library/example_11 */
/* fnptr: synth->synth_filter_float, targets: synth_filter_sse2, synth_filter_avx, synth_filter_fma3 */

static void sub_qmf32_float_c(SynthFilterContext *synth,
                              AVTXContext *imdct,
                              av_tx_fn imdct_fn,
                              float *pcm_samples,
                              int32_t **subband_samples_lo,
                              int32_t **subband_samples_hi,
                              float *hist1, int *offset, float *hist2,
                              const float *filter_coeff, ptrdiff_t npcmblocks,
                              float scale)
{
    LOCAL_ALIGNED_32(float, input, [32]);
    int i, j;

    for (j = 0; j < npcmblocks; j++) {
        // Load in one sample from each subband
        for (i = 0; i < 32; i++) {
            if ((i - 1) & 2)
                input[i] = -subband_samples_lo[i][j];
            else
                input[i] =  subband_samples_lo[i][j];
        }

        // One subband sample generates 32 interpolated ones
        synth->synth_filter_float(imdct, hist1, offset,
                                  hist2, filter_coeff,
                                  pcm_samples, input, scale, imdct_fn);
        pcm_samples += 32;
    }
}

av_cold void ff_synth_filter_init_x86(SynthFilterContext *s)
{
#if HAVE_X86ASM
    int cpu_flags = av_get_cpu_flags();

    if (EXTERNAL_SSE2(cpu_flags)) {
        s->synth_filter_float = synth_filter_sse2;
    }
    if (EXTERNAL_AVX_FAST(cpu_flags)) {
        s->synth_filter_float = synth_filter_avx;
    }
    if (EXTERNAL_FMA3_FAST(cpu_flags)) {
        s->synth_filter_float = synth_filter_fma3;
    }
#endif /* HAVE_X86ASM */
}

av_cold void ff_synth_filter_init(SynthFilterContext *c)
{
    c->synth_filter_float    = synth_filter_float;
    c->synth_filter_float_64 = synth_filter_float_64;
    c->synth_filter_fixed    = synth_filter_fixed;
    c->synth_filter_fixed_64 = synth_filter_fixed_64;

#if ARCH_AARCH64
    ff_synth_filter_init_aarch64(c);
#elif ARCH_ARM
    ff_synth_filter_init_arm(c);
#elif ARCH_X86
    ff_synth_filter_init_x86(c);
#endif
}

av_cold int ff_dca_core_init(DCACoreDecoder *s)
{
    int ret;
    float scale = 1.0f;

    if (!(s->float_dsp = avpriv_float_dsp_alloc(0)))
        return -1;
    if (!(s->fixed_dsp = avpriv_alloc_fixed_dsp(0)))
        return -1;

    ff_dcadct_init(&s->dcadct);

    if ((ret = av_tx_init(&s->imdct[0], &s->imdct_fn[0], AV_TX_FLOAT_MDCT,
                          1, 32, &scale, 0)) < 0)
        return ret;

    if ((ret = av_tx_init(&s->imdct[1], &s->imdct_fn[1], AV_TX_FLOAT_MDCT,
                          1, 64, &scale, 0)) < 0)
        return ret;

    ff_synth_filter_init(&s->synth);

    s->x96_rand = 1;
    return 0;
}

av_cold void ff_dcadsp_init(DCADSPContext *s)
{
    s->decode_hf     = decode_hf_c;
    s->decode_joint  = decode_joint_c;

    s->lfe_fir_float[0] = lfe_fir0_float_c;
    s->lfe_fir_float[1] = lfe_fir1_float_c;
    s->lfe_x96_float    = lfe_x96_float_c;
    s->sub_qmf_float[0] = sub_qmf32_float_c;
    s->sub_qmf_float[1] = sub_qmf64_float_c;

    s->lfe_fir_fixed    = lfe_fir_fixed_c;
    s->lfe_x96_fixed    = lfe_x96_fixed_c;
    s->sub_qmf_fixed[0] = sub_qmf32_fixed_c;
    s->sub_qmf_fixed[1] = sub_qmf64_fixed_c;

    s->decor   = decor_c;

    s->dmix_sub_xch   = dmix_sub_xch_c;
    s->dmix_sub       = dmix_sub_c;
    s->dmix_add       = dmix_add_c;
    s->dmix_scale     = dmix_scale_c;
    s->dmix_scale_inv = dmix_scale_inv_c;

    s->assemble_freq_bands = assemble_freq_bands_c;

    s->lbr_bank = lbr_bank_c;
    s->lfe_iir = lfe_iir_c;

#if ARCH_X86
    ff_dcadsp_init_x86(s);
#endif
}

tatic int filter_frame_float(DCACoreDecoder *s, AVFrame *frame)
{
    AVCodecContext *avctx = s->avctx;
    int x96_nchannels = 0, x96_synth = 0;
    int i, n, ch, ret, spkr, nsamples, nchannels;
    float *output_samples[DCA_SPEAKER_COUNT] = { NULL }, *ptr;
    const float *filter_coeff;

    if (s->ext_audio_mask & (DCA_CSS_X96 | DCA_EXSS_X96)) {
        x96_nchannels = s->x96_nchannels;
        x96_synth = 1;
    }

    // Filter primary channels
    for (ch = 0; ch < s->nchannels; ch++) {
        // Map this primary channel to speaker
        spkr = map_prm_ch_to_spkr(s, ch);
        if (spkr < 0)
            return AVERROR(EINVAL);

        // Filter bank reconstruction
        s->dcadsp->sub_qmf_float[x96_synth](
            &s->synth,
            s->imdct[x96_synth],
            s->imdct_fn[x96_synth],
            output_samples[spkr],
            s->subband_samples[ch],
            ch < x96_nchannels ? s->x96_subband_samples[ch] : NULL,
            s->dcadsp_data[ch].u.flt.hist1,
            &s->dcadsp_data[ch].offset,
            s->dcadsp_data[ch].u.flt.hist2,
            filter_coeff,
            s->npcmblocks,
            1.0f / (1 << (17 - x96_synth)));
    }
}


/* Stub implementation for synth_filter_sse2 */
void synth_filter_sse2(void) {}



/* Stub implementation for synth_filter_avx */
void synth_filter_avx(void) {}



/* Stub implementation for synth_filter_fma3 */
void synth_filter_fma3(void) {}
