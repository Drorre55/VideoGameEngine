#pragma once
#include "SDL3/SDL.h"
#include <cglm/cglm.h>

#define TEXTURE_NONE ((Uint32)-1)
#define TILE_WIDTH 8
#define TILE_WIDTH_BITS 3 // 2^3 = 8
#define TILE_PIXELS 64 // 8*8

typedef union {
    struct {
        Uint8 r, g, b, a;
    };
    Uint8 iter[4];
} Color;

typedef struct {
    __m256 r, g, b, a;
} ColorSIMD;

typedef struct {
    Uint32 width;
    Uint32 height;
    Uint32 width_in_tiles;
    Color* pixels; // Ptr to the contiguous mip-chain memory
} Mipmap;

typedef struct {
    Mipmap* mipmaps;
    Color* contiguous_buffer; // Single allocation for all mips
    Uint8 num_levels;
} TiledTexture;

typedef struct {
    TiledTexture* textures;
    Uint32 count;
    Uint32 capacity;
} TextureBank;

TiledTexture texture_load_from_file(const char* filepath);
static void _generate_sub_mipmaps(TiledTexture texture);
void texture_free(TiledTexture* mipmaps);

TextureBank texture_bank_create(Uint32 capacity);
void texture_bank_free(TextureBank* bank);
TiledTexture texture_clone(const TiledTexture src);
TextureBank texture_bank_deep_copy(const TextureBank* src);
Uint32 texture_bank_add(TextureBank* bank, TiledTexture mipmaps);
Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath);

Color texture_sample_nearest(const Mipmap mipmap, float u, float v);
static inline Color _texture_sample_bilinear(const Mipmap mipmap, float u, float v);
Color texture_sample_trilinear(const TiledTexture texture, float u, float v, vec2 duv_dx, vec2 duv_dy);

Uint32 _clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value);

static __m256i _bit_interleave_SIMD(__m256i x);
static void _unpack_rgba_to_float_soa_SIMD(__m256i packed_rgba, __m256* r, __m256* g, __m256* b, __m256* a);
static __m256 _bilinear_filter_channel_SIMD(__m256 tl, __m256 tr, __m256 bl, __m256 br, __m256 alpha, __m256 beta);
static void _sample_mip_level_soa_SIMD(const Color* mip_ptr, Uint32 width_in_tiles, __m256i u0, __m256i v0, __m256i tex_w_mask, __m256i tex_h_mask, __m256i int_mask, __m256 alpha, __m256 beta, __m256* out_R, __m256* out_G, __m256* out_B, __m256* out_A);
ColorSIMD texture_sample_trilinear_SIMD(int x, int y, int screen_width, float* depth_buffer, uint32_t* frame_buffer, __m256i int_mask, __m256 z_vec, __m256 u_vec, __m256 v_vec, TiledTexture* texture);

