#pragma once
#include "graphics_pipeline.h"
#include "SDL3/SDL.h"
#include "engine.h"


void move_camera_direction(Camera* camera, float relative_x, float relative_y, unsigned int window_width, unsigned int window_height) {
	float theta_x = -relative_x / window_width * (camera->field_of_view->x_degree_from_center * 2);
	camera->x_direction_vector[0] = camera->x_direction_vector[0] * cos(theta_x) - camera->x_direction_vector[1] * sin(theta_x);
	camera->x_direction_vector[1] = camera->x_direction_vector[0] * sin(theta_x) + camera->x_direction_vector[1] * cos(theta_x);

	float theta_y = -relative_y / window_height * (camera->field_of_view->y_degree_from_center * 2);
	camera->y_direction_vector[0] = camera->y_direction_vector[0] * cos(theta_y) - camera->y_direction_vector[1] * sin(theta_y);
	camera->y_direction_vector[1] = camera->y_direction_vector[0] * sin(theta_y) + camera->y_direction_vector[1] * cos(theta_y);

	SDL_Log("new camera direction: (%f, %f), (%f, %f)", camera->x_direction_vector[0], camera->x_direction_vector[1], camera->y_direction_vector[0], camera->y_direction_vector[1]);
}