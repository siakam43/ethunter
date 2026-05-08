/* CG-Bench fixture: fnptr-global-array/example_2 */
/* fnptr: quantize_and_encode_band_cost_rtz_arr : quantize_and_encode_band_cost_arr)[cb], targets: quantize_and_encode_band_cost_ZERO, quantize_and_encode_band_cost_SQUAD, quantize_and_encode_band_cost_UQUAD, quantize_and_encode_band_cost_SPAIR, quantize_and_encode_band_cost_UPAIR, quantize_and_encode_band_cost_ESC, quantize_and_encode_band_cost_NONE, quantize_and_encode_band_cost_NOISE, quantize_and_encode_band_cost_STEREO, quantize_and_encode_band_cost_ESC_RTZ */

static inline void quantize_and_encode_band(struct AACEncContext *s, PutBitContext *pb,
                                            const float *in, float *out, int size, int scale_idx,
                                            int cb, const float lambda, int rtz)
{
    (rtz ? quantize_and_encode_band_cost_rtz_arr : quantize_and_encode_band_cost_arr)[cb](s, pb, in, out, NULL, size, scale_idx, cb,
                                     lambda, INFINITY, NULL, NULL);
}

static const quantize_and_encode_band_func quantize_and_encode_band_cost_arr[] =
{
    quantize_and_encode_band_cost_ZERO,
    quantize_and_encode_band_cost_SQUAD,
    quantize_and_encode_band_cost_SQUAD,
    quantize_and_encode_band_cost_UQUAD,
    quantize_and_encode_band_cost_UQUAD,
    quantize_and_encode_band_cost_SPAIR,
    quantize_and_encode_band_cost_SPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_ESC,
    quantize_and_encode_band_cost_NONE,     /* CB 12 doesn't exist */
    quantize_and_encode_band_cost_NOISE,
    quantize_and_encode_band_cost_STEREO,
    quantize_and_encode_band_cost_STEREO,
};

static const quantize_and_encode_band_func quantize_and_encode_band_cost_rtz_arr[] =
{
    quantize_and_encode_band_cost_ZERO,
    quantize_and_encode_band_cost_SQUAD,
    quantize_and_encode_band_cost_SQUAD,
    quantize_and_encode_band_cost_UQUAD,
    quantize_and_encode_band_cost_UQUAD,
    quantize_and_encode_band_cost_SPAIR,
    quantize_and_encode_band_cost_SPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_UPAIR,
    quantize_and_encode_band_cost_ESC_RTZ,
    quantize_and_encode_band_cost_NONE,     /* CB 12 doesn't exist */
    quantize_and_encode_band_cost_NOISE,
    quantize_and_encode_band_cost_STEREO,
    quantize_and_encode_band_cost_STEREO,
};


/* Stub implementation for quantize_and_encode_band_cost_ZERO */
void quantize_and_encode_band_cost_ZERO(void) {}



/* Stub implementation for quantize_and_encode_band_cost_SQUAD */
void quantize_and_encode_band_cost_SQUAD(void) {}



/* Stub implementation for quantize_and_encode_band_cost_UQUAD */
void quantize_and_encode_band_cost_UQUAD(void) {}



/* Stub implementation for quantize_and_encode_band_cost_SPAIR */
void quantize_and_encode_band_cost_SPAIR(void) {}



/* Stub implementation for quantize_and_encode_band_cost_UPAIR */
void quantize_and_encode_band_cost_UPAIR(void) {}



/* Stub implementation for quantize_and_encode_band_cost_ESC */
void quantize_and_encode_band_cost_ESC(void) {}



/* Stub implementation for quantize_and_encode_band_cost_NONE */
void quantize_and_encode_band_cost_NONE(void) {}



/* Stub implementation for quantize_and_encode_band_cost_NOISE */
void quantize_and_encode_band_cost_NOISE(void) {}



/* Stub implementation for quantize_and_encode_band_cost_STEREO */
void quantize_and_encode_band_cost_STEREO(void) {}



/* Stub implementation for quantize_and_encode_band_cost_ESC_RTZ */
void quantize_and_encode_band_cost_ESC_RTZ(void) {}
