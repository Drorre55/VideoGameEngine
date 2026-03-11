#pragma once
#include "world_objects.h"
#define VIEW_FRUSTUM_MAX 1000.0f


void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera);
void cut_triangles_completely_outside_FOV(WorldObjects* world_objects);
unsigned int _count_points_inside_FOV(Triangle* triangle, bool* is_vertex_in_FOV);
bool _is_inside_FOV(vec3 coords, float extended_bounds_percentage);
void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects);