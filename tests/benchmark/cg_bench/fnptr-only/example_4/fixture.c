/* CG-Bench fixture: fnptr-only/example_4 */
/* fnptr: conv, targets: gray8aToPacked32, gray8aToPacked32_1, gray8aToPacked24, sws_convertPalette8ToPacked32, sws_convertPalette8ToPacked24 */

static int palToRgbWrapper(SwsContext *c, const uint8_t *src[], int srcStride[],
                           int srcSliceY, int srcSliceH, uint8_t *dst[],
                           int dstStride[])
{
    const enum AVPixelFormat srcFormat = c->srcFormat;
    const enum AVPixelFormat dstFormat = c->dstFormat;
    void (*conv)(const uint8_t *src, uint8_t *dst, int num_pixels,
                 const uint8_t *palette) = NULL;
    int i;
    uint8_t *dstPtr = dst[0] + dstStride[0] * srcSliceY;
    const uint8_t *srcPtr = src[0];

    if (srcFormat == AV_PIX_FMT_YA8) {
        switch (dstFormat) {
        case AV_PIX_FMT_RGB32  : conv = gray8aToPacked32; break;
        case AV_PIX_FMT_BGR32  : conv = gray8aToPacked32; break;
        case AV_PIX_FMT_BGR32_1: conv = gray8aToPacked32_1; break;
        case AV_PIX_FMT_RGB32_1: conv = gray8aToPacked32_1; break;
        case AV_PIX_FMT_RGB24  : conv = gray8aToPacked24; break;
        case AV_PIX_FMT_BGR24  : conv = gray8aToPacked24; break;
        }
    } else if (usePal(srcFormat)) {
        switch (dstFormat) {
        case AV_PIX_FMT_RGB32  : conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_BGR32  : conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_BGR32_1: conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_RGB32_1: conv = sws_convertPalette8ToPacked32; break;
        case AV_PIX_FMT_RGB24  : conv = sws_convertPalette8ToPacked24; break;
        case AV_PIX_FMT_BGR24  : conv = sws_convertPalette8ToPacked24; break;
        }
    }

    if (!conv)
        av_log(c, AV_LOG_ERROR, "internal error %s -> %s converter\n",
               av_get_pix_fmt_name(srcFormat), av_get_pix_fmt_name(dstFormat));
    else {
        for (i = 0; i < srcSliceH; i++) {
            conv(srcPtr, dstPtr, c->srcW, (uint8_t *) c->pal_rgb);
            srcPtr += srcStride[0];
            dstPtr += dstStride[0];
        }
    }

    return srcSliceH;
}


/* Stub implementation for gray8aToPacked32 */
void gray8aToPacked32(void) {}



/* Stub implementation for gray8aToPacked32_1 */
void gray8aToPacked32_1(void) {}



/* Stub implementation for gray8aToPacked24 */
void gray8aToPacked24(void) {}



/* Stub implementation for sws_convertPalette8ToPacked32 */
void sws_convertPalette8ToPacked32(void) {}



/* Stub implementation for sws_convertPalette8ToPacked24 */
void sws_convertPalette8ToPacked24(void) {}
