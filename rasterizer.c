#include "rasterizer.h"
#include "transformation_utils.h"
#include "cglm/cglm.h"
#include <immintrin.h>
#define CORNERS 3


#include <math.h>

// --- HELPER FUNCTIONS ---

// Simulated PDEP/Interleave bit math across 8 lanes: xxxxxxxxABCDEFGH -> 0A0B0C0D0E0F0G0H
inline __m256i bit_interleave_AVX2(__m256i x) {
    x = _mm256_and_si256(x, _mm256_set1_epi32(0x0000FFFF));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 8)), _mm256_set1_epi32(0x00FF00FF));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 4)), _mm256_set1_epi32(0x0F0F0F0F));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 2)), _mm256_set1_epi32(0x33333333));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 1)), _mm256_set1_epi32(0x55555555));
    return x;
}

// Unpack raw RRGGBBAA 32-bit integer blobs into 4 separate float registers (SoA format)
inline void unpack_rgba_to_float_soa(__m256i packed_rgba, __m256* r, __m256* g, __m256* b, __m256* a) {
    __m256i mask_R = _mm256_setr_epi8(0, -1, -1, -1,  4, -1, -1, -1,  8, -1, -1, -1, 12, -1, -1, -1,
                                      0, -1, -1, -1,  4, -1, -1, -1,  8, -1, -1, -1, 12, -1, -1, -1);
    __m256i mask_G = _mm256_setr_epi8(1, -1, -1, -1,  5, -1, -1, -1,  9, -1, -1, -1, 13, -1, -1, -1,
                                      1, -1, -1, -1,  5, -1, -1, -1,  9, -1, -1, -1, 13, -1, -1, -1);
    __m256i mask_B = _mm256_setr_epi8(2, -1, -1, -1,  6, -1, -1, -1, 10, -1, -1, -1, 14, -1, -1, -1,
                                      2, -1, -1, -1,  6, -1, -1, -1, 10, -1, -1, -1, 14, -1, -1, -1);
    __m256i mask_A = _mm256_setr_epi8(3, -1, -1, -1,  7, -1, -1, -1, 11, -1, -1, -1, 15, -1, -1, -1,
                                      3, -1, -1, -1,  7, -1, -1, -1, 11, -1, -1, -1, 15, -1, -1, -1);

    *r = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_R));
    *g = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_G));
    *b = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_B));
    *a = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_A));
}

// Single-Channel Parallel Bilinear Interpolation using Fused Multiply-Add
inline __m256 bilinear_filter_channel(__m256 tl, __m256 tr, __m256 bl, __m256 br, __m256 alpha, __m256 beta) {
    __m256 top = _mm256_fmadd_ps(alpha, _mm256_sub_ps(tr, tl), tl);
    __m256 bot = _mm256_fmadd_ps(alpha, _mm256_sub_ps(br, bl), bl);
    return _mm256_fmadd_ps(beta, _mm256_sub_ps(bot, top), top);
}

// --- CORE PIPELINE IMPLEMENTATION ---

// Evaluates texture filtering for a single mipmap pointer target
void sample_mip_level_soa(const Color* mip_ptr, __m256i u0, __m256i v0,
    __m256i tex_w_mask, __m256i tex_h_mask, __m256i int_mask,
    __m256 alpha, __m256 beta,
    __m256* out_R, __m256* out_G, __m256* out_B, __m256* out_A)
{
    // GL_REPEAT Wrapping Model applied to coordinates and neighbors (+1)
    __m256i u0_w = _mm256_and_si256(u0, tex_w_mask);
    __m256i v0_w = _mm256_and_si256(v0, tex_h_mask);
    __m256i u1_w = _mm256_and_si256(_mm256_add_epi32(u0, _mm256_set1_epi32(1)), tex_w_mask);
    __m256i v1_w = _mm256_and_si256(_mm256_add_epi32(v0, _mm256_set1_epi32(1)), tex_h_mask);

    // Dynamic SIMD Bit Interleaving to map to flat Morton addresses
    __m256i u0_intrlv = bit_interleave_AVX2(u0_w);
    __m256i u1_intrlv = bit_interleave_AVX2(u1_w);
    __m256i v0_intrlv = _mm256_slli_epi32(bit_interleave_AVX2(v0_w), 1);
    __m256i v1_intrlv = _mm256_slli_epi32(bit_interleave_AVX2(v1_w), 1);

    __m256i morton_TL = _mm256_or_si256(u0_intrlv, v0_intrlv);
    __m256i morton_TR = _mm256_or_si256(u1_intrlv, v0_intrlv);
    __m256i morton_BL = _mm256_or_si256(u0_intrlv, v1_intrlv);
    __m256i morton_BR = _mm256_or_si256(u1_intrlv, v1_intrlv);

    // Hardware Masked Integer Gather requests (Safely rejects occluded lanes)
    __m256i zero_int = _mm256_setzero_si256();
    __m256i packed_TL = _mm256_mask_i32gather_epi32(zero_int, (const int*)mip_ptr, morton_TL, int_mask, 4);
    __m256i packed_TR = _mm256_mask_i32gather_epi32(zero_int, (const int*)mip_ptr, morton_TR, int_mask, 4);
    __m256i packed_BL = _mm256_mask_i32gather_epi32(zero_int, (const int*)mip_ptr, morton_BL, int_mask, 4);
    __m256i packed_BR = _mm256_mask_i32gather_epi32(zero_int, (const int*)mip_ptr, morton_BR, int_mask, 4);

    // Dynamic SoA Channel Unpacking
    __m256 tl_R, tl_G, tl_B, tl_A;
    __m256 tr_R, tr_G, tr_B, tr_A;
    __m256 bl_R, bl_G, bl_B, bl_A;
    __m256 br_R, br_G, br_B, br_A;

    unpack_rgba_to_float_soa(packed_TL, &tl_R, &tl_G, &tl_B, &tl_A);
    unpack_rgba_to_float_soa(packed_TR, &tr_R, &tr_G, &tr_B, &tr_A);
    unpack_rgba_to_float_soa(packed_BL, &bl_R, &bl_G, &bl_B, &bl_A);
    unpack_rgba_to_float_soa(packed_BR, &br_R, &br_G, &br_B, &br_A);

    // Run parallel linear filters for all 4 channels
    *out_R = bilinear_filter_channel(tl_R, tr_R, bl_R, br_R, alpha, beta);
    *out_G = bilinear_filter_channel(tl_G, tr_G, bl_G, br_G, alpha, beta);
    *out_B = bilinear_filter_channel(tl_B, tr_B, bl_B, br_B, alpha, beta);
    *out_A = bilinear_filter_channel(tl_A, tr_A, bl_A, br_A, alpha, beta);
}

