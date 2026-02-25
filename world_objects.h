#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"

#define _USE_MATH_DEFINES
#define FOV_CHOSEN 90

typedef struct {
	float x, y, z;
} Vec3;

typedef struct {
	Vec3* coords;
	unsigned int color[4];
} Point;

typedef struct {
	Point* corner1;
	Point* corner2;
	Point* corner3;
} Triangle;

typedef struct {
	Point** point_objects;
	unsigned int num_points;
	Triangle** triangle_objects;
	unsigned int num_triangles;
} WorldObjects;

typedef struct {
	float x_degree_from_center;
	float y_degree_from_center;
} FOV;

typedef struct {
	Vec3* global_coords;
	Vec3* x_direction_vector;
	Vec3* y_direction_vector;
	Vec3* z_direction_vector;
	FOV* field_of_view;
} Camera;

Camera* load_camera(unsigned int window_width, unsigned int window_height);
WorldObjects* load_world_objects();