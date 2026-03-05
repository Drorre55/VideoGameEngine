#include "scale_to_FOV_transform.h"
#define VIEW_FRUSTUM_MAX 10000.0f

// convert coords from camera space to field of view space - x,y values scaled to [-1, 1], and z is the distance
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera) {
	for (int i = 0; i < world_objects->num_triangles; i++) {
		Triangle* current_triangle = world_objects->triangle_objects[i];

		_transform_point_scale_to_FOV(current_triangle->corner1->coords, camera);
		_transform_point_scale_to_FOV(current_triangle->corner2->coords, camera);
		_transform_point_scale_to_FOV(current_triangle->corner3->coords, camera);
	}
}

void _transform_point_scale_to_FOV(Vec3* coords, Camera* camera)
{
	float distance = sqrt(pow(coords->x, 2) + pow(coords->y, 2) + pow(coords->z, 2));

	coords->x /= tan(camera->field_of_view->x_degree_from_center) * coords->z;
	coords->y /= tan(camera->field_of_view->y_degree_from_center) * coords->z;
	coords->z = (1.0f / distance - 1.0f / VIEW_FRUSTUM_MAX) / (1.0f - 1.0f / VIEW_FRUSTUM_MAX) ;
}

void cut_objects_completely_outside_FOV(WorldObjects* FOV_space_objects) {
	// We take the max traingles possible right now to avoid reallocation
	Triangle** triangles_in_FOV = (Triangle**)malloc(sizeof(Triangle*) * FOV_space_objects->num_triangles);
	if (triangles_in_FOV == NULL) {
		SDL_LogError(1, "Problem with malloc. can't cut objects outside of screen");
		return;
	}
	
	unsigned int num_triangles_in_FOV = 0;
	for (int i = 0; i < FOV_space_objects->num_triangles; i++) {
		Triangle* current_triangle = FOV_space_objects->triangle_objects[i];
		if (_count_points_inside_FOV(current_triangle) > 0) {
			triangles_in_FOV[num_triangles_in_FOV] = current_triangle;
			num_triangles_in_FOV++;
		}
	}
	free(FOV_space_objects->triangle_objects);
	FOV_space_objects->triangle_objects = triangles_in_FOV;
	FOV_space_objects->num_triangles = num_triangles_in_FOV;
}

unsigned int _count_points_inside_FOV(Triangle* triangle) {
	unsigned int num_points_on_screen = 
		_is_inside_FOV(triangle->corner1->coords, 1.2) + 
		_is_inside_FOV(triangle->corner2->coords, 1.2) +
		_is_inside_FOV(triangle->corner3->coords, 1.2);
	return num_points_on_screen;
}

unsigned int _is_inside_FOV(Vec3* coords, float extended_bounds_percentage)
{
	float max_bound = 1.0f * extended_bounds_percentage;
	float min_bound = -max_bound;
	unsigned int is_outside_x_range = coords->x < min_bound || coords->x > max_bound;
	unsigned int is_outside_y_range = coords->y < min_bound || coords->y > max_bound;
	unsigned int is_outside_z_range = coords->z < 0.0f || coords->z > 1.0f;
	return !is_outside_x_range && !is_outside_y_range && !is_outside_z_range;
}

void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects) {
	for (int i = 0; i < world_objects->num_triangles; i++) {
		Triangle* current_triangle = world_objects->triangle_objects[i];

		_transform_point_xy_to_01_scale(current_triangle->corner1->coords);
		_transform_point_xy_to_01_scale(current_triangle->corner2->coords);
		_transform_point_xy_to_01_scale(current_triangle->corner3->coords);
	}
}

void _transform_point_xy_to_01_scale(Vec3* coords)
{
	coords->x = (coords->x + 1) / 2;
	coords->y = (coords->y + 1) / 2;
}