// THE UNIFIED BLOCK EXECUTION LOOP ENTRYPOINT
void process_rasterizer_block_4x2(int x, int y, int screen_width, float* depth_buffer, uint32_t* frame_buffer,
    __m256i int_mask, __m256 z_vec,
    __m256 u_vec, __m256 v_vec, TiledTexture* texture)
{

    // --- STAGE 2: PARALLEL QUAD DERIVATIVES & ANISOTROPIC FOOTPRINT ---
    // Scale normalized values up to Base Mip level dimensions
    float base_w = (float)texture->mipmaps[0].width;
    float base_h = (float)texture->mipmaps[0].height;
    __m256 u_tex = _mm256_mul_ps(u_vec, _mm256_set1_ps(base_w));
    __m256 v_tex = _mm256_mul_ps(v_vec, _mm256_set1_ps(base_h));

    // Shuffles to compare horizontal/vertical quad execution space neighbors
    __m256 abs_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    __m256 u_shuf_x = _mm256_permute_ps(u_tex, _MM_SHUFFLE(2, 3, 0, 1));
    __m256 v_shuf_x = _mm256_permute_ps(v_tex, _MM_SHUFFLE(2, 3, 0, 1));
    __m256 dudx = _mm256_and_ps(_mm256_sub_ps(u_tex, u_shuf_x), abs_mask);
    __m256 dvdx = _mm256_and_ps(_mm256_sub_ps(v_tex, v_shuf_x), abs_mask);

    __m256 u_shuf_y = _mm256_permute2f128_ps(u_tex, u_tex, 0x01);
    __m256 v_shuf_y = _mm256_permute2f128_ps(v_tex, v_tex, 0x01);
    __m256 dudy = _mm256_and_ps(_mm256_sub_ps(u_tex, u_shuf_y), abs_mask);
    __m256 dvdy = _mm256_and_ps(_mm256_sub_ps(v_tex, v_shuf_y), abs_mask);

    //// Extract maximum anisotropic bounding footprint footprint
    //__m256 rho = _mm256_max_ps(_mm256_max_ps(dudx, dvdx), _mm256_max_ps(dudy, dvdy));

    // Multiply the normalized footprint bounds by the texture dimensions to map to texel-space delta footprints
    __m256 rho = _mm256_max_ps(_mm256_max_ps(dudx, dvdx), _mm256_max_ps(dudy, dvdy));

    // --- STAGE 3: SOTA LOG2 IEEE-754 BIT EXTRAPOLATION ---
    __m256i rho_int = _mm256_castps_si256(rho);
    __m256 rho_raw_float = _mm256_cvtepi32_ps(rho_int);

    __m256 magic_scale = _mm256_set1_ps(1.1920928955e-7f); // 1.0f / (1 << 23)
    __m256 magic_offset = _mm256_set1_ps(127.0f);
    __m256 lod = _mm256_fmsub_ps(rho_raw_float, magic_scale, magic_offset);

    // Boundary Mip Clamping [0.0f, Max_Mip]
    __m256 ceil_mip = _mm256_set1_ps((float)(texture->num_levels - 1));
    lod = _mm256_min_ps(_mm256_max_ps(lod, _mm256_setzero_ps()), ceil_mip);

    // Mip Split: Integer Layer N and Float Linear Blending Weight Gamma
    __m256i mip_N = _mm256_cvttps_epi32(lod);
    __m256 lod_floor = _mm256_round_ps(lod, _MM_FROUND_TO_NEG_INF);

    // --- STAGE 4: SCALED COORDINATE DISPATCH FOR TRILINEAR LAYERS ---
    // Extract base floor tracking values and fractional alignment subtexels (alpha, beta)
    __m256 u_floor = _mm256_round_ps(u_tex, _MM_FROUND_TO_NEG_INF);
    __m256 v_floor = _mm256_round_ps(v_tex, _MM_FROUND_TO_NEG_INF);
    __m256 alpha = _mm256_sub_ps(u_tex, u_floor);
    __m256 beta = _mm256_sub_ps(v_tex, v_floor);

    __m256i u0 = _mm256_cvttps_epi32(u_floor);
    __m256i v0 = _mm256_cvttps_epi32(v_floor);

    // To process accurate cross-mip sampling cleanly in vector lanes without JIT,
    // evaluate the scalar levels step sequentially but process their data in fully wide SIMD registers.
    __m256 mipN_R, mipN_G, mipN_B, mipN_A;
    __m256 mipNext_R, mipNext_G, mipNext_B, mipNext_A;

    // Use lane 0 as the uniform base layer driver for memory offsets inside this 4x2 execution packet
    // --- STAGE 4: SCALED COORDINATE DISPATCH FOR TRILINEAR LAYERS ---
    __m128i lo = _mm256_castsi256_si128(mip_N);
    __m128i hi = _mm256_extracti128_si256(mip_N, 1);
    __m128i m = _mm_min_epi32(lo, hi);
    m = _mm_min_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(2, 3, 0, 1)));
    m = _mm_min_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(1, 0, 3, 2)));
    int current_mip_idx = _mm_extract_epi32(m, 0);

    if (current_mip_idx < 0) current_mip_idx = 0;
    if (current_mip_idx > texture->num_levels - 1) current_mip_idx = texture->num_levels - 1;
    int next_mip_idx = (current_mip_idx < texture->num_levels - 1) ? current_mip_idx + 1 : current_mip_idx;

    // u_tex/v_tex are in MIP-0 texel space -- each mip has its own (smaller) texel grid,
    // so rescale down by 2^mip before flooring/fractioning, separately per level.
    __m256 scale_N = _mm256_set1_ps(1.0f / (float)(1 << current_mip_idx));
    __m256 scale_Next = _mm256_set1_ps(1.0f / (float)(1 << next_mip_idx));

    __m256 u_tex_N = _mm256_mul_ps(u_tex, scale_N);
    __m256 v_tex_N = _mm256_mul_ps(v_tex, scale_N);
    __m256 u_floor_N = _mm256_round_ps(u_tex_N, _MM_FROUND_TO_NEG_INF);
    __m256 v_floor_N = _mm256_round_ps(v_tex_N, _MM_FROUND_TO_NEG_INF);
    __m256 alpha_N = _mm256_sub_ps(u_tex_N, u_floor_N);
    __m256 beta_N = _mm256_sub_ps(v_tex_N, v_floor_N);
    __m256i u0_N = _mm256_cvttps_epi32(u_floor_N);
    __m256i v0_N = _mm256_cvttps_epi32(v_floor_N);

    __m256 u_tex_Next = _mm256_mul_ps(u_tex, scale_Next);
    __m256 v_tex_Next = _mm256_mul_ps(v_tex, scale_Next);
    __m256 u_floor_Next = _mm256_round_ps(u_tex_Next, _MM_FROUND_TO_NEG_INF);
    __m256 v_floor_Next = _mm256_round_ps(v_tex_Next, _MM_FROUND_TO_NEG_INF);
    __m256 alpha_Next = _mm256_sub_ps(u_tex_Next, u_floor_Next);
    __m256 beta_Next = _mm256_sub_ps(v_tex_Next, v_floor_Next);
    __m256i u0_Next = _mm256_cvttps_epi32(u_floor_Next);
    __m256i v0_Next = _mm256_cvttps_epi32(v_floor_Next);

    __m256i mask_w_N = _mm256_set1_epi32(texture->mipmaps[current_mip_idx].width - 1);
    __m256i mask_h_N = _mm256_set1_epi32(texture->mipmaps[current_mip_idx].height - 1);
    __m256i mask_w_Next = _mm256_set1_epi32(texture->mipmaps[next_mip_idx].width - 1);
    __m256i mask_h_Next = _mm256_set1_epi32(texture->mipmaps[next_mip_idx].height - 1);

    sample_mip_level_soa(texture->mipmaps[current_mip_idx].pixels, u0_N, v0_N, mask_w_N, mask_h_N, int_mask, alpha_N, beta_N, &mipN_R, &mipN_G, &mipN_B, &mipN_A);
    sample_mip_level_soa(texture->mipmaps[next_mip_idx].pixels, u0_Next, v0_Next, mask_w_Next, mask_h_Next, int_mask, alpha_Next, beta_Next, &mipNext_R, &mipNext_G, &mipNext_B, &mipNext_A);

    // --- STAGE 4, after current_mip_idx is extracted/clamped ---
    __m256 base_level = _mm256_set1_ps((float)current_mip_idx);
    __m256 gamma = _mm256_sub_ps(lod, base_level);
    gamma = _mm256_max_ps(_mm256_min_ps(gamma, _mm256_set1_ps(1.0f)), _mm256_setzero_ps());

    // --- STAGE 5: FINAL FMA TRILINEAR BLENDING ---
    __m256 final_R = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_R, mipN_R), mipN_R);
    __m256 final_G = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_G, mipN_G), mipN_G);
    __m256 final_B = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_B, mipN_B), mipN_B);
    __m256 final_A = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_A, mipN_A), mipN_A);
    // --- STAGE 6: RE-PACKING AND MASKED STORAGE OUTPUT ---
    __m256i out_A = _mm256_cvtps_epi32(final_A);
    __m256i out_B = _mm256_slli_epi32(_mm256_cvtps_epi32(final_B), 8);
    __m256i out_G = _mm256_slli_epi32(_mm256_cvtps_epi32(final_G), 16);
    __m256i out_R = _mm256_slli_epi32(_mm256_cvtps_epi32(final_R), 24);
    __m256i packed_output = _mm256_or_si256(_mm256_or_si256(out_R, out_G), _mm256_or_si256(out_B, out_A));
    // Update the buffers using native hardware masked stores
