#include "rasterizer.h"
#include "transformation_utils.h"
#include <math.h>
#include <stdlib.h>

static int clamp_int(int v, int lo, int hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static void draw_scanline(uint32_t* frame, unsigned int frame_width, unsigned int frame_height,
	int y, float x_left, float x_right, uint32_t color)
{
	int x0 = clamp_int((int)ceilf(x_left), 0, (int)frame_width - 1);
	int x1 = clamp_int((int)floorf(x_right), 0, (int)frame_width - 1);

	if (y < 0 || y >= (int)frame_height) return;

	for (int x = x0; x <= x1; ++x) {
		_draw_pixel(frame, frame_width, frame_height, x, y, color);
	}
}

/* assumes points are in pixel space (x,y in screen coords) */
static void draw_flat_bottom(uint32_t* frame, unsigned int frame_width, unsigned int frame_height,
	Point* v0, Point* v1, Point* v2)
{
	/* v0 is top, v1 and v2 share same y (bottom) */
	float inv_slope_left = 0.0f;
	float inv_slope_right = 0.0f;
	float dy_left = v1->y_coord - v0->y_coord;
	float dy_right = v2->y_coord - v0->y_coord;

	if (dy_left != 0.0f) inv_slope_left = (v1->x_coord - v0->x_coord) / dy_left;
	if (dy_right != 0.0f) inv_slope_right = (v2->x_coord - v0->x_coord) / dy_right;

	int y_start = clamp_int((int)ceilf(v0->y_coord), 0, (int)frame_height - 1);
	int y_end = clamp_int((int)floorf(v1->y_coord), 0, (int)frame_height - 1);

	for (int y = y_start; y <= y_end; ++y) {
		float t = (float)(y)-v0->y_coord;
		float x_left = v0->x_coord + inv_slope_left * t;
		float x_right = v0->x_coord + inv_slope_right * t;
		if (x_left > x_right) {
			float tmp = x_left; x_left = x_right; x_right = tmp;
		}
		draw_scanline(frame, frame_width, frame_height, y, x_left, x_right, v0->color);
	}
}

static void draw_flat_top(uint32_t* frame, unsigned int frame_width, unsigned int frame_height,
	Point* v0, Point* v1, Point* v2)
{
	/* v0 and v1 share same y (top), v2 is bottom */
	float dy_left = v2->y_coord - v0->y_coord;
	float dy_right = v2->y_coord - v1->y_coord;

	float inv_slope_left = 0.0f;
	float inv_slope_right = 0.0f;
	if (dy_left != 0.0f) inv_slope_left = (v2->x_coord - v0->x_coord) / dy_left;
	if (dy_right != 0.0f) inv_slope_right = (v2->x_coord - v1->x_coord) / dy_right;

	int y_start = clamp_int((int)ceilf(v0->y_coord), 0, (int)frame_height - 1);
	int y_end = clamp_int((int)floorf(v2->y_coord), 0, (int)frame_height - 1);

	for (int y = y_start; y <= y_end; ++y) {
		float t = (float)(y)-v0->y_coord;
		float x_left = v0->x_coord + inv_slope_left * t;
		float x_right = v1->x_coord + inv_slope_right * t;
		if (x_left > x_right) {
			float tmp = x_left; x_left = x_right; x_right = tmp;
		}
		draw_scanline(frame, frame_width, frame_height, y, x_left, x_right, v0->color);
	}
}

void rasterize_objects_to_frame(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, WorldObjects* on_screen_objects) {
	for (int i = 0; i < (int)on_screen_objects->num_triangles; i++) {
		Triangle* current_triangle = on_screen_objects->triangle_objects[i];

		_transform_point_to_pixel_space(current_triangle->corner1, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner2, frame_width, frame_height);
		_transform_point_to_pixel_space(current_triangle->corner3, frame_width, frame_height);

		_draw_triangle(frame, frame_width, frame_height, current_triangle);

		free(current_triangle->corner1);
		free(current_triangle->corner2);
		free(current_triangle->corner3);
		free(current_triangle);
	}
	free(on_screen_objects->triangle_objects);
	free(on_screen_objects);
}

void _transform_point_to_pixel_space(Point* point, unsigned int frame_width, unsigned int frame_height)
{
	point->x_coord = point->x_coord * (float)frame_width;
	point->y_coord = point->y_coord * (float)frame_height;
}

void _draw_triangle(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, Triangle* triangle)
{
	/* sort points by y ascending (top -> bottom) */
	Point** sorted_points = _sort_points_by_y(triangle->corner1, triangle->corner2, triangle->corner3);
	if (sorted_points == NULL) return;

	/* _sort_points_by_y returns descending (largest y first) in your codebase,
	   so reverse to get ascending order (top = index 2) */
	Point* top = sorted_points[2];
	Point* mid = sorted_points[1];
	Point* bottom = sorted_points[0];

	free(sorted_points);

	/* if any degenerate triangle (zero height), draw a horizontal line or single pixel */
	if (fabsf(top->y_coord - bottom->y_coord) < 1e-6f) {
		int y = clamp_int((int)roundf(top->y_coord), 0, (int)frame_height - 1);
		float x_min = fminf(fminf(top->x_coord, mid->x_coord), bottom->x_coord);
		float x_max = fmaxf(fmaxf(top->x_coord, mid->x_coord), bottom->x_coord);
		draw_scanline(frame, frame_width, frame_height, y, x_min, x_max, top->color);
		return;
	}

	/* handle flat-top or flat-bottom or general triangle by splitting */
	if (fabsf(mid->y_coord - top->y_coord) < 1e-6f) {
		/* flat-top: top and mid share same y */
		/* ensure left/right ordering of top pair */
		if (mid->x_coord < top->x_coord) {
			draw_flat_top(frame, frame_width, frame_height, mid, top, bottom);
		}
		else {
			draw_flat_top(frame, frame_width, frame_height, top, mid, bottom);
		}
	}
	else if (fabsf(mid->y_coord - bottom->y_coord) < 1e-6f) {
		/* flat-bottom: mid and bottom share same y */
		if (mid->x_coord < bottom->x_coord) {
			draw_flat_bottom(frame, frame_width, frame_height, top, mid, bottom);
		}
		else {
			draw_flat_bottom(frame, frame_width, frame_height, top, bottom, mid);
		}
	}
	else {
		/* general triangle: split at y = mid->y_coord along the edge top->bottom */
		float t = (mid->y_coord - top->y_coord) / (bottom->y_coord - top->y_coord);
		float x_split = top->x_coord + t * (bottom->x_coord - top->x_coord);

		Point v_split;
		v_split.x_coord = x_split;
		v_split.y_coord = mid->y_coord;
		v_split.color = top->color; /* color chosen from top; you can interpolate if needed */

		/* draw two parts: top->mid->split (flat-bottom) and mid->split->bottom (flat-top) */
		/* ensure ordering left/right where required */
		if (mid->x_coord < v_split.x_coord) {
			//draw_flat_bottom(frame, frame_width, frame_height, top, mid, &v_split);
			//draw_flat_top(frame, frame_width, frame_height, mid, &v_split, bottom);
		}
		else {
			draw_flat_bottom(frame, frame_width, frame_height, top, &v_split, mid);
			draw_flat_top(frame, frame_width, frame_height, &v_split, mid, bottom);
		}
	}
}

Point** _sort_points_by_y(Point* point_a, Point* point_b, Point* point_c)
{
	Point** points_desc = (Point**)malloc(3 * sizeof(Point*));
	if (points_desc == NULL) {
		SDL_LogError(1, "problem with malloc. can't sort points");
		return NULL;
	}

	/* reuse existing logic but keep it unchanged; it returns descending (largest y first) */
	if (point_a->y_coord >= point_b->y_coord &&
		point_a->y_coord >= point_c->y_coord) {
		points_desc[0] = point_a;
		if (point_b->y_coord >= point_c->y_coord) {
			points_desc[1] = point_b;
			points_desc[2] = point_c;
		}
		else {
			points_desc[1] = point_c;
			points_desc[2] = point_b;
		}
	}
	else if (point_b->y_coord >= point_a->y_coord &&
		point_b->y_coord >= point_c->y_coord) {
		points_desc[0] = point_b;
		if (point_a->y_coord >= point_c->y_coord) {
			points_desc[1] = point_a;
			points_desc[2] = point_c;
		}
		else {
			points_desc[1] = point_c;
			points_desc[2] = point_a;
		}
	}
	else {
		points_desc[0] = point_c;
		if (point_a->y_coord >= point_b->y_coord) {
			points_desc[1] = point_a;
			points_desc[2] = point_b;
		}
		else {
			points_desc[1] = point_b;
			points_desc[2] = point_a;
		}
	}

	return points_desc;
}

void _draw_pixel(uint32_t* frame, unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y, uint32_t color)
{
	if (!_is_in_frame(frame_width, frame_height, x, y))
		return;

	frame[y * frame_width + x] = color;
}

unsigned int _is_in_frame(unsigned int frame_width, unsigned int frame_height, unsigned int x, unsigned int y)
{
	return !(x < 0 || x >= frame_width || y < 0 || y >= frame_height);
}