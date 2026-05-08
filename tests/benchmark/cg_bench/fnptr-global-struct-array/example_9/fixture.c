/* CG-Bench fixture: fnptr-global-struct-array/example_9 */
/* fnptr: speex_modes[s->mode].decode, targets: nb_decode, sb_decode */

static int speex_decode_frame(AVCodecContext *avctx, AVFrame *frame,
                              int *got_frame_ptr, AVPacket *avpkt)
{
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
        if (avctx->ch_layout.nb_channels == 2)
            speex_decode_stereo(dst + i * s->frame_size, s->frame_size, &s->stereo);
        if (get_bits_left(&s->gb) < 5 ||
            show_bits(&s->gb, 5) == 15) {
            frames_per_packet = i + 1;
            break;
        }
    }

    dst = (float *)frame->extended_data[0];
    s->fdsp->vector_fmul_scalar(dst, dst, scale, frame->nb_samples * frame->ch_layout.nb_channels);
    frame->nb_samples = s->frame_size * frames_per_packet;

    *got_frame_ptr = 1;

    return (get_bits_count(&s->gb) + 7) >> 3;
}

static const SpeexMode speex_modes[SPEEX_NB_MODES] = {
    {
        .modeID = 0,
        .decode = nb_decode,
        .frame_size = NB_FRAME_SIZE,
        .subframe_size = NB_SUBFRAME_SIZE,
        .lpc_size = NB_ORDER,
        .submodes = {
            NULL, &nb_submode1, &nb_submode2, &nb_submode3, &nb_submode4,
            &nb_submode5, &nb_submode6, &nb_submode7, &nb_submode8
        },
        .default_submode = 5,
    },
    {
        .modeID = 1,
        .decode = sb_decode,
        .frame_size = NB_FRAME_SIZE,
        .subframe_size = NB_SUBFRAME_SIZE,
        .lpc_size = 8,
        .folding_gain = 0.9f,
        .submodes = {
            NULL, &wb_submode1, &wb_submode2, &wb_submode3, &wb_submode4
        },
        .default_submode = 3,
    },
    {
        .modeID = 2,
        .decode = sb_decode,
        .frame_size = 320,
        .subframe_size = 80,
        .lpc_size = 8,
        .folding_gain = 0.7f,
        .submodes = {
            NULL, &wb_submode1
        },
        .default_submode = 1,
    },
};


/* Stub implementation for nb_decode */
void nb_decode(void) {}



/* Stub implementation for sb_decode */
void sb_decode(void) {}
