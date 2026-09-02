#pragma once
#include "world_objects.h"

Uint32 rgba_to_uint32(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void rotate_x_axis(vec3 vertice, float rotation_degree);
void rotate_y_axis(vec3 vertice, float rotation_degree);
void rotate_z_axis(vec3 vertice, float rotation_degree);
vec3* calc_normal(vec3 a, vec3 b, vec3 c, vec3 dest);