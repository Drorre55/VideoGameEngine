#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL3/SDL.h"
#include "world_objects.h"
#include "graphics_pipeline.h"
#include "user_input.h"

// Usage:
//   ./engine                  normal interactive mode
//   ./engine --benchmark      deterministic 10s benchmark, then exits
//   ./engine --benchmark=20   same, but runs for 20s instead of 10s
//
// Benchmark camera script: holds still for the first 5s (isolates raw
// rasterizer/pipeline cost), then rotates at a fixed speed for the rest
// of the run (exercises movement-triggered work, e.g. culling), so every
// run does exactly the same thing and VTune results are comparable
// before/after a code change.

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define BENCHMARK_DEFAULT_DURATION_MS 10000
#define BENCHMARK_STILL_PHASE_MS 5000
#define BENCHMARK_ROTATE_SPEED_RAD_PER_SEC 0.5f  // slow pan, ~28.6 deg/sec

static SDL_Window* window = NULL;
static SDL_Texture* texture = NULL;
static SDL_Renderer* renderer = NULL;

static Uint32* framebuffer;
float* z_buffer;
static WorldObjects *world_objects, *world_objects_render_copy;
static Camera* camera;

bool show_fps;

// --- benchmark state ---
static bool benchmark_mode = false;
static Uint32 benchmark_duration_ms = BENCHMARK_DEFAULT_DURATION_MS;
static Uint64 benchmark_start_time = 0;
static Uint64 benchmark_frame_count = 0;
static float benchmark_min_frame_ms = 1e9f;
static float benchmark_max_frame_ms = 0.0f;
static double benchmark_frame_ms_sum = 0.0;


static void parse_args(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--benchmark") == 0) {
			benchmark_mode = true;
		}
		else if (strncmp(argv[i], "--benchmark=", 12) == 0) {
			benchmark_mode = true;
			benchmark_duration_ms = (Uint32)(atof(argv[i] + 12) * 1000.0);
		}
	}
}

/*
 * Deterministic yaw-only rotation, reusing the existing mouse-look API.
 * move_camera_direction() expects a pixel delta (relative_x/relative_y)
 * scaled internally by FOV and window size, so we back-solve for the
 * synthetic relative_x that produces exactly BENCHMARK_ROTATE_SPEED_RAD_PER_SEC
 * radians of yaw this frame, independent of frame rate. relative_y stays 0,
 * so pitch (x_axis rotation of y/z direction vectors) never changes.
 */
static void benchmark_update_camera(Camera* cam, Uint64 elapsed_ms, float delta_time) {
	if (elapsed_ms < BENCHMARK_STILL_PHASE_MS) {
		return; // still phase: hold position
	}

	float rotation_rad = BENCHMARK_ROTATE_SPEED_RAD_PER_SEC * delta_time;
	float full_horizontal_fov = cam->field_of_view->x_degree_from_center * 2.0f;
	float synthetic_relative_x = rotation_rad * (float)WINDOW_WIDTH / full_horizontal_fov;

	move_camera_direction(synthetic_relative_x, 0.0f, cam, WINDOW_WIDTH, WINDOW_HEIGHT);
}

static void print_benchmark_summary(Uint64 elapsed_ms) {
	// printf, not SDL_Log: we mute SDL_LOG_CATEGORY_APPLICATION for the
	// timed loop below (see main), and this should print regardless.
	if (benchmark_frame_count == 0) {
		printf("benchmark: no frames were rendered in %llu ms\n", (unsigned long long)elapsed_ms);
		return;
	}
	float elapsed_s = elapsed_ms / 1000.0f;
	float avg_ms = (float)(benchmark_frame_ms_sum / (double)benchmark_frame_count);
	printf("benchmark: %llu frames in %.2fs (%.1f fps avg) | render time min/avg/max = %.2f/%.2f/%.2f ms\n",
		(unsigned long long)benchmark_frame_count,
		elapsed_s,
		benchmark_frame_count / elapsed_s,
		benchmark_min_frame_ms, avg_ms, benchmark_max_frame_ms);
}

SDL_AppResult initialize() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_LogError(1, "Couldn't initialize SDL: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!SDL_CreateWindowAndRenderer("My game engine!!", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
		SDL_LogError(1, "Couldn't create window or renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (!texture) {
		SDL_LogError(1, "Couldn't create texture: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	framebuffer = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof * framebuffer);
	if (!framebuffer) {
		SDL_LogError(1, "problem with allocating framebuffer");
		return SDL_APP_FAILURE;
	}

	z_buffer = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof * z_buffer);
	if (!z_buffer) {
		SDL_LogError(1, "problem with allocating z_buffer");
		return SDL_APP_FAILURE;
	}

	// Set text color to bright green for the FPS counter
	SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
	SDL_RenderDebugText(renderer, 0.0f, 0.0f, " ");
	show_fps = 0;

	return SDL_APP_CONTINUE;
}

SDL_AppResult shutdown() {
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	free(framebuffer);
	free(z_buffer);
	free_world_objects(world_objects, true);
	free_camera(camera);

	SDL_Quit();
	return SDL_APP_SUCCESS;
}

SDL_AppResult load_world() {
	world_objects = load_world_objects();
	world_objects_render_copy = world_objects_deep_copy(world_objects, false);
	camera = load_camera(WINDOW_WIDTH, WINDOW_HEIGHT);

	return SDL_APP_CONTINUE;
}

SDL_AppResult handle_input(float delta_time) {
	SDL_AppResult app_result;

	if (benchmark_mode) {
		// Still drain events so the OS doesn't flag the window as hung, and
		// allow a manual abort (close button / ESC) without touching the
		// scripted camera state.
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				return SDL_APP_SUCCESS;
			}
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				return SDL_APP_SUCCESS;
			}
		}

		Uint64 elapsed = SDL_GetTicks() - benchmark_start_time;
		benchmark_update_camera(camera, elapsed, delta_time);

		return SDL_APP_CONTINUE;
	}

	app_result = user_events(camera, &show_fps, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (app_result != SDL_APP_CONTINUE)
		return app_result;

	move_camera_location(*direction_user_should_move(), camera, delta_time);

	return SDL_APP_CONTINUE;
}


