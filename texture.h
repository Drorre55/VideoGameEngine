#pragma once

#include "SDL3/SDL.h"

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
    Uint32 channels;
    Uint8* pixels;
} Texture2D;

typedef struct {
    Texture2D* textures;
    Uint32 count;
    Uint32 capacity;
} TextureBank;

Texture2D texture_load_from_file(const char* filepath);
void texture_free(Texture2D* texture);

TextureBank texture_bank_create(Uint32 capacity);
void texture_bank_free(TextureBank* bank);
Texture2D texture_clone(const Texture2D* src);
TextureBank texture_bank_deep_copy(const TextureBank* src);
Uint32 texture_bank_add(TextureBank* bank, Texture2D texture);
Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath);

Color texture_sample_nearest(const Texture2D* texture, float u, float v);
Color texture_sample_bilinear(const Texture2D* texture, float u, float v);

Uint32 clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value);
