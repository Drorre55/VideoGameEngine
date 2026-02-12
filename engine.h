#pragma once
#include <stdint.h>

#define _USE_MATH_DEFINES
#define FOV_CHOSEN 90

typedef struct {
	float x_coord;
	float y_coord;
	float z_coord;
	uint32_t color;
} Point;

typedef struct {
	Point* corner1;
	Point* corner2;
	Point* corner3;
	Point* corner4;
} Box;

typedef struct {
	Point* corner1;
	Point* corner2;
	Point* corner3;
} Triangle;

typedef struct {
	Point** point_objects;
	unsigned int num_points;
	Box** box_objects;
	unsigned int num_boxes;
	Triangle** triangle_objects;
	unsigned int num_triangles;
} WorldObjects;

typedef struct {
	float x_degree_from_center;
	float y_degree_from_center;
} FOV;

typedef struct {
	Point* global_coords;
	float x_direction_vector[2];
	float y_direction_vector[2];
	FOV* field_of_view;
} Camera;

Camera* load_camera(unsigned int window_width, unsigned int window_height);
WorldObjects* load_world_objects();
WorldObjects* get_on_screen_objects(WorldObjects* world_objects, Camera* camera, unsigned int window_width);
void rasterize_objects_to_frame(uint32_t* frame, Camera* camera, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects);

