/*
 * et_bench fixture: fnptr-global-array/example_2
 *
 * Scenario: AAC encoder band cost dispatch via two parallel function pointer
 * arrays (normal and "return-to-zero" variants). The caller selects which
 * array to use based on the rtz flag, then indexes by codebook (cb).
 */

#include <math.h>

/* --- Types --- */
typedef void (*quantize_and_encode_band_func)(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy);

/* --- Target functions (10 unique callees) --- */

static void quantize_and_encode_band_cost_ZERO(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_SQUAD(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_UQUAD(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_SPAIR(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_UPAIR(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_ESC(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_NONE(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_NOISE(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_STEREO(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

static void quantize_and_encode_band_cost_ESC_RTZ(
    void *s, void *pb, const float *in, float *out, void *samples,
    int size, int scale_idx, int cb, float lambda, float upperlim,
    void *residual, void *energy)
{
	(void)s; (void)pb; (void)in; (void)out; (void)samples;
	(void)size; (void)scale_idx; (void)cb; (void)lambda;
	(void)upperlim; (void)residual; (void)energy;
}

/* --- Global function pointer arrays --- */

static const quantize_and_encode_band_func quantize_and_encode_band_cost_arr[] = {
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

static const quantize_and_encode_band_func quantize_and_encode_band_cost_rtz_arr[] = {
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

/* --- Caller: selects array by rtz flag, indexes by cb --- */

static void quantize_and_encode_band(void *s, void *pb,
                                     const float *in, float *out, int size, int scale_idx,
                                     int cb, const float lambda, int rtz)
{
    if (rtz) {
        quantize_and_encode_band_cost_rtz_arr[cb](
            s, pb, in, out, NULL, size, scale_idx, cb,
            lambda, INFINITY, NULL, NULL);
    } else {
        quantize_and_encode_band_cost_arr[cb](
            s, pb, in, out, NULL, size, scale_idx, cb,
            lambda, INFINITY, NULL, NULL);
    }
}
