#pragma once
#include "SDL3/SDL.h"
#include "world_objects.h"

SDL_AppResult user_events(Camera* camera, bool* show_fps, Uint32 window_width, Uint32 window_height);
vec3* direction_user_should_move();