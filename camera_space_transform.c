#include "camera_space_transform.h"
#include "transformation_utils.h"


void transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera) {
	for (Uint32 i = 0; i < world_objects->num_vertices; i++) {
		_transform_vertex_to_camera_space(world_objects->vertices[i], camera);
	}
}

void _transform_vertex_to_camera_space(vec3 world_vertex, Camera* camera)
{
	vec3 relative_vertex;
	glm_vec3_sub(world_vertex, camera->global_coords, relative_vertex);

	world_vertex[0] = glm_vec3_dot(relative_vertex, camera->x_direction_vector);
	world_vertex[1] = glm_vec3_dot(relative_vertex, camera->y_direction_vector);
	world_vertex[2] = glm_vec3_dot(relative_vertex, camera->z_direction_vector);
}
