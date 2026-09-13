#define _CRT_SECURE_NO_WARNINGS
#include "world_objects.h"
#include "asset_loader.h"
#include <math.h>


WorldObjects* load_world_objects() {
    WorldObjects* test_scene = load_obj_file("./Assets/renderer_test_scene.obj");
    //WorldObjects* tree = load_obj_file("./Assets/tree/tree1.obj");
    //_scale_world_objects(tree, 0.5);

    WorldObjects* terrain_mesh = _generate_terrain_mesh(100, 10, 10.f, "./Assets/forest_ground_06_4k.blend/textures/forest_ground_06_diff_4k.jpg");
    WorldObjects* all_world_objects[2] = { test_scene, terrain_mesh };//, tree };
    
    WorldObjects* world_objects = _concat_world_objects(all_world_objects, 2);
    free_world_objects(test_scene, false);
    //free_world_objects(tree);
    free(terrain_mesh);

    return world_objects;
}

void _scale_world_objects(WorldObjects* world_objects, float scale) {
    for (Uint32 i = 0; i < world_objects->num_vertices; i++) {
        glm_vec3_scale(world_objects->vertices[i], scale, world_objects->vertices[i]);
    }
}

WorldObjects* _generate_terrain_mesh(Uint32 radius, Uint32 triangle_edge_size, float texture_tile_radius, const char* texture_path) {
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
    obj->uvs = malloc(sizeof(vec2) * obj->num_vertices);
    if (!obj->uvs) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->colors);
        free(obj->vertices);
        free(obj);
        return NULL;
    }
    obj->triangles = malloc(sizeof(Triangle) * obj->num_triangles);
    if (!obj->triangles) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->uvs); 
        free(obj->colors);
        free(obj->vertices);
        free(obj);
        return NULL;
    }
    obj->triangle_texture_indices = malloc(sizeof(Uint32) * obj->num_triangles);
    if (!obj->triangle_texture_indices) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->triangles);
        free(obj->uvs);
        free(obj->colors);
        free(obj->vertices);
        free(obj);
        return NULL;
    }

    vec2* gradients3 = perlin_gradients(3, 42);
    vec2* gradients6 = perlin_gradients(6, 42);
    vec2* gradients12 = perlin_gradients(12, 42);
    vec2* gradients24 = perlin_gradients(24, 42);

    // Preperation for normalization [0, 1]
    vec2 normalize_numerator, normalize_denominator, normalized_point;
    vec2 max_bounds = { (float)(radius), (float)(radius) };
    glm_vec2_scale(max_bounds, 2.f, normalize_denominator);

    // Set vertices position
    vec3 bottom_left = { -(float)(radius), 0.0f, -(float)(radius) };
    vec3 up_step = { 0.0f, 0.0f, (float)triangle_edge_size };
    vec3 right_step = { (float)triangle_edge_size, 0.0f, 0.0f };
    vec3 up_steps_from_origin, current_bottom, current_top;
    glm_vec3_copy(bottom_left, current_bottom);
    glm_vec3_add(current_bottom, up_step, current_top);
    for (Uint32 col = 0; col < num_vertices_in_row - 1; col += 2) {
        glm_vec3_copy(current_bottom, obj->vertices[col]);
        vec2 vertex2d = { (float)obj->vertices[col][0], (float)obj->vertices[col][2] };
        glm_vec2_add(vertex2d, max_bounds, normalize_numerator);
        glm_vec2_div(normalize_numerator, normalize_denominator, normalized_point);
        obj->vertices[col][1] = 100 * (
            perlin_noise(normalized_point, 3, gradients3) 
            + 0.5 * perlin_noise(normalized_point, 6, gradients6)
            + 0.25 * perlin_noise(normalized_point, 12, gradients12)
            + 0.125 * perlin_noise(normalized_point, 24, gradients24));
        obj->uvs[col][0] = _normalize_to_01(obj->vertices[col][0], (2.f * texture_tile_radius));
        obj->uvs[col][1] = _normalize_to_01(obj->vertices[col][2], (2.f * texture_tile_radius));
        
        glm_vec3_copy(current_top, obj->vertices[col + 1]);
        vec2 vertex2d_next = { (float)obj->vertices[col + 1][0], (float)obj->vertices[col + 1][2] };
        glm_vec2_add(vertex2d_next, max_bounds, normalize_numerator);
        glm_vec2_div(normalize_numerator, normalize_denominator, normalized_point);
        obj->vertices[col + 1][1] = 100 * (
            perlin_noise(normalized_point, 3, gradients3)
            + 0.5 * perlin_noise(normalized_point, 6, gradients6)
            + 0.25 * perlin_noise(normalized_point, 12, gradients12)
            + 0.125 * perlin_noise(normalized_point, 24, gradients24));
        obj->uvs[col + 1][0] = _normalize_to_01(obj->vertices[col + 1][0], (2.f * texture_tile_radius));
        obj->uvs[col + 1][1] = _normalize_to_01(obj->vertices[col + 1][2], (2.f * texture_tile_radius));

        glm_vec3_add(current_bottom, right_step, current_bottom);
        glm_vec3_add(current_top, right_step, current_top);
    }
    for (Uint32 row = 1; row < grid_row_count; row++) {
        glm_vec3_scale(up_step, row, up_steps_from_origin);
        glm_vec3_add(bottom_left, up_steps_from_origin, current_bottom);
        glm_vec3_add(current_bottom, up_step, current_top);

        for (Uint32 col = 0; col < num_vertices_in_row - 1; col+=2) {
            Uint32 bottom_idx = row * num_vertices_in_row + col;
            Uint32 prev_top_idx = (row - 1) * num_vertices_in_row + col + 1;
            Uint32 top_idx = bottom_idx + 1;
            glm_vec3_copy(obj->vertices[prev_top_idx], obj->vertices[bottom_idx]);
            glm_vec2_copy(obj->uvs[prev_top_idx], obj->uvs[bottom_idx]);

            glm_vec3_copy(current_top, obj->vertices[top_idx]);
            vec2 vertex2d = { (float)obj->vertices[top_idx][0], (float)obj->vertices[top_idx][2] };
            glm_vec2_add(vertex2d, max_bounds, normalize_numerator);
            glm_vec2_div(normalize_numerator, normalize_denominator, normalized_point);
            obj->vertices[row * num_vertices_in_row + col + 1][1] = 100 * (
                perlin_noise(normalized_point, 3, gradients3)
                + 0.5 * perlin_noise(normalized_point, 6, gradients6)
                + 0.25 * perlin_noise(normalized_point, 12, gradients12)
                + 0.125 * perlin_noise(normalized_point, 24, gradients24));
            obj->uvs[top_idx][0] = _normalize_to_01(obj->vertices[top_idx][0], (2.f * texture_tile_radius));
            obj->uvs[top_idx][1] = _normalize_to_01(obj->vertices[top_idx][2], (2.f * texture_tile_radius));

            glm_vec3_add(current_top, right_step, current_top);
        }
    }
    // Set triangles' vertices' indices
    vec3 normal;
    for (Uint32 row = 0; row < grid_row_count; row++) {
        for (Uint32 col = 0; col < num_triangles_in_row; col+=2) {
            Uint32 triangle_idx = row * num_triangles_in_row + col;
            obj->triangles[triangle_idx].corner1_idx = row * num_vertices_in_row + col + 1;
            obj->triangles[triangle_idx].corner2_idx = row * num_vertices_in_row + col;
            obj->triangles[triangle_idx].corner3_idx = row * num_vertices_in_row + col + 2;
        }
        for (Uint32 col = 1; col < num_triangles_in_row + 1; col+=2) {
            Uint32 triangle_idx = row * num_triangles_in_row + col;
            obj->triangles[triangle_idx].corner1_idx = row * num_vertices_in_row + col;
            obj->triangles[triangle_idx].corner2_idx = row * num_vertices_in_row + col + 1;
            obj->triangles[triangle_idx].corner3_idx = row * num_vertices_in_row + col + 2;
        }
    }

    obj->texture_bank = texture_bank_create(1);
    Uint32 terrain_texture_index = texture_bank_add_from_file(&obj->texture_bank, texture_path);
    for (Uint32 i = 0; i < obj->num_triangles; i++) {
        obj->triangle_texture_indices[i] = terrain_texture_index;
    }
    // Set fallback to green
    for (Uint32 i = 0; i < total_vertices; i++) {
        obj->colors[i] = (Color){ 150, 0, 0, 255 };
    }
    return obj;
}

