#include "world_objects.h"
#include <math.h>


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

	Point* x_direction = calloc(1, sizeof(Point));
	if (x_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->x_direction_vector = x_direction;
	camera->x_direction_vector->x_coord = -1.0;
	
	Point* y_direction = calloc(1, sizeof(Point));
	if (y_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(x_direction);
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->y_direction_vector = y_direction;
	camera->y_direction_vector->y_coord = -1.0;

	Point* z_direction = calloc(1, sizeof(Point));
	if (z_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->z_direction_vector = z_direction;
	camera->z_direction_vector->z_coord = 1.0;

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
	WorldObjects* world_objects = (WorldObjects**)calloc(1, sizeof(WorldObjects));
	if (world_objects == NULL) {
		SDL_Log("problem with malloc. can't load world objects");
		return NULL;
	}

	// Just for testing - exchange later with loading from file all world objects
	Point* point_a = (Point*)malloc(sizeof(Point));
	if (point_a == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_a->x_coord = 20.0;
	point_a->y_coord = 5.0;
	point_a->z_coord = 50.0;
	point_a->color = rgba_to_uint32(255, 255, 255, 255);

	Point* point_b = (Point*)malloc(sizeof(Point));
	if (point_b == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_b->x_coord = 25.0;
	point_b->y_coord = 5.0;
	point_b->z_coord = 50.0;
	point_b->color = rgba_to_uint32(255, 255, 255, 255);
	
	Point* point_c = (Point*)malloc(sizeof(Point));
	if (point_c == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(world_objects);
		return NULL;
	}
	point_c->x_coord = 20.0;
	point_c->y_coord = 20.0;
	point_c->z_coord = 50.0;
	point_c->color = rgba_to_uint32(255, 255, 255, 255);

	//Point* point_d = malloc(sizeof(Point));
	//if (point_d == NULL) {
	//	SDL_Log("Problem with malloc. can't load world objects");
	//	free(world_objects);
	//	return NULL;
	//}
	//point_d->x_coord = 25.0;
	//point_d->y_coord = 20.0;
	//point_d->z_coord = 50.0;
	//point_d->color = rgba_to_uint32(255, 255, 255, 255);
	
	Triangle* triangle = (Triangle*)malloc(sizeof(Triangle));
	if (triangle == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a);
		free(point_b);
		free(point_c);
		free(world_objects);
		return NULL;
	}

	world_objects->triangle_objects = (Triangle**)malloc(sizeof(Triangle*));
	if (world_objects->triangle_objects == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a);
		free(point_b);
		free(point_c);
		free(triangle);
		free(world_objects);
		return NULL;
	}
	triangle->corner1 = point_a;
	triangle->corner2 = point_b;
	triangle->corner3 = point_c;

	world_objects->triangle_objects[0] = triangle;
	world_objects->num_triangles++;

	//world_objects->point_objects[3] = point_d;
	//world_objects->num_points++;

	return world_objects;
}
