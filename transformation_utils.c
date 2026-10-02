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

int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

float fmax3(float a, float b, float c) {
    float m = a > b ? a : b;
    return m > c ? m : c;
}

void normalize_3_registers(__m256* x, __m256* y, __m256* z) {
    // 1. Compute dot product component-wise (x^2 + y^2 + z^2)
    __m256 x2 = _mm256_mul_ps(*x, *x);
    __m256 y2 = _mm256_mul_ps(*y, *y);
    __m256 z2 = _mm256_mul_ps(*z, *z);

    __m256 len_sq = _mm256_add_ps(_mm256_add_ps(x2, y2), z2);

    // 2. Compute 1 / sqrt(len_sq) using the fast hardware approximation
    __m256 inv_len = _mm256_rsqrt_ps(len_sq);

    // 3. Optional: One iteration of Newton-Raphson refinement for higher accuracy
    // Formula: y = y * (1.5 - 0.5 * x * y * y)
    __m256 half = _mm256_set1_ps(0.5f);
    __m256 one_half = _mm256_set1_ps(1.5f);
    __m256 res_half = _mm256_mul_ps(len_sq, half);
    __m256 sq_inv = _mm256_mul_ps(inv_len, inv_len);
    inv_len = _mm256_mul_ps(inv_len, _mm256_sub_ps(one_half, _mm256_mul_ps(res_half, sq_inv)));

    // 4. Multiply components by the inverse length
    *x = _mm256_mul_ps(*x, inv_len);
    *y = _mm256_mul_ps(*y, inv_len);
    *z = _mm256_mul_ps(*z, inv_len);
}

float horizontal_average_m128(__m128 v) {
    // Add high and low halves: (v0+v2, v1+v3, _, _)
    __m128 sum64 = _mm_add_ps(v, _mm_movehl_ps(v, v));

    // Add remaining elements: (v0+v1+v2+v3, _, _, _)
    __m128 sum32 = _mm_add_ss(sum64, _mm_shuffle_ps(sum64, sum64, 1));

    // Extract the scalar float result and divide by 4
    float total = _mm_cvtss_f32(sum32);
    return total / 4.0f;
}
