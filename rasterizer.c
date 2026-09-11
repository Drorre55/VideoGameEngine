#include "rasterizer.h"
#include "transformation_utils.h"
#include "cglm/cglm.h"
#include <immintrin.h>
#define CORNERS 3


void transform_to_pixel_space(WorldObjects* on_screen_objects, Uint32 frame_width, Uint32 frame_height)
{
	for (int i = 0; i < on_screen_objects->num_vertices; i++) {
		vec3* vertex = on_screen_objects->vertices[i];
		(*vertex)[0] = (*vertex)[0] * frame_width;
		(*vertex)[1] = (*vertex)[1] * frame_height;
	}
}

void rasterize_objects_to_frame(Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height, WorldObjects* on_screen_objects) {
	memset(z_buffer, 0, sizeof(float) * frame_width * frame_height);

	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
		_draw_triangle(i, on_screen_objects, frame, z_buffer, frame_width, frame_height);
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

static void _draw_triangle(Uint32 triangle_index, WorldObjects* world_objects, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
    vec3* vertices = world_objects->vertices;
    Color* colors = world_objects->colors;
    vec2* uvs = world_objects->uvs;

    Triangle triangle = world_objects->triangles[triangle_index];
    Triangle sorted_triangle;
    _sort_points_by_x(&triangle, &sorted_triangle, vertices);

    vec3* A = vertices[sorted_triangle.corner1_idx];
    vec3* B = vertices[sorted_triangle.corner2_idx];
    vec3* C = vertices[sorted_triangle.corner3_idx];
    vec4 A_color = { (float)colors[sorted_triangle.corner1_idx].r, (float)colors[sorted_triangle.corner1_idx].g,
        (float)colors[sorted_triangle.corner1_idx].b, (float)colors[sorted_triangle.corner1_idx].a };
    vec4 B_color = { (float)colors[sorted_triangle.corner2_idx].r, (float)colors[sorted_triangle.corner2_idx].g,
        (float)colors[sorted_triangle.corner2_idx].b, (float)colors[sorted_triangle.corner2_idx].a };
    vec4 C_color = { (float)colors[sorted_triangle.corner3_idx].r, (float)colors[sorted_triangle.corner3_idx].g,
        (float)colors[sorted_triangle.corner3_idx].b, (float)colors[sorted_triangle.corner3_idx].a };
    vec2* A_uv = uvs[sorted_triangle.corner1_idx];
    vec2* B_uv = uvs[sorted_triangle.corner2_idx];
    vec2* C_uv = uvs[sorted_triangle.corner3_idx];

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
     * Since vertices are sorted:
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

            // Pre-load your constant step derivatives into SIMD registers
            __m128 v_dA_dy = _mm_set1_ps(dA_dy);
            __m128 v_dB_dy = _mm_set1_ps(dB_dy);

            __m128 v_inv_Az_minus_C = _mm_set1_ps(inv_Az_minus_C);
            __m128 v_inv_Bz_minus_C = _mm_set1_ps(inv_Bz_minus_C);
            __m128 v_inv_Cz = _mm_set1_ps(inv_Cz);

            // Vectorize texture coordinate step constants (assuming uvs are 2D floats)
            __m128 v_Auv_minus_C_u = _mm_set1_ps(Auv_minus_C[0]);
            __m128 v_Auv_minus_C_v = _mm_set1_ps(Auv_minus_C[1]);
            __m128 v_Buv_minus_C_u = _mm_set1_ps(Buv_minus_C[0]);
            __m128 v_Buv_minus_C_v = _mm_set1_ps(Buv_minus_C[1]);
            __m128 v_C_uv_u = _mm_set1_ps((*C_uv)[0]);
            __m128 v_C_uv_v = _mm_set1_ps((*C_uv)[1]);
            // my test
            __m128 v_du_dx = _mm_set1_ps(duv_dx[0]);
            __m128 v_dv_dx = _mm_set1_ps(duv_dx[1]);
            __m128 v_du_dy = _mm_set1_ps(duv_dy[0]);
            __m128 v_dv_dy = _mm_set1_ps(duv_dy[1]);

            // Row loop
            // Step rows down by 2
            // Step rows vertically by 2 to maximize L1 texture cache locality
            for (int current_y = y_min; current_y <= y_max; current_y += 2) {
                // 1. SETUP LOGICAL OFFSETS FOR PIXEL 0 (qy=0) AND PIXEL 1 (qy=1)
                // Lane 0: offset 0.0f, Lane 1: offset 1.0f, Lanes 2 & 3: dummy values
                __m128 v_qy = _mm_set_ps(0.0f, 0.0f, 1.0f, 0.0f);

                // 2. COMPUTE BARYCENTRIC WEIGHTS FOR BOTH PIXELS SIMULTANEOUSLY
                __m128 v_base_wA = _mm_set1_ps(base_wA);
                __m128 v_base_wB = _mm_set1_ps(base_wB);

                __m128 v_wA = _mm_add_ps(v_base_wA, _mm_mul_ps(v_qy, v_dA_dy));
                __m128 v_wB = _mm_add_ps(v_base_wB, _mm_mul_ps(v_qy, v_dB_dy));
                __m128 v_wC = _mm_sub_ps(_mm_sub_ps(_mm_set1_ps(1.0f), v_wA), v_wB);

                // 3. GENERATE THE COVERAGE MASK
                // Check if wA >= 0, wB >= 0, wC >= 0 for both lanes
                __m128 v_zero = _mm_setzero_ps();
                __m128 mask_wA = _mm_cmpge_ps(v_wA, v_zero);
                __m128 mask_wB = _mm_cmpge_ps(v_wB, v_zero);
                __m128 mask_wC = _mm_cmpge_ps(v_wC, v_zero);
                __m128 mask_coverage = _mm_and_ps(_mm_and_ps(mask_wA, mask_wB), mask_wC);

                // If both pixels fall completely outside the triangle, skip the entire math block
                if (_mm_movemask_ps(mask_coverage) == 0) {
                    base_wA += (dA_dy * 2.0f);
                    base_wB += (dB_dy * 2.0f);
                    continue;
                }

                // 4. INTERPOLATE DEPTH (1/Z) & COMPUTE PERSPECTIVE CORRECTIVE Z
                __m128 v_depth = _mm_add_ps(v_inv_Cz,
                    _mm_add_ps(_mm_mul_ps(v_inv_Az_minus_C, v_wA), _mm_mul_ps(v_inv_Bz_minus_C, v_wB))
                );

                // Vectorized division: z = 1.0f / depth
                __m128 v_z = _mm_div_ps(_mm_set1_ps(1.0f), v_depth);

                // 5. INTERPOLATE PERSPECTIVE-CORRECTED U & V COORDINATES
                __m128 v_u = _mm_mul_ps(_mm_add_ps(v_C_uv_u, _mm_add_ps(_mm_mul_ps(v_Auv_minus_C_u, v_wA), _mm_mul_ps(v_Buv_minus_C_u, v_wB))), v_z);
                __m128 v_v = _mm_mul_ps(_mm_add_ps(v_C_uv_v, _mm_add_ps(_mm_mul_ps(v_Auv_minus_C_v, v_wA), _mm_mul_ps(v_Buv_minus_C_v, v_wB))), v_z);

                __m128 v_local_du_dx = _mm_mul_ps(v_du_dx, v_z);
                __m128 v_local_dv_dx = _mm_mul_ps(v_dv_dx, v_z);
                __m128 v_local_du_dy = _mm_mul_ps(v_du_dy, v_z);
                __m128 v_local_dv_dy = _mm_mul_ps(v_dv_dy, v_z);

                // Extract computed lanes back to scalar values to perform Z-buffering and texturing
                float u_lanes[4], v_lanes[4], depth_lanes[4], du_dx_lanes[4], dv_dx_lanes[4], du_dy_lanes[4], dv_dy_lanes[4];
                _mm_storeu_ps(u_lanes, v_u);
                _mm_storeu_ps(v_lanes, v_v);
                _mm_storeu_ps(depth_lanes, v_depth);
                _mm_storeu_ps(du_dx_lanes, v_local_du_dx);
                _mm_storeu_ps(dv_dx_lanes, v_local_dv_dx);
                _mm_storeu_ps(du_dy_lanes, v_local_du_dy);
                _mm_storeu_ps(dv_dy_lanes, v_local_dv_dy);
                int coverage_mask = _mm_movemask_ps(mask_coverage);

                // 6. SCALAR BACKEND: BLIT TO FRAMEBUFFER
                for (int qy = 0; qy < 2; qy++) {
                    // Check if this specific lane is marked valid by the SIMD edge test
                    if (!(coverage_mask & (1 << qy))) continue;

                    int py = current_y + qy;
                    if (py > y_max) continue;
                    float interpolated_depth = depth_lanes[qy];
                    int pixel_idx = py * frame_width + current_x;

                    if (interpolated_depth > z_buffer[pixel_idx]) {
                        z_buffer[pixel_idx] = interpolated_depth;

                        Uint32 final_color = 0xFFFFFFFF;
                        if (has_texture) {
                            vec2 scaled_duv_dx = { du_dx_lanes[qy], dv_dx_lanes[qy] };
                            vec2 scaled_duv_dy = { du_dy_lanes[qy], dv_dy_lanes[qy] };

                            // Invoke your trilinear sampler pipeline cleanly
                            Color texel = texture_sample_trilinear(texture, u_lanes[qy], v_lanes[qy], scaled_duv_dx, scaled_duv_dy);
                            final_color = _color_to_uint32(texel);
                        }
                        else {
                            float wA_scalar = base_wA + (qy * dA_dy);
                            float wB_scalar = base_wB + (qy * dB_dy);
                            // Correctly index into your vec4 cglm color arrays [0]=R, [1]=G, [2]=B
                            float r = C_color[0] + Acolor_minus_C[0] * wA_scalar + Bcolor_minus_C[0] * wB_scalar;
                            float g = C_color[1] + Acolor_minus_C[1] * wA_scalar + Bcolor_minus_C[1] * wB_scalar;
                            float b = C_color[2] + Acolor_minus_C[2] * wA_scalar + Bcolor_minus_C[2] * wB_scalar;

                            // Clamp the interpolated float channels to valid 0-255 bounds before casting
                            int ir = (Uint8)glm_clamp(r, 0.0f, 255.0f);
                            int ig = (Uint8)glm_clamp(g, 0.0f, 255.0f);
                            int ib = (Uint8)glm_clamp(b, 0.0f, 255.0f);

                            Color fallback_color = {
                                .r = ir,
                                .g = ig,
                                .b = ib,
                                .a = 255
                            };

                            final_color = _color_to_uint32(fallback_color);
                        }

                        frame[pixel_idx] = final_color;
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

static void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3* vertices)
{
	vec3* vertex_a = vertices[triangle->corner1_idx];
	vec3* vertex_b = vertices[triangle->corner2_idx];
	vec3* vertex_c = vertices[triangle->corner3_idx];

	if ((*vertex_a)[0] <= (*vertex_b)[0] && (*vertex_a)[0] <= (*vertex_c)[0]) {
		dest->corner1_idx = triangle->corner1_idx;
		if ((*vertex_b)[0] <= (*vertex_c)[0]) {
			dest->corner2_idx = triangle->corner2_idx;
			dest->corner3_idx = triangle->corner3_idx;
		}
		else {
			dest->corner2_idx = triangle->corner3_idx;
			dest->corner3_idx = triangle->corner2_idx;
		}
	}
	else if ((*vertex_b)[0] <= (*vertex_a)[0] && (*vertex_b)[0] <= (*vertex_c)[0]) {
		dest->corner1_idx = triangle->corner2_idx;
		if ((*vertex_a)[0] <= (*vertex_c)[0]) {
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
		if ((*vertex_a)[0] <= (*vertex_b)[0]) {
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