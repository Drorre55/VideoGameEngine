#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>
#include <cglm/cglm.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
	float noise;
	vec2 derivative;
} PerlinResult;


PerlinResult calc_perlin(vec2 point, Uint32 octaves, vec2* gradients);
vec2* perlin_gradients(Uint32 octaves, Uint32 seed);
static inline float _smoothstep_polynomial(float x);
static inline float _smoothstep_polynomial_derivative(float x);
inline Uint32 _hash2d(Uint32 x, Uint32 y, Uint32 seed);
