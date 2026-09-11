#include "rasterizer.h"
#include "transformation_utils.h"
#include "cglm/cglm.h"
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
        y_max = y_max >= (int)frame_height ? y_max = frame_height - 1 : y_max;

        if (y_min <= y_max) {
            // Initialize barycentric coordinates at first pixel center
            float first_pixel_center_y = (float)y_min + 0.5f;
            float first_x_minus_C = first_pixel_center_x - Cx;
            float first_y_minus_C = first_pixel_center_y - Cy;
            float A_weight = (first_x_minus_C * dA_dx) + (first_y_minus_C * dA_dy);
            float B_weight = (first_x_minus_C * dB_dx) + (first_y_minus_C * dB_dy);

            float interpolated_depth = inv_Cz + inv_Az_minus_C * A_weight + inv_Bz_minus_C * B_weight;
            float inv_interp_depth = 1.f / interpolated_depth;

            float u = (*C_uv)[0] + Auv_minus_C[0] * A_weight + Buv_minus_C[0] * B_weight;
            float v = (*C_uv)[1] + Auv_minus_C[1] * A_weight + Buv_minus_C[1] * B_weight;

            float pixel_color_channel[4];
            for (int channel = 0; channel < 4; channel++) {
                pixel_color_channel[channel] = C_color[channel]
                    + Acolor_minus_C[channel] * A_weight
                    + Bcolor_minus_C[channel] * B_weight;
            }

            int pixel_idx = y_min * frame_width + current_x;

            // Row loop
            for (int current_y = y_min; current_y <= y_max; current_y++) {
                if (interpolated_depth > z_buffer[pixel_idx]) {
                    z_buffer[pixel_idx] = interpolated_depth;

                    Color interpolated_color;
                    if (has_texture && texture.num_levels > 0)
                    {
                        float interpolated_u = u / interpolated_depth;
                        float interpolated_v = v / interpolated_depth;
                        vec2 scaled_duv_dx, scaled_duv_dy;
                        glm_vec2_scale(duv_dx, inv_interp_depth, scaled_duv_dx);
                        glm_vec2_scale(duv_dy, inv_interp_depth, scaled_duv_dy);
                        interpolated_color = texture_sample_trilinear(texture, interpolated_u, interpolated_v, scaled_duv_dx, scaled_duv_dy);
                    }
                    else {
                        glm_vec4_clamp(pixel_color_channel, 0.f, 255.f);
                        interpolated_color.r = (Uint8)pixel_color_channel[0];
                        interpolated_color.g = (Uint8)pixel_color_channel[1];
                        interpolated_color.b = (Uint8)pixel_color_channel[2];
                        interpolated_color.a = (Uint8)pixel_color_channel[3];
                    }
                    frame[pixel_idx] = _color_to_uint32(interpolated_color);
                }
                // Step down 1 pixel
                A_weight += dA_dy;
                B_weight += dB_dy;
                interpolated_depth += dz_dy;
                u += duv_dy[0];
                v += duv_dy[1];
                for (int channel = 0; channel < 4; channel++) {
                    pixel_color_channel[channel] += dcolor_dy[channel];
                }
                pixel_idx += frame_width;
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