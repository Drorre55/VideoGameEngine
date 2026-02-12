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
	camera->x_direction_vector[0] = 0.0;
	camera->x_direction_vector[1] = -1.0;
	camera->y_direction_vector[0] = 0.0;
	camera->y_direction_vector[1] = -1.0;

	FOV* field_of_view = malloc(sizeof(FOV));
	if (field_of_view == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->field_of_view = field_of_view;
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

	// Just for testing - exchange later with loading from file all world objects
	Point* point_a = malloc(sizeof(Point));
	if (point_a == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_a->x_coord = 20.0;
	point_a->y_coord = 5.0;
	point_a->z_coord = 1.0;
	point_a->color = rgba_to_uint32(255, 255, 255, 255);

	Point* point_b = malloc(sizeof(Point));
	if (point_b == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_b->x_coord = 25.0;
	point_b->y_coord = 5.0;
	point_b->z_coord = 1.0;
	point_b->color = rgba_to_uint32(255, 255, 255, 255);
	
	Point* point_c = malloc(sizeof(Point));
	if (point_c == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_c->x_coord = 20.0;
	point_c->y_coord = 20.0;
	point_c->z_coord = 1.0;
	point_c->color = rgba_to_uint32(255, 255, 255, 255);

	Point* point_d = malloc(sizeof(Point));
	if (point_d == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_d->x_coord = 25.0;
	point_d->y_coord = 20.0;
	point_d->z_coord = 1.0;
	point_d->color = rgba_to_uint32(255, 255, 255, 255);
	
	world_objects->point_objects = malloc(sizeof(Point*) * 4);
	if (world_objects->point_objects == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a);
		free(world_objects);
		return NULL;
	}
	world_objects->point_objects[0] = point_a;
	world_objects->num_points++;

	world_objects->point_objects[1] = point_b;
	world_objects->num_points++;

	world_objects->point_objects[2] = point_c;
	world_objects->num_points++;

	world_objects->point_objects[3] = point_d;
	world_objects->num_points++;

	return world_objects;
}

Point* _calculate_local_point(Camera* camera, Point* point_b, unsigned int window_width) {
	Point* local_point = malloc(sizeof(Point));
	if (local_point == NULL) {
		SDL_Log("problem with malloc. can't calc local point");
		return NULL;
	}
	local_point->x_coord = point_b->x_coord - camera->global_coords->x_coord;
	local_point->y_coord = point_b->y_coord - camera->global_coords->y_coord;
	local_point->z_coord = point_b->z_coord - camera->global_coords->z_coord - (tan((180.0 - FOV_CHOSEN) / 2.0) * window_width);
	local_point->color = point_b->color;
	return local_point;
}

WorldObjects* get_on_screen_objects(WorldObjects* world_objects, Camera* camera, unsigned int window_width) {
	WorldObjects* on_screen_objects = calloc(1, sizeof(WorldObjects));
	if (on_screen_objects == NULL) {
		SDL_Log("problem with malloc. can't get on screen objects");
		return NULL;
	}

	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = world_objects->point_objects[i];
		Point* local_point = _calculate_local_point(camera, current_point, window_width);

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
		}
		else {
			free(local_point);
		}
	}
	return on_screen_objects;
}

void rasterize_objects_to_frame(uint32_t* frame, Camera* camera, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	for (int i = 0; i < on_screen_objects->num_points; i++) {
		Point* currentPoint = on_screen_objects->point_objects[i];
		// Explenation: tan(x_degrees_from_direction) * z_screen
		
		float x_degrees_from_direction = acos(
			(currentPoint->x_coord * camera->x_direction_vector[0] + currentPoint->z_coord * camera->x_direction_vector[1]) /
			sqrt(pow(currentPoint->x_coord, 2) + pow(currentPoint->z_coord, 2)) *
			sqrt(pow(camera->x_direction_vector[0], 2) + pow(camera->x_direction_vector[1], 2))
		);

		float y_degrees_from_direction = acos(
			(currentPoint->y_coord * camera->y_direction_vector[0] + currentPoint->z_coord * camera->y_direction_vector[1]) /
			sqrt(pow(currentPoint->y_coord, 2) + pow(currentPoint->z_coord, 2)) *
			sqrt(pow(camera->y_direction_vector[0], 2) + pow(camera->y_direction_vector[1], 2))
		);
		
		float x_coord_on_screen = tan(x_degrees_from_direction) * (tan(M_PI * ((180.0 - FOV_CHOSEN) / 2.0) / 180.0) * frame_width);
		float y_coord_on_screen = tan(y_degrees_from_direction) * (tan(M_PI * ((180.0 - ((float)frame_height / (float)frame_width) * FOV_CHOSEN) / 2.0) / 180.0) * frame_height);
		SDL_Log("screen coords: (%f, %f)", x_coord_on_screen, y_coord_on_screen);
		unsigned int x_screen = (int)x_coord_on_screen;
		unsigned int y_screen = (int)y_coord_on_screen;

		unsigned int x_roof_screen = x_screen + 1;
		unsigned int y_roof_screen = y_screen + 1;

		unsigned int x_s[] = { x_screen, x_roof_screen };
		unsigned int y_s[] = { y_screen, y_roof_screen };
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 2; j++)
				if ((y_s[j] * frame_width + x_s[i]) < (frame_width * frame_height))
					frame[y_s[j] * frame_width + x_s[i]] = currentPoint->color;
		}
	}
}
