#pragma once
#include "engine.h"
#include "transformation_utils.h"
#include "camera_space_transform.h"
#include "scale_to_FOV_transform.h"
#include "rasterizer.h"


void move_camera_direction(Camera* camera, float relative_x, float relative_y, unsigned int window_width, unsigned int window_height);

void run_graphics_pipeline(
	uint32_t* framebuffer, 
	WorldObjects* world_objects, 
	Camera* camera, 
	unsigned int frame_width, 
	unsigned int frame_height
);