/* CG-Bench fixture: fnptr-library/example_12 */
/* fnptr: s->vectorscope, targets: vectorscope8, vectorscope16 */

static int config_input(AVFilterLink *inlink)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(inlink->format);
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
    AVFilterContext *ctx  = inlink->dst;
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
        return AVERROR(ENOMEM);
    }
    av_frame_copy_props(out, in);

    s->vectorscope(s, in, out, s->pd);
    s->graticulef(s, out, s->x, s->y, s->pd, s->cs);

    for (plane = 0; plane < 4; plane++) {
        if (out->data[plane]) {
            out->data[plane]    += (s->size - 1) * out->linesize[plane];
            out->linesize[plane] = -out->linesize[plane];
        }
    }

    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
}

static void vectorscope8(VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd);
static void vectorscope16(VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd);


/* Stub implementation for vectorscope8 */
void vectorscope8(VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd) {}



/* Stub implementation for vectorscope16 */
void vectorscope16(VectorscopeContext *s, AVFrame *in, AVFrame *out, int pd) {}
