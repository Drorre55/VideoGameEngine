#include "scale_to_FOV_transform.h"


// convert coords from camera space to field of view space - x,y values scaled to [-1, 1], and z is the distance
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera) {
	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = world_objects->point_objects[i];

		float distance = sqrt(pow(current_point->x_coord, 2) + pow(current_point->y_coord, 2) + pow(current_point->z_coord, 2));

		current_point->x_coord /= tan(camera->field_of_view->x_degree_from_center) * current_point->z_coord;
		current_point->y_coord /= tan(camera->field_of_view->y_degree_from_center) * current_point->z_coord;
		current_point->z_coord = distance;
		//SDL_Log("transform_scale_to_FOV: (%f, %f, %f)", current_point->x_coord, current_point->y_coord, distance);
	}
}

void cut_objects_outside_FOV(WorldObjects* world_objects) {
	// We take the max points possible right now to avoid reallocation
	Point** points_on_screen = (Point**)malloc(sizeof(Point*) * world_objects->num_points);
	if (points_on_screen == NULL) {
		SDL_LogError(1, "Problem with malloc. can't cut objects outside of screen");
		return;
	}
	
	unsigned int num_points_on_screen = 0;
	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = world_objects->point_objects[i];

		unsigned int is_outside_x_range = current_point->x_coord < -1 || current_point->x_coord > 1;
		unsigned int is_outside_y_range = current_point->y_coord < -1 || current_point->y_coord > 1;
		if (is_outside_x_range || is_outside_y_range) {
			free(current_point);
		}
		else {
			points_on_screen[num_points_on_screen] = current_point;
			num_points_on_screen++;
		}
	}
	free(world_objects->point_objects);
	world_objects->point_objects = points_on_screen;
	world_objects->num_points = num_points_on_screen;
}

void transform_xy_from_FOV_space_to_01_scale(WorldObjects* world_objects, Camera* camera) {
	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = world_objects->point_objects[i];

		current_point->x_coord = (current_point->x_coord + 1) / 2;
		current_point->y_coord = (current_point->y_coord + 1) / 2;
	}
}
