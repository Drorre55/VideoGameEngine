#pragma once
#include "world_objects.h"
#include "transformation_utils.h"

typedef struct {
	float x, y, z;
	float u, v;
	float r, g, b, a;
	float nx, ny, nz;
} RenderVertex;

void rasterize_objects_to_frame(
	Uint32* frame, 
	float* z_buffer,
	Uint32 frame_width, 
	Uint32 frame_height, 
	WorldObjects* on_screen_objects,
	vec3 light_direction
);
void transform_to_pixel_space(WorldObjects* on_screen_objects, Uint32 frame_width, Uint32 frame_height);
static void _draw_triangle_SIMD(Uint32 triangle_index, WorldObjects* world_objects, vec3 light_direction, Uint32* frame_buffer, float* z_buffer, Uint32 frame_width, Uint32 frame_height);
static void _draw_triangle_legacy(Uint32 triangle_index, WorldObjects* world_objects, vec3 light_direction, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height);
static _calc_step_constants(float* A_variable, float* B_variable, float* C_variable, Uint32 num_variables, float dA_dx, float dA_dy, float dB_dx, float dB_dy, float* Avar_minus_C, float* Bvar_minus_C, float* dest_dvar_dx, float* dest_dvar_dy);
static void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3* positions);
static inline Uint32 _color_to_uint32(Color color);