// Extract split elements for 128-bit vector lanes
    __m128i mask_row0 = _mm256_castsi256_si128(int_mask);
    __m128i mask_row1 = _mm256_extracti128_si256(int_mask, 1);

    __m128 z_row0 = _mm256_castps256_ps128(z_vec);
    __m128 z_row1 = _mm256_extractf128_ps(z_vec, 1);

    __m128i color_row0 = _mm256_castsi256_si128(packed_output);
    __m128i color_row1 = _mm256_extracti128_si256(packed_output, 1);

    // Save Row Y cleanly
    _mm_maskstore_ps(depth_buffer + (y * screen_width) + x, mask_row0, z_row0);
    _mm_maskstore_epi32((int*)(frame_buffer + (y * screen_width) + x), mask_row0, color_row0);

    // Save Row Y+1 cleanly
    _mm_maskstore_ps(depth_buffer + ((y + 1) * screen_width) + x, mask_row1, z_row1);
    _mm_maskstore_epi32((int*)(frame_buffer + ((y + 1) * screen_width) + x), mask_row1, color_row1);
}

#include <stdint.h>
#include <stdio.h>

// Struct tracking standard 2D screen space vertices with attributes passed from vertex shader
typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
} Vertex;

// Helper to find the minimum of 3 integers
inline int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

// Helper to find the maximum of 3 integers
inline int max3(int a, int b, int c) {
    int m = a > b ? a : b;
    return m > c ? m : c;
}

