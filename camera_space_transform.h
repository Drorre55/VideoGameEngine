#pragma once
#include "engine.h"

WorldObjects* transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera);
float* _get_degrees_between_camera_and_axis(Camera* camera);
