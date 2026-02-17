#pragma once
#include "world_objects.h"

void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera);
void _transform_point_scale_to_FOV(Point* point, Camera* camera);
void cut_objects_completely_outside_FOV(WorldObjects* world_objects);
unsigned int _count_points_inside_FOV(Triangle* triangle);
unsigned int _is_inside_FOV(Point* point);
void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects);
void _transform_point_xy_to_01_scale(Point* point);