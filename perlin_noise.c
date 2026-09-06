#include "perlin_noise.h"


float perlin_noise(vec2 point, Uint32 octaves, vec2* gradients) {
	vec2 scaled_point = { point[0] * octaves, point[1] * octaves };
	vec2 cell_bottom_left;
	glm_vec2_floor(scaled_point, cell_bottom_left);

	vec2 point_normalized_in_cell;
	glm_vec2_sub(scaled_point, cell_bottom_left, point_normalized_in_cell);

	vec2 corners_offset_vector00, corners_offset_vector01, corners_offset_vector10, corners_offset_vector11;
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 0.f, 0.f }, corners_offset_vector00);
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 0.f, 1.f }, corners_offset_vector01);
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 1.f, 0.f }, corners_offset_vector10);
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 1.f, 1.f }, corners_offset_vector11);

	float corner_influence00 = glm_vec2_dot(corners_offset_vector00,
		gradients[(Uint32)cell_bottom_left[0] * (octaves + 1) + (Uint32)cell_bottom_left[1]]);
	float corner_influence01 = glm_vec2_dot(corners_offset_vector01,
		gradients[(Uint32)cell_bottom_left[0] * (octaves + 1) + (Uint32)cell_bottom_left[1] + 1]);
	float corner_influence10 = glm_vec2_dot(corners_offset_vector10,
		gradients[((Uint32)cell_bottom_left[0] + 1) * (octaves + 1) + (Uint32)cell_bottom_left[1]]);
	float corner_influence11 = glm_vec2_dot(corners_offset_vector11,
		gradients[((Uint32)cell_bottom_left[0] + 1) * (octaves + 1) + (Uint32)cell_bottom_left[1] + 1]);

	float interp_row0 = perlin_smoothstep(corner_influence00, corner_influence01, point_normalized_in_cell[1]);
	float interp_row1 = perlin_smoothstep(corner_influence10, corner_influence11, point_normalized_in_cell[1]);
	float interp_col = perlin_smoothstep(interp_row0, interp_row1, point_normalized_in_cell[0]);

	return interp_col;
}

vec2* perlin_gradients(Uint32 octaves, Uint32 seed) {
	vec2* gradients = malloc((octaves + 1) * (octaves + 1) * sizeof(vec2));
	for (Uint32 i = 0; i < octaves + 1; i++) {
		for (Uint32 j = 0; j < octaves + 1; j++) {
			Uint32 random = hash2d(j, i, seed);
			float random_radian = M_PI * (float)random / 180.0f;
			vec2 gradient = { cosf(random_radian), sinf(random_radian) };
			glm_vec2_copy(gradient, gradients[i * (octaves + 1) + j]);
		}
	}
	return gradients;
}

static float perlin_smoothstep(float edge0, float edge1, float x) {
	// Evaluate the 5th order polynomial : 6x ^ 5 - 15x ^ 4 + 10x ^ 3
	return edge0 + (edge1 - edge0) * (x * x * x * (x * (x * 6.0 - 15.0) + 10.0));
}

inline Uint32 hash2d(Uint32 x, Uint32 y, Uint32 seed) {
	Uint32 hash = seed * (x * 3266489917U + y * 668265263U);
	hash = (hash ^ (hash >> 16)) * 2246822519U;
	hash = (hash ^ (hash >> 13)) * 3266489917U;
	return hash ^ (hash >> 16);
}