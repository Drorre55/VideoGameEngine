#pragma once
#include "camera.h"


typedef struct {
	Uint8 r, g, b, a;
} Color;

typedef struct {
	Uint32 corner1_idx;
	Uint32 corner2_idx;
	Uint32 corner3_idx;
} Triangle;

typedef struct {
	vec3* vertices;
	Triangle* triangles;
	Color* colors;
	Uint32 num_vertices;
	Uint32 num_triangles;
} WorldObjects;

WorldObjects* load_world_objects();
void free_world_objects(WorldObjects* world_objects);
WorldObjects* world_objects_deep_copy(WorldObjects* world_objects);