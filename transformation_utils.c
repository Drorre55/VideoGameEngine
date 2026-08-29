#include "transformation_utils.h"
#include "cglm/cglm.h"


Uint32 rgba_to_uint32(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	return ((Uint32)r << 24) | ((Uint32)g << 16) | ((Uint32)b << 8) | (Uint32)a;
}

void rotate_x_axis(vec3 vertex, float rotation_radians)
{
    float y = vertex[1];
    float z = vertex[2];
    float cosine = cosf(rotation_radians);
    float sine = sinf(rotation_radians);

    vertex[1] = y * cosine - z * sine;
    vertex[2] = y * sine + z * cosine;
}

void rotate_y_axis(vec3 vertex, float rotation_radians)
{
    float x = vertex[0];
    float z = vertex[2];
    float cosine = cosf(rotation_radians);
    float sine = sinf(rotation_radians);

    vertex[0] = x * cosine + z * sine;
    vertex[2] = -x * sine + z * cosine;
}

void rotate_z_axis(vec3 vertex, float rotation_radians)
{
    float x = vertex[0];
    float y = vertex[1];
    float cosine = cosf(rotation_radians);
    float sine = sinf(rotation_radians);

    vertex[0] = x * cosine - y * sine;
    vertex[1] = x * sine + y * cosine;
}
float* lin_interp2d(float* source_vec, float* vector_x, float* vector_y, Uint32 num_objects)
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

