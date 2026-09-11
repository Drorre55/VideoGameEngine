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

static uint32_t _encode_morton_2d(uint32_t x, uint32_t y);

TiledTexture texture_load_from_file(const char* filepath);
static void _generate_sub_mipmaps(TiledTexture texture);
void texture_free(TiledTexture* mipmaps);

TextureBank texture_bank_create(Uint32 capacity);
void texture_bank_free(TextureBank* bank);
TiledTexture texture_clone(const TiledTexture src);
static Mipmap _mipmap_clone(const Mipmap src);
TextureBank texture_bank_deep_copy(const TextureBank* src);
Uint32 texture_bank_add(TextureBank* bank, TiledTexture mipmaps);
Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath);

Color texture_sample_nearest(const Mipmap mipmap, float u, float v);
Color texture_sample_bilinear(const Mipmap mipmap, float u, float v);
Color texture_sample_trilinear(const TiledTexture texture, float u, float v, vec2 duv_dx, vec2 duv_dy);

Uint32 clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value);
