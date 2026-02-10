#include <stdio.h>
#include "SDL3/SDL.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static SDL_Window* window = NULL;
static SDL_Texture* texture = NULL;
static SDL_Renderer* renderer = NULL;

static uint32_t* framebuffer;

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

SDL_AppResult handle_input() {
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;

		// TODO: Add events
		}
	}

	return SDL_APP_CONTINUE;
}


SDL_AppResult render() {
	memset(framebuffer, 0x000000FF, WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));
	for (int row = 0; row < WINDOW_HEIGHT; row++) {
		for (int column = 0; column < WINDOW_WIDTH; column++) {
			framebuffer[row * WINDOW_WIDTH + column] = SDL_MapRGBA(SDL_GetPixelFormatDetails(texture->format), NULL, 0, 0, 255, 200);
		}
	}

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

	if (engine_status == SDL_APP_CONTINUE) {
		while (1) {
			engine_status = handle_input();
			if (engine_status != SDL_APP_CONTINUE)
				break;

			engine_status = render();
			if (engine_status != SDL_APP_CONTINUE)
				break;
		}
	}
	
	shutdown();
	SDL_Log("succesfully shutdown engine");
}