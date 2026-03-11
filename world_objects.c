#include "world_objects.h"
#include <math.h>


Camera* load_camera(unsigned int window_width, unsigned int window_height) {
	Camera* camera = calloc(1, sizeof(Camera));
	if (camera == NULL) {
		SDL_Log("problem with malloc. can't load camera");
		return NULL;
	}

	vec3* cam_point = (vec3*)calloc(1, sizeof(vec3));
	if (cam_point == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(camera);
		return NULL;
	}
	camera->global_coords = cam_point;

	vec3* x_direction = (vec3*)calloc(1, sizeof(vec3));
	if (x_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->x_direction_vector = x_direction;
	(*camera->x_direction_vector)[0] = -1.0;
	
	vec3* y_direction = (vec3*)calloc(1, sizeof(vec3));
	if (y_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(x_direction);
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->y_direction_vector = y_direction;
	(*camera->y_direction_vector)[1] = -1.0;

	vec3* z_direction = (vec3*)calloc(1, sizeof(vec3));
	if (z_direction == NULL) {
		SDL_Log("Problem with malloc. can't load camera");
		free(cam_point);
		free(camera);
		return NULL;
	}
	camera->z_direction_vector = z_direction;
	(*camera->z_direction_vector)[2] = 1.0;

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

void free_camera(Camera* camera)
{
	free(camera->field_of_view);
	free(camera->global_coords);
	free(camera->x_direction_vector);
	free(camera->y_direction_vector);
	free(camera->z_direction_vector);
	free(camera);
}

WorldObjects* load_world_objects() {
	WorldObjects* world_objects = (WorldObjects*)calloc(1, sizeof(WorldObjects));
	if (world_objects == NULL) {
		SDL_Log("problem with malloc. can't load world objects");
		return NULL;
	}
	vec3** vertices = (vec3**)malloc(sizeof(vec3*) * 3);
	if (vertices == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free_world_objects(world_objects);
		return NULL;
	}
	world_objects->vertices = vertices;

	Color** colors = (Color**)malloc(sizeof(Color*) * 3);
	if (colors == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free_world_objects(world_objects);
		return NULL;
	}
	world_objects->colors = colors;

	Triangle** triangles = (Triangle**)malloc(sizeof(Triangle*));
	if (triangles == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free_world_objects(world_objects);
		return NULL;
	}
	world_objects->triangles = triangles;

	// Just for testing - exchange later with loading from file all world objects
	Color* point_a_color = (Color*)malloc(sizeof(Color));
	if (point_a_color == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free_world_objects(world_objects);
		return NULL;
	}
	vec3* point_a_coords = (vec3*)malloc(sizeof(vec3));
	if (point_a_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_color);
		free_world_objects(world_objects);
		return NULL;
	}
	point_a_color->r = 0;
	point_a_color->g = 0;
	point_a_color->b = 255;
	point_a_color->a = 255;
	(*point_a_coords)[0] = 5.0;
	(*point_a_coords)[1] = 10.0;
	(*point_a_coords)[2] = 50.0;

	Color* point_b_color = (Color*)malloc(sizeof(Color));
	if (point_b_color == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_color);
		free(point_a_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	vec3* point_b_coords = (vec3*)malloc(sizeof(vec3));
	if (point_b_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_color);
		free(point_a_coords);
		free(point_b_color);
		free(point_b_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	point_b_color->r = 0;
	point_b_color->g = 255;
	point_b_color->b = 0;
	point_b_color->a = 255;
	(*point_b_coords)[0] = 25.0;
	(*point_b_coords)[1] = 5.0;
	(*point_b_coords)[2] = 50.0;
	
	Color* point_c_color = (Color*)malloc(sizeof(Color));
	if (point_c_color == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_color);
		free(point_a_coords);
		free(point_b_color);
		free(point_b_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	vec3* point_c_coords = (vec3*)malloc(sizeof(vec3));
	if (point_c_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a_color);
		free(point_a_coords);
		free(point_b_color);
		free(point_b_coords);
		free(point_c_color);
		free_world_objects(world_objects);
		return NULL;
	}
	point_c_color->r = 255;
	point_c_color->g = 0;
	point_c_color->b = 0;
	point_c_color->a = 255;
	(*point_c_coords)[0] = 20.0;
	(*point_c_coords)[1] = 20.0;
	(*point_c_coords)[2] = 50.0;

	Triangle* triangle = (Triangle*)malloc(sizeof(Triangle));
	if (triangle == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a_color);
		free(point_a_coords);
		free(point_b_color);
		free(point_b_coords);
		free(point_c_color);
		free(point_c_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	triangle->corner1_idx = world_objects->num_vertices++;
	triangle->corner2_idx = world_objects->num_vertices++;
	triangle->corner3_idx = world_objects->num_vertices++;

	world_objects->vertices[triangle->corner1_idx] = point_a_coords;
	world_objects->vertices[triangle->corner2_idx] = point_b_coords;
	world_objects->vertices[triangle->corner3_idx] = point_c_coords;

	world_objects->colors[triangle->corner1_idx] = point_a_color;
	world_objects->colors[triangle->corner2_idx] = point_b_color;
	world_objects->colors[triangle->corner3_idx] = point_c_color;

	world_objects->triangles[world_objects->num_triangles++] = triangle;

	Color* point_a1_color = (Color*)malloc(sizeof(Color));
	if (point_a1_color == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free_world_objects(world_objects);
		return NULL;
	}
	vec3* point_a1_coords = (vec3*)malloc(sizeof(vec3));
	if (point_a1_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a1_color);
		free_world_objects(world_objects);
		return NULL;
	}
	point_a1_color->r = 255;
	point_a1_color->g = 255;
	point_a1_color->b = 255;
	point_a1_color->a = 255;
	(*point_a1_coords)[0] = 15.0;
	(*point_a1_coords)[1] = 10.0;
	(*point_a1_coords)[2] = 100.0;

	Color* point_b1_color = (Color*)malloc(sizeof(Color));
	if (point_b1_color == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a1_color);
		free(point_a1_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	vec3* point_b1_coords = (vec3*)malloc(sizeof(vec3));
	if (point_b1_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free_world_objects(world_objects);
		return NULL;
	}
	point_b1_color->r = 255;
	point_b1_color->g = 255;
	point_b1_color->b = 255;
	point_b1_color->a = 255;
	(*point_b1_coords)[0] = 25.0;
	(*point_b1_coords)[1] = 5.0;
	(*point_b1_coords)[2] = 100.0;

	Color* point_c1_color = (Color*)malloc(sizeof(Color));
	if (point_c1_color == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free(point_b1_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	vec3* point_c1_coords = (vec3*)malloc(sizeof(vec3));
	if (point_c1_coords == NULL) {
		SDL_Log("Problem with malloc. can't load world objects");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free(point_b1_coords);
		free(point_c1_color);
		free_world_objects(world_objects);
		return NULL;
	}
	point_c1_color->r = 255;
	point_c1_color->g = 255;
	point_c1_color->b = 255;
	point_c1_color->a = 255;
	(*point_c1_coords)[0] = 20.0;
	(*point_c1_coords)[1] = 20.0;
	(*point_c1_coords)[2] = 100.0;

	Triangle* triangle1 = (Triangle*)malloc(sizeof(Triangle));
	if (triangle1 == NULL) {
		SDL_Log("Problem with malloc. can't store point objects");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free(point_b1_coords);
		free(point_c1_color);
		free(point_c1_coords);
		free_world_objects(world_objects);
		return NULL;
	}
	triangle1->corner1_idx = world_objects->num_vertices++;
	triangle1->corner2_idx = world_objects->num_vertices++;
	triangle1->corner3_idx = world_objects->num_vertices++;

	vec3** temp_vertices1 = (vec3**)realloc(world_objects->vertices, world_objects->num_vertices * sizeof * temp_vertices1);
	if (temp_vertices1 == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free(point_b1_coords);
		free(point_c1_color);
		free(point_c1_coords);
		free(triangle1);
		free_world_objects(world_objects);
		return NULL;
	}
	world_objects->vertices = temp_vertices1;
	world_objects->vertices[triangle1->corner1_idx] = point_a1_coords;
	world_objects->vertices[triangle1->corner2_idx] = point_b1_coords;
	world_objects->vertices[triangle1->corner3_idx] = point_c1_coords;

	Color** temp_colors1 = (Color**)realloc(world_objects->colors, world_objects->num_vertices * sizeof * temp_colors1);
	if (temp_colors1 == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free(point_b1_coords);
		free(point_c1_color);
		free(point_c1_coords);
		free(triangle1);
		free_world_objects(world_objects);
		return NULL;
	}
	world_objects->colors = temp_colors1;
	world_objects->colors[triangle1->corner1_idx] = point_a1_color;
	world_objects->colors[triangle1->corner2_idx] = point_b1_color;
	world_objects->colors[triangle1->corner3_idx] = point_c1_color;

	Triangle** temp_triangles1 = (Triangle**)realloc(world_objects->triangles, (world_objects->num_triangles + 1) * sizeof * temp_triangles1);
	if (temp_triangles1 == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free(point_a1_color);
		free(point_a1_coords);
		free(point_b1_color);
		free(point_b1_coords);
		free(point_c1_color);
		free(point_c1_coords);
		free(triangle1);
		free_world_objects(world_objects);
		return NULL;
	}
	world_objects->triangles = temp_triangles1;
	world_objects->triangles[world_objects->num_triangles++] = triangle1;

	return world_objects;
}

void free_world_objects(WorldObjects* world_objects)
{
	for (int i = 0; i < world_objects->num_vertices; i++) {
		free(world_objects->vertices[i]);
		free(world_objects->colors[i]);
	}
	for (int j = 0; j < world_objects->num_triangles; j++) {
		free(world_objects->triangles[j]);
	}
	free(world_objects);
}

WorldObjects* world_objects_deep_copy(WorldObjects* world_objects)
{
	WorldObjects* objects_cpy = (WorldObjects*)malloc(sizeof(WorldObjects));
	if (objects_cpy == NULL) {
		SDL_Log("problem with malloc. can't load world objects");
		return NULL;
	}
	vec3** vertices_cpy = (vec3**)malloc(sizeof(vec3*) * world_objects->num_vertices);
	if (vertices_cpy == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free(objects_cpy);
		return NULL;
	}
	Color** colors_cpy = (Color**)malloc(sizeof(Color*) * world_objects->num_vertices);
	if (colors_cpy == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free(objects_cpy);
		return NULL;
	}
	Triangle** triangles_cpy = (Triangle**)malloc(sizeof(Triangle*) * world_objects->num_triangles);
	if (triangles_cpy == NULL) {
		SDL_LogError(1, "Problem with realloc. can't add vertices");
		free(objects_cpy);
		return NULL;
	}

	for (int i = 0; i < world_objects->num_vertices; i++) {
		vec3* vertex_cpy = (vec3*)(malloc(sizeof(vec3)));
		if (vertex_cpy == NULL) {
			SDL_LogError(1, "Problem with malloc. can't transform to camera space");
			free(vertices_cpy);
			free(triangles_cpy);
			free(colors_cpy);
			free(objects_cpy);
			return NULL;
		}
		memcpy(vertex_cpy, world_objects->vertices[i], sizeof(vec3));
		vertices_cpy[i] = vertex_cpy;
	}
	for (int i = 0; i < world_objects->num_vertices; i++) {
		Color* color_cpy = (Color*)(malloc(sizeof(Color)));
		if (color_cpy == NULL) {
			SDL_LogError(1, "Problem with malloc. can't transform to camera space");
			free(vertices_cpy);
			free(triangles_cpy);
			free(colors_cpy);
			free(objects_cpy);
			return NULL;
		}
		memcpy(color_cpy, world_objects->colors[i], sizeof(Color));
		colors_cpy[i] = color_cpy;
	}
	for (int j = 0; j < world_objects->num_triangles; j++) {
		Triangle* triangle_cpy = (Triangle*)(malloc(sizeof(Triangle)));
		if (triangle_cpy == NULL) {
			SDL_LogError(1, "Problem with malloc. can't transform to camera space");
			free(vertices_cpy);
			free(triangles_cpy);
			free(colors_cpy);
			free(objects_cpy);
			return NULL;
		}
		memcpy(triangle_cpy, world_objects->triangles[j], sizeof(Triangle));
		triangles_cpy[j] = triangle_cpy;
	}
	objects_cpy->vertices = vertices_cpy;
	objects_cpy->colors = colors_cpy;
	objects_cpy->triangles = triangles_cpy;
	objects_cpy->num_vertices = world_objects->num_vertices;
	objects_cpy->num_triangles = world_objects->num_triangles;

	return objects_cpy;
}
