#include "rasterizer.h"
#include "transformation_utils.h"
#include "cglm/cglm.h"
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define CORNERS 3


void transform_to_pixel_space(WorldObjects* on_screen_objects, Uint32 frame_width, Uint32 frame_height)
{
	for (int i = 0; i < on_screen_objects->num_vertices; i++) {
		vec3* position = on_screen_objects->positions[i];
		(*position)[0] = (*position)[0] * frame_width;
		(*position)[1] = (*position)[1] * frame_height;
	}
}

void rasterize_objects_to_frame(Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height, WorldObjects* on_screen_objects, vec3 light_direction) {
	memset(z_buffer, 0, sizeof(float) * frame_width * frame_height);

	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
        _draw_triangle_SIMD(i, on_screen_objects, light_direction, frame, z_buffer, frame_width, frame_height);
		//_draw_triangle_legacy(i, on_screen_objects, light_direction, frame, z_buffer, frame_width, frame_height);
	}
}

static inline void _draw_triangle_SIMD(Uint32 triangle_index, WorldObjects* world_objects, vec3 light_direction, Uint32* frame_buffer, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
    vec3* positions = world_objects->positions;
    Color* colors = world_objects->colors;
    vec2* uvs = world_objects->uvs;
    vec3* normals = world_objects->normals;

    Triangle triangle = world_objects->triangles[triangle_index];

    RenderVertex v0 = {
        .x = positions[triangle.corner1_idx][0],
        .y = positions[triangle.corner1_idx][1],
        .z = positions[triangle.corner1_idx][2],
        .u = uvs[triangle.corner1_idx][0],
        .v = uvs[triangle.corner1_idx][1],
        .r = (float)colors[triangle.corner1_idx].r,
        .g = (float)colors[triangle.corner1_idx].g,
        .b = (float)colors[triangle.corner1_idx].b,
        .a = (float)colors[triangle.corner1_idx].a,
        .nx = normals[triangle.corner1_idx][0],
        .ny = normals[triangle.corner1_idx][1],
        .nz = normals[triangle.corner1_idx][2],
    };
    RenderVertex v1 = {
        .x = positions[triangle.corner2_idx][0],
        .y = positions[triangle.corner2_idx][1],
        .z = positions[triangle.corner2_idx][2],
        .u = uvs[triangle.corner2_idx][0],
        .v = uvs[triangle.corner2_idx][1],
        .r = (float)colors[triangle.corner2_idx].r,
        .g = (float)colors[triangle.corner2_idx].g,
        .b = (float)colors[triangle.corner2_idx].b,
        .a = (float)colors[triangle.corner2_idx].a,
        .nx = normals[triangle.corner2_idx][0],
        .ny = normals[triangle.corner2_idx][1],
        .nz = normals[triangle.corner2_idx][2],
    };
    RenderVertex v2 = {
        .x = positions[triangle.corner3_idx][0],
        .y = positions[triangle.corner3_idx][1],
        .z = positions[triangle.corner3_idx][2],
        .u = uvs[triangle.corner3_idx][0],
        .v = uvs[triangle.corner3_idx][1],
        .r = (float)colors[triangle.corner3_idx].r,
        .g = (float)colors[triangle.corner3_idx].g,
        .b = (float)colors[triangle.corner3_idx].b,
        .a = (float)colors[triangle.corner3_idx].a,
        .nx = normals[triangle.corner3_idx][0],
        .ny = normals[triangle.corner3_idx][1],
        .nz = normals[triangle.corner3_idx][2],
    };

    const int has_texture =
        world_objects->triangle_texture_indices &&
        triangle_index < world_objects->num_triangles &&
        world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE &&
        world_objects->triangle_texture_indices[triangle_index] <
        world_objects->texture_bank.count;

    TiledTexture texture;
    if (has_texture) {
        Uint32 texture_index = world_objects->triangle_texture_indices[triangle_index];
        texture = world_objects->texture_bank.textures[texture_index];
    }

    // =========================================================================
    // STAGE 1: TRIANGLE SETUP & CONSTANT GENERATION
    // =========================================================================

    // 1. Calculate Cross-product area delta (Twice the triangle area)
    float area = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);

    // Backface culling: Skip degenerate or back-facing triangles
    if (fabsf(area) <= 0.0f) return;
    float inv_area = 1.0f / area;

    // 2. Precompute standard Edge Function Step Deltas
    float dE01_dx = v1.y - v0.y;  float dE01_dy = v0.x - v1.x;
    float dE12_dx = v2.y - v1.y;  float dE12_dy = v1.x - v2.x;
    float dE20_dx = v0.y - v2.y;  float dE20_dy = v2.x - v0.x;

    // 3. Compute Attribute Plane Equation Gradients (Matrix Inversion Trick)
    // Interpolation derivatives for Depth (Z)
    float dz_dx = ((v1.z - v0.z) * (v2.y - v0.y) - (v2.z - v0.z) * (v1.y - v0.y)) * inv_area;
    float dz_dy = ((v2.z - v0.z) * (v1.x - v0.x) - (v1.z - v0.z) * (v2.x - v0.x)) * inv_area;

    // Declare placeholder gradients for texturing and color channels
    float du_dx = 0, du_dy = 0, dv_dx = 0, dv_dy = 0;
    float dr_dx = 0, dr_dy = 0, dg_dx = 0, dg_dy = 0, db_dx = 0, db_dy = 0;
    float dnx_dx = 0, dnx_dy = 0, dny_dx = 0, dny_dy = 0, dnz_dx = 0, dnz_dy = 0;

    if (has_texture) {
        // Interpolation derivatives for Texture U coordinate
        du_dx = ((v1.u - v0.u) * (v2.y - v0.y) - (v2.u - v0.u) * (v1.y - v0.y)) * inv_area;
        du_dy = ((v2.u - v0.u) * (v1.x - v0.x) - (v1.u - v0.u) * (v2.x - v0.x)) * inv_area;

        // Interpolation derivatives for Texture V coordinate
        dv_dx = ((v1.v - v0.v) * (v2.y - v0.y) - (v2.v - v0.v) * (v1.y - v0.y)) * inv_area;
        dv_dy = ((v2.v - v0.v) * (v1.x - v0.x) - (v1.v - v0.v) * (v2.x - v0.x)) * inv_area;
    }
    else {
        // Color channel gradients (Linear screen-space interpolation shown here for flat/gouraud shading)
        dr_dx = ((v1.r - v0.r) * (v2.y - v0.y) - (v2.r - v0.r) * (v1.y - v0.y)) * inv_area;
        dr_dy = ((v2.r - v0.r) * (v1.x - v0.x) - (v1.r - v0.r) * (v2.x - v0.x)) * inv_area;
        dg_dx = ((v1.g - v0.g) * (v2.y - v0.y) - (v2.g - v0.g) * (v1.y - v0.y)) * inv_area;
        dg_dy = ((v2.g - v0.g) * (v1.x - v0.x) - (v1.g - v0.g) * (v2.x - v0.x)) * inv_area;
        db_dx = ((v1.b - v0.b) * (v2.y - v0.y) - (v2.b - v0.b) * (v1.y - v0.y)) * inv_area;
        db_dy = ((v2.b - v0.b) * (v1.x - v0.x) - (v1.b - v0.b) * (v2.x - v0.x)) * inv_area;
    }

    dnx_dx = ((v1.nx - v0.nx) * (v2.y - v0.y) - (v2.nx - v0.nx) * (v1.y - v0.y)) * inv_area;
    dnx_dy = ((v2.nx - v0.nx) * (v1.x - v0.x) - (v1.nx - v0.nx) * (v2.x - v0.x)) * inv_area;
    dny_dx = ((v1.ny - v0.ny) * (v2.y - v0.y) - (v2.ny - v0.ny) * (v1.y - v0.y)) * inv_area;
    dny_dy = ((v2.ny - v0.ny) * (v1.x - v0.x) - (v1.ny - v0.ny) * (v2.x - v0.x)) * inv_area;
    dnz_dx = ((v1.nz - v0.nz) * (v2.y - v0.y) - (v2.nz - v0.nz) * (v1.y - v0.y)) * inv_area;
    dnz_dy = ((v2.nz - v0.nz) * (v1.x - v0.x) - (v1.nz - v0.nz) * (v2.x - v0.x)) * inv_area;

    // 4. Bounding Box Setup aligned perfectly to 4x2 block chunks
    int min_x = (min3((int)v0.x, (int)v1.x, (int)v2.x)) & ~3; // Round down to multiple of 4
    int max_x = ((int)ceilf(fmax3(v0.x, v1.x, v2.x)) + 3) & ~3; // Round up to multiple of 4
    int min_y = (min3((int)v0.y, (int)v1.y, (int)v2.y)) & ~1; // Round down to multiple of 2
    int max_y = ((int)ceilf(fmax3(v0.y, v1.y, v2.y)) + 1) & ~1; // Round up to multiple of 2

    // Clip bounding box against screen bounds
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > (int)frame_width)  max_x = frame_width;
    if (max_y > (int)frame_height) max_y = frame_height;

    // =========================================================================
    // STAGE 2: FIXED AVX2 CONSTANTS FOR THE 4x2 PACKET
    // =========================================================================

    // Constant pixel coordinate offsets relative to the top-left (X, Y) corner of the 4x2 block
    // Layout sequence matches depth buffer offset configuration:
    // Row Y  : (0,0), (1,0), (2,0), (3,0)
    // Row Y+1: (0,1), (1,1), (2,1), (3,1)
    __m256 x_offsets = _mm256_setr_ps(0.5f, 1.5f, 2.5f, 3.5f, 0.5f, 1.5f, 2.5f, 3.5f);
    __m256 y_offsets = _mm256_setr_ps(0.5f, 0.5f, 0.5f, 0.5f, 1.5f, 1.5f, 1.5f, 1.5f);

    // Broadcast setup gradients into wide vector registers
    __m256 vdE01_dx = _mm256_set1_ps(dE01_dx); __m256 vdE01_dy = _mm256_set1_ps(dE01_dy);
    __m256 vdE12_dx = _mm256_set1_ps(dE12_dx); __m256 vdE12_dy = _mm256_set1_ps(dE12_dy);
    __m256 vdE20_dx = _mm256_set1_ps(dE20_dx); __m256 vdE20_dy = _mm256_set1_ps(dE20_dy);

    __m256 vdz_dx = _mm256_set1_ps(dz_dx); __m256 vdz_dy = _mm256_set1_ps(dz_dy);
    __m256 vdu_dx = _mm256_set1_ps(du_dx); __m256 vdu_dy = _mm256_set1_ps(du_dy);
    __m256 vdv_dx = _mm256_set1_ps(dv_dx); __m256 vdv_dy = _mm256_set1_ps(dv_dy);

    __m256 vdr_dx = _mm256_set1_ps(dr_dx); __m256 vdr_dy = _mm256_set1_ps(dr_dy);
    __m256 vdg_dx = _mm256_set1_ps(dg_dx); __m256 vdg_dy = _mm256_set1_ps(dg_dy);
    __m256 vdb_dx = _mm256_set1_ps(db_dx); __m256 vdb_dy = _mm256_set1_ps(db_dy);

    __m256 vdnx_dx = _mm256_set1_ps(dnx_dx); __m256 vdnx_dy = _mm256_set1_ps(dnx_dy);
    __m256 vdny_dx = _mm256_set1_ps(dny_dx); __m256 vdny_dy = _mm256_set1_ps(dny_dy);
    __m256 vdnz_dx = _mm256_set1_ps(dnz_dx); __m256 vdnz_dy = _mm256_set1_ps(dnz_dy);

    // Top-left evaluation coordinates tracking anchor position v0
    __m256 vv0_x = _mm256_set1_ps(v0.x);  __m256 vv0_y = _mm256_set1_ps(v0.y);

    // =========================================================================
    // STAGE 3: INTERLEAVED GRID BLOCK TRAVERSAL LOOP
    // =========================================================================
    for (int y = min_y; y < max_y; y += 2) {
        // 1. Pre-broadcast y coordinates once per row to save SIMD additions
        __m256 py = _mm256_add_ps(_mm256_set1_ps((float)y), y_offsets);
        __m256 dy = _mm256_sub_ps(py, vv0_y);

        for (int x = min_x; x < max_x; x += 4) {

            // 3. Compute absolute screen pixel positions for all 8 lanes
            __m256 px = _mm256_add_ps(_mm256_set1_ps((float)x), x_offsets);

            // 1. Calculate Edge Functions across all 8 lanes using linear step offsets
            __m256 e01 = _mm256_add_ps(
                _mm256_mul_ps(_mm256_sub_ps(px, _mm256_set1_ps(v0.x)), vdE01_dx),
                _mm256_mul_ps(_mm256_sub_ps(py, _mm256_set1_ps(v0.y)), vdE01_dy)
            );
            __m256 e12 = _mm256_add_ps(
                _mm256_mul_ps(_mm256_sub_ps(px, _mm256_set1_ps(v1.x)), vdE12_dx),
                _mm256_mul_ps(_mm256_sub_ps(py, _mm256_set1_ps(v1.y)), vdE12_dy)
            );
            __m256 e20 = _mm256_add_ps(
                _mm256_mul_ps(_mm256_sub_ps(px, _mm256_set1_ps(v2.x)), vdE20_dx),
                _mm256_mul_ps(_mm256_sub_ps(py, _mm256_set1_ps(v2.y)), vdE20_dy)
            );

            // 2. Compute Triangle Coverage Mask
            // Test if all three edge function values are greater than or equal to zero
            __m256 mask01 = _mm256_cmp_ps(e01, _mm256_setzero_ps(), _CMP_GE_OQ);
            __m256 mask12 = _mm256_cmp_ps(e12, _mm256_setzero_ps(), _CMP_GE_OQ);
            __m256 mask20 = _mm256_cmp_ps(e20, _mm256_setzero_ps(), _CMP_GE_OQ);

            __m256 coverage_mask = _mm256_and_ps(_mm256_and_ps(mask01, mask12), mask20);

            // Coarse Block Optimization: If no pixels are inside the triangle, skip texture math entirely
            if (_mm256_movemask_ps(coverage_mask) == 0) {
                continue;
            }

            // Calculate distance deltas from the baseline anchor position v0
            __m256 dx = _mm256_sub_ps(px, vv0_x);

            // 4. Parallel FMA Attribute Interpolation
            // Equation structure: H = H0 + dx * dH_dx + dy * dH_dy
            // Interpolate Depth (1/w) for standard Early Depth Test matching
            __m256 z_vec = _mm256_add_ps(_mm256_set1_ps(v0.z), _mm256_add_ps(_mm256_mul_ps(dx, vdz_dx), _mm256_mul_ps(dy, vdz_dy)));

            // Early-Z implementation block
            __m256i depth_offsets = _mm256_setr_epi32(
                (y * frame_width) + x, (y * frame_width) + x + 1,
                (y * frame_width) + x + 2, (y * frame_width) + x + 3,
                ((y + 1) * frame_width) + x, ((y + 1) * frame_width) + x + 1,
                ((y + 1) * frame_width) + x + 2, ((y + 1) * frame_width) + x + 3
            );
            __m256 current_depth = _mm256_i32gather_ps(z_buffer, depth_offsets, 4);
            __m256 depth_pass_mask = _mm256_cmp_ps(z_vec, current_depth, _CMP_GT_OQ); // Assumes 1/w buffer where larger = closer
            __m256 execution_mask = _mm256_and_ps(coverage_mask, depth_pass_mask);

            if (_mm256_movemask_ps(execution_mask) != 0) {
                __m256i int_mask = _mm256_castps_si256(execution_mask);

                __m256 pixel_nx = _mm256_add_ps(_mm256_set1_ps(v0.nx), _mm256_add_ps(_mm256_mul_ps(dx, vdnx_dx), _mm256_mul_ps(dy, vdnx_dy)));
                __m256 pixel_ny = _mm256_add_ps(_mm256_set1_ps(v0.ny), _mm256_add_ps(_mm256_mul_ps(dx, vdny_dx), _mm256_mul_ps(dy, vdny_dy)));
                __m256 pixel_nz = _mm256_add_ps(_mm256_set1_ps(v0.nz), _mm256_add_ps(_mm256_mul_ps(dx, vdnz_dx), _mm256_mul_ps(dy, vdnz_dy)));

                normalize_3_registers(&pixel_nx, &pixel_ny, &pixel_nz);
                __m256 lighting = _mm256_add_ps(
                    _mm256_mul_ps(pixel_nx, _mm256_set1_ps(light_direction[0])), _mm256_add_ps(
                        _mm256_mul_ps(pixel_ny, _mm256_set1_ps(light_direction[1])), _mm256_mul_ps(pixel_nz, _mm256_set1_ps(light_direction[2]))
                    )
                );
                lighting = _mm256_min_ps(_mm256_max_ps(lighting, _mm256_setzero_ps()), _mm256_set1_ps(1.));
                ColorSIMD color_SIMD;

                if (has_texture) {
                    // Interpolate the pre-projected homogenous attributes over screen space
                    __m256 u_over_z_vec = _mm256_add_ps(_mm256_set1_ps(v0.u), _mm256_add_ps(_mm256_mul_ps(dx, vdu_dx), _mm256_mul_ps(dy, vdu_dy)));
                    __m256 v_over_z_vec = _mm256_add_ps(_mm256_set1_ps(v0.v), _mm256_add_ps(_mm256_mul_ps(dx, vdv_dx), _mm256_mul_ps(dy, vdv_dy)));

                    __m256 w_vec = _mm256_div_ps(_mm256_set1_ps(1.0f), z_vec);

                    // Reconstruct perspective-correct U and V coordinates for texturing loop
                    __m256 u_vec = _mm256_mul_ps(u_over_z_vec, w_vec);
                    __m256 v_vec = _mm256_mul_ps(v_over_z_vec, w_vec);

                    // 5. Forward data vectors into Trilinear Texture Sampler
                    color_SIMD = texture_sample_trilinear_SIMD(x, y, frame_width, z_buffer, frame_buffer,
                        int_mask, z_vec, u_vec, v_vec, &texture, lighting);
                }
                else {
                    __m256 final_R = _mm256_mul_ps(_mm256_add_ps(_mm256_set1_ps(v0.r), _mm256_add_ps(_mm256_mul_ps(dx, vdr_dx), _mm256_mul_ps(dy, vdr_dy))), lighting);
                    __m256 final_G = _mm256_mul_ps(_mm256_add_ps(_mm256_set1_ps(v0.g), _mm256_add_ps(_mm256_mul_ps(dx, vdg_dx), _mm256_mul_ps(dy, vdg_dy))), lighting);
                    __m256 final_B = _mm256_mul_ps(_mm256_add_ps(_mm256_set1_ps(v0.b), _mm256_add_ps(_mm256_mul_ps(dx, vdb_dx), _mm256_mul_ps(dy, vdb_dy))), lighting);
                    __m256 final_A = _mm256_set1_ps(v0.a);

                    color_SIMD = (ColorSIMD){
                        .r = _mm256_add_ps(_mm256_set1_ps(v0.r), _mm256_add_ps(_mm256_mul_ps(dx, vdr_dx), _mm256_mul_ps(dy, vdr_dy))),
                            .g = _mm256_add_ps(_mm256_set1_ps(v0.g), _mm256_add_ps(_mm256_mul_ps(dx, vdg_dx), _mm256_mul_ps(dy, vdg_dy))),
                            .b = _mm256_add_ps(_mm256_set1_ps(v0.b), _mm256_add_ps(_mm256_mul_ps(dx, vdb_dx), _mm256_mul_ps(dy, vdb_dy))),
                            .a = _mm256_set1_ps(v0.a)
                    };

                }
                color_SIMD.r = _mm256_mul_ps(color_SIMD.r, lighting);
                color_SIMD.g = _mm256_mul_ps(color_SIMD.g, lighting);
                color_SIMD.b = _mm256_mul_ps(color_SIMD.b, lighting);

                // Assuming constant position alpha for solid shading
                // Clamp float colors straight to [0.0f, 255.0f] range
                __m256 max_color = _mm256_set1_ps(255.0f);
                color_SIMD.r = _mm256_min_ps(_mm256_max_ps(color_SIMD.r, _mm256_setzero_ps()), max_color);
                color_SIMD.g = _mm256_min_ps(_mm256_max_ps(color_SIMD.g, _mm256_setzero_ps()), max_color);
                color_SIMD.b = _mm256_min_ps(_mm256_max_ps(color_SIMD.b, _mm256_setzero_ps()), max_color);
                // Convert colors to integers, pack channels into standard AABBGGRR pixel formats
                __m256i out_R = _mm256_slli_epi32(_mm256_cvtps_epi32(color_SIMD.r), 24);
                __m256i out_G = _mm256_slli_epi32(_mm256_cvtps_epi32(color_SIMD.g), 16);
                __m256i out_B = _mm256_slli_epi32(_mm256_cvtps_epi32(color_SIMD.b), 8);
                __m256i out_A = _mm256_cvtps_epi32(color_SIMD.a);
                __m256i packed_output = _mm256_or_si256(_mm256_or_si256(out_R, out_G), _mm256_or_si256(out_B, out_A));
                // Write out directly using masked operations
                // --- CORRECTED RESOLUTION STAGE: SPLIT BLOCKS INTO TWO 4-LANE ROWS ---
                // Extract row masks
                __m128i mask_row0 = _mm256_castsi256_si128(int_mask);
                __m128i mask_row1 = _mm256_extracti128_si256(int_mask, 1);

                // Extract row depth elements
                __m128 z_row0 = _mm256_castps256_ps128(z_vec);
                __m128 z_row1 = _mm256_extractf128_ps(z_vec, 1);

                // Extract row color elements
                __m128i color_row0 = _mm256_castsi256_si128(packed_output);
                __m128i color_row1 = _mm256_extracti128_si256(packed_output, 1);

                // Write Row Y (Pixels 0, 1, 2, 3)
                _mm_maskstore_ps(z_buffer + (y * frame_width) + x, mask_row0, z_row0);
                _mm_maskstore_epi32((int*)(frame_buffer + (y * frame_width) + x), mask_row0, color_row0);

                // Write Row Y+1 (Pixels 4, 5, 6, 7)
                _mm_maskstore_ps(z_buffer + ((y + 1) * frame_width) + x, mask_row1, z_row1);
                _mm_maskstore_epi32((int*)(frame_buffer + ((y + 1) * frame_width) + x), mask_row1, color_row1);
            }
        }
    }
}

