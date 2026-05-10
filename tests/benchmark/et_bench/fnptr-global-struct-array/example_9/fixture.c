/* ET-Bench fixture: fnptr-global-struct-array/example_9 */
/* fnptr: speex_modes[s->mode].decode, targets: nb_decode, sb_decode */
/* Pattern: Speex codec mode table with per-mode decode function pointer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define NB_FRAME_SIZE 160
#define NB_SUBFRAME_SIZE 40
#define NB_ORDER 10
#define SPEEX_NB_MODES 3

typedef struct AVCodecContext {
    void *priv_data;
    int ch_layout_nb_channels;
} AVCodecContext;

typedef struct AVFrame {
    uint8_t **extended_data;
    int nb_samples;
} AVFrame;

typedef struct AVPacket {
    const uint8_t *data;
    int size;
} AVPacket;

typedef struct GetBitContext {
    const uint8_t *buffer;
    int size_in_bits;
    int index;
} GetBitContext;

typedef struct SpeexSubmode {
    int mode;
} SpeexSubmode;

static SpeexSubmode nb_submode1, nb_submode2, nb_submode3, nb_submode4;
static SpeexSubmode nb_submode5, nb_submode6, nb_submode7, nb_submode8;
static SpeexSubmode wb_submode1, wb_submode2, wb_submode3, wb_submode4;

typedef struct SpeexMode {
    int modeID;
    int (*decode)(AVCodecContext *avctx, void *st, GetBitContext *gb, float *dst);
    int frame_size;
    int subframe_size;
    int lpc_size;
    float folding_gain;
    SpeexSubmode *submodes[10];
    int default_submode;
} SpeexMode;

typedef struct SpeexContext {
    int mode;
    int frame_size;
    int frames_per_packet;
    int pkt_size;
    void *st[SPEEX_NB_MODES];
    SpeexSubmode stereo;
    GetBitContext gb;
    void *fdsp;
} SpeexContext;

/* Target decode functions */
static int nb_decode(AVCodecContext *avctx, void *st, GetBitContext *gb, float *dst) {
    (void)avctx; (void)st; (void)gb; (void)dst;
    return 0;
}

static int sb_decode(AVCodecContext *avctx, void *st, GetBitContext *gb, float *dst) {
    (void)avctx; (void)st; (void)gb; (void)dst;
    return 0;
}

/* Global mode table */
static const SpeexMode speex_modes[SPEEX_NB_MODES] = {
    { 0, nb_decode, NB_FRAME_SIZE, NB_SUBFRAME_SIZE, NB_ORDER, 0.0f,
      {NULL, &nb_submode1, &nb_submode2, &nb_submode3, &nb_submode4,
       &nb_submode5, &nb_submode6, &nb_submode7, &nb_submode8},
      5 },
    { 1, sb_decode, NB_FRAME_SIZE, NB_SUBFRAME_SIZE, 8, 0.9f,
      {NULL, &wb_submode1, &wb_submode2, &wb_submode3, &wb_submode4},
      3 },
    { 2, sb_decode, 320, 80, 8, 0.7f,
      {NULL, &wb_submode1},
      1 },
};

/* Helpers */
static int init_get_bits8(GetBitContext *gb, const uint8_t *data, int size) {
    gb->buffer = data;
    gb->size_in_bits = size * 8;
    gb->index = 0;
    return 0;
}

static int ff_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags) {
    (void)avctx; (void)flags;
    frame->nb_samples = 320;
    return 0;
}

static int get_bits_left(GetBitContext *gb) { return gb->size_in_bits - gb->index; }
static int show_bits(GetBitContext *gb, int n) { (void)n; return 0; }
static int get_bits_count(GetBitContext *gb) { return gb->index; }

static void speex_decode_stereo(float *dst, int frame_size, SpeexSubmode *stereo) {
    (void)dst; (void)frame_size; (void)stereo;
}

#define FFALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

/* Caller: speex_modes[s->mode].decode */
static int speex_decode_frame(AVCodecContext *avctx, AVFrame *frame,
                              int *got_frame_ptr, AVPacket *avpkt) {
    SpeexContext *s = avctx->priv_data;
    int frames_per_packet = s->frames_per_packet;
    const float scale = 1.f / 32768.f;
    int buf_size = avpkt->size;
    float *dst;
    int ret;

    if (s->pkt_size && avpkt->size == 62)
        buf_size = s->pkt_size;
    if ((ret = init_get_bits8(&s->gb, avpkt->data, buf_size)) < 0)
        return ret;

    frame->nb_samples = FFALIGN(s->frame_size * frames_per_packet, 4);
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    dst = (float *)frame->extended_data[0];
    for (int i = 0; i < frames_per_packet; i++) {
        ret = speex_modes[s->mode].decode(avctx, &s->st[s->mode], &s->gb, dst + i * s->frame_size);
        if (ret < 0)
            return ret;
        if (avctx->ch_layout_nb_channels == 2)
            speex_decode_stereo(dst + i * s->frame_size, s->frame_size, &s->stereo);
        if (get_bits_left(&s->gb) < 5 ||
            show_bits(&s->gb, 5) == 15) {
            frames_per_packet = i + 1;
            break;
        }
    }

    dst = (float *)frame->extended_data[0];
    /* vector_fmul_scalar omitted for brevity */
    frame->nb_samples = s->frame_size * frames_per_packet;

    *got_frame_ptr = 1;

    return (get_bits_count(&s->gb) + 7) >> 3;
}
