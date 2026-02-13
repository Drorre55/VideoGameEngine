#include "camera_space_transform.h"
#include "transformation_utils.h"


WorldObjects* transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera) {
	WorldObjects* camera_space_objects = (WorldObjects*)calloc(1, sizeof(WorldObjects));
	if (camera_space_objects == NULL) {
		SDL_LogError(1, "Problem with malloc. can't transform to camera space");
		return NULL;
	}
	Point** camera_space_points = (Point**)malloc(world_objects->num_points * sizeof(Point*));
	if (camera_space_points == NULL) {
		SDL_LogError(1, "Problem with malloc. can't transform to camera space");
		return NULL;
	}

	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = (Point*)(malloc(sizeof(Point)));
		if (current_point == NULL) {
			SDL_LogError(1, "Problem with malloc. can't transform to camera space");
			free(camera_space_points);
			return NULL;
		}
		current_point->x_coord = world_objects->point_objects[i]->x_coord;
		current_point->y_coord = world_objects->point_objects[i]->y_coord;
		current_point->z_coord = world_objects->point_objects[i]->z_coord;

		float* degrees_between_camera_and_axis = _get_degrees_between_camera_and_axis(camera);
		rotate_x_axis(current_point, degrees_between_camera_and_axis[0]);
		rotate_y_axis(current_point, degrees_between_camera_and_axis[1]);
		rotate_z_axis(current_point, degrees_between_camera_and_axis[2]);
		camera_space_points[i] = current_point;
		camera_space_objects->num_points++;
		
		free(degrees_between_camera_and_axis);
	}
	camera_space_objects->point_objects = camera_space_points;

	return camera_space_objects;
}

float* _get_degrees_between_camera_and_axis(Camera* camera) {
	float x_axis_rotation_degree = (float)atan(camera->y_direction_vector->z_coord / camera->y_direction_vector->y_coord);
	float y_axis_rotation_degree = (float)atan(camera->z_direction_vector->x_coord / camera->z_direction_vector->z_coord);
	float z_axis_rotation_degree = (float)atan(camera->x_direction_vector->y_coord / camera->x_direction_vector->x_coord);

	float* degrees_between_camera_and_axis = (float*)malloc(3 * sizeof(float));
	if (degrees_between_camera_and_axis == NULL) {
		SDL_LogError(1, "problem with malloc. can't get camera rotation degrees");
		return NULL;
	}
	degrees_between_camera_and_axis[0] = x_axis_rotation_degree;
	degrees_between_camera_and_axis[1] = y_axis_rotation_degree;
	degrees_between_camera_and_axis[2] = z_axis_rotation_degree;
	return degrees_between_camera_and_axis;
}
