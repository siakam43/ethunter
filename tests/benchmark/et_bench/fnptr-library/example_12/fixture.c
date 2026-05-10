/* ET-Bench fixture: fnptr-library/example_12 */
/* fnptr: s->vectorscope, targets: vectorscope8, vectorscope16 */
/* Pattern: library filter context with depth-based function pointer selection */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AVERROR(e) (-(e))
#define AVERROR_BUG (-1)
#define AVERROR_NOMEM (-2)
#define AVCOL_SPC_SMPTE170M 5
#define AVCOL_SPC_BT470BG 5
#define AVCOL_SPC_BT709 1

typedef struct AVFilterLink {
    struct AVFilterContext *dst;
    int format;
    int w, h;
    struct AVFilterLink **outputs;
} AVFilterLink;

typedef struct AVFilterContext {
    void *priv;
} AVFilterContext;

typedef struct AVFrame {
    uint8_t *data[4];
    int linesize[4];
    int colorspace;
} AVFrame;

typedef struct AVPixFmtDescriptor { int dummy; } AVPixFmtDescriptor;

const AVPixFmtDescriptor *av_pix_fmt_desc_get(int format);
int av_frame_copy_props(AVFrame *out, AVFrame *in);
AVFrame *ff_get_video_buffer(AVFilterLink *outlink, int w, int h);
void av_frame_free(AVFrame **frame);
int ff_filter_frame(AVFilterLink *outlink, AVFrame *out);

typedef struct VectorscopeContext {
    AVFilterContext *ctx;
    int size;
    int depth;
    int colorspace;
    int cs;
    int bgopacity;
    float tint[2];
    float ftint[2];
    float fintensity;
    float intensity;
    uint8_t bg_color[4];
    int pd;
    int x, y;
    void (*vectorscope)(struct VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd);
    void (*graticulef)(struct VectorscopeContext *s, AVFrame *out, int x, int y, int pd, int cs);
} VectorscopeContext;

static void vectorscope8(VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd) {
    (void)s; (void)in; (void)out; (void)pd;
}

static void vectorscope16(VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd) {
    (void)s; (void)in; (void)out; (void)pd;
}

static int config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    VectorscopeContext *s = ctx->priv;

    if (s->size == 256)
        s->vectorscope = vectorscope8;
    else
        s->vectorscope = vectorscope16;

    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    VectorscopeContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    AVFrame *out;
    int plane;

    s->bg_color[3] = s->bgopacity * (s->size - 1);
    s->tint[0] = .5f * (s->ftint[0] + 1.f) * (s->size - 1);
    s->tint[1] = .5f * (s->ftint[1] + 1.f) * (s->size - 1);
    s->intensity = s->fintensity * (s->size - 1);

    if (s->colorspace) {
        s->cs = (s->depth - 8) * 2 + s->colorspace - 1;
    } else {
        switch (in->colorspace) {
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            s->cs = (s->depth - 8) * 2 + 0;
            break;
        case AVCOL_SPC_BT709:
        default:
            s->cs = (s->depth - 8) * 2 + 1;
        }
    }

    out = ff_get_video_buffer(outlink, outlink->w, outlink->h);
    if (!out) {
        av_frame_free(&in);
        return AVERROR_NOMEM;
    }
    av_frame_copy_props(out, in);

    s->vectorscope(s, in, out, s->pd);
    s->graticulef(s, out, s->x, s->y, s->pd, s->cs);

    for (plane = 0; plane < 4; plane++) {
        if (out->data[plane]) {
            out->data[plane] += (s->size - 1) * out->linesize[plane];
            out->linesize[plane] = -out->linesize[plane];
        }
    }

    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
}

const AVPixFmtDescriptor *av_pix_fmt_desc_get(int f) { (void)f; return NULL; }
int av_frame_copy_props(AVFrame *o, AVFrame *i) { (void)o; (void)i; return 0; }
AVFrame *ff_get_video_buffer(AVFilterLink *l, int w, int h) { (void)l; (void)w; (void)h; return NULL; }
void av_frame_free(AVFrame **f) { (void)f; }
int ff_filter_frame(AVFilterLink *l, AVFrame *o) { (void)l; (void)o; return 0; }
