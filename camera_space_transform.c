#include "camera_space_transform.h"
#include "transformation_utils.h"


WorldObjects* transform_from_world_to_camera_space(WorldObjects* world_objects, Camera* camera) {
	WorldObjects* camera_space_objects = (WorldObjects*)calloc(1, sizeof(WorldObjects));
	if (camera_space_objects == NULL) {
		SDL_LogError(1, "Problem with malloc. can't transform to camera space");
		return NULL;
	}
	Triangle** camera_space_triangles = (Triangle**)malloc(world_objects->num_triangles * sizeof(Triangle*));
	if (camera_space_triangles == NULL) {
		SDL_LogError(1, "Problem with malloc. can't transform to camera space");
		return NULL;
	}
	
	float* degrees_between_camera_and_axis = _get_degrees_between_camera_and_axis(camera);

	for (int i = 0; i < world_objects->num_triangles; i++) {
		Triangle* transformed_triangle = (Triangle*)(malloc(sizeof(Triangle)));
		if (transformed_triangle == NULL) {
			SDL_LogError(1, "Problem with malloc. can't transform to camera space");
			free(camera_space_triangles);
			return NULL;
		}
		
		transformed_triangle->corner1 = _transform_point_from_world_to_camera_space(
			world_objects->triangle_objects[i]->corner1, camera, degrees_between_camera_and_axis);
		transformed_triangle->corner2 = _transform_point_from_world_to_camera_space(
			world_objects->triangle_objects[i]->corner2, camera, degrees_between_camera_and_axis);
		transformed_triangle->corner3 = _transform_point_from_world_to_camera_space(
			world_objects->triangle_objects[i]->corner3, camera, degrees_between_camera_and_axis);
		
		camera_space_triangles[i] = transformed_triangle;
		camera_space_objects->num_triangles++;
		
	}
	camera_space_objects->triangle_objects = camera_space_triangles;
	free(degrees_between_camera_and_axis);

	return camera_space_objects;
}

Point* _transform_point_from_world_to_camera_space(Point* point, Camera* camera, float rotation_degrees[3])
{
	Point* transformed_point = (Point*)(malloc(sizeof(Point)));
	if (transformed_point == NULL) {
		SDL_LogError(1, "Problem with malloc. can't transform to camera space");
		return NULL;
	}
	transformed_point->x_coord = point->x_coord - camera->global_coords->x_coord;
	transformed_point->y_coord = point->y_coord - camera->global_coords->y_coord;
	transformed_point->z_coord = point->z_coord - camera->global_coords->z_coord;

	rotate_x_axis(transformed_point, rotation_degrees[0]);
	rotate_y_axis(transformed_point, rotation_degrees[1]);
	rotate_z_axis(transformed_point, rotation_degrees[2]);
	return transformed_point;
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