// --- TRIANGLE RASTERIZATION TRAVERSAL LOOP ---
void rasterize_triangle_bound_SIMD(Uint32 triangle_index, WorldObjects* world_objects, Uint32* frame_buffer, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
    vec3* vertices = world_objects->vertices;
    Color* colors = world_objects->colors;
    vec2* uvs = world_objects->uvs;

    Triangle triangle = world_objects->triangles[triangle_index];

    Vertex v0 = {
        .x = vertices[triangle.corner1_idx][0],
        .y = vertices[triangle.corner1_idx][1],
        .z = vertices[triangle.corner1_idx][2],
        .u = uvs[triangle.corner1_idx][0],
        .v = uvs[triangle.corner1_idx][1],
        .r = (float)colors[triangle.corner1_idx].r,
        .g = (float)colors[triangle.corner1_idx].g,
        .b = (float)colors[triangle.corner1_idx].b,
        .a = (float)colors[triangle.corner1_idx].a,
    };
    Vertex v1 = {
        .x = vertices[triangle.corner2_idx][0],
        .y = vertices[triangle.corner2_idx][1],
        .z = vertices[triangle.corner2_idx][2],
        .u = uvs[triangle.corner2_idx][0],
        .v = uvs[triangle.corner2_idx][1],
        .r = (float)colors[triangle.corner2_idx].r,
        .g = (float)colors[triangle.corner2_idx].g,
        .b = (float)colors[triangle.corner2_idx].b,
        .a = (float)colors[triangle.corner2_idx].a,
    };
    Vertex v2 = {
        .x = vertices[triangle.corner3_idx][0],
        .y = vertices[triangle.corner3_idx][1],
        .z = vertices[triangle.corner3_idx][2],
        .u = uvs[triangle.corner3_idx][0],
        .v = uvs[triangle.corner3_idx][1],
        .r = (float)colors[triangle.corner3_idx].r,
        .g = (float)colors[triangle.corner3_idx].g,
        .b = (float)colors[triangle.corner3_idx].b,
        .a = (float)colors[triangle.corner3_idx].a,
    };

    const int has_texture =
        world_objects->triangle_texture_indices &&
        triangle_index < world_objects->num_triangles &&
        world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE &&
        world_objects->triangle_texture_indices[triangle_index] <
        world_objects->texture_bank.count;

    TiledTexture texture;
    if (has_texture) {
        Uint32 texture_index = world_objects->triangle_texture_indices[triangle_index];
        texture = world_objects->texture_bank.textures[texture_index];
    }


    // =========================================================================
    // STAGE 1: TRIANGLE SETUP & CONSTANT GENERATION
    // =========================================================================

    // 1. Calculate Cross-product area delta (Twice the triangle area)
    float area = fabsf((v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y));

    // Backface culling: Skip degenerate or back-facing triangles
    if (area <= 0.0f) return;
    float inv_area = 1.0f / area;

    // 2. Precompute standard Edge Function Step Deltas
    float dE01_dx = v1.y - v0.y;  float dE01_dy = v0.x - v1.x;
    float dE12_dx = v2.y - v1.y;  float dE12_dy = v1.x - v2.x;
    float dE20_dx = v0.y - v2.y;  float dE20_dy = v2.x - v0.x;

    // 3. Compute Attribute Plane Equation Gradients (Matrix Inversion Trick)
    // Interpolation derivatives for Depth (Z)
    float dz_dx = ((v1.z - v0.z) * (v2.y - v0.y) - (v2.z - v0.z) * (v1.y - v0.y)) * inv_area;
    float dz_dy = ((v2.z - v0.z) * (v1.x - v0.x) - (v1.z - v0.z) * (v2.x - v0.x)) * inv_area;

    // Declare placeholder gradients for texturing and color channels
    float du_dx = 0, du_dy = 0, dv_dx = 0, dv_dy = 0;
    float dr_dx = 0, dr_dy = 0, dg_dx = 0, dg_dy = 0, db_dx = 0, db_dy = 0;

    if (has_texture) {
        // Interpolation derivatives for Texture U coordinate
        du_dx = ((v1.u - v0.u) * (v2.y - v0.y) - (v2.u - v0.u) * (v1.y - v0.y)) * inv_area;
        du_dy = ((v2.u - v0.u) * (v1.x - v0.x) - (v1.u - v0.u) * (v2.x - v0.x)) * inv_area;

        // Interpolation derivatives for Texture V coordinate
        dv_dx = ((v1.v - v0.v) * (v2.y - v0.y) - (v2.v - v0.v) * (v1.y - v0.y)) * inv_area;
        dv_dy = ((v2.v - v0.v) * (v1.x - v0.x) - (v1.v - v0.v) * (v2.x - v0.x)) * inv_area;
    }
    else {
        // Color channel gradients (Linear screen-space interpolation shown here for flat/gouraud shading)
        dr_dx = ((v1.r - v0.r) * (v2.y - v0.y) - (v2.r - v0.r) * (v1.y - v0.y)) * inv_area;
        dr_dy = ((v2.r - v0.r) * (v1.x - v0.x) - (v1.r - v0.r) * (v2.x - v0.x)) * inv_area;
        dg_dx = ((v1.g - v0.g) * (v2.y - v0.y) - (v2.g - v0.g) * (v1.y - v0.y)) * inv_area;
        dg_dy = ((v2.g - v0.g) * (v1.x - v0.x) - (v1.g - v0.g) * (v2.x - v0.x)) * inv_area;
        db_dx = ((v1.b - v0.b) * (v2.y - v0.y) - (v2.b - v0.b) * (v1.y - v0.y)) * inv_area;
        db_dy = ((v2.b - v0.b) * (v1.x - v0.x) - (v1.b - v0.b) * (v2.x - v0.x)) * inv_area;
    }

    // 4. Bounding Box Setup aligned perfectly to 4x2 block chunks
    int min_x = (min3((int)v0.x, (int)v1.x, (int)v2.x)) & ~3; // Round down to multiple of 4
    int max_x = (max3((int)v0.x, (int)v1.x, (int)v2.x) + 3) & ~3; // Round up to multiple of 4
    int min_y = (min3((int)v0.y, (int)v1.y, (int)v2.y)) & ~1; // Round down to multiple of 2
    int max_y = (max3((int)v0.y, (int)v1.y, (int)v2.y) + 1) & ~1; // Round up to multiple of 2

    // Clip bounding box against screen bounds
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > frame_width)  max_x = frame_width;
    if (max_y > frame_height) max_y = frame_height;

    // =========================================================================
    // STAGE 2: FIXED AVX2 CONSTANTS FOR THE 4x2 PACKET
    // =========================================================================

    // Constant pixel coordinate offsets relative to the top-left (X, Y) corner of the 4x2 block
    // Layout sequence matches depth buffer offset configuration:
    // Row Y  : (0,0), (1,0), (2,0), (3,0)
    // Row Y+1: (0,1), (1,1), (2,1), (3,1)
    __m256 x_offsets = _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 2.0f, 3.0f);
    __m256 y_offsets = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Broadcast setup gradients into wide vector registers
    __m256 vdE01_dx = _mm256_set1_ps(dE01_dx); __m256 vdE01_dy = _mm256_set1_ps(dE01_dy);
    __m256 vdE12_dx = _mm256_set1_ps(dE12_dx); __m256 vdE12_dy = _mm256_set1_ps(dE12_dy);
    __m256 vdE20_dx = _mm256_set1_ps(dE20_dx); __m256 vdE20_dy = _mm256_set1_ps(dE20_dy);

    __m256 vdz_dx = _mm256_set1_ps(dz_dx); __m256 vdz_dy = _mm256_set1_ps(dz_dy);
    __m256 vdu_dx = _mm256_set1_ps(du_dx); __m256 vdu_dy = _mm256_set1_ps(du_dy);
    __m256 vdv_dx = _mm256_set1_ps(dv_dx); __m256 vdv_dy = _mm256_set1_ps(dv_dy);

    __m256 vdr_dx = _mm256_set1_ps(dr_dx); __m256 vdr_dy = _mm256_set1_ps(dr_dy);
    __m256 vdg_dx = _mm256_set1_ps(dg_dx); __m256 vdg_dy = _mm256_set1_ps(dg_dy);
    __m256 vdb_dx = _mm256_set1_ps(db_dx); __m256 vdb_dy = _mm256_set1_ps(db_dy);

    // Top-left evaluation coordinates tracking anchor vertex v0
    __m256 vv0_x = _mm256_set1_ps(v0.x);  __m256 vv0_y = _mm256_set1_ps(v0.y);

    // =========================================================================
    // STAGE 3: INTERLEAVED GRID BLOCK TRAVERSAL LOOP
    // =========================================================================
    for (int y = min_y; y < max_y; y += 2) {
        // 1. Pre-broadcast y coordinates once per row to save SIMD additions
        __m256 py = _mm256_add_ps(_mm256_set1_ps((float)y), y_offsets);
        __m256 dy = _mm256_sub_ps(py, vv0_y);

        for (int x = min_x; x < max_x; x += 4) {
            
            // 3. Compute absolute screen pixel positions for all 8 lanes
            __m256 px = _mm256_add_ps(_mm256_set1_ps((float)x), x_offsets);
            
            // 1. Calculate Edge Functions across all 8 lanes using linear step offsets
            __m256 e01 = _mm256_add_ps(
                _mm256_mul_ps(_mm256_sub_ps(px, _mm256_set1_ps(v0.x)), vdE01_dx),
                _mm256_mul_ps(_mm256_sub_ps(py, _mm256_set1_ps(v0.y)), vdE01_dy)
            );
            __m256 e12 = _mm256_add_ps(
                _mm256_mul_ps(_mm256_sub_ps(px, _mm256_set1_ps(v1.x)), vdE12_dx),
                _mm256_mul_ps(_mm256_sub_ps(py, _mm256_set1_ps(v1.y)), vdE12_dy)
            );
            __m256 e20 = _mm256_add_ps(
                _mm256_mul_ps(_mm256_sub_ps(px, _mm256_set1_ps(v2.x)), vdE20_dx),
                _mm256_mul_ps(_mm256_sub_ps(py, _mm256_set1_ps(v2.y)), vdE20_dy)
            );

            // 2. Compute Triangle Coverage Mask
            // Test if all three edge function values are greater than or equal to zero
            __m256 mask01 = _mm256_cmp_ps(e01, _mm256_setzero_ps(), _CMP_GE_OQ);
            __m256 mask12 = _mm256_cmp_ps(e12, _mm256_setzero_ps(), _CMP_GE_OQ);
            __m256 mask20 = _mm256_cmp_ps(e20, _mm256_setzero_ps(), _CMP_GE_OQ);

            __m256 coverage_mask = _mm256_and_ps(_mm256_and_ps(mask01, mask12), mask20);

            // Coarse Block Optimization: If no pixels are inside the triangle, skip texture math entirely
            if (_mm256_movemask_ps(coverage_mask) == 0) {
                continue;
            }

            // Calculate distance deltas from the baseline anchor vertex v0
            __m256 dx = _mm256_sub_ps(px, vv0_x);

            // 4. Parallel FMA Attribute Interpolation
            // Equation structure: H = H0 + dx * dH_dx + dy * dH_dy
            // Interpolate Depth (1/w) for standard Early Depth Test matching
            __m256 z_vec = _mm256_add_ps(_mm256_set1_ps(v0.z), _mm256_add_ps(_mm256_mul_ps(dx, vdz_dx), _mm256_mul_ps(dy, vdz_dy)));

            // Early-Z implementation block
            __m256i depth_offsets = _mm256_setr_epi32(
                (y * frame_width) + x, (y * frame_width) + x + 1,
                (y * frame_width) + x + 2, (y * frame_width) + x + 3,
                ((y + 1) * frame_width) + x, ((y + 1) * frame_width) + x + 1,
                ((y + 1) * frame_width) + x + 2, ((y + 1) * frame_width) + x + 3
            );
            __m256 current_depth = _mm256_i32gather_ps(z_buffer, depth_offsets, 4);
            __m256 depth_pass_mask = _mm256_cmp_ps(z_vec, current_depth, _CMP_GT_OQ); // Assumes 1/w buffer where larger = closer
            __m256 execution_mask = _mm256_and_ps(coverage_mask, depth_pass_mask);

            if (_mm256_movemask_ps(execution_mask) != 0) {
                __m256i int_mask = _mm256_castps_si256(execution_mask);

                if (has_texture) {
                    // Interpolate the pre-projected homogenous attributes over screen space
                    __m256 u_over_z_vec = _mm256_add_ps(_mm256_set1_ps(v0.u), _mm256_add_ps(_mm256_mul_ps(dx, vdu_dx), _mm256_mul_ps(dy, vdu_dy)));
                    __m256 v_over_z_vec = _mm256_add_ps(_mm256_set1_ps(v0.v), _mm256_add_ps(_mm256_mul_ps(dx, vdv_dx), _mm256_mul_ps(dy, vdv_dy)));

                    // 1-Cycle hardware reciprocal approximation of 1/w to get actual W depth
                    __m256 z_approx = _mm256_rcp_ps(z_vec);
                    // Newton-Raphson precision correction pass
                    __m256 w_vec = _mm256_mul_ps(z_approx, _mm256_fnmadd_ps(z_vec, z_approx, _mm256_set1_ps(2.0f)));

                    // Reconstruct perfect perspective-correct U and V coordinates for your texturing loop!
                    __m256 u_vec = _mm256_mul_ps(u_over_z_vec, w_vec);
                    __m256 v_vec = _mm256_mul_ps(v_over_z_vec, w_vec);

                    // 5. Forward data vectors directly into the state-of-the-art Trilinear Texture Sampler
                    process_rasterizer_block_4x2(x, y, frame_width, z_buffer, frame_buffer,
                        int_mask, z_vec, u_vec, v_vec, &texture);
                }
                else {
                    __m256 final_R = _mm256_add_ps(_mm256_set1_ps(v0.r), _mm256_add_ps(_mm256_mul_ps(dx, vdr_dx), _mm256_mul_ps(dy, vdr_dy)));
                    __m256 final_G = _mm256_add_ps(_mm256_set1_ps(v0.g), _mm256_add_ps(_mm256_mul_ps(dx, vdg_dx), _mm256_mul_ps(dy, vdg_dy)));
                    __m256 final_B = _mm256_add_ps(_mm256_set1_ps(v0.b), _mm256_add_ps(_mm256_mul_ps(dx, vdb_dx), _mm256_mul_ps(dy, vdb_dy)));
                    __m256 final_A = _mm256_set1_ps(v0.a);
                    // Assuming constant vertex alpha for solid shading
                    // // Clamp float colors straight to [0.0f, 255.0f] range
                    __m256 max_color = _mm256_set1_ps(255.0f);
                    final_R = _mm256_min_ps(_mm256_max_ps(final_R, _mm256_setzero_ps()), max_color);
                    final_G = _mm256_min_ps(_mm256_max_ps(final_G, _mm256_setzero_ps()), max_color);
                    final_B = _mm256_min_ps(_mm256_max_ps(final_B, _mm256_setzero_ps()), max_color);
                    // Convert colors to integers, pack channels into standard AABBGGRR pixel formats
                    __m256i out_A = _mm256_cvtps_epi32(final_A);
                    __m256i out_B = _mm256_slli_epi32(_mm256_cvtps_epi32(final_B), 8);
                    __m256i out_G = _mm256_slli_epi32(_mm256_cvtps_epi32(final_G), 16);
                    __m256i out_R = _mm256_slli_epi32(_mm256_cvtps_epi32(final_R), 24);
                    __m256i packed_output = _mm256_or_si256(_mm256_or_si256(out_R, out_G), _mm256_or_si256(out_B, out_A));
                    // Write out directly using masked operations
                    // --- CORRECTED RESOLUTION STAGE: SPLIT BLOCKS INTO TWO 4-LANE ROWS ---
                    // Extract row masks
                    __m128i mask_row0 = _mm256_castsi256_si128(int_mask);
                    __m128i mask_row1 = _mm256_extracti128_si256(int_mask, 1);

                    // Extract row depth elements
                    __m128 z_row0 = _mm256_castps256_ps128(z_vec);
                    __m128 z_row1 = _mm256_extractf128_ps(z_vec, 1);

                    // Extract row color elements
                    __m128i color_row0 = _mm256_castsi256_si128(packed_output);
                    __m128i color_row1 = _mm256_extracti128_si256(packed_output, 1);

                    // Write Row Y (Pixels 0, 1, 2, 3)
                    _mm_maskstore_ps(z_buffer + (y * frame_width) + x, mask_row0, z_row0);
                    _mm_maskstore_epi32((int*)(frame_buffer + (y * frame_width) + x), mask_row0, color_row0);

                    // Write Row Y+1 (Pixels 4, 5, 6, 7)
                    _mm_maskstore_ps(z_buffer + ((y + 1) * frame_width) + x, mask_row1, z_row1);
                    _mm_maskstore_epi32((int*)(frame_buffer + ((y + 1) * frame_width) + x), mask_row1, color_row1);
                }
            }
        }
    }
}



