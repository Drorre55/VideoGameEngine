#include <stdio.h>
#include "SDL3/SDL.h"
#include "world_objects.h"
#include "graphics_pipeline.h"
#include "user_input.h"


#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static SDL_Window* window = NULL;
static SDL_Texture* texture = NULL;
static SDL_Renderer* renderer = NULL;

static uint32_t* framebuffer;
static WorldObjects* worldObjects;
static Camera* camera;

SDL_AppResult initialize() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!SDL_CreateWindowAndRenderer("My game engine!!", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
		SDL_Log("Couldn't create window or renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (!texture) {
		SDL_Log("Couldn't create texture: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	framebuffer = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof * framebuffer);
	if (!framebuffer) {
		SDL_Log("problem with allocating framebuffer");
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult shutdown() {
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();
	return SDL_APP_SUCCESS;
}

SDL_AppResult load_world() {
	worldObjects = load_world_objects();
	camera = load_camera(WINDOW_WIDTH, WINDOW_HEIGHT);

	return SDL_APP_CONTINUE;
}

SDL_AppResult handle_input() {
	SDL_AppResult app_result;

	app_result = user_events(camera, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (app_result != SDL_APP_CONTINUE)
		return app_result;
	
	move_camera_location(direction_user_should_move(), camera);

	return SDL_APP_CONTINUE;
}


SDL_AppResult render() {
	memset(framebuffer, 0x000000FF, WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));
	for (int row = 0; row < WINDOW_HEIGHT; row++) {
		for (int column = 0; column < WINDOW_WIDTH; column++) {
			framebuffer[row * WINDOW_WIDTH + column] = rgba_to_uint32(0, 0, 255, 200);
		}
	}
	run_graphics_pipeline(framebuffer, worldObjects, camera, WINDOW_WIDTH, WINDOW_HEIGHT);
	SDL_UpdateTexture(texture, NULL, framebuffer, WINDOW_WIDTH * sizeof(uint32_t));

	SDL_RenderClear(renderer);

	SDL_RenderTexture(renderer, texture, NULL, NULL);

	SDL_RenderPresent(renderer);

	return SDL_APP_CONTINUE;
}

void main() {
	SDL_Log("starting engine");

	int engine_status = SDL_APP_CONTINUE;

	engine_status = initialize();
	SDL_Log("initialized engine");

	if (engine_status == SDL_APP_CONTINUE)
		engine_status = load_world();
	SDL_Log("loaded world");

	if (engine_status == SDL_APP_CONTINUE) {
		while (1) {
			Uint64 current_tick = SDL_GetTicks();
			engine_status = handle_input();
			if (engine_status != SDL_APP_CONTINUE)
				break;

			engine_status = render();
			if (engine_status != SDL_APP_CONTINUE)
				break;
			SDL_Log("render time: %d", SDL_GetTicks() - current_tick);
		}
	}
	
	shutdown();
	SDL_Log("succesfully shutdown engine");
}