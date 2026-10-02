#include "visibility_filters.h"


void visibility_culling(WorldObjects* world_objects, Camera* camera) {
	backface_culling(world_objects, camera);
}

void backface_culling(WorldObjects* world_objects, Camera* camera)
{
	for (Uint32 i = 0; i < world_objects->num_triangles; i++) {
		Triangle triangle = world_objects->triangles[i];
		vec3* position1 = world_objects->positions[world_objects->triangles[i].corner1_idx.position];
		vec3* position2 = world_objects->positions[world_objects->triangles[i].corner2_idx.position];
		vec3* position3 = world_objects->positions[world_objects->triangles[i].corner3_idx.position];

		vec3 normal, position_to_camera;
		calc_normal(position1, position2, position3, normal);
		glm_vec3_sub(position1, camera->global_coords, position_to_camera);

		if (glm_vec3_dot(position_to_camera, normal) >= 0) {
			world_objects->triangles[i] = 
				world_objects->triangles[world_objects->num_triangles - 1];
			glm_vec3_copy(world_objects->normals[world_objects->num_triangles - 1], world_objects->normals[i]);
			world_objects->triangle_texture_indices[i] = 
				world_objects->triangle_texture_indices[world_objects->num_triangles - 1];
			world_objects->num_triangles--;
			i--;
			continue;
		}
		glm_vec3_copy(normal, world_objects->normals[i]);
	}
}

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

   // A clipped triangle can have up to 9 polygon positions, producing 7 triangles.
	// Output triangles use independent positions, so each output triangle needs 3 positions.
	Uint32 maximum_triangles = camera_space_objects->num_triangles * 7;
	Uint32 maximum_vertices = maximum_triangles * 3;

	vec3* clipped_vertices = malloc(sizeof(vec3) * maximum_vertices);
	Color* clipped_colors = malloc(sizeof(Color) * maximum_vertices);
	vec2* clipped_uvs = malloc(sizeof(vec2) * maximum_vertices);
	Triangle* clipped_triangles = malloc(sizeof(Triangle) * maximum_triangles);
	vec3* clipped_normals = malloc(sizeof(vec3) * maximum_triangles);
	Uint32* clipped_texture_indices = malloc(sizeof(Uint32) * maximum_triangles);

	if (clipped_vertices == NULL || clipped_colors == NULL || clipped_uvs == NULL || clipped_triangles == NULL) {
		SDL_LogError(1, "Could not allocate memory for frustum clipping");
		free(clipped_vertices);
		free(clipped_colors);
		free(clipped_uvs);
		free(clipped_triangles);
		free(clipped_normals);
		free(clipped_texture_indices);
		return;
	}

	Uint32 position_count = 0;
	Uint32 triangle_count = 0;

	for (Uint32 triangle_index = 0; triangle_index < camera_space_objects->num_triangles; triangle_index++) {
		Triangle source_triangle = camera_space_objects->triangles[triangle_index];

		ClipVertex polygon_a[9];
		ClipVertex polygon_b[9];

		Uint32 polygon_count = 3;

		VertexIndices source_indices[3] = {
			source_triangle.corner1_idx,
			source_triangle.corner2_idx,
			source_triangle.corner3_idx
		};

		for (Uint32 i = 0; i < 3; i++) {
			VertexIndices source_index = source_indices[i];
			
			glm_vec3_copy(camera_space_objects->positions[source_index.position], polygon_a[i].position);
			polygon_a[i].color = camera_space_objects->colors[source_index.color];
			glm_vec2_copy(camera_space_objects->uvs[source_index.uv], polygon_a[i].uv);
			glm_vec3_copy(camera_space_objects->uvs[source_index.normal], polygon_a[i].normal);
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
			if (position_count + 3 > maximum_vertices || triangle_count >= maximum_triangles) {
				SDL_LogError(1, "Frustum clipping output exceeded allocated capacity");
				free(clipped_vertices);
				free(clipped_colors);
				free(clipped_uvs);
				free(clipped_triangles);
				free(clipped_normals);
				free(clipped_texture_indices);
				return;
			}

			Uint32 first_index = position_count++;
			Uint32 second_index = position_count++;
			Uint32 third_index = position_count++;

			ClipVertex* first = &input_polygon[0];
			ClipVertex* second = &input_polygon[i];
			ClipVertex* third = &input_polygon[i + 1];

			memcpy(clipped_vertices[first_index], first->position, sizeof(vec3));
			memcpy(clipped_vertices[second_index], second->position, sizeof(vec3));
			memcpy(clipped_vertices[third_index], third->position, sizeof(vec3));

			clipped_colors[first_index] = first->color;
			clipped_colors[second_index] = second->color;
			clipped_colors[third_index] = third->color;

			glm_vec2_copy(first->uv, clipped_uvs[first_index]);
			glm_vec2_copy(second->uv, clipped_uvs[second_index]);
			glm_vec2_copy(third->uv, clipped_uvs[third_index]);

			clipped_triangles[triangle_count].corner1_idx = first_index;
			clipped_triangles[triangle_count].corner2_idx = second_index;
			clipped_triangles[triangle_count].corner3_idx = third_index;

			glm_vec3_copy(camera_space_objects->normals[triangle_index], clipped_normals[triangle_count]);
			
			clipped_texture_indices[triangle_count] = camera_space_objects->triangle_texture_indices[triangle_index];

			triangle_count++;
		}
	}

	free(camera_space_objects->positions);
	free(camera_space_objects->colors);
	free(camera_space_objects->uvs);
	free(camera_space_objects->triangles);
	free(camera_space_objects->normals);
	free(camera_space_objects->triangle_texture_indices);

	camera_space_objects->positions = clipped_vertices;
	camera_space_objects->colors = clipped_colors;
	camera_space_objects->uvs = clipped_uvs;
	camera_space_objects->triangles = clipped_triangles;
	camera_space_objects->normals = clipped_normals;
	camera_space_objects->triangle_texture_indices = clipped_texture_indices;
	camera_space_objects->num_vertices = position_count;
	camera_space_objects->num_triangles = triangle_count;
}

