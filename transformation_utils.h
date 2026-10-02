#pragma once

#include "SDL3/SDL.h"
#include "cglm/cglm.h"

Uint32 rgba_to_uint32(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void rotate_x_axis(vec3 vertice, float rotation_degree);
void rotate_y_axis(vec3 vertice, float rotation_degree);
void rotate_z_axis(vec3 vertice, float rotation_degree);
vec3* calc_normal(vec3 a, vec3 b, vec3 c, vec3 dest);

int min3(int a, int b, int c);
float fmax3(float a, float b, float c);

void normalize_3_registers(__m256* x, __m256* y, __m256* z);
float horizontal_average_m128(__m128 v);
