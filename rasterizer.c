#include "rasterizer.h"
#include "transformation_utils.h"
#include "cglm/cglm.h"
#define CORNERS 3


void rasterize_objects_to_frame(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	float* z_buffer = malloc(frame_width * frame_height * sizeof(z_buffer));
	if (!z_buffer) {
		SDL_LogError(1, "problem with allocating z_buffer");
		return NULL;
	}
	for (int j = 0; j < frame_width * frame_height; j++)
		z_buffer[j] = INFINITY;

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
	free(z_buffer);
}

void _transform_point_to_pixel_space(Vec3* coords, unsigned int frame_width, unsigned int frame_height)
{
	coords->x = coords->x * frame_width;
	coords->y = coords->y * frame_height;
}

void _draw_triangle(Triangle* triangle, uint32_t* frame, float* z_buffer, unsigned int frame_width, unsigned int frame_height)
{
	Point** sorted_points = _sort_points_by_x(triangle->corner1, triangle->corner2, triangle->corner3);
	
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
		free(sorted_points);
		return;
	}
	const float inverse_triangle_area = 1.0f / triangle_area2;

	int half1_start_x = (int)floorf(glm_clamp(Ax, 0.0, frame_width - 1));
	int half1_end_x = (int)ceilf(glm_clamp(Bx, 0.0, frame_width - 1));
	int half2_end_x = (int)ceilf(glm_clamp(Cx, 0.0, frame_width - 1));

	// Precompute slopes for edge stepping (used to compute column y-range)
	float AB_slope = ABy / ABx;
	float AC_slope = ACy / ACx;
	float BC_slope = CBy / CBx;

	// get the edges of the triangle in this column
	float AB_function = Ay + AB_slope * (half1_start_x - Ax);
	float AC_function = Ay + AC_slope * (half1_start_x - Ax);
	float BC_function = By + BC_slope * (half1_end_x - Bx);

	float slope2 = AB_slope;
	float edge_function2 = AB_function;

	// walk columns from A.x to B.x (left half), then switch to B.x -> C.x (right half), and per column calc the edges pixels, then color between them
	for (int current_x = half1_start_x; current_x <= half2_end_x; current_x++) {
		if (current_x == half1_end_x) {
			slope2 = BC_slope;
			edge_function2 = BC_function;
		}

		int y_min = (int)floorf(glm_clamp(fminf(edge_function2, AC_function), 0.0, frame_height - 1));
		int y_max = (int)ceilf(glm_clamp(fmaxf(edge_function2, AC_function), 0.0, frame_height - 1));

		// calc weights using the Barycentric method ones per column, then foreach row change according to previous pixel
		float A_weight = ((current_x - Cx) * CBy - (y_min - Cy) * CBx) * inverse_triangle_area;
		float B_weight = ((current_x - Ax) * ACy - (y_min - Ay) * ACx) * inverse_triangle_area;
		float C_weight = 1.0f - A_weight - B_weight;
		
		for (int current_y = y_min; current_y <= y_max; current_y++) {
			float pixel_distance = Az * A_weight + Bz * B_weight + Cz * C_weight;
			int pixel_idx = current_y * frame_width + current_x;
			if (pixel_distance < z_buffer[pixel_idx]) {
				z_buffer[pixel_idx] = pixel_distance;

				unsigned int interpolated_color[4];
				//float weights[3] = { A_weight, B_weight, C_weight };
				//_interpolate_color(weights, corners_color , interpolated_color);
				for (int color_ingrediant_idx = 0; color_ingrediant_idx < 4; color_ingrediant_idx++) {
					float color_ingrediant = (float)corners_color[0][color_ingrediant_idx] * A_weight
										   + (float)corners_color[1][color_ingrediant_idx] * B_weight
										   + (float)corners_color[2][color_ingrediant_idx] * C_weight;
					interpolated_color[color_ingrediant_idx] = (unsigned int)glm_clamp(color_ingrediant, 0.0f, 255.0f);
				}
				_draw_pixel(frame, frame_width, frame_height, (unsigned int)current_x, (unsigned int)current_y, interpolated_color);
			}
			// move y 1 unit up
			A_weight -= CBx * inverse_triangle_area;
			B_weight -= ACx * inverse_triangle_area;
			C_weight = 1.0f - A_weight - B_weight;
		}
		// move x 1 unit right
		edge_function2 += slope2;
		AC_function += AC_slope;
	}
	free(sorted_points);
}

Point** _sort_points_by_x(Point* point_a, Point* point_b, Point* point_c)
{
	Point** points_asc = (Point**)malloc(3 * sizeof(Point*));
	if (points_asc == NULL) {
		SDL_LogError(1, "problem with malloc. can't sort points");
		return NULL;
	}

	if (point_a->coords->x <= point_b->coords->x &&
		point_a->coords->x <= point_c->coords->x) {
		points_asc[0] = point_a;
		if (point_b->coords->x <= point_c->coords->x) {
			points_asc[1] = point_b;
			points_asc[2] = point_c;
		}
		else {
			points_asc[1] = point_c;
			points_asc[2] = point_b;
		}
	}
	else if (point_b->coords->x <= point_a->coords->x &&
		point_b->coords->x <= point_c->coords->x) {
		points_asc[0] = point_b;
		if (point_a->coords->x <= point_c->coords->x) {
			points_asc[1] = point_a;
			points_asc[2] = point_c;
		}
		else {
			points_asc[1] = point_c;
			points_asc[2] = point_a;
		}
	}
	else {
		points_asc[0] = point_c;
		if (point_a->coords->x <= point_b->coords->x) {
			points_asc[1] = point_a;
			points_asc[2] = point_b;
		}
		else {
			points_asc[1] = point_b;
			points_asc[2] = point_a;
		}
	}
	return points_asc;
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