void transform_to_pixel_space(WorldObjects* on_screen_objects, Uint32 frame_width, Uint32 frame_height)
{
	for (int i = 0; i < on_screen_objects->num_vertices; i++) {
		vec3* vertex = on_screen_objects->vertices[i];
		(*vertex)[0] = (*vertex)[0] * frame_width;
		(*vertex)[1] = (*vertex)[1] * frame_height;
	}
}

void rasterize_objects_to_frame(Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height, WorldObjects* on_screen_objects) {
	memset(z_buffer, 0, sizeof(float) * frame_width * frame_height);

	for (int i = 0; i < on_screen_objects->num_triangles; i++) {
        rasterize_triangle_bound_SIMD(i, on_screen_objects, frame, z_buffer, frame_width, frame_height);
		//_draw_triangle(i, on_screen_objects, frame, z_buffer, frame_width, frame_height);
	}
}

static inline _calc_step_constants(float* A_variable, float* B_variable, float* C_variable, 
    Uint32 num_variables, float dA_dx, float dA_dy, float dB_dx, float dB_dy, float* Avar_minus_C, 
    float* Bvar_minus_C, float* dest_dvar_dx, float* dest_dvar_dy) 
{
    for (Uint32 i = 0; i < num_variables; i++) {
        Avar_minus_C[i] = A_variable[i] - C_variable[i];
        Bvar_minus_C[i] = B_variable[i] - C_variable[i];
        dest_dvar_dx[i] = Avar_minus_C[i] * dA_dx + Bvar_minus_C[i] * dB_dx;
        dest_dvar_dy[i] = Avar_minus_C[i] * dA_dy + Bvar_minus_C[i] * dB_dy;
    }
}

