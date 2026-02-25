#include "world_objects.h"
#include <math.h>


Camera* load_camera(unsigned int window_width, unsigned int window_height) {
	Camera* camera = calloc(1, sizeof(Camera));
	if (camera == NULL) {
		SDL_Log("problem with malloc. can't load camera");
		return NULL;
	}

	Vec3* cam_point = (Vec3*)calloc(1, sizeof(Vec3));
	if (cam_point == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(camera);
		return NULL;
	}
	camera->global_coords = cam_point;

	Vec3* x_direction = (Vec3*)calloc(1, sizeof(Vec3));
	if (x_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->x_direction_vector = x_direction;
	camera->x_direction_vector->x = -1.0;
	
	Vec3* y_direction = (Vec3*)calloc(1, sizeof(Vec3));
	if (y_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(x_direction);
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->y_direction_vector = y_direction;
	camera->y_direction_vector->y = -1.0;

	Vec3* z_direction = (Vec3*)calloc(1, sizeof(Vec3));
	if (z_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->z_direction_vector = z_direction;
	camera->z_direction_vector->z = 1.0;

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
	Vec3* point_a_coords = (Vec3*)malloc(sizeof(Vec3));
	if (point_a_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a);
		free(world_objects);
		return NULL;
	}
	point_a->coords = point_a_coords;
	point_a->coords->x = 5.0;
	point_a->coords->y = 10.0;
	point_a->coords->z = 50.0;
	point_a->color[0] = 0;
	point_a->color[1] = 0;
	point_a->color[2] = 255;
	point_a->color[3] = 255;

	Point* point_b = (Point*)malloc(sizeof(Point));
	if (point_b == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(world_objects);
		return NULL;
	}
	Vec3* point_b_coords = (Vec3*)malloc(sizeof(Vec3));
	if (point_b_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(world_objects);
		return NULL;
	}
	point_b->coords = point_b_coords;
	point_b->coords->x = 25.0;
	point_b->coords->y = 5.0;
	point_b->coords->z = 50.0;
	point_b->color[0] = 0;
	point_b->color[1] = 255;
	point_b->color[2] = 0;
	point_b->color[3] = 255;
	
	Point* point_c = (Point*)malloc(sizeof(Point));
	if (point_c == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(world_objects);
		return NULL;
	}
	Vec3* point_c_coords = (Vec3*)malloc(sizeof(Vec3));
	if (point_c_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c);
		free(world_objects);
		return NULL;
	}
	point_c->coords = point_c_coords;
	point_c->coords->x = 20.0;
	point_c->coords->y = 20.0;
	point_c->coords->z = 50.0;
	point_c->color[0] = 255;
	point_c->color[1] = 0;
	point_c->color[2] = 0;
	point_c->color[3] = 255;

	Triangle* triangle = (Triangle*)malloc(sizeof(Triangle));
	if (triangle == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(world_objects);
		return NULL;
	}
	triangle->corner1 = point_a;
	triangle->corner2 = point_b;
	triangle->corner3 = point_c;

	Point* point_a1 = (Point*)malloc(sizeof(Point));
	if (point_a1 == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(world_objects);
		return NULL;
	}
	Vec3* point_a1_coords = (Vec3*)malloc(sizeof(Vec3));
	if (point_a1_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1);
		free(world_objects);
		return NULL;
	}
	point_a1->coords = point_a1_coords;
	point_a1->coords->x = 15.0;
	point_a1->coords->y = 10.0;
	point_a1->coords->z = 100.0;
	point_a1->color[0] = 255;
	point_a1->color[1] = 255;
	point_a1->color[2] = 255;
	point_a1->color[3] = 255;

	Point* point_b1 = (Point*)malloc(sizeof(Point));
	if (point_b1 == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1_coords);
		free(point_a1);
		free(point_b1); 
		free(world_objects);
		return NULL;
	}
	Vec3* point_b1_coords = (Vec3*)malloc(sizeof(Vec3));
	if (point_b1_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1_coords);
		free(point_a1);
		free(point_b1);
		free(world_objects);
		return NULL;
	}
	point_b1->coords = point_b1_coords;
	point_b1->coords->x = 25.0;
	point_b1->coords->y = 5.0;
	point_b1->coords->z = 100.0;
	point_b1->color[0] = 255;
	point_b1->color[1] = 255;
	point_b1->color[2] = 255;
	point_b1->color[3] = 255;

	Point* point_c1 = (Point*)malloc(sizeof(Point));
	if (point_c1 == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1_coords);
		free(point_a1);
		free(point_b1_coords);
		free(point_b1);
		free(world_objects);
		return NULL;
	}
	Vec3* point_c1_coords = (Vec3*)malloc(sizeof(Vec3));
	if (point_c1_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1_coords);
		free(point_a1);
		free(point_b1_coords);
		free(point_b1);
		free(point_c1);
		free(world_objects);
		return NULL;
	}
	point_c1->coords = point_c1_coords;
	point_c1->coords->x = 20.0;
	point_c1->coords->y = 20.0;
	point_c1->coords->z = 100.0;
	point_c1->color[0] = 255;
	point_c1->color[1] = 255;
	point_c1->color[2] = 255;
	point_c1->color[3] = 255;

	Triangle* triangle1 = (Triangle*)malloc(sizeof(Triangle));
	if (triangle1 == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1_coords);
		free(point_a1);
		free(point_b1_coords);
		free(point_b1);
		free(point_c1_coords);
		free(point_c1);
		free(world_objects);
		return NULL;
	}
	triangle1->corner1 = point_a1;
	triangle1->corner2 = point_b1;
	triangle1->corner3 = point_c1;

	world_objects->triangle_objects = (Triangle**)malloc(sizeof(Triangle*) * 2);
	if (world_objects->triangle_objects == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a_coords);
		free(point_a);
		free(point_b_coords);
		free(point_b);
		free(point_c_coords);
		free(point_c);
		free(triangle);
		free(point_a1_coords);
		free(point_a1);
		free(point_b1_coords);
		free(point_b1);
		free(point_c1_coords);
		free(point_c1);
		free(triangle1);
		free(world_objects);
		return NULL;
	}

	world_objects->triangle_objects[0] = triangle;
	world_objects->num_triangles++;
	world_objects->triangle_objects[1] = triangle1;
	world_objects->num_triangles++;

	return world_objects;
}
