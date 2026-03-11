#pragma once
#include "graphics_pipeline.h"


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
