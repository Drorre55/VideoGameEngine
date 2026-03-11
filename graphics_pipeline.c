#pragma once
#include "graphics_pipeline.h"


void move_camera_direction(float relative_x, float relative_y, Camera* camera, unsigned int window_width, unsigned int window_height) {
	// In the future if I will add Screw rotation than add rotate z_axis
	float rotation_degree_x = -relative_x / window_width * (camera->field_of_view->x_degree_from_center * 2);
	rotate_y_axis(*camera->x_direction_vector, rotation_degree_x);
	rotate_y_axis(*camera->z_direction_vector, rotation_degree_x);
	
	float rotation_degree_y = relative_y / window_height * (camera->field_of_view->y_degree_from_center * 2);
	rotate_x_axis(*camera->y_direction_vector, rotation_degree_y);
	rotate_x_axis(*camera->z_direction_vector, rotation_degree_y);
}

void move_camera_location(vec3 direction, Camera* camera)
{
	if (!direction[0] && !direction[1] && !direction[2])
		return;
	(*camera->global_coords)[0] += ((*camera->x_direction_vector)[0] * -direction[0]);
	(*camera->global_coords)[1] += ((*camera->y_direction_vector)[1] * direction[1]);
	(*camera->global_coords)[2] += ((*camera->z_direction_vector)[2] * direction[2]);
}

void run_graphics_pipeline(uint32_t* framebuffer, float* z_buffer, WorldObjects* world_objects, Camera* camera, 
	unsigned int frame_width, unsigned int frame_height)
{
	WorldObjects* camera_space_objects = transform_from_world_to_camera_space(world_objects, camera);
	transform_scale_to_FOV(camera_space_objects, camera);
	cut_triangles_completely_outside_FOV(camera_space_objects);
	transform_xy_from_FOV_space_to_01_scale(camera_space_objects);
	transform_to_pixel_space(camera_space_objects, frame_width, frame_height);
	rasterize_objects_to_frame(framebuffer, z_buffer, frame_width, frame_height, camera_space_objects);
}