inline float horizontal_average_m128(__m128 v) {
    // Add high and low halves: (v0+v2, v1+v3, _, _)
    __m128 sum64 = _mm_add_ps(v, _mm_movehl_ps(v, v));

    // Add remaining elements: (v0+v1+v2+v3, _, _, _)
    __m128 sum32 = _mm_add_ss(sum64, _mm_shuffle_ps(sum64, sum64, 1));

    // Extract the scalar float result and divide by 4
    float total = _mm_cvtss_f32(sum32);
    return total / 4.0f;
}

static void _draw_triangle(Uint32 triangle_index, WorldObjects* world_objects, Uint32* frame, float* z_buffer, Uint32 frame_width, Uint32 frame_height)
{
    vec3* vertices = world_objects->vertices;
    Color* colors = world_objects->colors;
    vec2* uvs = world_objects->uvs;

    Triangle triangle = world_objects->triangles[triangle_index];
    Triangle sorted_triangle;
    _sort_points_by_x(&triangle, &sorted_triangle, vertices);

    vec3* A = vertices[sorted_triangle.corner1_idx];
    vec3* B = vertices[sorted_triangle.corner2_idx];
    vec3* C = vertices[sorted_triangle.corner3_idx];
    vec4 A_color = { (float)colors[sorted_triangle.corner1_idx].r, (float)colors[sorted_triangle.corner1_idx].g,
        (float)colors[sorted_triangle.corner1_idx].b, (float)colors[sorted_triangle.corner1_idx].a };
    vec4 B_color = { (float)colors[sorted_triangle.corner2_idx].r, (float)colors[sorted_triangle.corner2_idx].g,
        (float)colors[sorted_triangle.corner2_idx].b, (float)colors[sorted_triangle.corner2_idx].a };
    vec4 C_color = { (float)colors[sorted_triangle.corner3_idx].r, (float)colors[sorted_triangle.corner3_idx].g,
        (float)colors[sorted_triangle.corner3_idx].b, (float)colors[sorted_triangle.corner3_idx].a };
    vec2* A_uv = uvs[sorted_triangle.corner1_idx];
    vec2* B_uv = uvs[sorted_triangle.corner2_idx];
    vec2* C_uv = uvs[sorted_triangle.corner3_idx];

    // Copy corner data to local variables for faster access
    float Ax = (*A)[0], Bx = (*B)[0], Cx = (*C)[0];
    float Ay = (*A)[1], By = (*B)[1], Cy = (*C)[1];
    float inv_Az = (*A)[2], inv_Bz = (*B)[2], inv_Cz = (*C)[2];

    // Precompute edges
    float ABx = Bx - Ax, ABy = By - Ay;
    float ACx = Cx - Ax, ACy = Cy - Ay;
    float BCx = Cx - Bx, BCy = Cy - By;

    /*
     * Degenerate triangle check.
     *
     * Sorting by X may change winding, but barycentric interpolation
     * still works as long as we preserve the resulting area sign.
     */
    float triangle_area2 = ABx * ACy - ABy * ACx;

    if (fabsf(triangle_area2) < 1e-8f)
        return;

    float inverse_triangle_area = 1.0f / triangle_area2;

    /*
     * ----------------------------------------------------------------
     * Barycentric derivatives
     * ----------------------------------------------------------------
     *
     * wA = edge(BC, P) / area
     * wB = edge(CA, P) / area
     *
     * wC is implicit.
     */

    float dA_dx = -BCy * inverse_triangle_area;
    float dA_dy = BCx * inverse_triangle_area;

    float dB_dx = ACy * inverse_triangle_area;
    float dB_dy = -ACx * inverse_triangle_area;
    
    // Calc incremental progression constants
    float inv_Az_minus_C, inv_Bz_minus_C, dz_dx, dz_dy;
    _calc_step_constants(&inv_Az, &inv_Bz, &inv_Cz, 1, dA_dx, dA_dy, dB_dx, dB_dy, &inv_Az_minus_C, 
        &inv_Bz_minus_C, &dz_dx, &dz_dy);

    vec2 Auv_minus_C, Buv_minus_C, duv_dx, duv_dy;
    _calc_step_constants(A_uv, B_uv, C_uv, 2, dA_dx, dA_dy, dB_dx, dB_dy, Auv_minus_C, Buv_minus_C, 
        duv_dx, duv_dy);

    vec4 Acolor_minus_C, Bcolor_minus_C, dcolor_dx, dcolor_dy;
    _calc_step_constants(A_color, B_color, C_color, 4, dA_dx, dA_dy, dB_dx, dB_dy, Acolor_minus_C, 
        Bcolor_minus_C, dcolor_dx, dcolor_dy);

    const int has_texture =
        world_objects->triangle_texture_indices &&
        triangle_index < world_objects->num_triangles &&
        world_objects->triangle_texture_indices[triangle_index] != TEXTURE_NONE &&
        world_objects->triangle_texture_indices[triangle_index] <
        world_objects->texture_bank.count;
    
    TiledTexture texture;
    if (has_texture) {
        Uint32 texture_index = world_objects->triangle_texture_indices[triangle_index];
        texture = world_objects->texture_bank.textures[texture_index];
    }

    /*
     * ----------------------------------------------------------------
     * Edge setup
     *
     * Since vertices are sorted:
     *
     * A.x <= B.x <= C.x
     *
     * We walk columns from A to C.
     *
     * One boundary is always AC.
     *
     * The other boundary is:
     *
     * AB for A -> B
     * BC for B -> C
     * ----------------------------------------------------------------
     */

    // Slopes dy/dx.
    float AB_slope = fabsf(ABx) > 1e-8f ? ABy / ABx : 0.0f;
    float AC_slope = fabsf(ACx) > 1e-8f ? ACy / ACx : 0.0f;
    float BC_slope = fabsf(BCx) > 1e-8f ? BCy / BCx : 0.0f;

    // Pixel-center column bounds. A pixel column x represents samples at x + 0.5.
    int start_x = (int)ceilf(Ax - 0.5f);
    int middle_x = (int)ceilf(Bx - 0.5f);
    int end_x = (int)ceilf(Cx - 0.5f) - 1;

    // Clamp x boundaries to frame boundaries
    start_x = start_x < 0 ? 0 : start_x;
    end_x = end_x >= (int)frame_width ? frame_width - 1 : end_x;

    if (start_x > end_x) return;

    // Evaluate AC at first pixel center
    float sample_x = (float)start_x + 0.5f;
    float AC_y = Ay + AC_slope * (sample_x - Ax);

    // Evaluate AB initially
    float secondary_y = Ay + AB_slope * (sample_x - Ax);
    float secondary_slope = AB_slope;

    // Main column loop
    for (int current_x = start_x; current_x <= end_x; current_x++) {
        float first_pixel_center_x = (float)current_x + 0.5f;

        // Switch from AB to BC exactly when entering the B->C half.
        if (current_x >= middle_x) {
            secondary_slope = BC_slope;
            secondary_y = By + BC_slope * (first_pixel_center_x - Bx);
        }

        // Vertical boundaries
        float top_y = fminf(AC_y, secondary_y);
        float bottom_y = fmaxf(AC_y, secondary_y);

        /*
         * Pixel-center top-left convention.
         * We want rows whose pixel center lies inside the vertical boundry
         *
         * First sample >= top, Last sample < bottom
         *
         * This excludes the bottom/right boundary consistently.
         */

         // Snap vertical boundaries to grid
        int y_min = (int)ceilf(top_y - 0.5f);
        int y_max = (int)ceilf(bottom_y - 0.5f) - 1;

        // Clamp y boundaries to frame boundaries
        y_min = y_min < 0 ? 0 : y_min;
        y_max = y_max >= (int)frame_height ? frame_height - 1 : y_max;

        // Align y bounds to even numbers to prevent incomplete quad cuts
        y_min &= ~1;

        if (y_min <= y_max) {
            // Initialize barycentric coordinates at first pixel center
            float first_pixel_center_y = (float)y_min + 0.5f;
            float first_x_minus_C = first_pixel_center_x - Cx;
            float first_y_minus_C = first_pixel_center_y - Cy;
            
            // Base weights for the top-left corner of the quad [x, y]
            float base_wA = (first_x_minus_C * dA_dx) + (first_y_minus_C * dA_dy);
            float base_wB = (first_x_minus_C * dB_dx) + (first_y_minus_C * dB_dy);

            // Pre-load your constant step derivatives into SIMD registers
            __m128 v_dA_dy = _mm_set1_ps(dA_dy);
            __m128 v_dB_dy = _mm_set1_ps(dB_dy);

            __m128 v_inv_Az_minus_C = _mm_set1_ps(inv_Az_minus_C);
            __m128 v_inv_Bz_minus_C = _mm_set1_ps(inv_Bz_minus_C);
            __m128 v_inv_Cz = _mm_set1_ps(inv_Cz);

            // Vectorize texture coordinate step constants (assuming uvs are 2D floats)
            __m128 v_Auv_minus_C_u = _mm_set1_ps(Auv_minus_C[0]);
            __m128 v_Auv_minus_C_v = _mm_set1_ps(Auv_minus_C[1]);
            __m128 v_Buv_minus_C_u = _mm_set1_ps(Buv_minus_C[0]);
            __m128 v_Buv_minus_C_v = _mm_set1_ps(Buv_minus_C[1]);
            __m128 v_C_uv_u = _mm_set1_ps((*C_uv)[0]);
            __m128 v_C_uv_v = _mm_set1_ps((*C_uv)[1]);

            // Row loop
            // Step rows down by 2
            // Step rows vertically by 2 to maximize L1 texture cache locality
            for (int current_y = y_min; current_y <= y_max; current_y += 4) {
                // 1. SETUP LOGICAL OFFSETS FOR PIXEL 0 (qy=0) AND PIXEL 1 (qy=1)
                // Lane 0: offset 0.0f, Lane 1: offset 1.0f, Lanes 2 & 3: dummy values
                __m128 v_qy = _mm_setr_ps(0.0f, 1.0f, 2.0f, 3.0f);

                // 2. COMPUTE BARYCENTRIC WEIGHTS FOR BOTH PIXELS SIMULTANEOUSLY
                __m128 v_base_wA = _mm_set1_ps(base_wA);
                __m128 v_base_wB = _mm_set1_ps(base_wB);

                __m128 v_wA = _mm_add_ps(v_base_wA, _mm_mul_ps(v_qy, v_dA_dy));
                __m128 v_wB = _mm_add_ps(v_base_wB, _mm_mul_ps(v_qy, v_dB_dy));

                // 3. GENERATE THE COVERAGE MASK
                // Check if wA >= 0, wB >= 0, wC >= 0 for both lanes
                __m128 v_zero = _mm_setzero_ps();
                __m128 mask_wA = _mm_cmpge_ps(v_wA, v_zero);
                __m128 mask_wB = _mm_cmpge_ps(v_wB, v_zero);
                __m128 mask_coverage = _mm_and_ps(mask_wA, mask_wB);

                // If both pixels fall completely outside the triangle, skip the entire math block
                if (_mm_movemask_ps(mask_coverage) == 0) {
                    base_wA += (dA_dy * 4.0f);
                    base_wB += (dB_dy * 4.0f);
                    continue;
                }

                // 4. INTERPOLATE DEPTH (1/Z) & COMPUTE PERSPECTIVE CORRECTIVE Z
                __m128 v_depth = _mm_add_ps(v_inv_Cz, _mm_add_ps(_mm_mul_ps(v_inv_Az_minus_C, v_wA), _mm_mul_ps(v_inv_Bz_minus_C, v_wB)));

                // Vectorized division: z = 1.0f / depth
                __m128 v_z = _mm_div_ps(_mm_set1_ps(1.0f), v_depth);

                // 5. INTERPOLATE PERSPECTIVE-CORRECTED U & V COORDINATES
                __m128 v_u = _mm_mul_ps(_mm_add_ps(v_C_uv_u, _mm_add_ps(_mm_mul_ps(v_Auv_minus_C_u, v_wA), _mm_mul_ps(v_Buv_minus_C_u, v_wB))), v_z);
                __m128 v_v = _mm_mul_ps(_mm_add_ps(v_C_uv_v, _mm_add_ps(_mm_mul_ps(v_Auv_minus_C_v, v_wA), _mm_mul_ps(v_Buv_minus_C_v, v_wB))), v_z);

                float avg_inv_depth = horizontal_average_m128(v_z);
                vec2 scaled_duv_dx, scaled_duv_dy; 
                glm_vec2_scale(duv_dx, avg_inv_depth, scaled_duv_dx);
                glm_vec2_scale(duv_dy, avg_inv_depth, scaled_duv_dy);

                __m128i int_v_qy = _mm_cvttps_epi32(v_qy);
                __m128i v_py = _mm_add_epi32(int_v_qy, _mm_set1_epi32(current_y));
                __m128i v_pixel_idx = _mm_add_epi32(_mm_mul_epi32(v_py, _mm_set1_epi32(frame_width)), _mm_set1_epi32(current_x));

                // Extract computed lanes back to scalar values to perform Z-buffering and texturing
                float u_lanes[4], v_lanes[4], depth_lanes[4];
                int py_lanes[4], pixel_idx_lanes[4];
                _mm_storeu_ps(u_lanes, v_u);
                _mm_storeu_ps(v_lanes, v_v);
                _mm_storeu_ps(depth_lanes, v_depth);
                _mm_storeu_epi32(py_lanes, v_py);
                _mm_storeu_epi32(pixel_idx_lanes, v_pixel_idx);
                int coverage_mask = _mm_movemask_ps(mask_coverage);


                // 6. SCALAR BACKEND: BLIT TO FRAMEBUFFER
                // TODO: Check for if statements and index based array retrieval
                for (int qy = 0; qy < 4; qy++) {
                    // Check if this specific lane is marked valid by the SIMD edge test
                    if (!(coverage_mask & (1 << qy))) continue;

                    if (py_lanes[qy] > y_max) continue;
                    int pixel_idx = pixel_idx_lanes[qy]; // v_py * const + const -> easily vectorized
                    float interpolated_depth = depth_lanes[qy]; // already vectorized

                    if (interpolated_depth > z_buffer[pixel_idx]) {
                        z_buffer[pixel_idx] = interpolated_depth;

                        Uint32 final_color = 0xFFFFFFFF; // const easily vectorized
                        if (has_texture) {
                            // Invoke your trilinear sampler pipeline cleanly
                            Color texel = texture_sample_trilinear(texture, u_lanes[qy], v_lanes[qy], scaled_duv_dx, scaled_duv_dy);
                            final_color = _color_to_uint32(texel); // TODO: Check for array[4] manipulation for each group value
                        }
                        else {
                            float wA_scalar = base_wA + (qy * dA_dy); // const + (v_qy * const) -> easily vectorized
                            float wB_scalar = base_wB + (qy * dB_dy); // const + (v_qy * const) -> easily vectorized
                            // Correctly index into your vec4 cglm color arrays [0]=R, [1]=G, [2]=B
                            float r = C_color[0] + Acolor_minus_C[0] * wA_scalar + Bcolor_minus_C[0] * wB_scalar; // easily vectorized
                            float g = C_color[1] + Acolor_minus_C[1] * wA_scalar + Bcolor_minus_C[1] * wB_scalar; // easily vectorized
                            float b = C_color[2] + Acolor_minus_C[2] * wA_scalar + Bcolor_minus_C[2] * wB_scalar; // easily vectorized

                            // Clamp the interpolated float channels to valid 0-255 bounds before casting
                            int ir = (Uint8)glm_clamp(r, 0.0f, 255.0f); // Clamping easily vectorized
                            int ig = (Uint8)glm_clamp(g, 0.0f, 255.0f); // Clamping easily vectorized
                            int ib = (Uint8)glm_clamp(b, 0.0f, 255.0f); // Clamping easily vectorized

                            Color fallback_color = {
                                .r = ir,
                                .g = ig,
                                .b = ib,
                                .a = 255
                            };

                            final_color = _color_to_uint32(fallback_color);
                        }

                        frame[pixel_idx] = final_color;
                    }
                }
                // Step base weights down by 2 vertical lines
                base_wA += (dA_dy * 4.0f);
                base_wB += (dB_dy * 4.0f);
            }
        }
        // Step to next column
        AC_y += AC_slope;
        secondary_y += secondary_slope;
    }
}

