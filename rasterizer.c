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
		//_draw_triangle(i, on_screen_objects, frame, z_buffer, frame_width, frame_height);
		//_draw_triangle_direct(i, on_screen_objects, frame, z_buffer, frame_width, frame_height);
        _draw_triangle_incremental(i, on_screen_objects, frame, z_buffer, frame_width, frame_height);
	}
	free_world_objects(on_screen_objects);
}

static void _draw_triangle(Uint32 triangle_index, WorldObjects* world_objects, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
	vec3* vertices = world_objects->vertices;
	Color* colors = world_objects->colors;
	vec2* uvs = world_objects->uvs;
	Triangle sorted_triangle;
	Triangle triangle = world_objects->triangles[triangle_index];
	_sort_points_by_x(&triangle, &sorted_triangle, vertices);
	
	vec3* A = vertices[sorted_triangle.corner1_idx];
	vec3* B = vertices[sorted_triangle.corner2_idx];
	vec3* C = vertices[sorted_triangle.corner3_idx];

	// Copy corner data to local variables for faster access
	float Ax = (*A)[0], Bx = (*B)[0], Cx = (*C)[0];
	float Ay = (*A)[1], By = (*B)[1], Cy = (*C)[1];
	float inv_Az = (*A)[2], inv_Bz = (*B)[2], inv_Cz = (*C)[2];
	
	Uint8 corners_color[3][4];
	memcpy(corners_color[0], &colors[sorted_triangle.corner1_idx], sizeof(Color));
	memcpy(corners_color[1], &colors[sorted_triangle.corner2_idx], sizeof(Color));
	memcpy(corners_color[2], &colors[sorted_triangle.corner3_idx], sizeof(Color));

	vec2 corners_uv[3];
	glm_vec2_copy(uvs[sorted_triangle.corner1_idx], corners_uv[0]);
	glm_vec2_copy(uvs[sorted_triangle.corner2_idx], corners_uv[1]); 
	glm_vec2_copy(uvs[sorted_triangle.corner3_idx], corners_uv[2]); 

	// Precompute edges
	float ABx = Bx - Ax, ABy = By - Ay;
	float ACx = Cx - Ax, ACy = Cy - Ay;
	float CBx = Bx - Cx, CBy = By - Cy;

	// Compute (area * 2) for barycentric coordinates. If zero, triangle is degenerate
	const float triangle_area2 = (Ax - Cx) * CBy - CBx * (Ay - Cy);
	if (triangle_area2 == 0.0f) {
		return;
	}
	const float inverse_triangle_area = 1.0f / triangle_area2;

	int half1_start_x = (int)floorf(glm_clamp(Ax, 0.0, frame_width - 1.0f));
	int half1_end_x = (int)ceilf(glm_clamp(Bx, 0.0, frame_width - 1.0f));
	int half2_end_x = (int)ceilf(glm_clamp(Cx, 0.0, frame_width - 1.0f));
	if (half1_start_x == half2_end_x) return;

	// Precompute slopes for edge stepping (used to compute column y-range) + Guard against near-vertical edges (ABx/CBx/ACx ~ 0)
	float AB_slope, AC_slope, BC_slope;
	AB_slope = AC_slope = BC_slope = 0.0f;
	if (fabsf(ABx) > 1e-8f)
		AB_slope = ABy / ABx;
	if (fabsf(ACx) > 1e-8f)
		AC_slope = ACy / ACx;
	if (fabsf(CBx) > 1e-8f)
		BC_slope = CBy / CBx;

	float ABy_min = fminf(Ay, By), ABy_max = fmaxf(Ay, By);
	float BCy_min = fminf(By, Cy), BCy_max = fmaxf(By, Cy);
	float ACy_min = fminf(Ay, Cy), ACy_max = fmaxf(Ay, Cy);

	float AB_function = glm_clamp(Ay + AB_slope * (half1_start_x - Ax), ABy_min, ABy_max);
	float AC_function = glm_clamp(Ay + AC_slope * (half1_start_x - Ax), ACy_min, ACy_max);
	float BC_function = glm_clamp(By + BC_slope * (half1_end_x - Bx), BCy_min, BCy_max);

	// derivatives of barycentric weights
	float dA_dx = CBy * inverse_triangle_area; // dwA/dx
	float dA_dy = -CBx * inverse_triangle_area; // dwA/dy
	float dB_dx = ACy * inverse_triangle_area; // dwB/dx
	float dB_dy = -ACx * inverse_triangle_area; // dwB/dy

	// precompute z deltas
	float inv_Az_minus_C = inv_Az - inv_Cz;
	float inv_Bz_minus_C = inv_Bz - inv_Cz;
	float dz_dy = inv_Az_minus_C * dA_dy + inv_Bz_minus_C * dB_dy;
	// dx delta available if needed for column-to-column stepping
	float dz_dx = inv_Az_minus_C * dA_dx + inv_Bz_minus_C * dB_dx;

	// precompute color channel deltas
	float Acolor_minus_C[4], Bcolor_minus_C[4], Ccolor[4];
	float dcolor_dy[4], dcolor_dx[4];
	for (Uint8 color_channel = 0; color_channel < 4; color_channel++) {
		Acolor_minus_C[color_channel] = (float)corners_color[0][color_channel] - (float)corners_color[2][color_channel];
		Bcolor_minus_C[color_channel] = (float)corners_color[1][color_channel] - (float)corners_color[2][color_channel];
		Ccolor[color_channel] = (float)corners_color[2][color_channel];
		dcolor_dy[color_channel] = Acolor_minus_C[color_channel] * dA_dy + Bcolor_minus_C[color_channel] * dB_dy;
		dcolor_dx[color_channel] = Acolor_minus_C[color_channel] * dA_dx + Bcolor_minus_C[color_channel] * dB_dx;
	}

	// precompute uv deltas
	float Auv_minus_C[2], Buv_minus_C[2], Cuv[2];
	float duv_dy[2], duv_dx[2];
	for (Uint8 uv = 0; uv < 2; uv++) {
		Auv_minus_C[uv] = corners_uv[0][uv] - corners_uv[2][uv];
		Buv_minus_C[uv] = corners_uv[1][uv] - corners_uv[2][uv];
		Cuv[uv] = corners_uv[2][uv];
		duv_dy[uv] = Auv_minus_C[uv] * dA_dy + Buv_minus_C[uv] * dB_dy;
		duv_dx[uv] = Auv_minus_C[uv] * dA_dx + Buv_minus_C[uv] * dB_dx;
	}

	float slope2 = AB_slope;
	float edge_function2 = AB_function;
	float edge_min_y = ABy_min, edge_max_y = ABy_max;

	const int has_texture = world_objects->triangle_texture_indices
		&& triangle_index < world_objects->num_triangles
		&& world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE
		&& world_objects->triangle_texture_indices[triangle_index] < world_objects->texture_bank.count;
	
	Uint32 texture_index = has_texture ? world_objects->triangle_texture_indices[triangle_index] : TEXTURE_NONE;
	Texture2D* texture = has_texture ? &world_objects->texture_bank.textures[texture_index] : NULL;

	// walk columns from A.x to B.x (left half), then switch to B.x -> C.x (right half), and per column calc the edges pixels, then color between them
	for (int current_x = half1_start_x; current_x <= half2_end_x; current_x++) {
		if (current_x == half1_end_x) {
			slope2 = BC_slope;
			edge_function2 = BC_function;
			edge_min_y = BCy_min;
			edge_max_y = BCy_max;
		}

		int y_min = (int)floorf(glm_clamp(fminf(edge_function2, AC_function), 0.0, frame_height - 1.0f));
		int y_max = (int)ceilf(glm_clamp(fmaxf(edge_function2, AC_function), 0.0, frame_height - 1.0f));

		// calc weights using the Barycentric method once per column for the starting row
		float A_weight = ((float)current_x + 0.5 - Cx) * dA_dx + ((float)y_min + 0.5 - Cy) * dA_dy;
		float B_weight = ((float)current_x + 0.5 - Cx) * dB_dx + ((float)y_min + 0.5 - Cy) * dB_dy;

		// initialize interpolated depth and color at the first pixel in this column
		float interpolated_depth = inv_Az_minus_C * A_weight + inv_Bz_minus_C * B_weight + inv_Cz;
		float pixel_color_channel[4];
		for (int color_channel = 0; color_channel < 4; color_channel++) {
			pixel_color_channel[color_channel] = Ccolor[color_channel] 
				+ Acolor_minus_C[color_channel] * A_weight 
				+ Bcolor_minus_C[color_channel] * B_weight;
		}
		float u = Cuv[0] + Auv_minus_C[0] * A_weight + Buv_minus_C[0] * B_weight;
		float v = Cuv[1] + Auv_minus_C[1] * A_weight + Buv_minus_C[1] * B_weight;

		int pixel_idx = y_min * frame_width + current_x;
		for (int current_y = y_min; current_y <= y_max; current_y++) {
			if (interpolated_depth >= z_buffer[pixel_idx]) {
				z_buffer[pixel_idx] = interpolated_depth;

				Uint32 interpolated_color[4];
				if (has_texture && texture && texture->pixels) {
					float interpolated_u = u / interpolated_depth;
					float interpolated_v = v / interpolated_depth;

					Color sample = texture_sample_nearest(texture, interpolated_u, interpolated_v);
					interpolated_color[0] = sample.r;
					interpolated_color[1] = sample.g;
					interpolated_color[2] = sample.b;
					interpolated_color[3] = sample.a;
				}
				else {
					for (int color_channel = 0; color_channel < 4; color_channel++) {
						interpolated_color[color_channel] = (Uint32)max(pixel_color_channel[color_channel], 0.0f);
					}
				}
				frame[pixel_idx] = (interpolated_color[0] << 24) | (interpolated_color[1] << 16) | (interpolated_color[2] << 8) | interpolated_color[3];
			}
			// step one pixel down: update weights, depth and color using precomputed dy deltas
			A_weight += dA_dy;
			B_weight += dB_dy;
			interpolated_depth += dz_dy;
			for (int color_channel = 0; color_channel < 4; color_channel++)
				pixel_color_channel[color_channel] += dcolor_dy[color_channel];
			u += duv_dy[0];
			v += duv_dy[1];
			pixel_idx += frame_width;
		}
		// move x 1 unit right for edge functions
		edge_function2 = glm_clamp(edge_function2 + slope2, edge_min_y, edge_max_y);
		AC_function = glm_clamp(AC_function + AC_slope, ACy_min, ACy_max);
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
static inline float _edge_function(float Ax, float Ay, float Bx, float By, float Px, float Py) {
	return (Px - Ax) * (By - Ay) - (Py - Ay) * (Bx - Ax);
}

/*
 * Your rasterizer uses:
 *
 *     wA = edge(C, B, P)
 *     wB = edge(A, C, P)
 *     wC = edge(B, A, P)
 *
 * with positive values meaning "inside".
 *
 * Y increases DOWNWARD in your framebuffer.
 *
 * For this edge-function orientation, an edge is top-left when:
 *
 *     dy > 0
 *
 * or
 *
 *     dy == 0 && dx < 0
 *
 * This is the opposite-looking condition from the commonly quoted
 * version because your edge functions are oriented in the opposite
 * direction.
 */
static inline bool _is_top_left_edge(float Ax, float Ay, float Bx, float By) {
	float dx = Bx - Ax;
	float dy = By - Ay;

	return (dy > 0.0f) || (dy == 0.0f && dx < 0.0f);
}

static inline bool _edge_inside(float edge_value, bool top_left) {
	return edge_value > 0.0f || (edge_value == 0.0f && top_left);
}

static void _draw_triangle_direct(
    Uint32 triangle_index,
    WorldObjects* world_objects,
    Uint32* frame,
    float* z_buffer,
    Uint32 frame_width,
    Uint32 frame_height)
{
    vec3* vertices = world_objects->vertices;
    Color* colors = world_objects->colors;
    vec2* uvs = world_objects->uvs;

    Triangle triangle = world_objects->triangles[triangle_index];

    // IMPORTANT:
    // Do NOT sort vertices here.
    // Edge-function rasterization must preserve the original triangle ordering.

    Uint32 ia = triangle.corner1_idx;
    Uint32 ib = triangle.corner2_idx;
    Uint32 ic = triangle.corner3_idx;

    vec3* A = vertices[ia];
    vec3* B = vertices[ib];
    vec3* C = vertices[ic];

    float Ax = (*A)[0];
    float Ay = (*A)[1];

    float Bx = (*B)[0];
    float By = (*B)[1];

    float Cx = (*C)[0];
    float Cy = (*C)[1];

    float inv_Az = (*A)[2];
    float inv_Bz = (*B)[2];
    float inv_Cz = (*C)[2];

    /*
     * Signed triangle area.
     *
     * Edge(A, B, C)
     */
    float area =
        (Bx - Ax) * (Cy - Ay) -
        (By - Ay) * (Cx - Ax);

    if (fabsf(area) < 1e-8f)
        return;

    /*
     * Normalize edge tests so both possible screen-space
     * winding directions rasterize correctly.
     */
    float winding_sign = area < 0.0f ? -1.0f : 1.0f;

    float inverse_area = 1.0f / fabsf(area);

    // ----------------------------------------------------
    // Bounding box
    // ----------------------------------------------------

    float min_x_f = fminf(Ax, fminf(Bx, Cx));
    float max_x_f = fmaxf(Ax, fmaxf(Bx, Cx));

    float min_y_f = fminf(Ay, fminf(By, Cy));
    float max_y_f = fmaxf(Ay, fmaxf(By, Cy));

    int min_x = (int)floorf(min_x_f);
    int max_x = (int)ceilf(max_x_f);

    int min_y = (int)floorf(min_y_f);
    int max_y = (int)ceilf(max_y_f);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;

    if (max_x >= (int)frame_width)
        max_x = frame_width - 1;

    if (max_y >= (int)frame_height)
        max_y = frame_height - 1;

    if (min_x > max_x || min_y > max_y)
        return;

    // ----------------------------------------------------
    // Edge coefficients
    //
    // E_AB(P) = (P.x - A.x)(B.y - A.y)
    //         - (P.y - A.y)(B.x - A.x)
    //
    // Written as:
    //
    // E(x,y) = A*x + B*y + C
    //
    // so incremental stepping is cheap.
    // ----------------------------------------------------

    /*
     * Edge BC -> barycentric weight for A
     */
    float E0_dx = By - Cy;
    float E0_dy = Cx - Bx;
    float E0_c =
        Bx * Cy -
        Cx * By;

    /*
     * Edge CA -> barycentric weight for B
     */
    float E1_dx = Cy - Ay;
    float E1_dy = Ax - Cx;
    float E1_c =
        Cx * Ay -
        Ax * Cy;

    /*
     * Edge AB -> barycentric weight for C
     */
    float E2_dx = Ay - By;
    float E2_dy = Bx - Ax;
    float E2_c =
        Ax * By -
        Bx * Ay;

    /*
     * Normalize all edges to positive-inside convention.
     */
    E0_dx *= winding_sign;
    E0_dy *= winding_sign;
    E0_c *= winding_sign;

    E1_dx *= winding_sign;
    E1_dy *= winding_sign;
    E1_c *= winding_sign;

    E2_dx *= winding_sign;
    E2_dy *= winding_sign;
    E2_c *= winding_sign;

    // ----------------------------------------------------
    // Top-left rule
    //
    // After winding normalization, determine which edges
    // own pixels exactly on the boundary.
    // ----------------------------------------------------

    /*
     * Because framebuffer Y grows downward, the conventional
     * top-left classification must match this coordinate system.
     */

    float BC_dx_geom = Cx - Bx;
    float BC_dy_geom = Cy - By;

    float CA_dx_geom = Ax - Cx;
    float CA_dy_geom = Ay - Cy;

    float AB_dx_geom = Bx - Ax;
    float AB_dy_geom = By - Ay;

    /*
     * For Y-down coordinates:
     *
     * edge is top-left if:
     * dy < 0
     * OR
     * dy == 0 && dx > 0
     */

    int edge0_top_left =
        (BC_dy_geom < 0.0f) ||
        (BC_dy_geom == 0.0f && BC_dx_geom > 0.0f);

    int edge1_top_left =
        (CA_dy_geom < 0.0f) ||
        (CA_dy_geom == 0.0f && CA_dx_geom > 0.0f);

    int edge2_top_left =
        (AB_dy_geom < 0.0f) ||
        (AB_dy_geom == 0.0f && AB_dx_geom > 0.0f);

    // ----------------------------------------------------
    // Perspective-correct UV data
    // ----------------------------------------------------

    float Au = uvs[ia][0];
    float Av = uvs[ia][1];

    float Bu = uvs[ib][0];
    float Bv = uvs[ib][1];

    float Cu = uvs[ic][0];
    float Cv = uvs[ic][1];

    // ----------------------------------------------------
    // Texture
    // ----------------------------------------------------

    const int has_texture =
        world_objects->triangle_texture_indices &&
        triangle_index < world_objects->num_triangles &&
        world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE &&
        world_objects->triangle_texture_indices[triangle_index] <
        world_objects->texture_bank.count;

    Texture2D* texture = NULL;

    if (has_texture) {
        Uint32 texture_index =
            world_objects->triangle_texture_indices[triangle_index];

        texture =
            &world_objects->texture_bank.textures[texture_index];
    }

    // ----------------------------------------------------
    // Evaluate edges at first pixel center
    // ----------------------------------------------------

    float start_x = (float)min_x + 0.5f;
    float start_y = (float)min_y + 0.5f;

    float row_E0 =
        E0_dx * start_x +
        E0_dy * start_y +
        E0_c;

    float row_E1 =
        E1_dx * start_x +
        E1_dy * start_y +
        E1_c;

    float row_E2 =
        E2_dx * start_x +
        E2_dy * start_y +
        E2_c;

    // ----------------------------------------------------
    // Rasterization
    // ----------------------------------------------------

    for (int y = min_y; y <= max_y; y++) {

        float E0 = row_E0;
        float E1 = row_E1;
        float E2 = row_E2;

        int pixel_idx = y * frame_width + min_x;

        for (int x = min_x; x <= max_x; x++) {

            /*
             * Top-left inclusion rule.
             *
             * Pixel is inside if:
             *
             * E > 0
             * OR
             * E == 0 and edge owns boundary
             */

            int inside0 =
                (E0 > 0.0f) ||
                (E0 == 0.0f && edge0_top_left);

            int inside1 =
                (E1 > 0.0f) ||
                (E1 == 0.0f && edge1_top_left);

            int inside2 =
                (E2 > 0.0f) ||
                (E2 == 0.0f && edge2_top_left);

            if (inside0 && inside1 && inside2) {

                // Barycentric weights
                float wA = E0 * inverse_area;
                float wB = E1 * inverse_area;
                float wC = E2 * inverse_area;

                // Interpolate inverse depth
                float interpolated_depth =
                    wA * inv_Az +
                    wB * inv_Bz +
                    wC * inv_Cz;

                if (interpolated_depth >= z_buffer[pixel_idx]) {

                    z_buffer[pixel_idx] = interpolated_depth;

                    if (has_texture &&
                        texture &&
                        texture->pixels) {

                        /*
                         * UVs are already:
                         *
                         * u/z
                         * v/z
                         *
                         * from transform_scale_to_FOV().
                         *
                         * Therefore interpolate them linearly
                         * and divide by interpolated 1/z.
                         */

                        float interpolated_u_over_z =
                            wA * Au +
                            wB * Bu +
                            wC * Cu;

                        float interpolated_v_over_z =
                            wA * Av +
                            wB * Bv +
                            wC * Cv;

                        float interpolated_u =
                            interpolated_u_over_z /
                            interpolated_depth;

                        float interpolated_v =
                            interpolated_v_over_z /
                            interpolated_depth;

                        Color sample =
                            texture_sample_bilinear(
                                texture,
                                interpolated_u,
                                interpolated_v
                            );

                        frame[pixel_idx] =
                            ((Uint32)sample.r << 24) |
                            ((Uint32)sample.g << 16) |
                            ((Uint32)sample.b << 8) |
                            ((Uint32)sample.a);

                    }
                    else {

                        // Standard affine color interpolation

                        float r =
                            wA * colors[ia].r +
                            wB * colors[ib].r +
                            wC * colors[ic].r;

                        float g =
                            wA * colors[ia].g +
                            wB * colors[ib].g +
                            wC * colors[ic].g;

                        float b =
                            wA * colors[ia].b +
                            wB * colors[ib].b +
                            wC * colors[ic].b;

                        float a =
                            wA * colors[ia].a +
                            wB * colors[ib].a +
                            wC * colors[ic].a;

                        frame[pixel_idx] =
                            ((Uint32)r << 24) |
                            ((Uint32)g << 16) |
                            ((Uint32)b << 8) |
                            ((Uint32)a);
                    }
                }
            }

            // Increment one pixel right
            E0 += E0_dx;
            E1 += E1_dx;
            E2 += E2_dx;

            pixel_idx++;
        }

        // Increment one pixel down
        row_E0 += E0_dy;
        row_E1 += E1_dy;
        row_E2 += E2_dy;
    }
}

static void _draw_triangle_incremental(Uint32 triangle_index, WorldObjects* world_objects, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
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

    // Copy corner data to local variables for faster access
    float Ax = (*A)[0], Bx = (*B)[0], Cx = (*C)[0];
    float Ay = (*A)[1], By = (*B)[1], Cy = (*C)[1];
    float inv_Az = (*A)[2], inv_Bz = (*B)[2], inv_Cz = (*C)[2];

    /*
     * Degenerate triangle check.
     *
     * Sorting by X may change winding, but barycentric interpolation
     * still works as long as we preserve the resulting area sign.
     */
    float triangle_area2 = (Bx - Ax) * (Cy - Ay) - (By - Ay) * (Cx - Ax);

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

    float dA_dx = (By - Cy) * inverse_triangle_area;
    float dA_dy = (Cx - Bx) * inverse_triangle_area;

    float dB_dx = (Cy - Ay) * inverse_triangle_area;
    float dB_dy = (Ax - Cx) * inverse_triangle_area;

    float inv_Az_minus_C = inv_Az - inv_Cz;
    float inv_Bz_minus_C = inv_Bz - inv_Cz;

    // Incremental depth derivatives.
    float dz_dx = inv_Az_minus_C * dA_dx + inv_Bz_minus_C * dB_dx;
    float dz_dy = inv_Az_minus_C * dA_dy + inv_Bz_minus_C * dB_dy;

    // UV derivatives
    float Auv_minus_C[2];
    float Buv_minus_C[2];
    float duv_dx[2];
    float duv_dy[2];
    float Cu = uvs[sorted_triangle.corner3_idx][0];
    float Cv = uvs[sorted_triangle.corner3_idx][1];
    for (int uv = 0; uv < 2; uv++) {
        Auv_minus_C[uv] = uvs[sorted_triangle.corner1_idx][uv] - uvs[sorted_triangle.corner3_idx][uv];
        Buv_minus_C[uv] = uvs[sorted_triangle.corner2_idx][uv] - uvs[sorted_triangle.corner3_idx][uv];
        duv_dx[uv] = Auv_minus_C[uv] * dA_dx + Buv_minus_C[uv] * dB_dx;
        duv_dy[uv] = Auv_minus_C[uv] * dA_dy + Buv_minus_C[uv] * dB_dy;
    }

    // Color derivatives
    float Acolor_minus_C[4], Bcolor_minus_C[4];
    float dcolor_dx[4], dcolor_dy[4];
    Uint8* Ccolor = (Uint8*)&colors[sorted_triangle.corner3_idx];
    for (int channel = 0; channel < 4; channel++) {
        float Acolor = ((Uint8*)&colors[sorted_triangle.corner1_idx])[channel];
        float Bcolor = ((Uint8*)&colors[sorted_triangle.corner2_idx])[channel];
        float Ccolor_channel = Ccolor[channel];

        Acolor_minus_C[channel] = Acolor - Ccolor_channel;
        Bcolor_minus_C[channel] = Bcolor - Ccolor_channel;

        dcolor_dx[channel] = Acolor_minus_C[channel] * dA_dx + Bcolor_minus_C[channel] * dB_dx;
        dcolor_dy[channel] = Acolor_minus_C[channel] * dA_dy + Bcolor_minus_C[channel] * dB_dy;
    }

    const int has_texture =
        world_objects->triangle_texture_indices &&
        triangle_index < world_objects->num_triangles &&
        world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE &&
        world_objects->triangle_texture_indices[triangle_index] <
        world_objects->texture_bank.count;
    Texture2D* texture = NULL;

    if (has_texture) {
        Uint32 texture_index = world_objects->triangle_texture_indices[triangle_index];
        texture = &world_objects->texture_bank.textures[texture_index];
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
    float AB_dx = Bx - Ax;
    float AB_dy = By - Ay;

    float AC_dx = Cx - Ax;
    float AC_dy = Cy - Ay;

    float BC_dx = Cx - Bx;
    float BC_dy = Cy - By;

    // Slopes dy/dx.
    float AB_slope = 0.0f;
    if (fabsf(AB_dx) > 1e-8f)
        AB_slope = AB_dy / AB_dx;

    float AC_slope = 0.0f;
    if (fabsf(AC_dx) > 1e-8f)
        AC_slope = AC_dy / AC_dx;

    float BC_slope = 0.0f;
    if (fabsf(BC_dx) > 1e-8f)
        BC_slope = BC_dy / BC_dx;

    // Pixel-center column bounds. A pixel column x represents samples at x + 0.5.
    int start_x = (int)ceilf(Ax - 0.5f);
    int middle_x = (int)ceilf(Bx - 0.5f);
    int end_x = (int)ceilf(Cx - 0.5f) - 1;

    // Clamp x boundaries to frame
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

        // Clamp y boundaries to frame
        y_min = y_min < 0 ? 0 : y_min;
        y_max = y_max >= (int)frame_height ? y_max = frame_height - 1 : y_max;

        if (y_min <= y_max) {
            // Initialize barycentric coordinates at first pixel center
            float first_pixel_center_y = (float)y_min + 0.5f;
            float A_weight = ((first_pixel_center_x - Cx) * dA_dx) + ((first_pixel_center_y - Cy) * dA_dy);
            float B_weight = ((first_pixel_center_x - Cx) * dB_dx) + ((first_pixel_center_y - Cy) * dB_dy);

            float interpolated_depth = inv_Cz + inv_Az_minus_C * A_weight + inv_Bz_minus_C * B_weight;

            float u = Cu + Auv_minus_C[0] * A_weight + Buv_minus_C[0] * B_weight;
            float v = Cv + Auv_minus_C[1] * A_weight + Buv_minus_C[1] * B_weight;

            float pixel_color_channel[4];
            for (int channel = 0; channel < 4; channel++) {
                pixel_color_channel[channel] = Ccolor[channel]
                    + Acolor_minus_C[channel] * A_weight
                    + Bcolor_minus_C[channel] * B_weight;
            }

            int pixel_idx = y_min * frame_width + current_x;

            // Row loop
            for (int current_y = y_min; current_y <= y_max; current_y++) {
                if (interpolated_depth > z_buffer[pixel_idx]) {
                    z_buffer[pixel_idx] = interpolated_depth;

                    Uint32 interpolated_color[4];
                    if (has_texture)
                    {
                        float interpolated_u = u / interpolated_depth;
                        float interpolated_v = v / interpolated_depth;

                        Color sample = texture_sample_bilinear(texture, interpolated_u, interpolated_v);
                        interpolated_color[0] = sample.r;
                        interpolated_color[1] = sample.g;
                        interpolated_color[2] = sample.b;
                        interpolated_color[3] = sample.a;
                    }
                    else {
                        interpolated_color[0] = (Uint32)glm_clamp(pixel_color_channel[0], 0.f, 255.f);
                        interpolated_color[1] = (Uint32)glm_clamp(pixel_color_channel[1], 0.f, 255.f);
                        interpolated_color[2] = (Uint32)glm_clamp(pixel_color_channel[2], 0.f, 255.f);
                        interpolated_color[3] = (Uint32)glm_clamp(pixel_color_channel[3], 0.f, 255.f);
                    }
                    frame[pixel_idx] =
                        (interpolated_color[0] << 24) |
                        (interpolated_color[1] << 16) |
                        (interpolated_color[2] << 8) |
                        interpolated_color[3];
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