#define _CRT_SECURE_NO_WARNINGS
#include "world_objects.h"
#include <math.h>
#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"


WorldObjects* load_world_objects() {
    const char* filepath = "./renderer_test_scene.obj";
    SDL_Log("Loading objects");
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
    obj->colors = malloc(sizeof(Color) * obj->num_vertices);
    obj->triangles = malloc(sizeof(Triangle) * obj->num_triangles);

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

            memcpy(obj->vertices[i0], p0, sizeof(vec3));
            memcpy(obj->vertices[i1], p1, sizeof(vec3));
            memcpy(obj->vertices[i2], p2, sizeof(vec3));

            // Temp until import actual colors or texture from file 
            Color c = _face_color_from_normal(p0, p1, p2);
            obj->colors[i0] = c;
            obj->colors[i1] = c;
            obj->colors[i2] = c;

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

void free_world_objects(WorldObjects* world_objects)
{
    if (!world_objects) return;

    free(world_objects->vertices);
    free(world_objects->triangles);
    free(world_objects->colors);

    world_objects->vertices = NULL;
    world_objects->triangles = NULL;
    world_objects->colors = NULL;
    world_objects->num_vertices = 0;
    world_objects->num_triangles = 0;

    free(world_objects);
    world_objects = NULL;
}

WorldObjects* world_objects_deep_copy(const WorldObjects* src) {
    if (!src) return NULL;

    // 1. Allocate main WorldObjects container
    WorldObjects* copy = malloc(sizeof(WorldObjects));
    if (!copy) return NULL;

    // 2. Copy primitive counts
    copy->num_vertices = src->num_vertices;
    copy->num_triangles = src->num_triangles;

    // Initialize pointers to NULL so cleanup is safe if an allocation fails
    copy->vertices = NULL;
    copy->triangles = NULL;
    copy->colors = NULL;

    // 3. Allocate and copy contiguous vertex array
    if (src->num_vertices > 0 && src->vertices != NULL) {
        copy->vertices = malloc(sizeof(vec3) * src->num_vertices);
        if (!copy->vertices) {
            free_world_objects(copy);
            return NULL;
        }
        memcpy(copy->vertices, src->vertices, sizeof(vec3) * src->num_vertices);
    }

    // 4. Allocate and copy contiguous triangle array
    if (src->num_triangles > 0 && src->triangles != NULL) {
        copy->triangles = malloc(sizeof(Triangle) * src->num_triangles);
        if (!copy->triangles) {
            free_world_objects(copy);
            return NULL;
        }
        memcpy(copy->triangles, src->triangles, sizeof(Triangle) * src->num_triangles);
    }

    // 5. Allocate and copy contiguous color array
    if (src->num_triangles > 0 && src->colors != NULL) {
        copy->colors = malloc(sizeof(Color) * src->num_vertices);
        if (!copy->colors) {
            free_world_objects(copy);
            return NULL;
        }
        memcpy(copy->colors, src->colors, sizeof(Color) * src->num_vertices);
    }
    return copy;
}