float _normalize_to_01(float a, float b) {
    return (a - b * floorf(a / b)) / b;
}

WorldObjects* _concat_world_objects(WorldObjects** world_objects, Uint8 num_objects)
{
    WorldObjects* objects = world_objects_deep_copy(world_objects[0], false);

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
        vec2* temp_uvs = realloc(objects->uvs, concat_num_vertices * sizeof(vec2));
        if (!temp_uvs) {
            SDL_LogError(1, "Error: Failed to concat_world_objects");
            free(temp_vertices);
            free(temp_triangles);
            free(temp_colors);
            return NULL;
        }
        Uint32* temp_triangle_texture_indices = realloc(objects->triangle_texture_indices, concat_num_triangles * sizeof(Uint32));
        if (!temp_triangle_texture_indices) {
            SDL_LogError(1, "Error: Failed to concat_world_objects");
            free(temp_vertices);
            free(temp_triangles);
            free(temp_colors);
            free(temp_uvs);
            return NULL;
        }
        objects->vertices = temp_vertices;
        objects->triangles = temp_triangles;
        objects->colors = temp_colors;
        objects->uvs = temp_uvs;
        objects->triangle_texture_indices = temp_triangle_texture_indices;
        Uint32 objects_texture_count = objects->texture_bank.count;

        for (Uint32 j = 0; j < world_objects[i]->num_vertices; j++) {
            Uint32 concat_idx = objects->num_vertices + j;
            glm_vec3_copy(world_objects[i]->vertices[j], objects->vertices[concat_idx]);
            memcpy(&(objects->colors[concat_idx]), &(world_objects[i]->colors[j]), sizeof(Color));
            glm_vec2_copy(world_objects[i]->uvs[j], objects->uvs[concat_idx]);
        }
        for (Uint32 j = 0; j < world_objects[i]->texture_bank.count; j++) {
            TiledTexture tex_copy = texture_clone(world_objects[i]->texture_bank.textures[j]);
            texture_bank_add(&objects->texture_bank, tex_copy);
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
            
            if (world_objects[i]->triangle_texture_indices[j] != TEXTURE_NONE) {
                objects->triangle_texture_indices[concat_idx] = 
                    world_objects[i]->triangle_texture_indices[j] 
                    + objects_texture_count;
            }
            else {
                objects->triangle_texture_indices[concat_idx] =
                    world_objects[i]->triangle_texture_indices[j];
            }
        }

        objects->num_vertices = concat_num_vertices;
        objects->num_triangles = concat_num_triangles;
    }

    return objects;
}

