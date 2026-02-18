#include "rasterizer.h"
#include "transformation_utils.h"


void rasterize_objects_to_frame(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
		Triangle* current_triangle = on_screen_objects->triangle_objects[i];

		_transform_point_to_pixel_space(current_triangle->corner1, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner2, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner3, frame_width, frame_height);
		
		_draw_triangle(frame, frame_width, frame_height, current_triangle);

		free(current_triangle->corner1);
		free(current_triangle->corner2);
		free(current_triangle->corner3);

		free(current_triangle);
	}
	free(on_screen_objects->triangle_objects);
	free(on_screen_objects);
}

void _transform_point_to_pixel_space(Point* point, unsigned int frame_width, unsigned int frame_height)
{
	point->x_coord = point->x_coord * frame_width;
	point->y_coord = point->y_coord * frame_height;
}

void _draw_triangle(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, Triangle* triangle)
{
	Point** sorted_points = _sort_points_by_x(triangle->corner1, triangle->corner2, triangle->corner3);
	
	Point* A = sorted_points[0];
	Point* B = sorted_points[1];
	Point* C = sorted_points[2];

	float AB_slope = (B->y_coord - A->y_coord) / (B->x_coord - A->x_coord);
	float AC_slope = (C->y_coord - A->y_coord) / (C->x_coord - A->x_coord);
	
	// walk from min x value and per column calc the edges pixels, then color between them
	for (float i = A->x_coord; i <= B->x_coord; i++) {
		// get the edges of the triangle in this column
		float AB_function = A->y_coord + AB_slope * (i - A->x_coord);
		float AC_function = A->y_coord + AC_slope * (i - A->x_coord);

		float y_coord_range[2];
		if (AB_function <= AC_function) {
			y_coord_range[0] = AB_function;
			y_coord_range[1] = AC_function;
		}
		else {
			y_coord_range[0] = AC_function;
			y_coord_range[1] = AB_function;
		}
		for (int j = y_coord_range[0]; j <= y_coord_range[1]; j++) {
			_draw_pixel(frame, frame_width, frame_height, (int)i, j, A->color);
		}
	}
	// do the same from the mid x corner. (we can't continue the same becaue the first slope is irrelivant)
	float BC_slope = (C->y_coord - B->y_coord) / (C->x_coord - B->x_coord);
	for (float i = B->x_coord; i <= C->x_coord; i++) {
		float BC_function = B->y_coord + BC_slope * (i - B->x_coord);
		float AC_function = A->y_coord + AC_slope * (i - A->x_coord);

		float y_coord_range[2];
		if (BC_function <= AC_function) {
			y_coord_range[0] = BC_function;
			y_coord_range[1] = AC_function;
		}
		else {
			y_coord_range[0] = AC_function;
			y_coord_range[1] = BC_function;
		}

		for (int j = y_coord_range[0]; j <= y_coord_range[1]; j++) {
			_draw_pixel(frame, frame_width, frame_height, (int)i, j, A->color);
		}
	}
	_draw_pixel(frame, frame_width, frame_height, (int)A->x_coord, (int)A->y_coord, A->color);
	_draw_pixel(frame, frame_width, frame_height, (int)B->x_coord, (int)B->y_coord, B->color);
	_draw_pixel(frame, frame_width, frame_height, (int)C->x_coord, (int)C->y_coord, C->color);
}

Point** _sort_points_by_x(Point* point_a, Point* point_b, Point* point_c)
{
	Point** points_asc = (Point**)malloc(3 * sizeof(Point*));
	if (points_asc == NULL) {
		SDL_LogError(1, "problem with malloc. can't sort points");
		return NULL;
	}

	if (point_a->x_coord <= point_b->x_coord &&
		point_a->x_coord <= point_c->x_coord) {
		points_asc[0] = point_a;
		if (point_b->x_coord <= point_c->x_coord) {
			points_asc[1] = point_b;
			points_asc[2] = point_c;
		}
		else {
			points_asc[1] = point_c;
			points_asc[2] = point_b;
		}
	}
	else if (point_b->x_coord <= point_a->x_coord &&
		point_b->x_coord <= point_c->x_coord) {
		points_asc[0] = point_b;
		if (point_a->x_coord <= point_c->x_coord) {
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
		if (point_a->x_coord <= point_b->x_coord) {
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

void _draw_pixel(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y, uint32_t color)
{
	if (!_is_in_frame(frame_width, frame_height, x, y))
		return;

	frame[y * frame_width + x] = color;
}

unsigned int _is_in_frame(unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y)
{
	return !(x < 0 || x >= frame_width || y < 0 || y >= frame_height);
}
