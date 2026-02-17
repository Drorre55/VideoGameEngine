#include "scale_to_FOV_transform.h"


// convert coords from camera space to field of view space - x,y values scaled to [-1, 1], and z is the distance
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera) {
	for (int i = 0; i < world_objects->num_triangles; i++) {
		Triangle* current_triangle = world_objects->triangle_objects[i];

		_transform_point_scale_to_FOV(current_triangle->corner1, camera);
		_transform_point_scale_to_FOV(current_triangle->corner2, camera);
		_transform_point_scale_to_FOV(current_triangle->corner3, camera);
	}
}

void _transform_point_scale_to_FOV(Point* point, Camera* camera)
{
	float distance = sqrt(pow(point->x_coord, 2) + pow(point->y_coord, 2) + pow(point->z_coord, 2));

	point->x_coord /= tan(camera->field_of_view->x_degree_from_center) * point->z_coord;
	point->y_coord /= tan(camera->field_of_view->y_degree_from_center) * point->z_coord;
	point->z_coord = distance;
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
		_is_inside_FOV(triangle->corner1) + 
		_is_inside_FOV(triangle->corner2) +
		_is_inside_FOV(triangle->corner3);
	
	return num_points_on_screen;
}

unsigned int _is_inside_FOV(Point* point)
{
	unsigned int is_outside_x_range = point->x_coord < -1 || point->x_coord > 1;
	unsigned int is_outside_y_range = point->y_coord < -1 || point->y_coord > 1;
	return !is_outside_x_range && !is_outside_y_range;
}

void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects) {
	for (int i = 0; i < world_objects->num_triangles; i++) {
		Triangle* current_triangle = world_objects->triangle_objects[i];

		_transform_point_xy_to_01_scale(current_triangle->corner1);
		_transform_point_xy_to_01_scale(current_triangle->corner2);
		_transform_point_xy_to_01_scale(current_triangle->corner3);
	}
}

void _transform_point_xy_to_01_scale(Point* point)
{
	point->x_coord = (point->x_coord + 1) / 2;
	point->y_coord = (point->y_coord + 1) / 2;
}
