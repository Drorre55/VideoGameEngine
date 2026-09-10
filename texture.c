#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "stb_image.h"

#include <string.h>
#include <cglm/cglm.h>

Texture texture_load_from_file(const char* filepath)
{
    Texture texture = { 0 };
    if (!filepath) 
        return texture;
    
    Mipmap mipmap = { 0 };

    int width = 0;
    int height = 0;
    int placeholder = 0;

    // Force RGBA so renderer uses a consistent format.
    Uint8* pixels = stbi_load(filepath, &width, &height, &placeholder, 4);

    if (!pixels) {
        SDL_LogError(1, "Failed to load texture: %s", filepath);
        return texture;
    }

    mipmap.width = (Uint32)width;
    mipmap.height = (Uint32)height;
    mipmap.pixels = pixels;

    texture.num_levels = (Uint8)log2f(min(width, height)) + 1;
    texture.mipmaps = malloc(texture.num_levels * sizeof(Mipmap));
    texture.mipmaps[0] = mipmap;
    _generate_mipmaps(texture);

    return texture;
}

// We assume square image in the power of 2
static void _generate_mipmaps(Texture texture) {
    Mipmap original_image = texture.mipmaps[0];
    for (Uint8 i = 1; i < texture.num_levels; i++) {
        Uint16 new_width = 1 << (texture.num_levels - 1 - i); // 2^(texture.num_levels - i)
        Uint16 kernel_size = 1 << i; // 2^i

        Mipmap* mipmap = malloc(sizeof(Mipmap));
        if (!mipmap) {
            SDL_LogError(1, "Failed creating mipmap");
            return;
        }
        Uint8* pixels = malloc(new_width * new_width * 4 * sizeof(Uint8));
        if (!pixels) {
            SDL_LogError(1, "Failed creating mipmap");
            return;
        }

        for (Uint16 row = 0; row < new_width; row++) {
            for (Uint16 col = 0; col < new_width; col++) {
                float sum_r, sum_g, sum_b, sum_a;
                sum_r = sum_g = sum_b = sum_a = 0;
                for (Uint16 kernel_row = 0; kernel_row < kernel_size; kernel_row++) {
                    for (Uint16 kernel_col = 0; kernel_col < kernel_size; kernel_col++) {
                        Uint32 original_image_idx = (
                            (row * kernel_size + kernel_row) * original_image.width 
                            + col * kernel_size + kernel_col) * 4;
                        sum_r += (float)original_image.pixels[original_image_idx + 0];
                        sum_g += (float)original_image.pixels[original_image_idx + 1];
                        sum_b += (float)original_image.pixels[original_image_idx + 2];
                        sum_a += (float)original_image.pixels[original_image_idx + 3];
                    }
                }
                Uint32 new_idx = (row * new_width + col) * 4;
                pixels[new_idx + 0] = (Uint8) (sum_r / (kernel_size * kernel_size));
                pixels[new_idx + 1] = (Uint8) (sum_g / (kernel_size * kernel_size));
                pixels[new_idx + 2] = (Uint8) (sum_b / (kernel_size * kernel_size));
                pixels[new_idx + 3] = (Uint8) (sum_a / (kernel_size * kernel_size));
            }
        }
        mipmap->pixels = pixels;
        mipmap->width = new_width;
        mipmap->height = new_width;
        texture.mipmaps[i] = *mipmap;
    }
}

void texture_free(Texture* texture)
{
    if (!texture || texture->num_levels == 0)
        return;

    for (Uint8 i = 0; i < texture->num_levels; i++) {
        _mipmap_free(&texture->mipmaps[i]);
    }
    texture->mipmaps = NULL;
    texture->num_levels = 0;
}

static void _mipmap_free(Mipmap* mipmap)
{
    if (!mipmap || !mipmap->pixels)
        return;

    stbi_image_free(mipmap->pixels);
    mipmap->pixels = NULL;
    mipmap->width = 0;
    mipmap->height = 0;
}

TextureBank texture_bank_create(Uint32 capacity)
{
    TextureBank bank = { 0 };

    if (capacity == 0)
        capacity = 4;
    
    bank.capacity = capacity;
    bank.textures = malloc(sizeof(Texture) * bank.capacity);
    if (!bank.textures) {
        bank.capacity = 0;
        return bank;
    }
    memset(bank.textures, 0, sizeof(Texture) * bank.capacity);
    return bank;
}

void texture_bank_free(TextureBank* bank)
{
    if (!bank)
        return;

    for (Uint32 i = 0; i < bank->count; i++)
        texture_free(&bank->textures[i]);

    free(bank->textures);
    bank->textures = NULL;
    bank->count = 0;
    bank->capacity = 0;
}

Texture texture_clone(const Texture src)
{
    Texture copy = { 0 };
    copy.num_levels = src.num_levels;

    if (src.mipmaps && copy.num_levels > 0)
        copy.mipmaps = malloc(copy.num_levels * sizeof(Mipmap));
        if (copy.mipmaps) {
            for (Uint8 i = 0; i < copy.num_levels; i++) {
                copy.mipmaps[i] = _mipmap_clone(src.mipmaps[i]);
            }
        }
    return copy;
}

