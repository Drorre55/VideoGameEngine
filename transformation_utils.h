#pragma once
#include "world_objects.h"

uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void rotate_x_axis(vec3 vertice, float rotation_degree);
void rotate_y_axis(vec3 vertice, float rotation_degree);
void rotate_z_axis(vec3 vertice, float rotation_degree);
float* lin_interp2d(float* source_vec, float* vector_x, float* vector_y, unsigned int num_objects);
