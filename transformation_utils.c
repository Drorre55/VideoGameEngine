#include "transformation_utils.h"
#include "cglm/cglm.h"


uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

void rotate_x_axis(Vec3* vertice, float rotation_degree)
{
	vertice->y = vertice->y * cos(rotation_degree) - vertice->z * sin(rotation_degree);
	vertice->z = vertice->y * sin(rotation_degree) + vertice->z * cos(rotation_degree);
}

void rotate_y_axis(Vec3* vertice, float rotation_degree)
{
	vertice->x = vertice->x * cos(rotation_degree) + vertice->z * sin(rotation_degree);
	vertice->z = vertice->x * -sin(rotation_degree) + vertice->z * cos(rotation_degree);
}

void rotate_z_axis(Vec3* vertice, float rotation_degree)
{
	vertice->x = vertice->x * cos(rotation_degree) - vertice->y * sin(rotation_degree);
	vertice->y = vertice->x * sin(rotation_degree) + vertice->y * cos(rotation_degree);
}

float* lin_interp2d(float* source_vec, float* vector_x, float* vector_y, unsigned int num_objects)
{
	float* distances = calloc(3, sizeof(float));
	if (distances == NULL) {
		SDL_LogError(1, "Problem with calloc. can't perform lin_interp2d");
		return NULL;
	}
	for (int i = 0; i < 3; i++) {
		float x_diff = source_vec[0] - vector_x[i];
		float y_diff = source_vec[1] - vector_y[i];
		distances[i] = x_diff * x_diff + y_diff * y_diff;
	}
	// normalize by the sum of distances
	float sum_distances = distances[0] + distances[1] + distances[2];
	glm_vec3_divs(distances, sum_distances, distances);

	return distances;
}

