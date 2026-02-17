#include "rasterizer.h"


void rasterize_objects_to_frame(uint32_t* frame, Camera* camera, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
		Triangle* current_triangle = on_screen_objects->triangle_objects[i];

		_transform_point_to_pixel_space(current_triangle->corner1, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner2, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner3, frame_width, frame_height);
		
		_draw_pixel(frame, frame_width, frame_height, (int)current_triangle->corner1->x_coord,
			(int)current_triangle->corner1->y_coord, current_triangle->corner1->color);
		free(current_triangle->corner1);

		_draw_pixel(frame, frame_width, frame_height, (int)current_triangle->corner2->x_coord,
			(int)current_triangle->corner2->y_coord, current_triangle->corner2->color);
		free(current_triangle->corner2);

		_draw_pixel(frame, frame_width, frame_height, (int)current_triangle->corner3->x_coord,
			(int)current_triangle->corner3->y_coord, current_triangle->corner3->color);
		free(current_triangle->corner3);

		free(current_triangle);
	}
	free(on_screen_objects->point_objects);
	free(on_screen_objects);
}

void _transform_point_to_pixel_space(Point* point, unsigned int frame_width, unsigned int frame_height)
{
	point->x_coord = point->x_coord * frame_width;
	point->y_coord = point->y_coord * frame_height;
}

void _draw_pixel(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y, uint32_t color)
{
	if (!_is_in_frame(frame_width, frame_height, x, y))
		return;

	unsigned int x_roof_screen = x + 1;
	unsigned int y_roof_screen = y + 1;

	unsigned int x_s[] = { x, x_roof_screen };
	unsigned int y_s[] = { y, y_roof_screen };
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++)
			if (_is_in_frame(frame_width, frame_height, x_s[i], y_s[j]))
				frame[y_s[j] * frame_width + x_s[i]] = color;
	}
}

unsigned int _is_in_frame(unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y)
{
	return !(x < 0 || x >= frame_width || y < 0 || y >= frame_height);
}
