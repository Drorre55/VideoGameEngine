#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "cglm/cglm.h"

#define _USE_MATH_DEFINES
#define FOV_CHOSEN 90

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

typedef struct {
	float x_degree_from_center;
	float y_degree_from_center;
} FOV;

typedef struct {
	vec3* global_coords;
	vec3* x_direction_vector;
	vec3* y_direction_vector;
	vec3* z_direction_vector;
	FOV* field_of_view;
} Camera;

Camera* load_camera(unsigned int window_width, unsigned int window_height);
void free_camera(Camera* camera);
WorldObjects* load_world_objects();
void free_world_objects(WorldObjects* world_objects);
WorldObjects* world_objects_deep_copy(WorldObjects* world_objects);