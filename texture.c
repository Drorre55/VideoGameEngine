#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "stb_image.h"

#include <string.h>
#include <cglm/cglm.h>

Texture2D texture_load_from_file(const char* filepath)
{
    Texture2D texture = { 0 };

    if (!filepath) {
        return texture;
    }

    int width = 0;
    int height = 0;
    int channels_in_file = 0;

    // Force RGBA so renderer uses a consistent format.
    unsigned char* pixels = stbi_load(filepath, &width, &height, &channels_in_file, 4);

    if (!pixels) {
        SDL_LogError(1, "Failed to load texture: %s", filepath);
        return texture;
    }

    texture.width = (Uint32)width;
    texture.height = (Uint32)height;
    texture.channels = 4;
    texture.pixels = pixels;

    return texture;
}

void texture_free(Texture2D* texture)
{
    if (!texture || !texture->pixels) {
        return;
    }

    stbi_image_free(texture->pixels);
    texture->pixels = NULL;
    texture->width = 0;
    texture->height = 0;
    texture->channels = 0;
}

TextureBank texture_bank_create(Uint32 capacity)
{
    TextureBank bank = { 0 };

    if (capacity == 0) {
        capacity = 4;
    }

    bank.capacity = capacity;
    bank.textures = malloc(sizeof(Texture2D) * bank.capacity);
    if (!bank.textures) {
        bank.capacity = 0;
        return bank;
    }

    memset(bank.textures, 0, sizeof(Texture2D) * bank.capacity);
    return bank;
}

void texture_bank_free(TextureBank* bank)
{
    if (!bank) {
        return;
    }

    for (Uint32 i = 0; i < bank->count; i++) {
        texture_free(&bank->textures[i]);
    }

    free(bank->textures);
    bank->textures = NULL;
    bank->count = 0;
    bank->capacity = 0;
}

Texture2D texture_clone(const Texture2D* src)
{
    Texture2D copy = { 0 };
    if (!src) return copy;

    copy.width = src->width;
    copy.height = src->height;
    copy.channels = src->channels;

    if (src->pixels && src->width > 0 && src->height > 0 && src->channels > 0) {
        size_t pixel_count = (size_t)src->width * (size_t)src->height * (size_t)src->channels;
        copy.pixels = malloc(pixel_count);
        if (copy.pixels) {
            memcpy(copy.pixels, src->pixels, pixel_count);
        }
    }

    return copy;
}

TextureBank texture_bank_deep_copy(const TextureBank* src)
{
    TextureBank dst = { 0 };
    if (!src) return dst;

    dst.capacity = src->capacity ? src->capacity : src->count;
    dst.count = src->count;

    if (dst.count == 0) return dst;

    dst.textures = calloc(dst.capacity ? dst.capacity : dst.count, sizeof(Texture2D));
    if (!dst.textures) return dst;

    for (Uint32 i = 0; i < src->count; i++) {
        dst.textures[i] = texture_clone(&src->textures[i]);
    }

    return dst;
}

Uint32 texture_bank_add(TextureBank* bank, Texture2D texture) {
    if (!bank->textures && bank->capacity == 0) {
        *bank = texture_bank_create(4);
    }
    if (bank->count + 1 >= bank->capacity) {
        Uint32 new_capacity = bank->capacity * 2;
        Texture2D* resized = realloc(bank->textures, sizeof(Texture2D) * new_capacity);
        if (!resized) {
            return TEXTURE_NONE;
        }

        bank->textures = resized;
        memset(&bank->textures[bank->capacity], 0, sizeof(Texture2D) * (new_capacity - bank->capacity));
        bank->capacity = new_capacity;
    }
    bank->textures[bank->count++] = texture;
    return bank->count - 1;
}

Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath)
{
    Texture2D texture = texture_load_from_file(filepath);
    if (!texture.pixels) {
        return TEXTURE_NONE;
    }

    return texture_bank_add(bank, texture);
}

