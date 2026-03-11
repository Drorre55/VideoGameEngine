#pragma once
#include "world_objects.h"
#include "transformation_utils.h"
#include "camera_space_transform.h"
#include "scale_to_FOV_transform.h"
#include "rasterizer.h"


void run_graphics_pipeline(
	uint32_t* framebuffer, 
	float* z_buffer,
	WorldObjects* world_objects, 
	Camera* camera, 
	unsigned int frame_width, 
	unsigned int frame_height
);