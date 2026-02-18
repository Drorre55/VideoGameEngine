#pragma once
#include "world_objects.h"

void rotate_x_axis(Point* vertice, float rotation_degree);
void rotate_y_axis(Point* vertice, float rotation_degree);
void rotate_z_axis(Point* vertice, float rotation_degree);
float* normalize_vector2(float vector[2]);
float dot_product_vector2(float vector[2], float vector2[2]);
float cross_product_vector2(float vector[2], float vector2[2]);