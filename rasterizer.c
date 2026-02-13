#include "rasterizer.h"


void rasterize_objects_to_frame(uint32_t* frame, Camera* camera, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	for (int i = 0; i < on_screen_objects->num_points; i++) {
		Point* current_point = on_screen_objects->point_objects[i];

		float x_coord_on_screen = current_point->x_coord * frame_width;
		float y_coord_on_screen = current_point->y_coord * frame_height;
		//SDL_Log("screen coords: (%f, %f)", x_coord_on_screen, y_coord_on_screen);
		unsigned int x_screen = (int)x_coord_on_screen;
		unsigned int y_screen = (int)y_coord_on_screen;

		unsigned int x_roof_screen = x_screen + 1;
		unsigned int y_roof_screen = y_screen + 1;

		unsigned int x_s[] = { x_screen, x_roof_screen };
		unsigned int y_s[] = { y_screen, y_roof_screen };
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 2; j++)
				if ((y_s[j] * frame_width + x_s[i]) < (frame_width * frame_height))
					frame[y_s[j] * frame_width + x_s[i]] = current_point->color;
		}
	}
}