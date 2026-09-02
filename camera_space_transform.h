#pragma once
#include "world_objects.h"

void transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera);
void _transform_vertex_to_camera_space(vec3 vertex, Camera* camera);