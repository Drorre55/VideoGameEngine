#include "scale_to_FOV_transform.h"

// convert coords from camera space to field of view space - x,y values scaled to [-1, 1], and z [0,1]
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera) {
	float horizontal_scale = tanf(camera->field_of_view->x_degree_from_center);
	float vertical_scale = tanf(camera->field_of_view->y_degree_from_center);

	for (Uint32 i = 0; i < world_objects->num_vertices; i++) {
		vec3* position = world_objects->positions[i];

		float position_z = (*position)[2];

		(*position)[0] /= horizontal_scale * position_z;
		(*position)[1] /= vertical_scale * position_z;
		// inverse z -> small_value = far, big_value = close
		(*position)[2] = 1.f / position_z; //(position_z - VIEW_FRUSTUM_MIN) / (VIEW_FRUSTUM_MAX - VIEW_FRUSTUM_MIN);

		world_objects->uvs[i][0] /= position_z;
		world_objects->uvs[i][1] /= position_z;
	}
}

void transform_FOV_space_to_01_scale(WorldObjects* world_objects) {
	for (int i = 0; i < world_objects->num_vertices; i++) {
		vec3* position = world_objects->positions[i];
		(*position)[0] = ((*position)[0] + 1) / 2;
		(*position)[1] = ((*position)[1] + 1) / 2;
	}
}