static Mipmap _mipmap_clone(const Mipmap src)
{
    Mipmap copy = { 0 };

    copy.width = src.width;
    copy.height = src.height;

    if (src.pixels && src.width > 0 && src.height > 0) {
        size_t pixel_count = (size_t)src.width * (size_t)src.height * 4;
        copy.pixels = malloc(pixel_count * sizeof(Uint8));
        if (copy.pixels) {
            memcpy(copy.pixels, src.pixels, pixel_count * sizeof(Uint8));
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

    dst.textures = calloc(dst.capacity ? dst.capacity : dst.count, sizeof(Texture));
    if (!dst.textures) return dst;

    for (Uint32 i = 0; i < src->count; i++) {
        dst.textures[i] = texture_clone(src->textures[i]);
    }

    return dst;
}

Uint32 texture_bank_add(TextureBank* bank, Texture mipmap) {
    if (!bank->textures && bank->capacity == 0) {
        *bank = texture_bank_create(4);
    }
    if (bank->count + 1 >= bank->capacity) {
        Uint32 new_capacity = bank->capacity * 2;
        Texture* resized = realloc(bank->textures, sizeof(Texture) * new_capacity);
        if (!resized) {
            return TEXTURE_NONE;
        }

        bank->textures = resized;
        memset(&bank->textures[bank->capacity], 0, sizeof(Texture) * (new_capacity - bank->capacity));
        bank->capacity = new_capacity;
    }
    bank->textures[bank->count++] = mipmap;
    return bank->count - 1;
}

Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath)
{
    Texture texture = texture_load_from_file(filepath);
    if (texture.num_levels == 0) {
        return TEXTURE_NONE;
    }
    return texture_bank_add(bank, texture);
}

Color texture_sample_nearest(const Mipmap mipmap, float u, float v)
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

Color texture_sample_bilinear(const Mipmap mipmap, float u, float v)
{
    Color pixel = { 0, 0, 0, 255 };

    if (!mipmap.pixels || mipmap.width == 0 || mipmap.height == 0)
        return pixel;

    float uu = u - floorf(u);
    float vv = v - floorf(v);

    float x = uu * (float)(mipmap.width - 1);
    float y = vv * (float)(mipmap.height - 1);

    Uint32 x0 = (Uint32)floorf(x);
    Uint32 y0 = (Uint32)floorf(y);
    Uint32 x1 = clamp_u32(x0 + 1, 0, mipmap.width - 1);
    Uint32 y1 = clamp_u32(y0 + 1, 0, mipmap.height - 1);

    float tx = x - (float)x0;
    float ty = y - (float)y0;
    
    Uint32 idx00 = (y0 * mipmap.width + x0) * 4;
    Uint32 idx10 = (y0 * mipmap.width + x1) * 4;
    Uint32 idx01 = (y1 * mipmap.width + x0) * 4;
    Uint32 idx11 = (y1 * mipmap.width + x1) * 4;

    float r00 = (float)mipmap.pixels[idx00 + 0];
    float g00 = (float)mipmap.pixels[idx00 + 1];
    float b00 = (float)mipmap.pixels[idx00 + 2];
    float a00 = (float)mipmap.pixels[idx00 + 3];

    float r10 = (float)mipmap.pixels[idx10 + 0];
    float g10 = (float)mipmap.pixels[idx10 + 1];
    float b10 = (float)mipmap.pixels[idx10 + 2];
    float a10 = (float)mipmap.pixels[idx10 + 3];

    float r01 = (float)mipmap.pixels[idx01 + 0];
    float g01 = (float)mipmap.pixels[idx01 + 1];
    float b01 = (float)mipmap.pixels[idx01 + 2];
    float a01 = (float)mipmap.pixels[idx01 + 3];

    float r11 = (float)mipmap.pixels[idx11 + 0];
    float g11 = (float)mipmap.pixels[idx11 + 1];
    float b11 = (float)mipmap.pixels[idx11 + 2];
    float a11 = (float)mipmap.pixels[idx11 + 3];

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

Color texture_sample_trilinear(const Texture texture, float u, float v, vec2 duv_dx, vec2 duv_dy)
{
    Mipmap original_image = texture.mipmaps[0];

    float tex_w = (float)original_image.width;
    float tex_h = (float)original_image.height;

    // Derivatives in texel space
    float du_dx = fabsf(duv_dx[0] * tex_w);
    float dv_dx = fabsf(duv_dx[1] * tex_h);
    float du_dy = fabsf(duv_dy[0] * tex_w);
    float dv_dy = fabsf(duv_dy[1] * tex_h);

    // Standard footprint estimate: use the larger of the two projected lengths
    float length_x = sqrtf(du_dx * du_dx + dv_dx * dv_dx);
    float length_y = sqrtf(du_dy * du_dy + dv_dy * dv_dy);
    float rho = fmaxf(length_x, length_y);

    float lod = log2f(fmax(rho, 1.f));
    lod = glm_clamp(lod, 0.f, (float)texture.num_levels - 1);
    
    Uint32 low_mipmap = floorf(lod);
    Uint32 high_mipmap = ceilf(lod);

    low_mipmap = clamp_u32(low_mipmap, 0, texture.num_levels - 1);
    high_mipmap = clamp_u32(high_mipmap, 0, texture.num_levels - 1);
    
    if (low_mipmap == high_mipmap)
        return texture_sample_bilinear(texture.mipmaps[low_mipmap], u, v);
    
    Color low_mipmap_color = texture_sample_bilinear(texture.mipmaps[low_mipmap], u, v);
    Color high_mipmap_color = texture_sample_bilinear(texture.mipmaps[high_mipmap], u, v);

    Color pixel = { 0, 0, 0, 255 };

    float t = lod - low_mipmap;
    pixel.r = (Uint8)glm_lerp(low_mipmap_color.r, high_mipmap_color.r, t);
    pixel.g = (Uint8)glm_lerp(low_mipmap_color.g, high_mipmap_color.g, t);
    pixel.b = (Uint8)glm_lerp(low_mipmap_color.b, high_mipmap_color.b, t);
    pixel.a = (Uint8)glm_lerp(low_mipmap_color.a, high_mipmap_color.a, t);

    return pixel;
}