SDL_AppResult render(int fps) {
	memset(framebuffer, 0x00000000, WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(Uint32));
	for (int row = 0; row < WINDOW_HEIGHT; row++) {
		for (int column = 0; column < WINDOW_WIDTH; column++) {
			// rgba_to_uint32(0, 0, 255, 200);
			framebuffer[row * WINDOW_WIDTH + column] = (78 << 24) | (159 << 16) | (229 << 8) | 255;
		}
	}
	world_objects_assign_to_copy(world_objects, world_objects_render_copy);
	run_graphics_pipeline(framebuffer, z_buffer, world_objects_render_copy, camera, WINDOW_WIDTH, WINDOW_HEIGHT);
	SDL_UpdateTexture(texture, NULL, framebuffer, WINDOW_WIDTH * sizeof(Uint32));

	SDL_RenderClear(renderer);

	SDL_RenderTexture(renderer, texture, NULL, NULL);

	if (show_fps) {
		// Scale up the tiny 8x8 debug font for better visibility
		SDL_SetRenderScale(renderer, 2.0f, 2.0f);

		// Render text at coordinates (x=10, y=10)
		SDL_RenderDebugTextFormat(renderer, 10.0f, 10.0f, "FPS: %d", fps);

		// Reset scale for the rest of your game rendering
		SDL_SetRenderScale(renderer, 1.0f, 1.0f);
	}

	SDL_RenderPresent(renderer);

	return SDL_APP_CONTINUE;
}

int main(int argc, char* argv[]) {
	parse_args(argc, argv);

	SDL_Log("starting engine%s", benchmark_mode ? " (benchmark mode)" : "");

	int engine_status = SDL_APP_CONTINUE;

	engine_status = initialize();
	SDL_Log("initialized engine");

	if (engine_status == SDL_APP_CONTINUE)
		engine_status = load_world();
	SDL_Log("loaded world");

	if (benchmark_mode) {
		show_fps = false; // keep debug-text overhead out of the profile
		benchmark_start_time = SDL_GetTicks();
		SDL_Log("benchmark: running for %u ms (still 0-%dms, rotate %d-%ums)",
			benchmark_duration_ms, BENCHMARK_STILL_PHASE_MS, BENCHMARK_STILL_PHASE_MS, benchmark_duration_ms);

		// move_camera_location()/move_camera_direction() call SDL_Log every
		// single frame. Left on, that's stdout I/O happening hundreds of
		// times inside the exact window VTune is sampling - mute it here.
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN);
	}

	Uint64 last_time_fps, last_time_delta;
	last_time_fps = last_time_delta = SDL_GetTicks();
	int frames, fps;
	frames = fps = 0;
	float delta_time;
	if (engine_status == SDL_APP_CONTINUE) {
		while (1) {
			Uint64 current_time = SDL_GetTicks();
			frames++;
			if (current_time > last_time_fps + 1000) {
				fps = frames;
				frames = 0;
				last_time_fps = current_time;
			}
			delta_time = (float)(current_time - last_time_delta) / 1000.0f;
			last_time_delta = current_time;

			if (benchmark_mode) {
				Uint64 elapsed = current_time - benchmark_start_time;
				if (elapsed >= benchmark_duration_ms) {
					print_benchmark_summary(elapsed);
					break;
				}
			}

			engine_status = handle_input(delta_time);
			if (engine_status != SDL_APP_CONTINUE)
				break;

			Uint64 render_start = SDL_GetTicks();
			engine_status = render(fps);
			if (engine_status != SDL_APP_CONTINUE)
				break;

			if (benchmark_mode) {
				float render_ms = (float)(SDL_GetTicks() - render_start);
				benchmark_frame_count++;
				benchmark_frame_ms_sum += (double)render_ms;
				if (render_ms < benchmark_min_frame_ms) benchmark_min_frame_ms = render_ms;
				if (render_ms > benchmark_max_frame_ms) benchmark_max_frame_ms = render_ms;
			}
		}
	}

	if (benchmark_mode) {
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
	}

	shutdown();
	SDL_Log("succesfully shutdown engine");
	return 0;
}