#pragma once
#include "graphics_pipeline.h"


void run_graphics_pipeline(Uint32* framebuffer, float* z_buffer, WorldObjects* world_objects, Camera* camera, 
	Uint32 frame_width, Uint32 frame_height)
{
	WorldObjects* objects_copy = world_objects_deep_copy(world_objects);
	visibility_culling(objects_copy, camera);
	transform_from_world_to_camera_space(objects_copy, camera);
	clip_triangles_to_frustum(objects_copy, camera);
	transform_scale_to_FOV(objects_copy, camera);
	transform_FOV_space_to_01_scale(objects_copy);
	transform_to_pixel_space(objects_copy, frame_width, frame_height);
	rasterize_objects_to_frame(framebuffer, z_buffer, frame_width, frame_height, objects_copy);
}
