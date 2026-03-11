#include "camera_space_transform.h"
#include "transformation_utils.h"


WorldObjects* transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera) {
	WorldObjects* camera_space_objects = world_objects_deep_copy(world_objects);
	
	vec3* degrees_between_camera_and_axis = _get_degrees_between_camera_and_axis(camera);
	
	for (int i = 0; i < camera_space_objects->num_vertices; i++) {
		_transform_vertex_to_camera_space(*(camera_space_objects->vertices[i]), camera, *degrees_between_camera_and_axis);
	}
	free(degrees_between_camera_and_axis);

	return camera_space_objects;
}

void _transform_vertex_to_camera_space(vec3 world_vertex, Camera* camera, vec3 rotation_degrees)
{
	world_vertex[0] -= (*camera->global_coords)[0];
	world_vertex[1] -= (*camera->global_coords)[1];
	world_vertex[2] -= (*camera->global_coords)[2];

	rotate_x_axis(world_vertex, rotation_degrees[0]);
	rotate_y_axis(world_vertex, rotation_degrees[1]);
	rotate_z_axis(world_vertex, rotation_degrees[2]);
}

vec3* _get_degrees_between_camera_and_axis(Camera* camera) {
	float x_axis_rotation_degree = (float)atan((*camera->y_direction_vector)[2] / (*camera->y_direction_vector)[1]);
	float y_axis_rotation_degree = (float)atan((*camera->z_direction_vector)[0] / (*camera->z_direction_vector)[2]);
	float z_axis_rotation_degree = (float)atan((*camera->x_direction_vector)[1] / (*camera->x_direction_vector)[0]);

	vec3* degrees_between_camera_and_axis = (vec3*)malloc(sizeof(vec3));
	if (degrees_between_camera_and_axis == NULL) {
		SDL_LogError(1, "problem with malloc. can't get camera rotation degrees");
		return NULL;
	}
	(*degrees_between_camera_and_axis)[0] = x_axis_rotation_degree;
	(*degrees_between_camera_and_axis)[1] = y_axis_rotation_degree;
	(*degrees_between_camera_and_axis)[2] = z_axis_rotation_degree;
	return degrees_between_camera_and_axis;
}
