/* ET-Bench fixture: fnptr-only/example_12 */
/* fnptr: deinterleaveBytes, targets: deinterleaveBytes_c */
/* Source: FFmpeg-style NV12 to planar conversion */

#include <stdlib.h>
#include <string.h>

typedef enum {
    AV_PIX_FMT_NONE = -1,
    AV_PIX_FMT_NV12,
    AV_PIX_FMT_NV21,
    AV_PIX_FMT_NB
} AVPixelFormat;

typedef struct SwsContext {
    AVPixelFormat srcFormat;
    AVPixelFormat dstFormat;
    int srcW;
    int chrSrcW;
} SwsContext;

typedef void (*deinterleave_func_t)(const unsigned char *src,
                                     unsigned char *dst1,
                                     unsigned char *dst2,
                                     int width, int height,
                                     int srcStride,
                                     int dst1Stride, int dst2Stride);

deinterleave_func_t deinterleaveBytes;

static void deinterleaveBytes_c(const unsigned char *src,
                                 unsigned char *dst1,
                                 unsigned char *dst2,
                                 int width, int height,
                                 int srcStride,
                                 int dst1Stride, int dst2Stride)
{
    const unsigned char *sp = src;
    unsigned char *dp1 = dst1;
    unsigned char *dp2 = dst2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dp1[x] = sp[x * 2];
            dp2[x] = sp[x * 2 + 1];
        }
        sp += srcStride;
        dp1 += dst1Stride;
        dp2 += dst2Stride;
    }
}

static void copyPlane(const unsigned char *src, int srcStride,
                       int srcSliceY, int srcSliceH, int width,
                       unsigned char *dst, int dstStride)
{
    const unsigned char *sp = src + srcStride * srcSliceY;
    unsigned char *dp = dst;

    for (int y = 0; y < srcSliceH; y++) {
        memcpy(dp, sp, width);
        sp += srcStride;
        dp += dstStride;
    }
}

static int nv12ToPlanarWrapper(SwsContext *c, const unsigned char *src[],
                               int srcStride[], int srcSliceY,
                               int srcSliceH, unsigned char *dstParam[],
                               int dstStride[])
{
    unsigned char *dst1 = dstParam[1] + dstStride[1] * srcSliceY / 2;
    unsigned char *dst2 = dstParam[2] + dstStride[2] * srcSliceY / 2;

    copyPlane(src[0], srcStride[0], srcSliceY, srcSliceH, c->srcW,
              dstParam[0], dstStride[0]);

    if (c->srcFormat == AV_PIX_FMT_NV12)
        deinterleaveBytes(src[1], dst1, dst2, c->chrSrcW, (srcSliceH + 1) / 2,
                          srcStride[1], dstStride[1], dstStride[2]);
    else
        deinterleaveBytes(src[1], dst2, dst1, c->chrSrcW, (srcSliceH + 1) / 2,
                          srcStride[1], dstStride[2], dstStride[1]);

    return srcSliceH;
}

static void rgb2rgb_init_c(void)
{
    deinterleaveBytes = deinterleaveBytes_c;
}