Color texture_sample_nearest(const Texture2D* texture, float u, float v)
{
    //Color pixel = { 0, 0, 0, 255 };
    /*int checker =
        (((int)(u * 10.0f) & 1) ^
            ((int)(v * 10.0f) & 1));

    Color pixel = checker ? (Color) { 255, 255, 255, 255 } : (Color) { 0, 0, 0, 255 };*/
    Uint8 r = (Uint8)(glm_clamp(u, 0.0f, 1.0f) * 255.0f);
    Uint8 g = (Uint8)(glm_clamp(v, 0.0f, 1.0f) * 255.0f);
    Color pixel = { r, g, 0, 255 };

    /*if (!texture || !texture->pixels || texture->width == 0 || texture->height == 0) {
        return pixel;
    }

    float uu = u - floorf(u);
    float vv = v - floorf(v);

    Uint32 x = (Uint32)(uu * (float)texture->width) % texture->width;
    Uint32 y = (Uint32)(vv * (float)texture->height) % texture->height;

    Uint32 index = (y * texture->width + x) * 4;
    pixel.r = texture->pixels[index + 0];
    pixel.g = texture->pixels[index + 1];
    pixel.b = texture->pixels[index + 2];
    pixel.a = texture->pixels[index + 3];*/

    return pixel;
}

Color texture_sample_bilinear(const Texture2D* texture, float u, float v)
{
    Color pixel = { 0, 0, 0, 255 };

    if (!texture || !texture->pixels || texture->width == 0 || texture->height == 0)
        return pixel;

    float uu = u - floorf(u);
    float vv = v - floorf(v);

    float x = uu * (float)(texture->width - 1);
    float y = vv * (float)(texture->height - 1);

    Uint32 x0 = (Uint32)floorf(x);
    Uint32 y0 = (Uint32)floorf(y);
    Uint32 x1 = clamp_u32(x0 + 1, 0, texture->width - 1);
    Uint32 y1 = clamp_u32(y0 + 1, 0, texture->height - 1);

    float tx = x - (float)x0;
    float ty = y - (float)y0;

    Uint32 idx00 = (y0 * texture->width + x0) * 4;
    Uint32 idx10 = (y0 * texture->width + x1) * 4;
    Uint32 idx01 = (y1 * texture->width + x0) * 4;
    Uint32 idx11 = (y1 * texture->width + x1) * 4;

    float r00 = (float)texture->pixels[idx00 + 0];
    float g00 = (float)texture->pixels[idx00 + 1];
    float b00 = (float)texture->pixels[idx00 + 2];
    float a00 = (float)texture->pixels[idx00 + 3];

    float r10 = (float)texture->pixels[idx10 + 0];
    float g10 = (float)texture->pixels[idx10 + 1];
    float b10 = (float)texture->pixels[idx10 + 2];
    float a10 = (float)texture->pixels[idx10 + 3];

    float r01 = (float)texture->pixels[idx01 + 0];
    float g01 = (float)texture->pixels[idx01 + 1];
    float b01 = (float)texture->pixels[idx01 + 2];
    float a01 = (float)texture->pixels[idx01 + 3];

    float r11 = (float)texture->pixels[idx11 + 0];
    float g11 = (float)texture->pixels[idx11 + 1];
    float b11 = (float)texture->pixels[idx11 + 2];
    float a11 = (float)texture->pixels[idx11 + 3];

    float r0 = r00 + (r10 - r00) * tx;
    float g0 = g00 + (g10 - g00) * tx;
    float b0 = b00 + (b10 - b00) * tx;
    float a0 = a00 + (a10 - a00) * tx;

    float r1 = r01 + (r11 - r01) * tx;
    float g1 = g01 + (g11 - g01) * tx;
    float b1 = b01 + (b11 - b01) * tx;
    float a1 = a01 + (a11 - a01) * tx;

    pixel.r = (Uint8)(r0 + (r1 - r0) * ty);
    pixel.g = (Uint8)(g0 + (g1 - g0) * ty);
    pixel.b = (Uint8)(b0 + (b1 - b0) * ty);
    pixel.a = (Uint8)(a0 + (a1 - a0) * ty);

    return pixel;
}

inline Uint32 clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value)
{
    if (value < min_value) return min_value;
    else if (value > max_value) return max_value;
    return value;
}