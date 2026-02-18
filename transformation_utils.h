#pragma once
#include "world_objects.h"

uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void rotate_x_axis(Point* vertice, float rotation_degree);
void rotate_y_axis(Point* vertice, float rotation_degree);
void rotate_z_axis(Point* vertice, float rotation_degree);
float* normalize_vector2(float vector[2]);
float dot_product_vector2(float vector[2], float vector2[2]);
float cross_product_vector2(float vector[2], float vector2[2]);