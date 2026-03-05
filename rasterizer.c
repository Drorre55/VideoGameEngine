#include "rasterizer.h"
#include "transformation_utils.h"
#include "cglm/cglm.h"
#define CORNERS 3


void rasterize_objects_to_frame(uint32_t* frame, float* z_buffer, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	memset(z_buffer, 0, frame_width * frame_height * sizeof(float));

	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
		Triangle* current_triangle = on_screen_objects->triangle_objects[i];
		
		_transform_point_to_pixel_space(current_triangle->corner1->coords, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner2->coords, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner3->coords, frame_width, frame_height);
		
		_draw_triangle(current_triangle, frame, z_buffer, frame_width, frame_height);

		free(current_triangle->corner1->coords);
		free(current_triangle->corner1);
		free(current_triangle->corner2->coords);
		free(current_triangle->corner2);
		free(current_triangle->corner3->coords);
		free(current_triangle->corner3);

		free(current_triangle);
	}
	free(on_screen_objects->triangle_objects);
	free(on_screen_objects);
}

void _transform_point_to_pixel_space(Vec3* coords, unsigned int frame_width, unsigned int frame_height)
{
	coords->x = coords->x * frame_width;
	coords->y = coords->y * frame_height;
}

void _draw_triangle(Triangle* triangle, uint32_t* frame, float* z_buffer, unsigned int frame_width, unsigned int frame_height)
{
	Point* sorted_points[3];
	_sort_points_by_x(triangle->corner1, triangle->corner2, triangle->corner3, sorted_points);
	
	Point* A = sorted_points[0];
	Point* B = sorted_points[1];
	Point* C = sorted_points[2];

	// Copy corner data to local variables for faster access
	float Ax = A->coords->x, Bx = B->coords->x, Cx = C->coords->x;
	float Ay = A->coords->y, By = B->coords->y, Cy = C->coords->y;
	float Az =A->coords->z, Bz = B->coords->z, Cz = C->coords->z;
	
	unsigned int corners_color[3][4];
	memcpy(corners_color[0], A->color, sizeof(A->color));
	memcpy(corners_color[1], B->color, sizeof(B->color));
	memcpy(corners_color[2], C->color, sizeof(C->color));

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

	// Precompute slopes for edge stepping (used to compute column y-range)
	float AB_slope = ABy / ABx;
	float AC_slope = ACy / ACx;
	float BC_slope = CBy / CBx;

	// get the edges of the triangle in this column
	float AB_function = Ay + AB_slope * (half1_start_x - Ax);
	float AC_function = Ay + AC_slope * (half1_start_x - Ax);
	float BC_function = By + BC_slope * (half1_end_x - Bx);

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
	for (int color_channel = 0; color_channel < 4; color_channel++) {
		Acolor_minus_C[color_channel] = (float)corners_color[0][color_channel] - (float)corners_color[2][color_channel];
		Bcolor_minus_C[color_channel] = (float)corners_color[1][color_channel] - (float)corners_color[2][color_channel];
		Ccolor[color_channel] = (float)corners_color[2][color_channel];
		dcolor_dy[color_channel] = Acolor_minus_C[color_channel] * dA_dy + Bcolor_minus_C[color_channel] * dB_dy;
		dcolor_dx[color_channel] = Acolor_minus_C[color_channel] * dA_dx + Bcolor_minus_C[color_channel] * dB_dx;
	}

	float slope2 = AB_slope;
	float edge_function2 = AB_function;

	// walk columns from A.x to B.x (left half), then switch to B.x -> C.x (right half), and per column calc the edges pixels, then color between them
	for (int current_x = half1_start_x; current_x <= half2_end_x; current_x++) {
		if (current_x == half1_end_x) {
			slope2 = BC_slope;
			edge_function2 = BC_function;
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
			if (pixel_distance > z_buffer[pixel_idx]) {
				z_buffer[pixel_idx] = pixel_distance;

				uint32_t interpolated_color[4];
				for (int color_ingrediant_idx = 0; color_ingrediant_idx < 4; color_ingrediant_idx++) {
					interpolated_color[color_ingrediant_idx] = (uint32_t)pixel_color_channel[color_ingrediant_idx];
				}
				// draw_pixel[pixel_idx] => rgba_to_uint32
				frame[pixel_idx] = (interpolated_color[0] << 24) | (interpolated_color[1] << 16) | (interpolated_color[2] << 8) | interpolated_color[3];
			}
			// step one pixel down in y: update weights, depth and color using precomputed dy deltas
			A_weight += dA_dy;
			B_weight += dB_dy;
			pixel_distance += dz_dy;
			for (int color_channel = 0; color_channel < 4; color_channel++)
				pixel_color_channel[color_channel] += dcolor_dy[color_channel];
			pixel_idx += frame_width;
		}
		// move x 1 unit right for edge functions
		edge_function2 += slope2;
		AC_function += AC_slope;
	}
}

void _sort_points_by_x(Point* point_a, Point* point_b, Point* point_c, Point* dest[3])
{
	/*Point** dest = (Point**)malloc(3 * sizeof(Point*));
	if (dest == NULL) {
		SDL_LogError(1, "problem with malloc. can't sort points");
		return NULL;
	}*/

	if (point_a->coords->x <= point_b->coords->x &&
		point_a->coords->x <= point_c->coords->x) {
		dest[0] = point_a;
		if (point_b->coords->x <= point_c->coords->x) {
			dest[1] = point_b;
			dest[2] = point_c;
		}
		else {
			dest[1] = point_c;
			dest[2] = point_b;
		}
	}
	else if (point_b->coords->x <= point_a->coords->x &&
		point_b->coords->x <= point_c->coords->x) {
		dest[0] = point_b;
		if (point_a->coords->x <= point_c->coords->x) {
			dest[1] = point_a;
			dest[2] = point_c;
		}
		else {
			dest[1] = point_c;
			dest[2] = point_a;
		}
	}
	else {
		dest[0] = point_c;
		if (point_a->coords->x <= point_b->coords->x) {
			dest[1] = point_a;
			dest[2] = point_b;
		}
		else {
			dest[1] = point_b;
			dest[2] = point_a;
		}
	}
}

void _draw_pixel(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y, unsigned int* color)
{
	if (!_is_in_frame(x, y, frame_width, frame_height))
		return;
	frame[y * frame_width + x] = rgba_to_uint32(color[0], color[1], color[2], color[3]);
}

unsigned int _is_in_frame(unsigned int x, unsigned int y, unsigned int frame_width, unsigned int frame_height)
{
	return !(x < 0 || x >= frame_width || y < 0 || y >= frame_height);
}
