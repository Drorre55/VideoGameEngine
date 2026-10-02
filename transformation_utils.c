#include "transformation_utils.h"
#include "cglm/cglm.h"


Uint32 rgba_to_uint32(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	return ((Uint32)r << 24) | ((Uint32)g << 16) | ((Uint32)b << 8) | (Uint32)a;
}

void rotate_x_axis(vec3 position, float rotation_radians)
{
    float y = position[1];
    float z = position[2];
    float cosine = cosf(rotation_radians);
    float sine = sinf(rotation_radians);

    position[1] = y * cosine - z * sine;
    position[2] = y * sine + z * cosine;
}

void rotate_y_axis(vec3 position, float rotation_radians)
{
    float x = position[0];
    float z = position[2];
    float cosine = cosf(rotation_radians);
    float sine = sinf(rotation_radians);

    position[0] = x * cosine + z * sine;
    position[2] = -x * sine + z * cosine;
}

void rotate_z_axis(vec3 position, float rotation_radians)
{
    float x = position[0];
    float y = position[1];
    float cosine = cosf(rotation_radians);
    float sine = sinf(rotation_radians);

    position[0] = x * cosine - y * sine;
    position[1] = x * sine + y * cosine;
}

vec3* calc_normal(vec3 a, vec3 b, vec3 c, vec3 dest) {
    vec3 e1, e2;
    glm_vec3_sub(b, a, e1);
    glm_vec3_sub(c, a, e2);
    glm_vec3_cross(e2, e1, dest);
    glm_vec3_normalize(dest);
    return dest;
}
