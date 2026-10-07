#include "perlin_noise.h"


PerlinResult calc_perlin(vec2 point, Uint32 octaves, vec2* gradients) {
	PerlinResult result;

	vec2 scaled_point = { point[0] * octaves, point[1] * octaves };
	vec2 cell_bottom_left;
	glm_vec2_floor(scaled_point, cell_bottom_left);

	vec2 point_normalized_in_cell;
	glm_vec2_sub(scaled_point, cell_bottom_left, point_normalized_in_cell);

	vec2 corner_offset_vector00, corner_offset_vector01, corner_offset_vector10, corner_offset_vector11;
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 0.f, 0.f }, corner_offset_vector00);
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 0.f, 1.f }, corner_offset_vector01);
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 1.f, 0.f }, corner_offset_vector10);
	glm_vec2_sub(point_normalized_in_cell, (vec2) { 1.f, 1.f }, corner_offset_vector11);

	vec2 *gradient00, *gradient01, *gradient10, *gradient11;
	gradient00 = gradients[(Uint32)cell_bottom_left[0] * (octaves + 1) + (Uint32)cell_bottom_left[1]];
	gradient01 = gradients[(Uint32)cell_bottom_left[0] * (octaves + 1) + (Uint32)cell_bottom_left[1] + 1];
	gradient10 = gradients[((Uint32)cell_bottom_left[0] + 1) * (octaves + 1) + (Uint32)cell_bottom_left[1]];
	gradient11 = gradients[((Uint32)cell_bottom_left[0] + 1) * (octaves + 1) + (Uint32)cell_bottom_left[1] + 1];

	float corner_influence00 = glm_vec2_dot(corner_offset_vector00, gradient00);
	float corner_influence01 = glm_vec2_dot(corner_offset_vector01, gradient01);
	float corner_influence10 = glm_vec2_dot(corner_offset_vector10, gradient10);
	float corner_influence11 = glm_vec2_dot(corner_offset_vector11, gradient11);

	vec2 dcorner_influence00_dxz, dcorner_influence01_dxz, dcorner_influence10_dxz, dcorner_influence11_dxz;
	glm_vec2_copy(gradient00, dcorner_influence00_dxz);
	glm_vec2_copy(gradient01, dcorner_influence01_dxz);
	glm_vec2_copy(gradient10, dcorner_influence10_dxz);
	glm_vec2_copy(gradient11, dcorner_influence11_dxz);

	vec2 p_smoothstep_polynomial = {
		_smoothstep_polynomial(point_normalized_in_cell[0]),
		_smoothstep_polynomial(point_normalized_in_cell[1])
	};
	vec2 p_smoothstep_polynomial_der = {
		_smoothstep_polynomial_derivative(point_normalized_in_cell[0]),
		_smoothstep_polynomial_derivative(point_normalized_in_cell[1])
	};

	float interp_row0 = corner_influence00 + (corner_influence01 - corner_influence00) * p_smoothstep_polynomial[1];
	float interp_row1 = corner_influence10 + (corner_influence11 - corner_influence10) * p_smoothstep_polynomial[1];
	float interp_col = interp_row0 + (interp_row1 - interp_row0) * p_smoothstep_polynomial[0];
	result.noise = interp_col;

	/*vec2 dinterp_row0_dxz = {
		(
			dcorner_influence00_dxz[0] +
			(dcorner_influence01_dxz[0] - dcorner_influence00_dxz[0]) * p_smoothstep_polynomial[1]
			),
		(
			dcorner_influence00_dxz[1] +
			(dcorner_influence01_dxz[1] - dcorner_influence00_dxz[1]) * p_smoothstep_polynomial[1] *
			(corner_influence01 - corner_influence00) * p_smoothstep_polynomial_der[1]
			)
	};
	vec2 dinterp_row1_dxz = {
		(
			dcorner_influence10_dxz[0] +
			(dcorner_influence11_dxz[0] - dcorner_influence10_dxz[0]) * p_smoothstep_polynomial[1]
			),
		(
			dcorner_influence10_dxz[1] +
			(dcorner_influence11_dxz[1] - dcorner_influence10_dxz[1]) * p_smoothstep_polynomial[1] *
			(corner_influence11 - corner_influence10) * p_smoothstep_polynomial_der[1]
			)
	};
	vec2 dinterp_col_dxz = {
		(
			dinterp_row0_dxz[0] +
			(dinterp_row1_dxz[0] - dinterp_row0_dxz[0]) * p_smoothstep_polynomial[0] *
			(interp_row1 - interp_row0) * p_smoothstep_polynomial_der[0]
			),
		(
			dinterp_row0_dxz[1] +
			(dinterp_row1_dxz[1] - dinterp_row0_dxz[1]) * p_smoothstep_polynomial[0]
			)
	};*/
	vec2 dinterp_col_dxz = {
		(
			(interp_row1 - interp_row0) * p_smoothstep_polynomial_der[0]
			),
		(
			(corner_influence01 - corner_influence00 + (corner_influence11 - corner_influence10 - corner_influence01 + corner_influence00) * p_smoothstep_polynomial[0])
			)
	};
	glm_vec2_copy(dinterp_col_dxz, result.derivative);

	return result;
}

vec2* perlin_gradients(Uint32 octaves, Uint32 seed) {
	vec2* gradients = malloc((octaves + 1) * (octaves + 1) * sizeof(vec2));
	for (Uint32 i = 0; i < octaves + 1; i++) {
		for (Uint32 j = 0; j < octaves + 1; j++) {
			Uint32 random = _hash2d(j, i, seed);
			float random_radian = M_PI * (float)random / 180.0f;
			vec2 gradient = { cosf(random_radian), sinf(random_radian) };
			glm_vec2_copy(gradient, gradients[i * (octaves + 1) + j]);
		}
	}
	return gradients;
}

static inline float _smoothstep_polynomial(float x) {
	// 6x ^ 5 - 15x ^ 4 + 10x ^ 3
	return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

static inline float _smoothstep_polynomial_derivative(float x) {
	// 30x ^ 4 - 60x ^ 3 + 30x ^ 2
	return x * x * (x * (x * 30.0 - 60.0) + 30.0);
}

static inline Uint32 _hash2d(Uint32 x, Uint32 y, Uint32 seed) {
	Uint32 hash = seed * (x * 3266489917U + y * 668265263U);
	hash = (hash ^ (hash >> 16)) * 2246822519U;
	hash = (hash ^ (hash >> 13)) * 3266489917U;
	return hash ^ (hash >> 16);
}