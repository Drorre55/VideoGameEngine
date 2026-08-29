#pragma once
#include "world_objects.h"


void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera);
void filter_non_visible_triangles(WorldObjects* world_objects);
Uint32 _count_points_inside_FOV(Triangle triangle, bool* is_vertex_in_FOV);
bool _is_inside_FOV(vec3 coords, float extended_bounds_percentage);
void transform_FOV_space_to_01_scale(WorldObjects* world_objects);