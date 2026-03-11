#include "user_input.h"
#include "graphics_pipeline.h"


SDL_AppResult user_events(Camera* camera, unsigned int window_width, unsigned int window_height)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;

			// TODO: Add events
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			SDL_Log("mouse down on pixel: (%d, %d)", (int)event.button.y, (int)event.button.x);

		case SDL_EVENT_MOUSE_MOTION:
			if (event.motion.state == SDL_BUTTON_LEFT) {
				move_camera_direction(event.motion.xrel, event.motion.yrel, camera, window_width, window_height);
			}

			/* BUG: SDL dosn't recognise wheel movment correctly
			case SDL_EVENT_MOUSE_WHEEL:
				move_camera_location(camera, event.wheel.integer_y);*/
		}
	}
	return SDL_APP_CONTINUE;
}

/* returns 1 if moving forward with this keypress, -1 if moving backward, 0 if not moving. 
   it sees all keys that are pressed and cancels each other out if they're on contradicting directions. */
vec3* direction_user_should_move()
{
	vec3* direction = (vec3*)calloc(1, sizeof(vec3));
	if (direction == NULL) {
		SDL_LogError(1, "problem with calloc. can't get direction user should move");
		return NULL;
	}

	const bool* key_states = SDL_GetKeyboardState(NULL);

	if (key_states[SDL_SCANCODE_W])
		(*direction)[2] += 1;
	if (key_states[SDL_SCANCODE_S])
		(*direction)[2] -= 1;

	if (key_states[SDL_SCANCODE_D])
		(*direction)[0] += 1;
	if (key_states[SDL_SCANCODE_A])
		(*direction)[0] -= 1;
	
	// TODO: add jump on y axis
	return direction;
}
