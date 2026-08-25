#include "camera.h"


Camera* load_camera(Uint32 window_width, Uint32 window_height) {
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
	(*cam_point)[1] = -2.0;
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

void move_camera_direction(float relative_x, float relative_y, Camera* camera, Uint32 window_width, Uint32 window_height) {
	// In the future if I will add Screw rotation than add rotate z_axis
	float rotation_degree_x = relative_x / window_width * (camera->field_of_view->x_degree_from_center * 2);
	rotate_y_axis(*camera->x_direction_vector, rotation_degree_x);
	rotate_y_axis(*camera->z_direction_vector, rotation_degree_x);

	float rotation_degree_y = -relative_y / window_height * (camera->field_of_view->y_degree_from_center * 2);
	rotate_x_axis(*camera->y_direction_vector, rotation_degree_y);
	rotate_x_axis(*camera->z_direction_vector, rotation_degree_y);
	SDL_Log("new camera z direction: (%f, %f, %f)", (*camera->z_direction_vector)[0], (*camera->z_direction_vector)[1], (*camera->z_direction_vector)[2]);
}

void move_camera_location(vec3 direction, Camera* camera)
{
	if (!direction[0] && !direction[1] && !direction[2])
		return;
	vec3 x_movement, y_movement, z_movement, global_movement;
	glm_vec3_scale(*camera->x_direction_vector, direction[0], x_movement);
	glm_vec3_scale(*camera->y_direction_vector, direction[1], y_movement);
	glm_vec3_scale(*camera->z_direction_vector, direction[2], z_movement);
	global_movement[0] = x_movement[0] + y_movement[0] + z_movement[0];
	global_movement[1] = x_movement[1] + y_movement[1] + z_movement[1];
	global_movement[2] = x_movement[2] + y_movement[2] + z_movement[2];
	glm_normalize(global_movement);

	static float sensitivity = 0.1;
	(*camera->global_coords)[0] += -global_movement[0] * sensitivity;
	(*camera->global_coords)[1] += -global_movement[1] * sensitivity;
	(*camera->global_coords)[2] += global_movement[2] * sensitivity;

	SDL_Log("new camera location: (%f, %f, %f)", (*camera->global_coords)[0], (*camera->global_coords)[1], (*camera->global_coords)[2]);
}
