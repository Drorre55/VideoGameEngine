#pragma once
#include "world_objects.h"

void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera);
void cut_objects_outside_FOV(WorldObjects* world_objects);
void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects, Camera* camera);