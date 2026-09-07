#pragma once
#include "world_objects.h"

void rasterize_objects_to_frame(
	Uint32* frame, 
	float* z_buffer,
	Uint32 frame_width, 
	Uint32 frame_height, 
	WorldObjects* on_screen_objects
);
void transform_to_pixel_space(WorldObjects* on_screen_objects, Uint32 frame_width, Uint32 frame_height);
static void _draw_triangle(Uint32 triangle_index, WorldObjects* world_objects, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height);
static void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3* vertices);
static void _draw_pixel(Uint32* frame, Uint32 frame_width, Uint32 frame_height, Uint32 x, Uint32 y, Uint32* color);
static Uint32 _is_in_frame(Uint32 x, Uint32 y, Uint32 frame_width, Uint32 frame_height);