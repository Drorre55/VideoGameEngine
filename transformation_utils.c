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

float* normalize_vector2(float vector[2])
{
	float* normalized_vector = calloc(1, sizeof(float) * 2);
	if (normalized_vector == NULL) {
		SDL_LogError(1, "Problem with calloc. can't normalize vector");
		return NULL;
	}

	float vector_distance = sqrt(dot_product_vector2(vector, vector));

	normalized_vector[0] = vector[0] / vector_distance;
	normalized_vector[1] = vector[1] / vector_distance;
	return normalized_vector;
}

float dot_product_vector2(float vector[2], float vector2[2])
{
	return vector[0] * vector2[0] + vector[1] * vector2[1];
}

float cross_product_vector2(float vector[2], float vector2[2])
{
	return vector[0] * vector2[1] - vector[1] * vector2[0];
}
