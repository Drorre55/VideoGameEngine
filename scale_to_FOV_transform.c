#include "scale_to_FOV_transform.h"

// convert coords from camera space to field of view space - x,y values scaled to [-1, 1], and z [0,1]
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera) {
	float horizontal_scale = tanf(camera->field_of_view->x_degree_from_center);
	float vertical_scale = tanf(camera->field_of_view->y_degree_from_center);

	for (Uint32 i = 0; i < world_objects->num_vertices; i++) {
		vec3* vertex = world_objects->vertices[i];

		float vertex_z = (*vertex)[2];

		(*vertex)[0] /= horizontal_scale * vertex_z;
		(*vertex)[1] /= vertical_scale * vertex_z;
		(*vertex)[2] = (vertex_z - VIEW_FRUSTUM_MIN) / (VIEW_FRUSTUM_MAX - VIEW_FRUSTUM_MIN);
	}
}

void transform_FOV_space_to_01_scale(WorldObjects* world_objects) {
	for (int i = 0; i < world_objects->num_vertices; i++) {
		vec3* vertex = world_objects->vertices[i];
		(*vertex)[0] = ((*vertex)[0] + 1) / 2;
		(*vertex)[1] = ((*vertex)[1] + 1) / 2;
	}
}
