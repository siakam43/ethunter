/* ET-Bench fixture: fnptr-library/example_13 */
/* fnptr: s->out_transform, targets: equirect_to_xyz, cube3x2_to_xyz, cube1x6_to_xyz, cube6x1_to_xyz, eac_to_xyz, flat_to_xyz, dfisheye_to_xyz, barrel_to_xyz, stereographic_to_xyz, mercator_to_xyz, ball_to_xyz, hammer_to_xyz, sinusoidal_to_xyz, fisheye_to_xyz, pannini_to_xyz, cylindrical_to_xyz, cylindricalea_to_xyz, perspective_to_xyz, tetrahedron_to_xyz, barrelsplit_to_xyz, tspyramid_to_xyz, hequirect_to_xyz, equisolid_to_xyz, orthographic_to_xyz, octahedron_to_xyz */
/* Pattern: library 360 video filter with output projection function pointer set by format */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AVERROR_BUG (-1)
#define AV_LOG_ERROR 16

typedef struct AVFilterLink {
    struct AVFilterContext *dst;
    int w, h;
} AVFilterLink;

typedef struct AVFilterContext {
    void *priv;
} AVFilterContext;

typedef float vec3[3];

enum OutFormat {
    EQUIRECTANGULAR = 0,
    CUBEMAP_3_2,
    CUBEMAP_1_6,
    CUBEMAP_6_1,
    EQUIANGULAR,
    FLAT,
    DUAL_FISHEYE,
    BARREL,
    STEREOGRAPHIC,
    MERCATOR,
    BALL,
    HAMMER,
    SINUSOIDAL,
    FISHEYE,
    PANNINI,
    CYLINDRICAL,
    CYLINDRICALEA,
    PERSPECTIVE,
    TETRAHEDRON,
    BARREL_SPLIT,
    TSPYRAMID,
    HEQUIRECTANGULAR,
    EQUISOLID,
    ORTHOGRAPHIC,
    OCTAHEDRON
};

typedef struct V360Context {
    int out;
    int out_transpose;
    int (*out_transform)(struct V360Context *s, int i, int j, int w, int h, vec3 vec);
    void *(*prepare_out)(struct V360Context *s, AVFilterLink *outlink);
} V360Context;

/* All projection transform functions */
static int equirect_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int cube3x2_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int cube1x6_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int cube6x1_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int eac_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int flat_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int dfisheye_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int barrel_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int stereographic_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int mercator_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int ball_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int hammer_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int sinusoidal_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int fisheye_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int pannini_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int cylindrical_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int cylindricalea_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int perspective_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int tetrahedron_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int barrelsplit_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int tspyramid_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int hequirect_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int equisolid_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int orthographic_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }
static int octahedron_to_xyz(V360Context *s, int i, int j, int w, int h, vec3 vec) { (void)s; (void)i; (void)j; (void)w; (void)h; (void)vec; return 0; }

void av_log(void *ctx, int level, const char *fmt, ...) {
    (void)ctx; (void)level; (void)fmt;
}

static void *prepare_equirect_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_cube_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_eac_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_flat_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_fisheye_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_stereographic_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_cylindrical_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_cylindricalea_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_equisolid_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }
static void *prepare_orthographic_out(V360Context *s, AVFilterLink *l) { (void)s; (void)l; return NULL; }

static int config_output(AVFilterLink *outlink)
{
    V360Context *s = outlink->dst->priv;
    int w = outlink->w, h = outlink->h;
    float wf = w, hf = h;
    void *prepare_out = NULL;
    int ret = 0;

    switch (s->out) {
    case EQUIRECTANGULAR:
        s->out_transform = equirect_to_xyz;
        prepare_out = prepare_equirect_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case CUBEMAP_3_2:
        s->out_transform = cube3x2_to_xyz;
        prepare_out = prepare_cube_out;
        w = (int)lrintf(wf / 4.f * 3.f);
        h = (int)lrintf(hf);
        break;
    case CUBEMAP_1_6:
        s->out_transform = cube1x6_to_xyz;
        prepare_out = prepare_cube_out;
        w = (int)lrintf(wf / 4.f);
        h = (int)lrintf(hf * 3.f);
        break;
    case CUBEMAP_6_1:
        s->out_transform = cube6x1_to_xyz;
        prepare_out = prepare_cube_out;
        w = (int)lrintf(wf / 2.f * 3.f);
        h = (int)lrintf(hf / 2.f);
        break;
    case EQUIANGULAR:
        s->out_transform = eac_to_xyz;
        prepare_out = prepare_eac_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf / 8.f * 9.f);
        break;
    case FLAT:
        s->out_transform = flat_to_xyz;
        prepare_out = prepare_flat_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case DUAL_FISHEYE:
        s->out_transform = dfisheye_to_xyz;
        prepare_out = prepare_fisheye_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case BARREL:
        s->out_transform = barrel_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf / 4.f * 5.f);
        h = (int)lrintf(hf);
        break;
    case STEREOGRAPHIC:
        s->out_transform = stereographic_to_xyz;
        prepare_out = prepare_stereographic_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 2.f);
        break;
    case MERCATOR:
        s->out_transform = mercator_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 2.f);
        break;
    case BALL:
        s->out_transform = ball_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 2.f);
        break;
    case HAMMER:
        s->out_transform = hammer_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case SINUSOIDAL:
        s->out_transform = sinusoidal_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case FISHEYE:
        s->out_transform = fisheye_to_xyz;
        prepare_out = prepare_fisheye_out;
        w = (int)lrintf(wf * 0.5f);
        h = (int)lrintf(hf);
        break;
    case PANNINI:
        s->out_transform = pannini_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case CYLINDRICAL:
        s->out_transform = cylindrical_to_xyz;
        prepare_out = prepare_cylindrical_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 0.5f);
        break;
    case CYLINDRICALEA:
        s->out_transform = cylindricalea_to_xyz;
        prepare_out = prepare_cylindricalea_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case PERSPECTIVE:
        s->out_transform = perspective_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf / 2.f);
        h = (int)lrintf(hf);
        break;
    case TETRAHEDRON:
        s->out_transform = tetrahedron_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case BARREL_SPLIT:
        s->out_transform = barrelsplit_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf / 4.f * 3.f);
        h = (int)lrintf(hf);
        break;
    case TSPYRAMID:
        s->out_transform = tspyramid_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf);
        break;
    case HEQUIRECTANGULAR:
        s->out_transform = hequirect_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf / 2.f);
        h = (int)lrintf(hf);
        break;
    case EQUISOLID:
        s->out_transform = equisolid_to_xyz;
        prepare_out = prepare_equisolid_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 2.f);
        break;
    case ORTHOGRAPHIC:
        s->out_transform = orthographic_to_xyz;
        prepare_out = prepare_orthographic_out;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 2.f);
        break;
    case OCTAHEDRON:
        s->out_transform = octahedron_to_xyz;
        prepare_out = NULL;
        w = (int)lrintf(wf);
        h = (int)lrintf(hf * 2.f);
        break;
    default:
        av_log(NULL, AV_LOG_ERROR, "Specified output format is not handled.\n");
        return AVERROR_BUG;
    }
    (void)prepare_out;
    return ret;
}
