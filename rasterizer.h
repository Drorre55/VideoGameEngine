#pragma once
#include "engine.h"

void rasterize_objects_to_frame(
	uint32_t* frame, 
	Camera* camera, 
	unsigned int frame_width, 
	unsigned int frame_height, 
	WorldObjects* on_screen_objects
);