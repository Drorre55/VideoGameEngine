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
	for (Uint32 i = 0; i < (frame_width * frame_height); i++)
		z_buffer[i] = FLT_MAX;

	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
		Triangle current_triangle = on_screen_objects->triangles[i];
		_draw_triangle(current_triangle, on_screen_objects->vertices, on_screen_objects->colors, frame, z_buffer, frame_width, frame_height);
	}
	free_world_objects(on_screen_objects);
}

void _draw_triangle(Triangle triangle, vec3* vertices, Color* colors, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
	Triangle sorted_triangle;
	_sort_points_by_x(&triangle, &sorted_triangle, vertices);
	
	vec3* A = vertices[sorted_triangle.corner1_idx];
	vec3* B = vertices[sorted_triangle.corner2_idx];
	vec3* C = vertices[sorted_triangle.corner3_idx];

	// Copy corner data to local variables for faster access
	float Ax = (*A)[0], Bx = (*B)[0], Cx = (*C)[0];
	float Ay = (*A)[1], By = (*B)[1], Cy = (*C)[1];
	float Az = (*A)[2], Bz = (*B)[2], Cz = (*C)[2];
	
	Uint8 corners_color[3][4];
	memcpy(corners_color[0], &colors[sorted_triangle.corner1_idx], sizeof(Color));
	memcpy(corners_color[1], &colors[sorted_triangle.corner2_idx], sizeof(Color));
	memcpy(corners_color[2], &colors[sorted_triangle.corner3_idx], sizeof(Color));

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
	float AB_slope = ABy / ABx;
	float AC_slope = ACy / ACx;
	float BC_slope = CBy / CBx;

	AB_slope = (fabsf(AB_slope) > fabsf(ABy)) ? ABy : AB_slope;
	AC_slope = (fabsf(AC_slope) > fabsf(ACy)) ? ACy : AC_slope;
	BC_slope = (fabsf(BC_slope) > fabsf(CBy)) ? CBy : BC_slope;

	float ABy_min = fminf(Ay, By), ABy_max = fmaxf(Ay, By);
	float BCy_min = fminf(By, Cy), BCy_max = fmaxf(By, Cy);
	float ACy_min = fminf(Ay, Cy), ACy_max = fmaxf(Ay, Cy);

	float AB_function = glm_clamp(Ay + AB_slope * (half1_start_x - Ax), ABy_min, ABy_max);
	float AC_function = glm_clamp(Ay + AC_slope * (half1_start_x - Ax), ACy_min, ACy_max);
	float BC_function = glm_clamp(By + BC_slope * (half1_end_x - Bx), BCy_min, BCy_max);

	// get the edges of the triangle in this column
	/*float AB_function = Ay + AB_slope * (half1_start_x - Ax);
	float AC_function = Ay + AC_slope * (half1_start_x - Ax);
	float BC_function = By + BC_slope * (half1_end_x - Bx);*/
	
	// derivatives of barycentric weights
	float dA_dx = CBy * inverse_triangle_area; // dwA/dx
	float dA_dy = -CBx * inverse_triangle_area; // dwA/dy
	float dB_dx = ACy * inverse_triangle_area; // dwB/dx
	float dB_dy = -ACx * inverse_triangle_area; // dwB/dy

	// precompute z deltas
	float Az_minus_C = Az - Cz;
	float Bz_minus_C = Bz - Cz;
	float dz_dy = Az_minus_C * dA_dy + Bz_minus_C * dB_dy;
	// dx delta available if needed for column-to-column stepping
	float dz_dx = Az_minus_C * dA_dx + Bz_minus_C * dB_dx;

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

	float slope2 = AB_slope;
	float edge_function2 = AB_function;
	float edge_min_y = ABy_min, edge_max_y = ABy_max;

	// walk columns from A.x to B.x (left half), then switch to B.x -> C.x (right half), and per column calc the edges pixels, then color between them
	for (int current_x = half1_start_x; current_x <= half2_end_x; current_x++) {
		if (current_x == half1_end_x) {
			slope2 = BC_slope;
			edge_function2 = BC_function;
			edge_min_y = BCy_min; edge_max_y = BCy_max;
		}

		int y_min = (int)floorf(glm_clamp(fminf(edge_function2, AC_function), 0.0, frame_height - 1.0f));
		int y_max = (int)ceilf(glm_clamp(fmaxf(edge_function2, AC_function), 0.0, frame_height - 1.0f));

		// calc weights using the Barycentric method once per column for the starting row
		float A_weight = (current_x - Cx) * dA_dx + (y_min - Cy) * dA_dy;
		float B_weight = (current_x - Ax) * dB_dx + (y_min - Ay) * dB_dy;
		// C_weight not needed explicitly

		// initialize interpolated depth and color at the first pixel in this column
		float pixel_distance = Az_minus_C * A_weight + Bz_minus_C * B_weight + Cz;
		float pixel_color_channel[4];
		for (int color_channel = 0; color_channel < 4; color_channel++) {
			pixel_color_channel[color_channel] = Ccolor[color_channel] + Acolor_minus_C[color_channel] * A_weight + Bcolor_minus_C[color_channel] * B_weight;
		}

		int pixel_idx = y_min * frame_width + current_x;
		for (int current_y = y_min; current_y <= y_max; current_y++) {
			if (pixel_distance < z_buffer[pixel_idx]) {
				z_buffer[pixel_idx] = pixel_distance;

				Uint32 interpolated_color[4];
				for (int color_ingrediant_idx = 0; color_ingrediant_idx < 4; color_ingrediant_idx++) {
					interpolated_color[color_ingrediant_idx] = (Uint32)max(pixel_color_channel[color_ingrediant_idx], 0.0f);
				}
				// draw_pixel[pixel_idx] => rgba_to_uint32
				frame[pixel_idx] = (interpolated_color[0] << 24) | (interpolated_color[1] << 16) | (interpolated_color[2] << 8) | interpolated_color[3];
			}
			// step one pixel down: update weights, depth and color using precomputed dy deltas
			A_weight += dA_dy;
			B_weight += dB_dy;
			pixel_distance += dz_dy;
			for (int color_channel = 0; color_channel < 4; color_channel++)
				pixel_color_channel[color_channel] += dcolor_dy[color_channel];
			pixel_idx += frame_width;
		}
		// move x 1 unit right for edge functions
		/*edge_function2 += slope2;
		AC_function += AC_slope;*/

		edge_function2 = glm_clamp(edge_function2 + slope2, edge_min_y, edge_max_y);
		AC_function = glm_clamp(AC_function + AC_slope, ACy_min, ACy_max);
	}
}

void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3* vertices)
{
	vec3* vertex_a = vertices[triangle->corner1_idx];
	vec3* vertex_b = vertices[triangle->corner2_idx];
	vec3* vertex_c = vertices[triangle->corner3_idx];

	if ((*vertex_a)[0] <= (*vertex_b)[0] &&
		(*vertex_a)[0] <= (*vertex_c)[0]) {
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
	else if ((*vertex_b)[0] <= (*vertex_a)[0] &&
		(*vertex_b)[0] <= (*vertex_c)[0]) {
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

void _draw_pixel(Uint32* frame, Uint32 frame_width, Uint32 frame_height, Uint32 x, Uint32 y, Uint32* color)
{
	if (!_is_in_frame(x, y, frame_width, frame_height))
		return;
	frame[y * frame_width + x] = rgba_to_uint32(color[0], color[1], color[2], color[3]);
}

Uint32 _is_in_frame(Uint32 x, Uint32 y, Uint32 frame_width, Uint32 frame_height)
{
	return !(x < 0 || x >= frame_width || y < 0 || y >= frame_height);
}
