#define _CRT_SECURE_NO_WARNINGS
#include "world_objects.h"
#include "asset_loader.h"
#include <math.h>


WorldObjects* load_world_objects() {
    WorldObjects* test_scene = load_obj_file("./Assets/renderer_test_scene.obj");
    WorldObjects* tree = load_obj_file("./Assets/tree/tree 1.obj");
    scale_world_objects(tree, 0.5);

    WorldObjects* floor_mesh = _generate_ground_mesh(50, 5);
    WorldObjects* all_world_objects[3] = { floor_mesh, test_scene, tree };
    
    WorldObjects* world_objects = _concat_world_objects(all_world_objects, 3);
    free_world_objects(test_scene);
    free_world_objects(tree);
    free(floor_mesh);

    return world_objects;
}

void scale_world_objects(WorldObjects* world_objects, float scale) {
    for (Uint32 i = 0; i < world_objects->num_vertices; i++) {
        glm_vec3_scale(world_objects->vertices[i], scale, world_objects->vertices[i]);
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
    // Set vertices position
    vec3 bottom_left = { -(float)(radius), 0.0f, -(float)(radius) };
    vec3 up_step = { 0.0f, 0.0f, (float)triangle_edge_size };
    vec3 right_step = { (float)triangle_edge_size, 0.0f, 0.0f };
    srand(0);
    vec3 up_steps_from_origin, current_bottom, current_top;
    glm_vec3_copy(bottom_left, current_bottom);
    glm_vec3_add(current_bottom, up_step, current_top);
    for (Uint32 col = 0; col < num_vertices_in_row - 1; col += 2) {
        glm_vec3_copy(current_bottom, obj->vertices[col]);
        obj->vertices[col][1] = (float)(rand() % 40) / 10 - 2 + 1;
        glm_vec3_copy(current_top, obj->vertices[col + 1]);
        obj->vertices[col + 1][1] = (float)(rand() % 40) / 10 - 2 + 1;

        glm_vec3_add(current_bottom, right_step, current_bottom);
        glm_vec3_add(current_top, right_step, current_top);
    }
    for (Uint32 row = 1; row < grid_row_count; row++) {
        glm_vec3_scale(up_step, row, up_steps_from_origin);
        glm_vec3_add(bottom_left, up_steps_from_origin, current_bottom);
        glm_vec3_add(current_bottom, up_step, current_top);

        for (Uint32 col = 0; col < num_vertices_in_row - 1; col+=2) {
            glm_vec3_copy(
                obj->vertices[(row - 1) * num_vertices_in_row + col + 1], 
                obj->vertices[row * num_vertices_in_row + col]
            );
            glm_vec3_copy(current_top, obj->vertices[row * num_vertices_in_row + col + 1]);
            obj->vertices[row * num_vertices_in_row + col + 1][1] = (rand() % 40) / 10 - 2 + 1;

            glm_vec3_add(current_top, right_step, current_top);
        }
    }
    // Set triangles' vertices' indices
    for (Uint32 row = 0; row < grid_row_count; row++) {
        for (Uint32 col = 0; col < num_triangles_in_row; col++) {
            Uint32 triangle_idx = row * num_triangles_in_row + col;
            obj->triangles[triangle_idx].corner1_idx = row * num_vertices_in_row + col + 0;
            obj->triangles[triangle_idx].corner2_idx = row * num_vertices_in_row + col + 1;
            obj->triangles[triangle_idx].corner3_idx = row * num_vertices_in_row + col + 2;
        }
    }
    // Temporary set all ground green
    for (Uint32 i = 0; i < total_vertices; i++) {
        obj->colors[i] = (Color){ 0, 150, 0, 255 };
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