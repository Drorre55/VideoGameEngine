#define _CRT_SECURE_NO_WARNINGS
#include "world_objects.h"
#include <math.h>
#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"


WorldObjects* load_world_objects() {
    const char* filepath = "./Assets/renderer_test_scene.obj";
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

    WorldObjects* floor_mesh = _generate_ground_mesh(50, 5);
    WorldObjects* all_world_objects[2] = { floor_mesh, obj };
    
    WorldObjects* world_objects = _concat_world_objects(all_world_objects, 2);
    free_world_objects(obj);
    free(floor_mesh);

    for (int i = 0; i < world_objects->num_vertices; i++) {
        glm_vec3_print(world_objects->vertices[i], stdout);
    }

    return world_objects;
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

WorldObjects* _generate_ground_mesh(Uint32 radius, Uint32 triangle_edge_size)
{
    Uint32 grid_row_count = 2 * radius / triangle_edge_size;
    Uint32 grid_col_count = grid_row_count;
    Uint32 num_triangles_in_row = grid_col_count * 2;
    Uint32 total_triangles = grid_row_count * num_triangles_in_row;
    // Seperated vertices per row
    Uint32 num_vertices_in_row = 2 * (grid_col_count + 1);
    Uint32 total_vertices = grid_row_count * num_vertices_in_row;

    WorldObjects* obj = malloc(sizeof(WorldObjects));
    if (!obj) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        return NULL;
    }

    // each row vertices advance from bottom -> up -> right_bottom -> up...
    // [0]         2   4   6   8
    //             1   3   5   7
    obj->num_vertices = total_vertices;
    obj->num_triangles = total_triangles;
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

    vec3 bottom_left = { 
        -(float)(radius), 
        0.0f, 
        -(float)(radius) 
    };
    vec3 up_step = { 0.0f, 0.0f, (float)triangle_edge_size };
    vec3 right_step = { (float)triangle_edge_size, 0.0f, 0.0f };
    for (Uint32 row = 0; row < grid_row_count; row++) {
        vec3 up_steps_from_origin, current_bottom, current_top;
        glm_vec3_scale(up_step, row, up_steps_from_origin);
        glm_vec3_add(bottom_left, up_steps_from_origin, current_bottom);
        glm_vec3_add(current_bottom, up_step, current_top);

        for (Uint32 col = 0; col < num_vertices_in_row - 1; col+=2) {
            glm_vec3_copy(current_bottom, obj->vertices[row * num_vertices_in_row + col]);
            glm_vec3_copy(current_top, obj->vertices[row * num_vertices_in_row + col + 1]);

            glm_vec3_add(current_bottom, right_step, current_bottom);
            glm_vec3_add(current_top, right_step, current_top);
        }
        SDL_Log("finished row: %d. current_top: ", row);
        glm_vec3_print(current_top, stdout);
    }
    // Temp all ground green
    for (Uint32 i = 0; i < total_vertices; i++) {
        obj->colors[i] = (Color){ 0, 150, 0, 255 };
    }
    for (Uint32 row = 0; row < grid_row_count; row++) {
        for (Uint32 col = 0; col < num_triangles_in_row; col++) {
            Uint32 triangle_idx = row * num_triangles_in_row + col;
            obj->triangles[triangle_idx].corner1_idx = row * num_vertices_in_row + col + 0;
            obj->triangles[triangle_idx].corner2_idx = row * num_vertices_in_row + col + 1;
            obj->triangles[triangle_idx].corner3_idx = row * num_vertices_in_row + col + 2;
        }
    }

    return obj;
}

WorldObjects* _concat_world_objects(WorldObjects** world_objects, Uint8 num_objects)
{
    WorldObjects* objects = world_objects_deep_copy(world_objects[0]);

    for (Uint8 i = 1; i < num_objects; i++) {
        Uint32 concat_num_vertices = objects->num_vertices + world_objects[i]->num_vertices;
        Uint32 concat_num_triangles = objects->num_triangles + world_objects[i]->num_triangles;

        vec3* temp_vertices = realloc(objects->vertices, concat_num_vertices * sizeof(vec3));
        if (!temp_vertices) {
            SDL_LogError(1, "Error: Failed to concat_world_objects");
            return NULL;
        }
        Triangle* temp_triangles = realloc(objects->triangles, concat_num_triangles * sizeof(Triangle));
        if (!temp_triangles) {
            SDL_LogError(1, "Error: Failed to concat_world_objects");
            free(temp_vertices);
            return NULL;
        }
        Color* temp_colors = realloc(objects->colors, concat_num_vertices * sizeof(Color));
        if (!temp_colors) {
            SDL_LogError(1, "Error: Failed to concat_world_objects");
            free(temp_vertices);
            free(temp_triangles);
            return NULL;
        }
        objects->vertices = temp_vertices;
        objects->triangles = temp_triangles;
        objects->colors = temp_colors;
        
        for (Uint32 j = 0; j < world_objects[i]->num_vertices; j++) {
            Uint32 concat_idx = objects->num_vertices + j;
            glm_vec3_copy(world_objects[i]->vertices[j], objects->vertices[concat_idx]);
            
            memcpy(&(objects->colors[concat_idx]), &(world_objects[i]->colors[j]), sizeof(Color));
        }
        for (Uint32 j = 0; j < world_objects[i]->num_triangles; j++) {
            Uint32 concat_idx = objects->num_triangles + j;
            Triangle world_triangle = world_objects[i]->triangles[j];
            objects->triangles[concat_idx].corner1_idx = 
                world_triangle.corner1_idx + objects->num_vertices;
            objects->triangles[concat_idx].corner2_idx =
                world_triangle.corner2_idx + objects->num_vertices;
            objects->triangles[concat_idx].corner3_idx =
                world_triangle.corner3_idx + objects->num_vertices;
        }

        objects->num_vertices = concat_num_vertices;
        objects->num_triangles = concat_num_triangles;
    }

    return objects;
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
    if (src->num_vertices > 0 && src->colors != NULL) {
        copy->colors = malloc(sizeof(Color) * src->num_vertices);
        if (!copy->colors) {
            free_world_objects(copy);
            return NULL;
        }
        memcpy(copy->colors, src->colors, sizeof(Color) * src->num_vertices);
    }
    return copy;
}