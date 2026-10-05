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

    // Each triangle now gets its OWN 3 positions (no sharing),
    // so flat per-face color doesn't bleed into neighboring faces.
    obj->num_vertices = total_triangles * 3;
    obj->positions = malloc(sizeof(vec3) * obj->num_vertices);
    if (!obj->positions) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj);
        fast_obj_destroy(mesh);
        return NULL;
    }
    obj->colors = malloc(sizeof(Color) * obj->num_vertices);
    if (!obj->colors) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->positions);
        free(obj);
        fast_obj_destroy(mesh);
        return NULL;
    }
    obj->uvs = malloc(sizeof(vec2) * obj->num_vertices);
    if (!obj->uvs) {
        SDL_LogError(1, "Error: Failed to allocate UVs for '%s'", filepath);
        free(obj->colors);
        free(obj->positions);
        free(obj);
        fast_obj_destroy(mesh);
        return NULL;
    }
    obj->triangles = malloc(sizeof(Triangle) * obj->num_triangles);
    if (!obj->triangles) {
        SDL_LogError(1, "Error: Failed to generate ground mesh");
        free(obj->uvs);
        free(obj->colors);
        free(obj->positions);
        free(obj);
        fast_obj_destroy(mesh);
        return NULL;
    }
    obj->normals = malloc(sizeof(vec3) * obj->num_vertices);
    if (!obj->normals) {
        SDL_LogError(1, "Error: Failed to allocate triangle textures for '%s'", filepath);
        free(obj->triangles);
        free(obj->uvs);
        free(obj->colors);
        free(obj->positions);
        free(obj);
        fast_obj_destroy(mesh);
        return NULL;
    }
    obj->triangle_texture_indices = malloc(sizeof(Uint32) * obj->num_triangles);
    if (!obj->triangle_texture_indices) {
        SDL_LogError(1, "Error: Failed to allocate triangle textures for '%s'", filepath);
        free(obj->normals);
        free(obj->triangles);
        free(obj->uvs);
        free(obj->colors);
        free(obj->positions);
        free(obj);
        fast_obj_destroy(mesh);
        return NULL;
    }
    obj->texture_bank = texture_bank_create(1);
    
    for (Uint32 i = 0; i < obj->num_vertices; i++) {
        obj->uvs[i][0] = -1.f;
        obj->uvs[i][1] = -1.f;
    }
    for (Uint32 i = 0; i < obj->num_triangles; i++) {
        obj->triangle_texture_indices[i] = TEXTURE_NONE;
    }

    Uint32 index_cursor = 0;
    Uint32 tri_cursor = 0;
    Uint32 vert_cursor = 0;

    for (unsigned int f = 0; f < mesh->face_count; f++) {
        unsigned int face_verts = mesh->face_vertices[f];

        for (unsigned int v = 1; v < face_verts - 1; v++) {
            // Source positions from fast_obj (still 1-based, dummy 0 index)
            Uint32 pos0 = mesh->indices[index_cursor].p;
            Uint32 pos1 = mesh->indices[index_cursor + v].p;
            Uint32 pos2 = mesh->indices[index_cursor + v + 1].p;

            Uint32 tex0 = mesh->indices[index_cursor].t;
            Uint32 tex1 = mesh->indices[index_cursor + v].t;
            Uint32 tex2 = mesh->indices[index_cursor + v + 1].t;

            Uint32 norm0 = mesh->indices[index_cursor].n;
            Uint32 norm1 = mesh->indices[index_cursor + v].n;
            Uint32 norm2 = mesh->indices[index_cursor + v + 1].n;

            vec3 p0, p1, p2;
            p0[0] = mesh->positions[pos0 * 3 + 0];
            p0[1] = mesh->positions[pos0 * 3 + 1];
            p0[2] = mesh->positions[pos0 * 3 + 2];
            p1[0] = mesh->positions[pos1 * 3 + 0];
            p1[1] = mesh->positions[pos1 * 3 + 1];
            p1[2] = mesh->positions[pos1 * 3 + 2];
            p2[0] = mesh->positions[pos2 * 3 + 0];
            p2[1] = mesh->positions[pos2 * 3 + 1];
            p2[2] = mesh->positions[pos2 * 3 + 2];

            // Write 3 fresh positions for this triangle
            Uint32 i0 = vert_cursor++;
            Uint32 i1 = vert_cursor++;
            Uint32 i2 = vert_cursor++;

            glm_vec3_copy(p0, obj->positions[i0]);
            glm_vec3_copy(p1, obj->positions[i1]);
            glm_vec3_copy(p2, obj->positions[i2]);

            if (mesh->texcoords && tex0 != 0 && tex1 != 0 && tex2 != 0) {
                obj->uvs[i0][0] = mesh->texcoords[(tex0 * 2) + 0];
                obj->uvs[i0][1] = mesh->texcoords[(tex0 * 2) + 1];
                obj->uvs[i1][0] = mesh->texcoords[(tex1 * 2) + 0];
                obj->uvs[i1][1] = mesh->texcoords[(tex1 * 2) + 1];
                obj->uvs[i2][0] = mesh->texcoords[(tex2 * 2) + 0];
                obj->uvs[i2][1] = mesh->texcoords[(tex2 * 2) + 1];
            }

            if (mesh->normals && norm0 != 0 && norm1 != 0 && norm2 != 0) {
                obj->normals[i0][0] = mesh->normals[(norm0 * 3) + 0];
                obj->normals[i0][1] = mesh->normals[(norm0 * 3) + 1];
                obj->normals[i0][2] = mesh->normals[(norm0 * 3) + 2];
                obj->normals[i1][0] = mesh->normals[(norm1 * 3) + 0];
                obj->normals[i1][1] = mesh->normals[(norm1 * 3) + 1];
                obj->normals[i1][2] = mesh->normals[(norm1 * 3) + 2];
                obj->normals[i2][0] = mesh->normals[(norm2 * 3) + 0];
                obj->normals[i2][1] = mesh->normals[(norm2 * 3) + 1];
                obj->normals[i2][2] = mesh->normals[(norm2 * 3) + 2];
            }
            else {
                vec3 face_normal;
                calc_normal(p0, p1, p2, face_normal);
                glm_vec3_copy(face_normal, obj->normals[i0]);
                glm_vec3_copy(face_normal, obj->normals[i1]);
                glm_vec3_copy(face_normal, obj->normals[i2]);
            }
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
    vec3 normal, abs_normal;
    calc_normal(a, b, c, normal);
    glm_vec3_abs(normal, abs_normal);

    if (abs_normal[0] >= abs_normal[1] && abs_normal[0] >= abs_normal[2]) {
        return normal[0] > 0 ? (Color) { 255, 80, 80, 255 } : (Color) { 150, 0, 0, 255 };
    }
    else if (abs_normal[1] >= abs_normal[0] && abs_normal[1] >= abs_normal[2]) {
        return normal[1] > 0 ? (Color) { 80, 255, 80, 255 } : (Color) { 0, 150, 0, 255 };
    }
    else {
        return normal[2] > 0 ? (Color) { 80, 80, 255, 255 } : (Color) { 0, 0, 150, 255 };
    }
}
