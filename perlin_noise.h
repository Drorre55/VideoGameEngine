#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>
#include <cglm/cglm.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


float perlin_noise(vec2 point, Uint32 octaves, vec2* gradients);
vec2* perlin_gradients(Uint32 octaves, Uint32 seed);
static float perlin_smoothstep(float edge0, float edge1, float x);
Uint32 hash2d(Uint32 x, Uint32 y, Uint32 seed);
