#pragma once
#include "graphics_pipeline.h"


void run_graphics_pipeline(Uint32* framebuffer, float* z_buffer, WorldObjects* world_objects, Camera* camera, 
	Uint32 frame_width, Uint32 frame_height)
{
	visibility_culling(world_objects, camera);
	transform_from_world_to_camera_space(world_objects, camera);
	clip_triangles_to_frustum(world_objects, camera);
	transform_scale_to_FOV(world_objects, camera);
	transform_FOV_space_to_01_scale(world_objects);
	transform_to_pixel_space(world_objects, frame_width, frame_height);
	rasterize_objects_to_frame(framebuffer, z_buffer, frame_width, frame_height, world_objects);
}
