#include "engine.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "SDL3/SDL.h"


uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

Camera* load_camera(unsigned int window_width, unsigned int window_height) {
	Camera* camera = calloc(1, sizeof(Camera));
	if (camera == NULL) {
		SDL_Log("problem with malloc. can't load camera");
		return NULL;
	}

	Point* cam_point = calloc(1, sizeof(Point));
	if (cam_point == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(camera);
		return NULL;
	}
	camera->global_coords = cam_point;
	camera->x_direction_vector[0] = 0;
	camera->x_direction_vector[1] = 1;
	camera->y_direction_vector[0] = 0;
	camera->y_direction_vector[1] = 1;

	FOV* field_of_view = malloc(sizeof(FOV));
	if (field_of_view == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->field_of_view = field_of_view;
	// TODO: Need to change this function - the ratio is too big
	camera->field_of_view->x_degree_from_center = M_PI * (FOV_CHOSEN / 180.0) / 2.0; 
	camera->field_of_view->y_degree_from_center = M_PI * ((float)window_height / (float)window_width) * (FOV_CHOSEN / 180.0) / 2.0;
	return camera;
}

WorldObjects* load_world_objects() {
	WorldObjects* world_objects = calloc(1, sizeof(WorldObjects));
	if (world_objects == NULL) {
		SDL_Log("problem with malloc. can't load world objects");
		return NULL;
	}

	Point* point_a = malloc(sizeof(Point));
	if (point_a == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_a->x_coord = 200.0;
	point_a->y_coord = 100.0;
	point_a->z_coord = 1.0;
	point_a->color = rgba_to_uint32(255, 255, 255, 255);
	
	world_objects->point_objects = malloc(sizeof(Point*));
	if (world_objects->point_objects == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a);
		free(world_objects);
		return NULL;
	}
	world_objects->point_objects[0] = point_a;
	world_objects->num_points++;

	return world_objects;
}

Point* _calculate_local_point(Point* point_a, Point* point_b) {
	Point* local_point = malloc(sizeof(Point));
	if (local_point == NULL) {
		SDL_Log("problem with malloc. can't calc local point");
		return NULL;
	}
	local_point->x_coord = point_b->x_coord - point_a->x_coord;
	local_point->y_coord = point_b->y_coord - point_a->y_coord;
	local_point->z_coord = point_b->z_coord - point_a->z_coord;
	local_point->color = point_b->color;
	return local_point;
}

WorldObjects* get_on_screen_objects(WorldObjects* world_objects, Camera* camera) {
	WorldObjects* on_screen_objects = calloc(1, sizeof(WorldObjects));
	if (on_screen_objects == NULL) {
		SDL_Log("problem with malloc. can't get on screen objects");
		return NULL;
	}

	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = world_objects->point_objects[i];
		Point* local_point = _calculate_local_point(camera->global_coords, current_point);

		//SDL_Log("split: %f", acos(sqrt(pow(local_point->x_coord, 2) + pow(local_point->z_coord, 2)) * sqrt(pow(camera->x_direction_vector[0], 2) + pow(camera->x_direction_vector[1], 2)) / (local_point->x_coord * camera->x_direction_vector[0] + local_point->z_coord * camera->x_direction_vector[1])));
		float x_degrees_from_direction = acos(
			(local_point->x_coord * camera->x_direction_vector[0] + local_point->z_coord * camera->x_direction_vector[1]) /
			sqrt(pow(local_point->x_coord, 2) + pow(local_point->z_coord, 2)) *
			sqrt(pow(camera->x_direction_vector[0], 2) + pow(camera->x_direction_vector[1], 2))
		);

		float y_degrees_from_direction = acos(
			(local_point->y_coord * camera->y_direction_vector[0] + local_point->z_coord * camera->y_direction_vector[1]) /
			sqrt(pow(local_point->y_coord, 2) + pow(local_point->z_coord, 2)) *
			sqrt(pow(camera->y_direction_vector[0], 2) + pow(camera->y_direction_vector[1], 2))
		);

		SDL_Log("current degrees from direction: (%f, %f)", x_degrees_from_direction, y_degrees_from_direction);
		if ((x_degrees_from_direction <= camera->field_of_view->x_degree_from_center) &&
			(y_degrees_from_direction <= camera->field_of_view->y_degree_from_center)) 
		{
			Point** temp_points = realloc(on_screen_objects->point_objects, (on_screen_objects->num_points + 1) * sizeof(Point*));
			if (temp_points == NULL) {
				SDL_Log("Problem with realloc. can't get on screen objects");
				return NULL;
			}
			on_screen_objects->point_objects = temp_points;
			on_screen_objects->point_objects[on_screen_objects->num_points] = local_point;
			on_screen_objects->num_points++;
			//SDL_Log("added point to on screen objects. new num points: %d, point (x,y,z): (%f,%f,%f)", on_screen_objects->num_points, local_point->x_coord, local_point->y_coord, local_point->z_coord);
		}
		else {
			free(local_point);
		}
	}
	return on_screen_objects;
}

void rasterize_objects_to_frame(uint32_t* frame, unsigned int frame_width, WorldObjects* on_screen_objects) {
	for (int i = 0; i < on_screen_objects->num_points; i++) {
		Point* currentPoint = on_screen_objects->point_objects[i];
		// Explenation: tan(x_degrees_from_direction) * z_screen
		float x_coord_on_screen = currentPoint->x_coord / currentPoint->z_coord;
		float y_coord_on_screen = currentPoint->y_coord / currentPoint->z_coord;
		unsigned int x_screen = (int)x_coord_on_screen;
		unsigned int y_screen = (int)y_coord_on_screen;

		unsigned int x_roof_screen = x_screen + 1;
		unsigned int y_roof_screen = y_screen + 1;

		unsigned int x_s[] = { x_screen, x_roof_screen };
		unsigned int y_s[] = { y_screen, y_roof_screen };
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 2; j++)
				frame[y_s[j] * frame_width + x_s[i]] = currentPoint->color;
		}
	}
}
