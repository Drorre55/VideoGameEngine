#pragma once
#include "world_objects.h"
#define VIEW_FRUSTUM_MAX 1000.0f


void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera);
void _transform_point_scale_to_FOV(Vec3* coords, Camera* camera);
void cut_objects_completely_outside_FOV(WorldObjects* world_objects);
unsigned int _count_points_inside_FOV(Triangle* triangle);
unsigned int _is_inside_FOV(Vec3* coords, float extended_bounds_percentage);
void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects);
void _transform_point_xy_to_01_scale(Vec3* coords);