static void _draw_triangle_legacy(Uint32 triangle_index, WorldObjects* world_objects, vec3 light_direction, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
    vec3* positions = world_objects->positions;
    Color* colors = world_objects->colors;
    vec2* uvs = world_objects->uvs;
    vec3* normals = world_objects->normals;

    Triangle triangle = world_objects->triangles[triangle_index];
    Triangle sorted_triangle;
    _sort_points_by_x(&triangle, &sorted_triangle, positions);

    vec3* A = positions[sorted_triangle.corner1_idx];
    vec3* B = positions[sorted_triangle.corner2_idx];
    vec3* C = positions[sorted_triangle.corner3_idx];
    vec4 A_color = { (float)colors[sorted_triangle.corner1_idx].r, (float)colors[sorted_triangle.corner1_idx].g,
        (float)colors[sorted_triangle.corner1_idx].b, (float)colors[sorted_triangle.corner1_idx].a };
    vec4 B_color = { (float)colors[sorted_triangle.corner2_idx].r, (float)colors[sorted_triangle.corner2_idx].g,
        (float)colors[sorted_triangle.corner2_idx].b, (float)colors[sorted_triangle.corner2_idx].a };
    vec4 C_color = { (float)colors[sorted_triangle.corner3_idx].r, (float)colors[sorted_triangle.corner3_idx].g,
        (float)colors[sorted_triangle.corner3_idx].b, (float)colors[sorted_triangle.corner3_idx].a };
    vec2* A_uv = uvs[sorted_triangle.corner1_idx];
    vec2* B_uv = uvs[sorted_triangle.corner2_idx];
    vec2* C_uv = uvs[sorted_triangle.corner3_idx];
    vec3* A_normal = normals[sorted_triangle.corner1_idx];
    vec3* B_normal = normals[sorted_triangle.corner2_idx];
    vec3* C_normal = normals[sorted_triangle.corner3_idx];

    // Copy corner data to local variables for faster access
    float Ax = (*A)[0], Bx = (*B)[0], Cx = (*C)[0];
    float Ay = (*A)[1], By = (*B)[1], Cy = (*C)[1];
    float inv_Az = (*A)[2], inv_Bz = (*B)[2], inv_Cz = (*C)[2];

    // Precompute edges
    float ABx = Bx - Ax, ABy = By - Ay;
    float ACx = Cx - Ax, ACy = Cy - Ay;
    float BCx = Cx - Bx, BCy = Cy - By;

    /*
     * Degenerate triangle check.
     *
     * Sorting by X may change winding, but barycentric interpolation
     * still works as long as we preserve the resulting area sign.
     */
    float triangle_area2 = ABx * ACy - ABy * ACx;

    if (fabsf(triangle_area2) < 1e-8f)
        return;

    float inverse_triangle_area = 1.0f / triangle_area2;

    /*
     * ----------------------------------------------------------------
     * Barycentric derivatives
     * ----------------------------------------------------------------
     *
     * wA = edge(BC, P) / area
     * wB = edge(CA, P) / area
     *
     * wC is implicit.
     */

    float dA_dx = -BCy * inverse_triangle_area;
    float dA_dy = BCx * inverse_triangle_area;

    float dB_dx = ACy * inverse_triangle_area;
    float dB_dy = -ACx * inverse_triangle_area;

    // Calc incremental progression constants
    float inv_Az_minus_C, inv_Bz_minus_C, dz_dx, dz_dy;
    _calc_step_constants(&inv_Az, &inv_Bz, &inv_Cz, 1, dA_dx, dA_dy, dB_dx, dB_dy, &inv_Az_minus_C,
        &inv_Bz_minus_C, &dz_dx, &dz_dy);

    vec2 Auv_minus_C, Buv_minus_C, duv_dx, duv_dy;
    _calc_step_constants(A_uv, B_uv, C_uv, 2, dA_dx, dA_dy, dB_dx, dB_dy, Auv_minus_C, Buv_minus_C,
        duv_dx, duv_dy);

    vec4 Acolor_minus_C, Bcolor_minus_C, dcolor_dx, dcolor_dy;
    _calc_step_constants(A_color, B_color, C_color, 4, dA_dx, dA_dy, dB_dx, dB_dy, Acolor_minus_C,
        Bcolor_minus_C, dcolor_dx, dcolor_dy);

    vec3 Anormal_minus_C, Bnormal_minus_C, dnormal_dx, dnormal_dy;
    _calc_step_constants(A_normal, B_normal, C_normal, 3, dA_dx, dA_dy, dB_dx, dB_dy, Anormal_minus_C,
        Bnormal_minus_C, dnormal_dx, dnormal_dy);

    const int has_texture =
        world_objects->triangle_texture_indices &&
        triangle_index < world_objects->num_triangles &&
        world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE &&
        world_objects->triangle_texture_indices[triangle_index] <
        world_objects->texture_bank.count;

    TiledTexture texture;
    if (has_texture) {
        Uint32 texture_index = world_objects->triangle_texture_indices[triangle_index];
        texture = world_objects->texture_bank.textures[texture_index];
    }

    /*
     * ----------------------------------------------------------------
     * Edge setup
     *
     * Since positions are sorted:
     *
     * A.x <= B.x <= C.x
     *
     * We walk columns from A to C.
     *
     * One boundary is always AC.
     *
     * The other boundary is:
     *
     * AB for A -> B
     * BC for B -> C
     * ----------------------------------------------------------------
     */

     // Slopes dy/dx.
    float AB_slope = fabsf(ABx) > 1e-8f ? ABy / ABx : 0.0f;
    float AC_slope = fabsf(ACx) > 1e-8f ? ACy / ACx : 0.0f;
    float BC_slope = fabsf(BCx) > 1e-8f ? BCy / BCx : 0.0f;

    // Pixel-center column bounds. A pixel column x represents samples at x + 0.5.
    int start_x = (int)ceilf(Ax - 0.5f);
    int middle_x = (int)ceilf(Bx - 0.5f);
    int end_x = (int)ceilf(Cx - 0.5f) - 1;

    // Clamp x boundaries to frame boundaries
    start_x = start_x < 0 ? 0 : start_x;
    end_x = end_x >= (int)frame_width ? frame_width - 1 : end_x;

    if (start_x > end_x) return;

    // Evaluate AC at first pixel center
    float sample_x = (float)start_x + 0.5f;
    float AC_y = Ay + AC_slope * (sample_x - Ax);

    // Evaluate AB initially
    float secondary_y = Ay + AB_slope * (sample_x - Ax);
    float secondary_slope = AB_slope;

    // Main column loop
    for (int current_x = start_x; current_x <= end_x; current_x++) {
        float first_pixel_center_x = (float)current_x + 0.5f;

        // Switch from AB to BC exactly when entering the B->C half.
        if (current_x >= middle_x) {
            secondary_slope = BC_slope;
            secondary_y = By + BC_slope * (first_pixel_center_x - Bx);
        }

        // Vertical boundaries
        float top_y = fminf(AC_y, secondary_y);
        float bottom_y = fmaxf(AC_y, secondary_y);

        /*
         * Pixel-center top-left convention.
         * We want rows whose pixel center lies inside the vertical boundry
         *
         * First sample >= top, Last sample < bottom
         *
         * This excludes the bottom/right boundary consistently.
         */

         // Snap vertical boundaries to grid
        int y_min = (int)ceilf(top_y - 0.5f);
        int y_max = (int)ceilf(bottom_y - 0.5f) - 1;

        // Clamp y boundaries to frame boundaries
        y_min = y_min < 0 ? 0 : y_min;
        y_max = y_max >= (int)frame_height ? frame_height - 1 : y_max;

        // Align y bounds to even numbers to prevent incomplete quad cuts
        y_min &= ~1;

        if (y_min <= y_max) {
            // Initialize barycentric coordinates at first pixel center
            float first_pixel_center_y = (float)y_min + 0.5f;
            float first_x_minus_C = first_pixel_center_x - Cx;
            float first_y_minus_C = first_pixel_center_y - Cy;

            // Base weights for the top-left corner of the quad [x, y]
            float base_wA = (first_x_minus_C * dA_dx) + (first_y_minus_C * dA_dy);
            float base_wB = (first_x_minus_C * dB_dx) + (first_y_minus_C * dB_dy);

            // Row loop
            // Step rows down by 2
            // Step rows vertically by 2 to maximize L1 texture cache locality
            for (int current_y = y_min; current_y <= y_max; current_y += 2) {

                for (int qy = 0; qy < 2; qy++) {
                    int py = current_y + qy;
                    if (py > y_max) continue;

                    // Increment weights vertically
                    float wA = base_wA + (qy * dA_dy);
                    float wB = base_wB + (qy * dB_dy);
                    float wC = 1.0f - wA - wB;

                    if (wA >= 0.0f && wB >= 0.0f && wC >= 0.0f) {
                        float interpolated_depth = inv_Cz + inv_Az_minus_C * wA + inv_Bz_minus_C * wB;
                        int pixel_idx = py * frame_width + current_x;

                        if (interpolated_depth > z_buffer[pixel_idx]) {
                            z_buffer[pixel_idx] = interpolated_depth;

                            Color final_color = { .r = 0, .g = 0, .r = 0, .a = 255 };
                            if (has_texture) {
                                float z = 1.0f / interpolated_depth;

                                float u = ((*C_uv)[0] + Auv_minus_C[0] * wA + Buv_minus_C[0] * wB) * z;
                                float v = ((*C_uv)[1] + Auv_minus_C[1] * wA + Buv_minus_C[1] * wB) * z;

                                vec2 scaled_duv_dx, scaled_duv_dy;
                                glm_vec2_scale(duv_dx, z, scaled_duv_dx);
                                glm_vec2_scale(duv_dy, z, scaled_duv_dy);

                                // Invoke your trilinear sampler pipeline cleanly
                                final_color = texture_sample_trilinear(texture, u, v, scaled_duv_dx, scaled_duv_dy);
                            }
                            else {
                                // Correctly index into your vec4 cglm color arrays [0]=R, [1]=G, [2]=B
                                float r = C_color[0] + Acolor_minus_C[0] * wA + Bcolor_minus_C[0] * wB;
                                float g = C_color[1] + Acolor_minus_C[1] * wA + Bcolor_minus_C[1] * wB;
                                float b = C_color[2] + Acolor_minus_C[2] * wA + Bcolor_minus_C[2] * wB;

                                // Clamp the interpolated float channels to valid 0-255 bounds before casting
                                Uint8 ir = (Uint8)glm_clamp(r, 0.0f, 255.0f);
                                Uint8 ig = (Uint8)glm_clamp(g, 0.0f, 255.0f);
                                Uint8 ib = (Uint8)glm_clamp(b, 0.0f, 255.0f);

                                final_color = (Color) {
                                    .r = ir,
                                    .g = ig,
                                    .b = ib,
                                    .a = 255
                                };
                            }
                            vec3 pixel_normal;
                            pixel_normal[0] = (*C_normal)[0] + Anormal_minus_C[0] * wA + Bnormal_minus_C[0] * wB;
                            pixel_normal[1] = (*C_normal)[1] + Anormal_minus_C[1] * wA + Bnormal_minus_C[1] * wB;
                            pixel_normal[2] = (*C_normal)[2] + Anormal_minus_C[2] * wA + Bnormal_minus_C[2] * wB;

                            glm_vec3_normalize(pixel_normal);
                            float lighting = glm_clamp_zo(glm_vec3_dot(light_direction, pixel_normal));
                            final_color.g = (Uint8)((float)final_color.g * lighting);
                            final_color.b = (Uint8)((float)final_color.b * lighting);
                            final_color.r = (Uint8)((float)final_color.r * lighting);

                            frame[pixel_idx] = _color_to_uint32(final_color);
                        }
                    }
                }
                // Step base weights down by 2 vertical lines
                base_wA += (dA_dy * 2.0f);
                base_wB += (dB_dy * 2.0f);
            }
        }
        // Step to next column
        AC_y += AC_slope;
        secondary_y += secondary_slope;
    }
}

