#pragma once
#include "SDL3/SDL.h"
#include "world_objects.h"

SDL_AppResult user_events(Camera* camera, unsigned int window_width, unsigned int window_height);
vec3* direction_user_should_move();