#pragma once
#include "world_objects.h"

typedef struct {
	vec3 vertex;
	Color color;
} ClipVertex;

typedef struct {
	float a;
	float b;
	float c;
	float d;
} ClipPlane;

void visibility_culling(WorldObjects* world_objects, Camera* camera);
void backface_culling(WorldObjects* world_objects, Camera* camera);
void clip_triangles_to_frustum(WorldObjects* camera_space_objects, Camera* camera);
void transform_scale_to_FOV(WorldObjects* world_objects, Camera* camera);
void transform_FOV_space_to_01_scale(WorldObjects* world_objects);
static Uint32 _clip_polygon_against_plane(const ClipVertex* input, Uint32 input_count,
	ClipVertex* output, const ClipPlane* plane);
static float _plane_distance(const ClipPlane* plane, const vec3 vertex);
static ClipVertex _interpolate_clip_vertex(const ClipVertex* first, const ClipVertex* second, float interpolation);
