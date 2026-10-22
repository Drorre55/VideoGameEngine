#ifndef EXR_LOADER_H
#define EXR_LOADER_H
#include "cglm/cglm.h"

#ifdef __cplusplus
extern "C" {
#endif
    /*
     * Loads an EXR normal map into a malloc'd array of w*h vec3, row-major.
     *  encoded_01 : 1 if the file stores normals as [0,1] (n*0.5+0.5), 0 if true [-1,1]
     *  flip_v     : 1 to flip rows vertically (row 0 becomes the bottom)
     * Normals are renormalized. Returns NULL on failure. Free with free().
     */
    vec3* load_exr_normals(const char* path, int* out_w, int* out_h,
        int encoded_01, int flip_v);

#ifdef __cplusplus
}
#endif
#endif