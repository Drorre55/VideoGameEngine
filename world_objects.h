#pragma once
#include "camera.h"


typedef struct {
	unsigned int r, g, b, a;
} Color;

typedef struct {
	unsigned int corner1_idx;
	unsigned int corner2_idx;
	unsigned int corner3_idx;
} Triangle;

typedef struct {
	vec3** vertices;
	Triangle** triangles;
	Color** colors;
	unsigned int num_vertices;
	unsigned int num_triangles;
} WorldObjects;

WorldObjects* load_world_objects();
void free_world_objects(WorldObjects* world_objects);
WorldObjects* world_objects_deep_copy(WorldObjects* world_objects);