static Uint32 _clip_polygon_against_plane(const ClipVertex* input, Uint32 input_count,
	ClipVertex* output, const ClipPlane* plane)
{
	if (input_count == 0)
		return 0;

	Uint32 output_count = 0;
	// floating point errror tolerance
	const float epsilon = 0.f;//1e-6f;

	for (Uint32 i = 0; i < input_count; i++) {
		const ClipVertex* current = &input[i];
		const ClipVertex* previous = &input[(i - 1 + input_count) % input_count];

		float current_distance = _plane_distance(plane, current->position);
		float previous_distance = _plane_distance(plane, previous->position);

		bool current_inside = current_distance >= -epsilon;
		bool previous_inside = previous_distance >= -epsilon;

		if (current_inside != previous_inside) {
			float denominator = previous_distance - current_distance;
			float interpolation = 0.0f;

			if (fabsf(denominator) > epsilon)
				interpolation = previous_distance / denominator;

			output[output_count++] = _interpolate_clip_position(previous, current, interpolation);
		}

		if (current_inside) {
			output[output_count++] = *current;
		}
	}

	return output_count;
}

static float _plane_distance(const ClipPlane* plane, const vec3 position)
{
	return plane->a * position[0] +
		plane->b * position[1] +
		plane->c * position[2] +
		plane->d;
}

static ClipVertex _interpolate_clip_position(const ClipVertex* first, const ClipVertex* second, float interpolation)
{
	ClipVertex result;

	result.position[0] = first->position[0] + (second->position[0] - first->position[0]) * interpolation;
	result.position[1] = first->position[1] + (second->position[1] - first->position[1]) * interpolation;
	result.position[2] = first->position[2] + (second->position[2] - first->position[2]) * interpolation;

	result.color.r = (Uint8)(first->color.r + (second->color.r - first->color.r) * interpolation);
	result.color.g = (Uint8)(first->color.g + (second->color.g - first->color.g) * interpolation);
	result.color.b = (Uint8)(first->color.b + (second->color.b - first->color.b) * interpolation);
	result.color.a = (Uint8)(first->color.a + (second->color.a - first->color.a) * interpolation);

	result.uv[0] = first->uv[0] + (second->uv[0] - first->uv[0]) * interpolation;
	result.uv[1] = first->uv[1] + (second->uv[1] - first->uv[1]) * interpolation;

	return result;
}
