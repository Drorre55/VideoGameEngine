#pragma once
#include "world_objects.h"

WorldObjects* transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera);
void _transform_vertex_to_camera_space(vec3 vertex, Camera* camera, vec3 rotation_degrees);
vec3* _get_degrees_between_camera_and_axis(Camera* camera);
