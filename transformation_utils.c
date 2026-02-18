#include "transformation_utils.h"


uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

void rotate_x_axis(Point* vertice, float rotation_degree)
{
	vertice->y_coord = vertice->y_coord * cos(rotation_degree) - vertice->z_coord * sin(rotation_degree);
	vertice->z_coord = vertice->y_coord * sin(rotation_degree) + vertice->z_coord * cos(rotation_degree);
}

void rotate_y_axis(Point* vertice, float rotation_degree)
{
	vertice->x_coord = vertice->x_coord * cos(rotation_degree) + vertice->z_coord * sin(rotation_degree);
	vertice->z_coord = vertice->x_coord * -sin(rotation_degree) + vertice->z_coord * cos(rotation_degree);
}

void rotate_z_axis(Point* vertice, float rotation_degree)
{
	vertice->x_coord = vertice->x_coord * cos(rotation_degree) - vertice->y_coord * sin(rotation_degree);
	vertice->y_coord = vertice->x_coord * sin(rotation_degree) + vertice->y_coord * cos(rotation_degree);
}

float* normalize_vector(float* vector, unsigned int num_dims)
{
	float* normalized_vector = calloc(1, sizeof(float) * num_dims);
	if (normalized_vector == NULL) {
		SDL_LogError(1, "Problem with calloc. can't normalize vector");
		return NULL;
	}

	float vector_distance = sqrt(dot_product_vector(vector, vector, num_dims));

	for (int i = 0; i < num_dims; i++)
		normalized_vector[i] = vector[i] / vector_distance;
	return normalized_vector;
}

float dot_product_vector(float* vector, float* vector2, unsigned int num_dims)
{
	float result = 0;
	for (int i = 0; i < num_dims; i++)
		result += vector[i] * vector2[i];
	return result;
}

float cross_product_vector2(float vector[2], float vector2[2])
{
	return vector[0] * vector2[1] - vector[1] * vector2[0];
}

float* lin_interp2d(float* source_vec, float* vector_x, float* vector_y, unsigned int num_objects)
{
	float* distances = calloc(1, sizeof(float) * num_objects);
	if (distances == NULL) {
		SDL_LogError(1, "Problem with calloc. can't perform lin_interp2d");
		return NULL;
	}
	for (int i = 0; i < num_objects; i++) {
		float x_diff = fabs(source_vec[0] - vector_x[i]);
		float y_diff = fabs(source_vec[1] - vector_y[i]);
		float diff_vec[2] = { x_diff, y_diff };
		distances[i] = sqrt(pow(x_diff, 2) + pow(y_diff, 2));
	}
	// normalize by the sum of distances
	float sum_distances = 0;
	for (int i = 0; i < num_objects; i++)
		sum_distances += distances[i];
	for (int i = 0; i < num_objects; i++)
		distances[i] /= sum_distances;

	return distances;
}

float* multiply_vec(float* vector, float* vector_b, unsigned int num_dims)
{
	float* result_vec = calloc(1, sizeof(float) * num_dims);
	if (result_vec == NULL) {
		SDL_LogError(1, "Problem with calloc. can't perform lin_interp2d");
		return NULL;
	}
	for (int i = 0; i < num_dims; i++)
		result_vec[i] = vector[i] * vector_b[i];
	return result_vec;
}

