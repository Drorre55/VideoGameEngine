#pragma once
#include "world_objects.h"
#include "transformation_utils.h"
#include "camera_space_transform.h"
#include "scale_to_FOV_transform.h"
#include "rasterizer.h"


void run_graphics_pipeline(
	Uint32* framebuffer, 
	float* z_buffer,
	WorldObjects* world_objects, 
	Camera* camera, 
	Uint32 frame_width, 
	Uint32 frame_height
);