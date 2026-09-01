#pragma once
#include "world_objects.h"

WorldObjects* load_obj_file(const char* filepath);
static Color _face_color_from_normal(vec3 a, vec3 b, vec3 c);