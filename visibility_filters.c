#include "visibility_filters.h"


void clip_triangles_to_frustum(WorldObjects* camera_space_objects, Camera* camera)
{
	if (camera_space_objects == NULL || camera == NULL || camera_space_objects->num_triangles == 0)
		return;

	float horizontal_scale = tanf(camera->field_of_view->x_degree_from_center);
	float vertical_scale = tanf(camera->field_of_view->y_degree_from_center);

	
	// Each clipping plane is represented by:
	// a * x + b * y + c * z + d >= 0
	
	ClipPlane planes[6] = {
		// Near:       z >= near
		{ 0.0f, 0.0f, 1.0f, -VIEW_FRUSTUM_MIN },
		// Far:        z <= far
		{ 0.0f, 0.0f, -1.0f, VIEW_FRUSTUM_MAX },
		// Left:       x >= -z * horizontal_scale
		{ 1.0f, 0.0f, horizontal_scale, 0.0f },
		// Right:      x <= z * horizontal_scale
		{ -1.0f, 0.0f, horizontal_scale, 0.0f },
		// Bottom:     y >= -z * vertical_scale
		{ 0.0f, 1.0f, vertical_scale, 0.0f },
		// Top:        y <= z * vertical_scale
		{ 0.0f, -1.0f, vertical_scale, 0.0f }
	};

   // A clipped triangle can have up to 9 polygon vertices, producing 7 triangles.
	// Output triangles use independent vertices, so each output triangle needs 3 vertices.
	Uint32 maximum_triangles = camera_space_objects->num_triangles * 7;
	Uint32 maximum_vertices = maximum_triangles * 3;

	vec3* clipped_vertices = malloc(sizeof(vec3) * maximum_vertices);
	Color* clipped_colors = malloc(sizeof(Color) * maximum_vertices);
	Triangle* clipped_triangles = malloc(sizeof(Triangle) * maximum_triangles);

	if (clipped_vertices == NULL || clipped_colors == NULL || clipped_triangles == NULL) {
		SDL_LogError(1, "Could not allocate memory for frustum clipping");
		free(clipped_vertices);
		free(clipped_colors);
		free(clipped_triangles);
		return;
	}

	Uint32 vertex_count = 0;
	Uint32 triangle_count = 0;

	for (Uint32 triangle_index = 0; triangle_index < camera_space_objects->num_triangles; triangle_index++) {
		Triangle source_triangle = camera_space_objects->triangles[triangle_index];

		ClipVertex polygon_a[9];
		ClipVertex polygon_b[9];

		Uint32 polygon_count = 3;

		Uint32 source_indices[3] = {
			source_triangle.corner1_idx,
			source_triangle.corner2_idx,
			source_triangle.corner3_idx
		};

		for (Uint32 i = 0; i < 3; i++) {
			Uint32 source_index = source_indices[i];
			
			memcpy(polygon_a[i].vertex, camera_space_objects->vertices[source_index], sizeof(vec3));
			polygon_a[i].color = camera_space_objects->colors[source_index];
		}

		ClipVertex* input_polygon = polygon_a;
		ClipVertex* output_polygon = polygon_b;

		for (Uint32 plane_index = 0; plane_index < 6; plane_index++) {
			polygon_count = _clip_polygon_against_plane(input_polygon, polygon_count, 
				output_polygon, &planes[plane_index]);

			if (polygon_count == 0)
				break;

			ClipVertex* temporary = input_polygon;
			input_polygon = output_polygon;
			output_polygon = temporary;
		}
		if (polygon_count < 3)
			continue;
		 
		// Triangulate the clipped polygon
		for (Uint32 i = 1; i + 1 < polygon_count; i++) {
			if (vertex_count + 3 > maximum_vertices || triangle_count >= maximum_triangles) {
				SDL_LogError(1, "Frustum clipping output exceeded allocated capacity");
				free(clipped_vertices);
				free(clipped_colors);
				free(clipped_triangles);
				return;
			}

			Uint32 first_index = vertex_count++;
			Uint32 second_index = vertex_count++;
			Uint32 third_index = vertex_count++;

			ClipVertex* first = &input_polygon[0];
			ClipVertex* second = &input_polygon[i];
			ClipVertex* third = &input_polygon[i + 1];

			memcpy(clipped_vertices[first_index], first->vertex, sizeof(vec3));
			memcpy(clipped_vertices[second_index], second->vertex, sizeof(vec3));
			memcpy(clipped_vertices[third_index], third->vertex, sizeof(vec3));

			clipped_colors[first_index] = first->color;
			clipped_colors[second_index] = second->color;
			clipped_colors[third_index] = third->color;

			clipped_triangles[triangle_count].corner1_idx = first_index;
			clipped_triangles[triangle_count].corner2_idx = second_index;
			clipped_triangles[triangle_count].corner3_idx = third_index;

			triangle_count++;
		}
	}

	free(camera_space_objects->vertices);
	free(camera_space_objects->colors);
	free(camera_space_objects->triangles);

	camera_space_objects->vertices = clipped_vertices;
	camera_space_objects->colors = clipped_colors;
	camera_space_objects->triangles = clipped_triangles;
	camera_space_objects->num_vertices = vertex_count;
	camera_space_objects->num_triangles = triangle_count;
}

static Uint32 _clip_polygon_against_plane(const ClipVertex* input, Uint32 input_count,
	ClipVertex* output, const ClipPlane* plane)
{
	if (input_count == 0)
		return 0;

	Uint32 output_count = 0;
	// floating point errror tolerance
	const float epsilon = 1e-6f;

	for (Uint32 i = 0; i < input_count; i++) {
		const ClipVertex* current = &input[i];
		const ClipVertex* previous = &input[(i - 1 + input_count) % input_count];

		float current_distance = _plane_distance(plane, current->vertex);
		float previous_distance = _plane_distance(plane, previous->vertex);

		bool current_inside = current_distance >= -epsilon;
		bool previous_inside = previous_distance >= -epsilon;

		if (current_inside != previous_inside) {
			float denominator = previous_distance - current_distance;
			float interpolation = 0.0f;

			if (fabsf(denominator) > epsilon)
				interpolation = previous_distance / denominator;

			output[output_count++] = _interpolate_clip_vertex(previous, current, interpolation);
		}

		if (current_inside) {
			output[output_count++] = *current;
		}
	}

	return output_count;
}

static float _plane_distance(const ClipPlane* plane, const vec3 vertex)
{
	return plane->a * vertex[0] +
		plane->b * vertex[1] +
		plane->c * vertex[2] +
		plane->d;
}

static ClipVertex _interpolate_clip_vertex(const ClipVertex* first, const ClipVertex* second, float interpolation)
{
	ClipVertex result;

	result.vertex[0] = first->vertex[0] + (second->vertex[0] - first->vertex[0]) * interpolation;
	result.vertex[1] = first->vertex[1] + (second->vertex[1] - first->vertex[1]) * interpolation;
	result.vertex[2] = first->vertex[2] + (second->vertex[2] - first->vertex[2]) * interpolation;

	result.color.r = (Uint8)(first->color.r + (second->color.r - first->color.r) * interpolation);
	result.color.g = (Uint8)(first->color.g + (second->color.g - first->color.g) * interpolation);
	result.color.b = (Uint8)(first->color.b + (second->color.b - first->color.b) * interpolation);
	result.color.a = (Uint8)(first->color.a + (second->color.a - first->color.a) * interpolation);

	return result;
}