void free_world_objects(WorldObjects* world_objects, bool deep_free_textures)
{
    if (!world_objects) return;

    free(world_objects->vertices);
    free(world_objects->triangles);
    free(world_objects->colors);
    free(world_objects->uvs);
    free(world_objects->triangle_texture_indices);
    if (deep_free_textures)
        texture_bank_free(&world_objects->texture_bank);
    else
        world_objects->texture_bank.textures = NULL;

    world_objects->vertices = NULL;
    world_objects->triangles = NULL;
    world_objects->colors = NULL;
    world_objects->uvs = NULL;
    world_objects->triangle_texture_indices = NULL;
    world_objects->num_vertices = 0;
    world_objects->num_triangles = 0;

    free(world_objects);
    world_objects = NULL;
}

WorldObjects* world_objects_deep_copy(const WorldObjects* src, bool deep_copy_textures) {
    if (!src) return NULL;

    WorldObjects* copy = malloc(sizeof(WorldObjects));
    if (!copy) return NULL;

    copy->num_vertices = src->num_vertices;
    copy->num_triangles = src->num_triangles;

    copy->vertices = NULL;
    copy->triangles = NULL;
    copy->colors = NULL;
    copy->texture_bank = deep_copy_textures ? texture_bank_deep_copy(&src->texture_bank) : src->texture_bank;

    if (src->num_vertices > 0 && src->vertices != NULL) {
        copy->vertices = malloc(sizeof(vec3) * src->num_vertices);
        if (!copy->vertices) {
            free_world_objects(copy, deep_copy_textures);
            return NULL;
        }
        memcpy(copy->vertices, src->vertices, sizeof(vec3) * src->num_vertices);
    }
    if (src->num_triangles > 0 && src->triangles != NULL) {
        copy->triangles = malloc(sizeof(Triangle) * src->num_triangles);
        if (!copy->triangles) {
            free_world_objects(copy, deep_copy_textures);
            return NULL;
        }
        memcpy(copy->triangles, src->triangles, sizeof(Triangle) * src->num_triangles);
    }
    if (src->num_vertices > 0 && src->colors != NULL) {
        copy->colors = malloc(sizeof(Color) * src->num_vertices);
        if (!copy->colors) {
            free_world_objects(copy, deep_copy_textures);
            return NULL;
        }
        memcpy(copy->colors, src->colors, sizeof(Color) * src->num_vertices);
    }
    if (src->num_vertices > 0 && src->uvs != NULL) {
        copy->uvs = malloc(sizeof(vec2) * src->num_vertices);
        if (!copy->uvs) {
            free_world_objects(copy, deep_copy_textures);
            return NULL;
        }
        memcpy(copy->uvs, src->uvs, sizeof(vec2) * src->num_vertices);
    }
    if (src->num_triangles > 0 && src->triangle_texture_indices != NULL) {
        copy->triangle_texture_indices = malloc(sizeof(Uint32) * src->num_triangles);
        if (!copy->triangle_texture_indices) {
            free_world_objects(copy, deep_copy_textures);
            return NULL;
        }
        memcpy(copy->triangle_texture_indices, src->triangle_texture_indices, sizeof(Uint32) * src->num_triangles);
    }
    return copy;
}

void world_objects_assign_to_copy(const WorldObjects* src, WorldObjects* dest) {
    if (!src) return;

    dest->num_vertices = src->num_vertices;
    dest->num_triangles = src->num_triangles;
    dest->texture_bank = src->texture_bank;

    memcpy(dest->vertices, src->vertices, sizeof(vec3) * src->num_vertices);
    memcpy(dest->triangles, src->triangles, sizeof(Triangle) * src->num_triangles);
    memcpy(dest->colors, src->colors, sizeof(Color) * src->num_vertices);
    memcpy(dest->uvs, src->uvs, sizeof(vec2) * src->num_vertices);
    memcpy(dest->triangle_texture_indices, src->triangle_texture_indices, sizeof(Uint32) * src->num_triangles);
}