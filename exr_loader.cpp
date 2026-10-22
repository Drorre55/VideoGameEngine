#include "tinyexr.h"
#include "exr_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern "C" vec3* load_exr_normals(const char* path, int* out_w, int* out_h,
    int encoded_01, int flip_v)
{
    float* rgba = NULL;
    int w = 0, h = 0;
    const char* err = NULL;

    int ret = LoadEXR(&rgba, &w, &h, path, &err);
    if (ret != TINYEXR_SUCCESS) {
        fprintf(stderr, "EXR load failed (%s): %s\n", path, err ? err : "unknown");
        if (err) FreeEXRErrorMessage(err);
        return NULL;
    }

    vec3* out = (vec3*)malloc((size_t)w * h * sizeof(vec3));
    if (!out) { free(rgba); return NULL; }

    for (int y = 0; y < h; ++y) {
        int src_y = flip_v ? (h - 1 - y) : y;
        const float* src = rgba + (size_t)src_y * w * 4;   /* RGBA, 4 floats/pixel */
        vec3* dst = out + (size_t)y * w;

        for (int x = 0; x < w; ++x) {
            float nx = src[x * 4 + 0];
            float ny = src[x * 4 + 1];
            float nz = src[x * 4 + 2];

            if (encoded_01) {            /* [0,1] -> [-1,1] */
                nx = nx * 2.0f - 1.0f;
                ny = ny * 2.0f - 1.0f;
                nz = nz * 2.0f - 1.0f;
            }

            float len = sqrtf(nx * nx + ny * ny + nz * nz);
            if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
            else { nx = 0.0f; ny = 0.0f; nz = 1.0f; }  /* flat fallback */

            dst[x][0] = nx; dst[x][1] = ny; dst[x][2] = nz;
        }
    }

    free(rgba);                 /* LoadEXR's buffer is malloc'd */
    *out_w = w;
    *out_h = h;
    return out;
}