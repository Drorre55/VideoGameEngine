#pragma once
#include "world_objects.h"

void rasterize_objects_to_frame(
	uint32_t* frame, 
	unsigned int frame_width, 
	unsigned int frame_height, 
	WorldObjects* on_screen_objects
);
void _transform_point_to_pixel_space(Point* point, unsigned int frame_width, unsigned int frame_height);
void _draw_triangle(Triangle* triangle, uint32_t* frame, float* z_buffer, unsigned int frame_width, unsigned int frame_height);
Point** _sort_points_by_x(Point* point_a, Point* point_b, Point* point_c);
unsigned int* _interpolate_color(float weights[3], unsigned int corners_color[3][4]);
float _interpolate_distance(float* weights, float* corners_z, unsigned int num_objects);
void _draw_pixel(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y, unsigned int* color);
unsigned int _is_in_frame(unsigned int x, unsigned int y, unsigned int frame_width, unsigned int frame_height);