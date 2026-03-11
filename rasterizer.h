#pragma once
#include "world_objects.h"

void rasterize_objects_to_frame(
	uint32_t* frame, 
	float* z_buffer,
	unsigned int frame_width, 
	unsigned int frame_height, 
	WorldObjects* on_screen_objects
);
void transform_to_pixel_space(WorldObjects* on_screen_objects, unsigned int frame_width, unsigned int frame_height);
void _draw_triangle(Triangle* triangle, vec3** vertices, Color** colors, uint32_t* frame, float* z_buffer, unsigned int frame_width, unsigned int frame_height);
void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3** vertices);
void _draw_pixel(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y, unsigned int* color);
unsigned int _is_in_frame(unsigned int x, unsigned int y, unsigned int frame_width, unsigned int frame_height);