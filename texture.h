#pragma once
#include "SDL3/SDL.h"
#include <cglm/cglm.h>
#define TEXTURE_NONE ((Uint32)-1)

typedef union {
    struct {
        Uint8 r, g, b, a;
    };
    Uint8 iter[4];
} Color;

typedef struct {
    Uint32 width;
    Uint32 height;
    Uint8* pixels;
} Mipmap;

typedef struct {
    Mipmap* mipmaps;
    Uint8 num_levels;
} Texture;

typedef struct {
    Texture* textures;
    Uint32 count;
    Uint32 capacity;
} TextureBank;

Texture texture_load_from_file(const char* filepath);
static void _generate_mipmaps(Texture texture);
void texture_free(Texture* mipmaps);
static void _mipmap_free(Mipmap* mipmap);

TextureBank texture_bank_create(Uint32 capacity);
void texture_bank_free(TextureBank* bank);
Texture texture_clone(const Texture src);
static Mipmap _mipmap_clone(const Mipmap src);
TextureBank texture_bank_deep_copy(const TextureBank* src);
Uint32 texture_bank_add(TextureBank* bank, Texture mipmaps);
Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath);

Color texture_sample_nearest(const Mipmap mipmap, float u, float v);
Color texture_sample_bilinear(const Mipmap mipmap, float u, float v);
Color texture_sample_trilinear(const Texture texture, float u, float v, vec2 duv_dx, vec2 duv_dy);

Uint32 clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value);
