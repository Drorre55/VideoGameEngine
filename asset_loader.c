#define _CRT_SECURE_NO_WARNINGS
#define FAST_OBJ_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif
#include "asset_loader.h"
#include "fast_obj.h"

WorldObjects* load_obj_file(const char* filepath)
{
    SDL_Log("Loading objects: %s", filepath);
    fastObjMesh* mesh = fast_obj_read(filepath);
    if (!mesh) {
        SDL_LogError(1, "Error: Failed to load OBJ file '%s'", filepath);
        return NULL;
    }
    WorldObjects* obj = malloc(sizeof(WorldObjects));
    if (!obj) {
        fast_obj_destroy(mesh);
        return NULL;
    }

    // Count triangles first
    Uint32 total_triangles = 0;
    for (unsigned int i = 0; i < mesh->face_count; i++) {
        total_triangles += (mesh->face_vertices[i] - 2);
    }
    obj->num_triangles = total_triangles;

    // Each triangle now gets its OWN 3 vertices (no sharing),
    // so flat per-face color doesn't bleed into neighboring faces.
    obj->num_vertices = total_triangles * 3;
    obj->vertices = malloc(sizeof(vec3) * obj->num_vertices);
    if (!obj->vertices) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj);
        return NULL;
    }
    obj->colors = malloc(sizeof(Color) * obj->num_vertices);
    if (!obj->colors) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->vertices);
        free(obj);
        return NULL;
    }
    obj->triangles = malloc(sizeof(Triangle) * obj->num_triangles);
    if (!obj->triangles) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->colors);
        free(obj->vertices);
        free(obj);
        return NULL;
    }

    Uint32 index_cursor = 0;
    Uint32 tri_cursor = 0;
    Uint32 vert_cursor = 0;

    for (unsigned int f = 0; f < mesh->face_count; f++) {
        unsigned int face_verts = mesh->face_vertices[f];

        for (unsigned int v = 1; v < face_verts - 1; v++) {
            // Source positions from fast_obj (still 1-based, dummy 0 index)
            Uint32 src0 = mesh->indices[index_cursor].p;
            Uint32 src1 = mesh->indices[index_cursor + v].p;
            Uint32 src2 = mesh->indices[index_cursor + v + 1].p;

            vec3 p0, p1, p2;
            p0[0] = mesh->positions[src0 * 3 + 0];
            p0[1] = mesh->positions[src0 * 3 + 1];
            p0[2] = mesh->positions[src0 * 3 + 2];
            p1[0] = mesh->positions[src1 * 3 + 0];
            p1[1] = mesh->positions[src1 * 3 + 1];
            p1[2] = mesh->positions[src1 * 3 + 2];
            p2[0] = mesh->positions[src2 * 3 + 0];
            p2[1] = mesh->positions[src2 * 3 + 1];
            p2[2] = mesh->positions[src2 * 3 + 2];

            // Write 3 fresh vertices for this triangle
            Uint32 i0 = vert_cursor++;
            Uint32 i1 = vert_cursor++;
            Uint32 i2 = vert_cursor++;

            glm_vec3_copy(p0, obj->vertices[i0]);
            glm_vec3_copy(p1, obj->vertices[i1]);
            glm_vec3_copy(p2, obj->vertices[i2]);

            // Temp until import actual colors or texture from file 
            Color c = _face_color_from_normal(p0, p1, p2);
            memcpy(&(obj->colors[i0]), &c, sizeof(Color));
            memcpy(&(obj->colors[i1]), &c, sizeof(Color));
            memcpy(&(obj->colors[i2]), &c, sizeof(Color));

            obj->triangles[tri_cursor].corner1_idx = i0;
            obj->triangles[tri_cursor].corner2_idx = i1;
            obj->triangles[tri_cursor].corner3_idx = i2;

            tri_cursor++;
        }
        index_cursor += face_verts;
    }
    fast_obj_destroy(mesh);
    return obj;
}

static Color _face_color_from_normal(vec3 a, vec3 b, vec3 c) {
    vec3 e1, e2, n;
    glm_vec3_sub(b, a, e1);
    glm_vec3_sub(c, a, e2);
    glm_vec3_cross(e1, e2, n);

    vec3 abs_n;
    glm_vec3_normalize(n);
    glm_vec3_abs(n, abs_n);

    if (abs_n[0] >= abs_n[1] && abs_n[0] >= abs_n[2]) {
        return n[0] > 0 ? (Color) { 255, 80, 80, 255 } : (Color) { 150, 0, 0, 255 };
    }
    else if (abs_n[1] >= abs_n[0] && abs_n[1] >= abs_n[2]) {
        return n[1] > 0 ? (Color) { 80, 255, 80, 255 } : (Color) { 0, 150, 0, 255 };
    }
    else {
        return n[2] > 0 ? (Color) { 80, 80, 255, 255 } : (Color) { 0, 0, 150, 255 };
    }
}
