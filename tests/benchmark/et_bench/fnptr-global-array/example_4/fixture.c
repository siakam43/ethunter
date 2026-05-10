/*
 * et_bench fixture: fnptr-global-array/example_4
 *
 * Scenario: EXR decoder transfer characteristic function dispatch.
 * A global array trc_funcs[] maps color transfer characteristic IDs to
 * tone response curve functions. decode_init() indexes trc_funcs directly
 * and calls through the resulting function pointer.
 */

#include <stdint.h>

/* --- Types --- */

typedef double (*av_csp_trc_function)(double v);

/* --- Target functions (13 unique callees) --- */

static double trc_bt709(double v) { return v <= 0.018 ? 4.5 * v : 1.0993 * v - 0.0993; }
static double trc_gamma22(double v) { return v > 0.0 ? v * v : 0.0; }
static double trc_gamma28(double v) { return v > 0.0 ? v * v * v : 0.0; }
static double trc_smpte240M(double v) { return v <= 0.0 ? v : v * v; }
static double trc_linear(double v) { return v; }
static double trc_log(double v) { return v > 0.0 ? v : 0.0; }
static double trc_log_sqrt(double v) { return v > 0.0 ? v : 0.0; }
static double trc_iec61966_2_4(double v) { return v <= -0.0031308 ? 12.92 * v : 0.0; }
static double trc_bt1361(double v) { return v <= 0.018 ? 4.5 * v : 1.0993 * v - 0.0993; }
static double trc_iec61966_2_1(double v) { return v <= -0.0031308 ? 12.92 * v : 0.0; }
static double trc_smpte_st2084(double v) { return v >= 0.0 ? v : 0.0; }
static double trc_smpte_st428_1(double v) { return v >= 0.0 ? v : 0.0; }
static double trc_arib_std_b67(double v) { return v <= 0.5 ? v : v; }

/* --- Global function pointer array (positional, 16 entries with NULLs) --- */

static const av_csp_trc_function trc_funcs[16] = {
    NULL,          /* 0: unspecified */
    trc_bt709,     /* 1: BT709 */
    trc_gamma22,   /* 2: GAMMA22 */
    trc_gamma28,   /* 3: GAMMA28 */
    trc_smpte240M, /* 4: SMPTE240M */
    trc_linear,    /* 5: LINEAR */
    trc_log,       /* 6: LOG */
    trc_log_sqrt,  /* 7: LOG_SQRT */
    trc_iec61966_2_4, /* 8: IEC61966_2_4 */
    trc_bt1361,    /* 9: BT1361_ECG */
    trc_iec61966_2_1, /* 10: IEC61966_2_1 */
    trc_smpte_st2084, /* 11: SMPTE2084 */
    trc_smpte_st428_1, /* 12: SMPTE428 */
    trc_arib_std_b67, /* 13: ARIB_STD_B67 */
    NULL,          /* 14: reserved */
    NULL,          /* 15: reserved */
};

/* --- Caller: indexes trc_funcs directly and calls through --- */

typedef struct {
    uint16_t gamma_table[256];
} EXRContext;

static int decode_init(EXRContext *s)
{
    int i, j;
    double t = 0.0;

    for (j = 0; j < 16; j++) {
        if (trc_funcs[j]) {
            for (i = 0; i < 16; i++) {
                t = trc_funcs[j](t + (double)i / 16.0);
                s->gamma_table[i + j * 16] = (uint16_t)(t * 65535.0);
            }
        }
    }
    return 0;
}
