#include "camera_space_transform.h"
#include "transformation_utils.h"


void transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera) {
	for (Uint32 i = 0; i < world_objects->num_vertices; i++) {
		_transform_position_to_camera_space(world_objects->positions[i], camera);
	}
}

void _transform_position_to_camera_space(vec3 world_position, Camera* camera)
{
	vec3 relative_position;
	glm_vec3_sub(world_position, camera->global_coords, relative_position);

	world_position[0] = glm_vec3_dot(relative_position, camera->x_direction_vector);
	world_position[1] = glm_vec3_dot(relative_position, camera->y_direction_vector);
	world_position[2] = glm_vec3_dot(relative_position, camera->z_direction_vector);
}
