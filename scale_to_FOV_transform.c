#include "scale_to_FOV_transform.h"

// convert coords from camera space to field of view space - x,y values scaled to [-1, 1], and z is the distance
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera) {
	for (int i = 0; i < world_objects->num_vertices; i++) {
		vec3* vertex = world_objects->vertices[i];

		float distance = sqrt(pow((*vertex)[0], 2) + pow((*vertex)[1], 2) + pow((*vertex)[2], 2));

		(*vertex)[0] /= tan(camera->field_of_view->x_degree_from_center) * (*vertex)[2];
		(*vertex)[1] /= tan(camera->field_of_view->y_degree_from_center) * (*vertex)[2];
		(*vertex)[2] = (1.0f / distance - 1.0f / VIEW_FRUSTUM_MAX) / (1.0f - 1.0f / VIEW_FRUSTUM_MAX);
	}
}

void cut_triangles_completely_outside_FOV(WorldObjects* FOV_space_objects) {
	bool* is_vertex_in_FOV = (bool*)malloc(FOV_space_objects->num_vertices * sizeof(bool));
	if (is_vertex_in_FOV == NULL) {
		SDL_LogError(1, "Problem with malloc. can't cut triangles outside of FOV");
		return NULL;
	}
	for (int i = 0; i < FOV_space_objects->num_vertices; i++) {
		is_vertex_in_FOV[i] = _is_inside_FOV(*(FOV_space_objects->vertices[i]), 1.2);
	}

	// We take the max traingles possible right now to avoid reallocation
	Triangle** triangles_in_FOV = (Triangle**)malloc(FOV_space_objects->num_triangles * sizeof(Triangle*));
	if (triangles_in_FOV == NULL) {
		SDL_LogError(1, "Problem with malloc. can't cut objects outside of screen");
		return;
	}
	
	unsigned int num_triangles_in_FOV = 0;
	for (int i = 0; i < FOV_space_objects->num_triangles; i++) {
		Triangle* current_triangle = FOV_space_objects->triangles[i];
		if (_count_points_inside_FOV(current_triangle, is_vertex_in_FOV) > 0) {
			triangles_in_FOV[num_triangles_in_FOV] = current_triangle;
			num_triangles_in_FOV++;
		}
	}
	free(FOV_space_objects->triangles);
	FOV_space_objects->triangles = triangles_in_FOV;
	FOV_space_objects->num_triangles = num_triangles_in_FOV;
}

unsigned int _count_points_inside_FOV(Triangle* triangle, bool* is_vertex_in_FOV) {
	unsigned int num_points_on_screen = 
		is_vertex_in_FOV[triangle->corner1_idx] +
		is_vertex_in_FOV[triangle->corner2_idx] +
		is_vertex_in_FOV[triangle->corner3_idx];
	return num_points_on_screen;
}

bool _is_inside_FOV(vec3 coords, float extended_bounds_percentage)
{
	float max_bound = 1.0f * extended_bounds_percentage;
	float min_bound = -max_bound;
	bool is_outside_x_range = coords[0] < min_bound || coords[0] > max_bound;
	bool is_outside_y_range = coords[1] < min_bound || coords[1] > max_bound;
	bool is_outside_z_range = coords[2] < 0.0f || coords[2] > 1.0f;
	return !is_outside_x_range && !is_outside_y_range && !is_outside_z_range;
}

void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects) {
	for (int i = 0; i < world_objects->num_vertices; i++) {
		vec3* vertex = world_objects->vertices[i];
		(*vertex)[0] = ((*vertex)[0] + 1) / 2;
		(*vertex)[1] = ((*vertex)[1] + 1) / 2;
	}
}
