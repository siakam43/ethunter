/* CG-Bench fixture: fnptr-library/example_13 */
/* fnptr: s->out_transform, targets: equirect_to_xyz, cube3x2_to_xyz, cube1x6_to_xyz, cube6x1_to_xyz, eac_to_xyz, flat_to_xyz, dfisheye_to_xyz, barrel_to_xyz, stereographic_to_xyz, mercator_to_xyz, ball_to_xyz, hammer_to_xyz, sinusoidal_to_xyz, fisheye_to_xyz, pannini_to_xyz, cylindrical_to_xyz, cylindricalea_to_xyz, perspective_to_xyz, tetrahedron_to_xyz, barrelsplit_to_xyz, tspyramid_to_xyz, hequirect_to_xyz, equisolid_to_xyz, orthographic_to_xyz, octahedron_to_xyz */

static int v360_slice(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs)
{
    V360Context *s = ctx->priv;

    if (s->out_transpose)
        out_mask = s->out_transform(s, j, i, height, width, vec);
    else
        out_mask = s->out_transform(s, i, j, width, height, vec);
}

static int config_output(AVFilterLink *outlink)
{
    switch (s->out) {
    case EQUIRECTANGULAR:
        s->out_transform = equirect_to_xyz;
        prepare_out = prepare_equirect_out;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case CUBEMAP_3_2:
        s->out_transform = cube3x2_to_xyz;
        prepare_out = prepare_cube_out;
        w = lrintf(wf / 4.f * 3.f);
        h = lrintf(hf);
        break;
    case CUBEMAP_1_6:
        s->out_transform = cube1x6_to_xyz;
        prepare_out = prepare_cube_out;
        w = lrintf(wf / 4.f);
        h = lrintf(hf * 3.f);
        break;
    case CUBEMAP_6_1:
        s->out_transform = cube6x1_to_xyz;
        prepare_out = prepare_cube_out;
        w = lrintf(wf / 2.f * 3.f);
        h = lrintf(hf / 2.f);
        break;
    case EQUIANGULAR:
        s->out_transform = eac_to_xyz;
        prepare_out = prepare_eac_out;
        w = lrintf(wf);
        h = lrintf(hf / 8.f * 9.f);
        break;
    case FLAT:
        s->out_transform = flat_to_xyz;
        prepare_out = prepare_flat_out;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case DUAL_FISHEYE:
        s->out_transform = dfisheye_to_xyz;
        prepare_out = prepare_fisheye_out;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case BARREL:
        s->out_transform = barrel_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf / 4.f * 5.f);
        h = lrintf(hf);
        break;
    case STEREOGRAPHIC:
        s->out_transform = stereographic_to_xyz;
        prepare_out = prepare_stereographic_out;
        w = lrintf(wf);
        h = lrintf(hf * 2.f);
        break;
    case MERCATOR:
        s->out_transform = mercator_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf * 2.f);
        break;
    case BALL:
        s->out_transform = ball_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf * 2.f);
        break;
    case HAMMER:
        s->out_transform = hammer_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case SINUSOIDAL:
        s->out_transform = sinusoidal_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case FISHEYE:
        s->out_transform = fisheye_to_xyz;
        prepare_out = prepare_fisheye_out;
        w = lrintf(wf * 0.5f);
        h = lrintf(hf);
        break;
    case PANNINI:
        s->out_transform = pannini_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case CYLINDRICAL:
        s->out_transform = cylindrical_to_xyz;
        prepare_out = prepare_cylindrical_out;
        w = lrintf(wf);
        h = lrintf(hf * 0.5f);
        break;
    case CYLINDRICALEA:
        s->out_transform = cylindricalea_to_xyz;
        prepare_out = prepare_cylindricalea_out;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case PERSPECTIVE:
        s->out_transform = perspective_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf / 2.f);
        h = lrintf(hf);
        break;
    case TETRAHEDRON:
        s->out_transform = tetrahedron_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case BARREL_SPLIT:
        s->out_transform = barrelsplit_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf / 4.f * 3.f);
        h = lrintf(hf);
        break;
    case TSPYRAMID:
        s->out_transform = tspyramid_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf);
        break;
    case HEQUIRECTANGULAR:
        s->out_transform = hequirect_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf / 2.f);
        h = lrintf(hf);
        break;
    case EQUISOLID:
        s->out_transform = equisolid_to_xyz;
        prepare_out = prepare_equisolid_out;
        w = lrintf(wf);
        h = lrintf(hf * 2.f);
        break;
    case ORTHOGRAPHIC:
        s->out_transform = orthographic_to_xyz;
        prepare_out = prepare_orthographic_out;
        w = lrintf(wf);
        h = lrintf(hf * 2.f);
        break;
    case OCTAHEDRON:
        s->out_transform = octahedron_to_xyz;
        prepare_out = NULL;
        w = lrintf(wf);
        h = lrintf(hf * 2.f);
        break;
    default:
        av_log(ctx, AV_LOG_ERROR, "Specified output format is not handled.\n");
        return AVERROR_BUG;
    }
}


/* Stub implementation for equirect_to_xyz */
void equirect_to_xyz(void) {}



/* Stub implementation for cube3x2_to_xyz */
void cube3x2_to_xyz(void) {}



/* Stub implementation for cube1x6_to_xyz */
void cube1x6_to_xyz(void) {}



/* Stub implementation for cube6x1_to_xyz */
void cube6x1_to_xyz(void) {}



/* Stub implementation for eac_to_xyz */
void eac_to_xyz(void) {}



/* Stub implementation for flat_to_xyz */
void flat_to_xyz(void) {}



/* Stub implementation for dfisheye_to_xyz */
void dfisheye_to_xyz(void) {}



/* Stub implementation for barrel_to_xyz */
void barrel_to_xyz(void) {}



/* Stub implementation for stereographic_to_xyz */
void stereographic_to_xyz(void) {}



/* Stub implementation for mercator_to_xyz */
void mercator_to_xyz(void) {}



/* Stub implementation for ball_to_xyz */
void ball_to_xyz(void) {}



/* Stub implementation for hammer_to_xyz */
void hammer_to_xyz(void) {}



/* Stub implementation for sinusoidal_to_xyz */
void sinusoidal_to_xyz(void) {}



/* Stub implementation for fisheye_to_xyz */
void fisheye_to_xyz(void) {}



/* Stub implementation for pannini_to_xyz */
void pannini_to_xyz(void) {}



/* Stub implementation for cylindrical_to_xyz */
void cylindrical_to_xyz(void) {}



/* Stub implementation for cylindricalea_to_xyz */
void cylindricalea_to_xyz(void) {}



/* Stub implementation for perspective_to_xyz */
void perspective_to_xyz(void) {}



/* Stub implementation for tetrahedron_to_xyz */
void tetrahedron_to_xyz(void) {}



/* Stub implementation for barrelsplit_to_xyz */
void barrelsplit_to_xyz(void) {}



/* Stub implementation for tspyramid_to_xyz */
void tspyramid_to_xyz(void) {}



/* Stub implementation for hequirect_to_xyz */
void hequirect_to_xyz(void) {}



/* Stub implementation for equisolid_to_xyz */
void equisolid_to_xyz(void) {}



/* Stub implementation for orthographic_to_xyz */
void orthographic_to_xyz(void) {}



/* Stub implementation for octahedron_to_xyz */
void octahedron_to_xyz(void) {}
