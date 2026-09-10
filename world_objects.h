#pragma once

#include "camera.h"
#include "perlin_noise.h"
#include "texture.h"

typedef union {
	struct {
		Uint32 corner1_idx;
		Uint32 corner2_idx;
		Uint32 corner3_idx;
	};
	Uint32 iter[3];
} Triangle;

typedef struct {
	vec3* vertices;
	Triangle* triangles;
	Color* colors;
	vec2* uvs;
	Uint32* triangle_texture_indices;
	TextureBank texture_bank;
	Uint32 num_vertices;
	Uint32 num_triangles;
} WorldObjects;

WorldObjects* load_world_objects();
void _scale_world_objects(WorldObjects* world_objects, float scale);
WorldObjects* _generate_terrain_mesh(Uint32 radius, Uint32 triangle_size, float texture_tile_radius, const char* texture_path);
float _normalize_to_01(float a, float b);
WorldObjects* _concat_world_objects(WorldObjects** world_objects, Uint8 num_objects);
void free_world_objects(WorldObjects* world_objects, bool deep_free_textures);
WorldObjects* world_objects_deep_copy(const WorldObjects* world_objects, bool deep_copy_textures);
void world_objects_assign_to_copy(const WorldObjects* src, WorldObjects* dest);