static inline _calc_step_constants(float* A_variable, float* B_variable, float* C_variable,
    Uint32 num_variables, float dA_dx, float dA_dy, float dB_dx, float dB_dy, float* Avar_minus_C,
    float* Bvar_minus_C, float* dest_dvar_dx, float* dest_dvar_dy)
{
    for (Uint32 i = 0; i < num_variables; i++) {
        Avar_minus_C[i] = A_variable[i] - C_variable[i];
        Bvar_minus_C[i] = B_variable[i] - C_variable[i];
        dest_dvar_dx[i] = Avar_minus_C[i] * dA_dx + Bvar_minus_C[i] * dB_dx;
        dest_dvar_dy[i] = Avar_minus_C[i] * dA_dy + Bvar_minus_C[i] * dB_dy;
    }
}

static inline void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3* positions)
{
	vec3* position_a = positions[triangle->corner1_idx];
	vec3* position_b = positions[triangle->corner2_idx];
	vec3* position_c = positions[triangle->corner3_idx];

	if ((*position_a)[0] <= (*position_b)[0] && (*position_a)[0] <= (*position_c)[0]) {
		dest->corner1_idx = triangle->corner1_idx;
		if ((*position_b)[0] <= (*position_c)[0]) {
			dest->corner2_idx = triangle->corner2_idx;
			dest->corner3_idx = triangle->corner3_idx;
		}
		else {
			dest->corner2_idx = triangle->corner3_idx;
			dest->corner3_idx = triangle->corner2_idx;
		}
	}
	else if ((*position_b)[0] <= (*position_a)[0] && (*position_b)[0] <= (*position_c)[0]) {
		dest->corner1_idx = triangle->corner2_idx;
		if ((*position_a)[0] <= (*position_c)[0]) {
			dest->corner2_idx = triangle->corner1_idx;
			dest->corner3_idx = triangle->corner3_idx;
		}
		else {
			dest->corner2_idx = triangle->corner3_idx;
			dest->corner3_idx = triangle->corner1_idx;
		}
	}
	else {
		dest->corner1_idx = triangle->corner3_idx;
		if ((*position_a)[0] <= (*position_b)[0]) {
			dest->corner2_idx = triangle->corner1_idx;
			dest->corner3_idx = triangle->corner2_idx;
		}
		else {
			dest->corner2_idx = triangle->corner2_idx;
			dest->corner3_idx = triangle->corner1_idx;
		}
	}
}

static inline Uint32 _color_to_uint32(Color color) {
    return (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
}
