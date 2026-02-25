#include "rasterizer.h"
#include "transformation_utils.h"
#define CORNERS 3


void rasterize_objects_to_frame(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	float* z_buffer = malloc(frame_width * frame_height * sizeof * z_buffer);
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

		free(current_triangle->corner1);
		free(current_triangle->corner2);
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

	float AB_slope = (B->coords->y - A->coords->y) / (B->coords->x - A->coords->x);
	float AC_slope = (C->coords->y - A->coords->y) / (C->coords->x - A->coords->x);

	float corners_x[3] = { A->coords->x, B->coords->x, C->coords->x };
	float corners_y[3] = { A->coords->y, B->coords->y, C->coords->y };
	float corners_z[3] = { A->coords->z, B->coords->z, C->coords->z };
	unsigned int corners_color[3][4];
	memcpy(corners_color[0], A->color, sizeof(A->color));
	memcpy(corners_color[1], B->color, sizeof(B->color));
	memcpy(corners_color[2], C->color, sizeof(C->color));
	
	// walk from min x value and per column calc the edges pixels, then color between them
	for (float current_x = A->coords->x; current_x <= B->coords->x; current_x++) {
		// get the edges of the triangle in this column
		float AB_function = A->coords->y + AB_slope * (current_x - A->coords->x);
		float AC_function = A->coords->y + AC_slope * (current_x - A->coords->x);

		float y_coord_range[2];
		if (AB_function <= AC_function) {
			y_coord_range[0] = AB_function;
			y_coord_range[1] = AC_function;
		}
		else {
			y_coord_range[0] = AC_function;
			y_coord_range[1] = AB_function;
		}
		for (float current_y = y_coord_range[0]; current_y <= y_coord_range[1]; current_y++) {
			if (!_is_in_frame((int)current_x, (int)current_y, frame_width, frame_height))
				continue;
			float pixel_vec[2] = { current_x, current_y };
			
			float* interpolation_weights = lin_interp2d(pixel_vec, corners_x, corners_y, CORNERS);
			float pixel_distance = _interpolate_distance(interpolation_weights, corners_z, CORNERS);
			if (pixel_distance < z_buffer[(int)current_y * frame_width + (int)current_x]) {
				z_buffer[(int)current_y * frame_width + (int)current_x] = pixel_distance;
				unsigned int* interpolated_color = _interpolate_color(interpolation_weights, corners_color);
				_draw_pixel(frame, frame_width, frame_height, (int)current_x, (int)current_y, interpolated_color);
			}
		}
	}
	// do the same from the mid x corner. (we can't continue the same becaue the first slope is irrelivant)
	float BC_slope = (C->coords->y - B->coords->y) / (C->coords->x - B->coords->x);
	for (float current_x = B->coords->x; current_x <= C->coords->x; current_x++) {
		float BC_function = B->coords->y + BC_slope * (current_x - B->coords->x);
		float AC_function = A->coords->y + AC_slope * (current_x - A->coords->x);

		float y_coord_range[2];
		if (BC_function <= AC_function) {
			y_coord_range[0] = BC_function;
			y_coord_range[1] = AC_function;
		}
		else {
			y_coord_range[0] = AC_function;
			y_coord_range[1] = BC_function;
		}

		for (float current_y = y_coord_range[0]; current_y <= y_coord_range[1]; current_y++) {
			if (!_is_in_frame((int)current_x, (int)current_y, frame_width, frame_height))
				continue;
			float pixel_vec[2] = { current_x, current_y };

			float* interpolation_weights = lin_interp2d(pixel_vec, corners_x, corners_y, 3);
			float pixel_distance = _interpolate_distance(interpolation_weights, corners_z, CORNERS);
			if (pixel_distance < z_buffer[(int)current_y * frame_width + (int)current_x]) {
				z_buffer[(int)current_y * frame_width + (int)current_x] = pixel_distance;
				unsigned int* interpolated_color = _interpolate_color(interpolation_weights, corners_color);
				_draw_pixel(frame, frame_width, frame_height, (int)current_x, (int)current_y, interpolated_color);
			}
		}
	}
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

unsigned int* _interpolate_color(float weights[3], unsigned int corners_color[3][4]) {
	float* interpolated_color = calloc(1, sizeof(float) * 4);
	if (interpolated_color == NULL) {
		SDL_LogError(1, "Problem with calloc. can't interpolate color");
		return NULL;
	}

	for (int color_ingredient_idx = 0; color_ingredient_idx < 4; color_ingredient_idx++) {
		for (int corner_idx = 0; corner_idx < 3; corner_idx++) {
			interpolated_color[color_ingredient_idx] = interpolated_color[color_ingredient_idx] + corners_color[corner_idx][color_ingredient_idx] * weights[corner_idx];
		}
	}
	
	unsigned int* int_color = calloc(1, sizeof(unsigned int) * 4);
	if (int_color == NULL) {
		SDL_LogError(1, "Problem with calloc. can't interpolate color");
		return NULL;
	}
	for (int color_ingredient_idx = 0; color_ingredient_idx < 4; color_ingredient_idx++) {
		int_color[color_ingredient_idx] = (unsigned int)interpolated_color[color_ingredient_idx];
	}
	free(interpolated_color);
	return int_color;
}

float _interpolate_distance(float* weights, float* corners_z, unsigned int num_objects)
{
	float distance = 0;
	for (int i = 0; i < num_objects; i++) {
		distance += weights[i] * corners_z[i];
	}
	return distance;
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
