#pragma once
#include "world_objects.h"

uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void rotate_x_axis(Point* vertice, float rotation_degree);
void rotate_y_axis(Point* vertice, float rotation_degree);
void rotate_z_axis(Point* vertice, float rotation_degree);
float* normalize_vector(float* vector, unsigned int num_dims);
float dot_product_vector(float* vector, float* vector2, unsigned int num_dims);
float cross_product_vector2(float vector[2], float vector2[2]);
float* lin_interp2d(float* source_vec, float* vector_x, float* vector_y, unsigned int num_objects);
float* multiply_vec(float* vector, float* vector_b, unsigned int num_dims);