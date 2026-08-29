#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "cglm/cglm.h"
#include "transformation_utils.h"

#define _USE_MATH_DEFINES
#define FOV_CHOSEN 90
#define VIEW_FRUSTUM_MAX 1000.0f
#define VIEW_FRUSTUM_MIN 0.1f

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

Camera* load_camera(Uint32 window_width, Uint32 window_height);
void free_camera(Camera* camera);
void move_camera_direction(float relative_x, float relative_y, Camera* camera, Uint32 window_width, Uint32 window_height);
void move_camera_location(vec3 movement_vec, Camera* camera);