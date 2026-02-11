#include "engine.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "SDL3/SDL.h"


uint32_t rgba_to_uint32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

Camera* load_camera(unsigned int window_width, unsigned int window_height) {
	Camera* camera = malloc(sizeof(Camera));
	if (camera == NULL) {
		SDL_Log("problem with malloc. can't load camera");
		return NULL;
	}
	camera->x_direction = 0;
	camera->y_direction = 0;
	Point* cam_point = malloc(sizeof(Point));
	if (cam_point == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(camera);
		return NULL;
	}
	camera->global_coords = cam_point;
	camera->global_coords->x_coord = 0.0;
	camera->global_coords->y_coord = 0.0;
	camera->global_coords->z_coord = 0.0;

	FOV* field_of_view = malloc(sizeof(FOV));
	if (field_of_view == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->field_of_view = field_of_view;
	camera->field_of_view->x_degree_from_center = atan(window_width / 2);
	camera->field_of_view->y_degree_from_center = atan(window_height / 2);
	return camera;
}

WorldObjects* load_world_objects() {
	WorldObjects* world_objects = malloc(sizeof(WorldObjects));
	if (world_objects == NULL) {
		SDL_Log("problem with malloc. can't load world objects");
		return NULL;
	}
	world_objects->num_points = 0;
	world_objects->num_boxes = 0;
	world_objects->num_triangles = 0;

	Point* point_a = malloc(sizeof(Point));
	if (point_a == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_a->x_coord = 200.0;
	point_a->y_coord = 100.0;
	point_a->z_coord = 1.0;
	point_a->color = 0xFFFFFFFF; //rgba_to_uint32(255, 255, 255, 255);
	
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
	WorldObjects* on_screen_objects = malloc(sizeof(WorldObjects));
	if (on_screen_objects == NULL) {
		SDL_Log("problem with malloc. can't get on screen objects");
		return NULL;
	}
	on_screen_objects->point_objects = NULL;
	on_screen_objects->num_points = 0;
	on_screen_objects->num_boxes = 0;
	on_screen_objects->num_triangles = 0;

	for (int i = 0; i < world_objects->num_points; i++) {
		Point* current_point = world_objects->point_objects[i];
		Point* local_point = _calculate_local_point(camera->global_coords, current_point);
		float x_degrees_from_direction = atan(local_point->x_coord / local_point->z_coord);
		float y_degrees_from_direction = atan(local_point->y_coord / local_point->z_coord);

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
			SDL_Log("added point to on screen objects. new num points: %d, point (x,y,z): (%f,%f,%f)", on_screen_objects->num_points, local_point->x_coord, local_point->y_coord, local_point->z_coord);
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
		float x_degrees_from_direction = atan(currentPoint->x_coord / currentPoint->z_coord);
		float y_degrees_from_direction = atan(currentPoint->y_coord / currentPoint->z_coord);

		// Explenation: tan(x_degrees_from_direction) * z_screen
		unsigned int x_screen = (int)tan(x_degrees_from_direction);
		unsigned int y_screen = (int)tan(y_degrees_from_direction);

		frame[y_screen * frame_width + x_screen] = currentPoint->color;
	}
}