static void _sort_points_by_x(Triangle* triangle, Triangle* dest, vec3* vertices)
{
	vec3* vertex_a = vertices[triangle->corner1_idx];
	vec3* vertex_b = vertices[triangle->corner2_idx];
	vec3* vertex_c = vertices[triangle->corner3_idx];

	if ((*vertex_a)[0] <= (*vertex_b)[0] && (*vertex_a)[0] <= (*vertex_c)[0]) {
		dest->corner1_idx = triangle->corner1_idx;
		if ((*vertex_b)[0] <= (*vertex_c)[0]) {
			dest->corner2_idx = triangle->corner2_idx;
			dest->corner3_idx = triangle->corner3_idx;
		}
		else {
			dest->corner2_idx = triangle->corner3_idx;
			dest->corner3_idx = triangle->corner2_idx;
		}
	}
	else if ((*vertex_b)[0] <= (*vertex_a)[0] && (*vertex_b)[0] <= (*vertex_c)[0]) {
		dest->corner1_idx = triangle->corner2_idx;
		if ((*vertex_a)[0] <= (*vertex_c)[0]) {
			dest->corner2_idx = triangle->corner1_idx;
			dest->corner3_idx = triangle->corner3_idx;
		}
		else {
			dest->corner2_idx = triangle->corner3_idx;
			dest->corner3_idx = triangle->corner1_idx;
		}
	}
	else {
		dest->corner1_idx = triangle->corner3_idx;
		if ((*vertex_a)[0] <= (*vertex_b)[0]) {
			dest->corner2_idx = triangle->corner1_idx;
			dest->corner3_idx = triangle->corner2_idx;
		}
		else {
			dest->corner2_idx = triangle->corner2_idx;
			dest->corner3_idx = triangle->corner1_idx;
		}
	}
}

static inline Uint32 _color_to_uint32(Color color) {
    return (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
}
