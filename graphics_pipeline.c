#pragma once
#include "graphics_pipeline.h"


void move_camera_direction(Camera* camera, float relative_x, float relative_y, unsigned int window_width, unsigned int window_height) {
	// In the future if I will add Screw rotation than add rotate z_axis
	float rotation_degree_x = -relative_x / window_width * (camera->field_of_view->x_degree_from_center * 2);
	rotate_y_axis(camera->x_direction_vector, rotation_degree_x);
	rotate_y_axis(camera->z_direction_vector, rotation_degree_x);
	
	float rotation_degree_y = relative_y / window_height * (camera->field_of_view->y_degree_from_center * 2);
	rotate_x_axis(camera->y_direction_vector, rotation_degree_y);
	rotate_x_axis(camera->z_direction_vector, rotation_degree_y);

	SDL_Log("new camera direction: xyz (%f, %f, %f). rotation_degree_x: %f, rotation_degree_y: %f", camera->z_direction_vector->x_coord, camera->z_direction_vector->y_coord, camera->z_direction_vector->z_coord, rotation_degree_x, rotation_degree_y);
}

void run_graphics_pipeline(uint32_t* framebuffer, WorldObjects* world_objects, Camera* camera, 
	unsigned int frame_width, unsigned int frame_height)
{
	WorldObjects* camera_space_objects = transform_from_world_to_camera_space(world_objects, camera);
	transform_scale_to_FOV(camera_space_objects, camera);
	cut_objects_outside_FOV(camera_space_objects);
	transform_xy_from_FOV_space_to_01_scale(camera_space_objects, camera);
	rasterize_objects_to_frame(framebuffer, camera, frame_width, frame_height, camera_space_objects);
}
