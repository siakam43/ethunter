/* CG-Bench fixture: fnptr-global-array/example_4 */
/* fnptr: trc_func, targets: trc_bt709, trc_gamma22, trc_gamma28, trc_smpte240M, trc_linear, trc_log, trc_log_sqrt, trc_iec61966_2_4, trc_bt1361, trc_iec61966_2_1, trc_smpte_st2084, trc_smpte_st428_1, trc_arib_std_b67 */

static av_cold int decode_init(AVCodecContext *avctx)
{
    EXRContext *s = avctx->priv_data;
    ...
    av_csp_trc_function trc_func = NULL;

    ff_init_half2float_tables(&s->h2f_tables);

    s->avctx              = avctx;

    ff_exrdsp_init(&s->dsp);
    ...

    trc_func = av_csp_trc_func_from_id(s->apply_trc_type);
    if (trc_func) {
        for (i = 0; i < 65536; ++i) {
            t.i = half2float(i, &s->h2f_tables);
            t.f = trc_func(t.f);
            s->gamma_table[i] = t;
        }
        ...
    }
}

static const av_csp_trc_function trc_funcs[AVCOL_TRC_NB] = {
    [AVCOL_TRC_BT709] = trc_bt709,
    [AVCOL_TRC_GAMMA22] = trc_gamma22,
    [AVCOL_TRC_GAMMA28] = trc_gamma28,
    [AVCOL_TRC_SMPTE170M] = trc_bt709,
    [AVCOL_TRC_SMPTE240M] = trc_smpte240M,
    [AVCOL_TRC_LINEAR] = trc_linear,
    [AVCOL_TRC_LOG] = trc_log,
    [AVCOL_TRC_LOG_SQRT] = trc_log_sqrt,
    [AVCOL_TRC_IEC61966_2_4] = trc_iec61966_2_4,
    [AVCOL_TRC_BT1361_ECG] = trc_bt1361,
    [AVCOL_TRC_IEC61966_2_1] = trc_iec61966_2_1,
    [AVCOL_TRC_BT2020_10] = trc_bt709,
    [AVCOL_TRC_BT2020_12] = trc_bt709,
    [AVCOL_TRC_SMPTE2084] = trc_smpte_st2084,
    [AVCOL_TRC_SMPTE428] = trc_smpte_st428_1,
    [AVCOL_TRC_ARIB_STD_B67] = trc_arib_std_b67,
};

av_csp_trc_function av_csp_trc_func_from_id(enum AVColorTransferCharacteristic trc)
{
    av_csp_trc_function func;
    if (trc >= AVCOL_TRC_NB)
        return NULL;
    func = trc_funcs[trc];
    if (!func)
        return NULL;
    return func;
}


/* Stub implementation for trc_bt709 */
void trc_bt709(void) {}



/* Stub implementation for trc_gamma22 */
void trc_gamma22(void) {}



/* Stub implementation for trc_gamma28 */
void trc_gamma28(void) {}



/* Stub implementation for trc_smpte240M */
void trc_smpte240M(void) {}



/* Stub implementation for trc_linear */
void trc_linear(void) {}



/* Stub implementation for trc_log */
void trc_log(void) {}



/* Stub implementation for trc_log_sqrt */
void trc_log_sqrt(void) {}



/* Stub implementation for trc_iec61966_2_4 */
void trc_iec61966_2_4(void) {}



/* Stub implementation for trc_bt1361 */
void trc_bt1361(void) {}



/* Stub implementation for trc_iec61966_2_1 */
void trc_iec61966_2_1(void) {}



/* Stub implementation for trc_smpte_st2084 */
void trc_smpte_st2084(void) {}



/* Stub implementation for trc_smpte_st428_1 */
void trc_smpte_st428_1(void) {}



/* Stub implementation for trc_arib_std_b67 */
void trc_arib_std_b67(void) {}
