#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "stb_image.h"
#include <string.h>

// Pre-computed Morton indices for an 8x8 block.
// Format: [row][col], matching your corrected structural access order.
static const uint8_t MORTON_LUT[8][8] = {
    { 0,  1,  4,  5, 16, 17, 20, 21 },
    { 2,  3,  6,  7, 18, 19, 22, 23 },
    { 8,  9, 12, 13, 24, 25, 28, 29 },
    {10, 11, 14, 15, 26, 27, 30, 31 },
    {32, 33, 36, 37, 48, 49, 52, 53 },
    {34, 35, 38, 39, 50, 51, 54, 55 },
    {40, 41, 44, 45, 56, 57, 60, 61 },
    {42, 43, 46, 47, 58, 59, 62, 63 }
};

// Looks up a pixel color inside a specific mipmap level.
// This function bypasses linear memory rules entirely.
static inline Color _sample_tiled_pixel(const Mipmap* mipmap, Uint32 row, Uint32 col) {
    // 1. Isolate the tile coordinate vs the pixel position inside that tile
    Uint32 tile_row = row >> TILE_WIDTH_BITS; // row / TILE_WIDTH
    Uint32 tile_col = col >> TILE_WIDTH_BITS; // col / TILE_WIDTH

    Uint32 pixel_row = row & (TILE_WIDTH - 1u); // row % TILE_WIDTH
    Uint32 pixel_col = col & (TILE_WIDTH - 1u); // col % TILE_WIDTH

    // 2. Find the flat index of the tile itself (Row-major for tiles)
    Uint32 tile_index = (tile_row * mipmap->width_in_tiles) + tile_col;
    Uint32 tile_offset = tile_index * TILE_PIXELS;

    // 3. Find the Morton localized index inside the target 8x8 tile
    Uint32 local_morton_index = MORTON_LUT[pixel_row][pixel_col];
    
    // 4. Single memory lookup 
    return mipmap->pixels[tile_offset + local_morton_index];
}

TiledTexture create_tiled_texture(uint32_t width, uint32_t height) {
    TiledTexture texture = { 0 };

    // Calculate total required pixels across all mips
    Uint32 total_pixels = 0;
    Uint32 mipmap_width = width, mipmap_height = height;
    Uint8 num_levels = 0;

    while (mipmap_width >= 1 && mipmap_height >= 1) {
        // Pad dimensions to 8x8 tile boundaries
        Uint32 padded_w = ((mipmap_width + TILE_WIDTH - 1) & ~(TILE_WIDTH - 1));
        Uint32 padded_h = ((mipmap_height + TILE_WIDTH - 1) & ~(TILE_WIDTH - 1));
        total_pixels += padded_w * padded_h;

        num_levels++;
        if (mipmap_width == 1 && mipmap_height == 1) break;
        mipmap_width = mipmap_width > 1 ? mipmap_width >> 1 : 1; // mipmap_width / 2 unless already 1
        mipmap_height = mipmap_height > 1 ? mipmap_height >> 1 : 1; // mipmap_height / 2 unless already 1
    }
    texture.num_levels = num_levels;

    texture.mipmaps = malloc(num_levels * sizeof(Mipmap));

    // 64-byte aligned allocation for optimal CPU cache line alignment
    texture.contiguous_buffer = (Color*)_aligned_malloc(total_pixels * sizeof(Color), 64);

    // Assign pointers inside the flat pool
    Color* current_ptr = texture.contiguous_buffer;
    mipmap_width = width; mipmap_height = height;
    for (Uint32 i = 0; i < num_levels; i++) {
        Uint32 padded_w = ((mipmap_width + TILE_WIDTH - 1) & ~(TILE_WIDTH - 1));
        Uint32 padded_h = ((mipmap_height + TILE_WIDTH - 1) & ~(TILE_WIDTH - 1));

        texture.mipmaps[i].width = mipmap_width;
        texture.mipmaps[i].height = mipmap_height;
        texture.mipmaps[i].width_in_tiles = padded_w >> TILE_WIDTH_BITS; // padded_w / TILE_WIDTH
        texture.mipmaps[i].pixels = current_ptr;

        current_ptr += (padded_w * padded_h);
        mipmap_width = mipmap_width > 1 ? mipmap_width >> 1 : 1; // mipmap_width / 2 unless already 1
        mipmap_height = mipmap_height > 1 ? mipmap_height >> 1 : 1; // mipmap_height / 2 unless already 1
    }

    return texture;
}

/**
 * Converts a flat, row-major linear Color buffer
 * into our highly optimized Tiled / Morton structure for Mip 0.
 */
static void _populate_mipmap(Mipmap mipmap, const Color* linear_input, Uint32 width) {
    // Walk through the linear source dimensions
    for (Uint32 row = 0; row < width; row++) {
        for (Uint32 col = 0; col < width; col++) {
            // Read linear pixel
            Color pixel_color = linear_input[row * width + col];

            // Calculate where it belongs in our tiled architecture
            Uint32 tile_row = row >> TILE_WIDTH_BITS;
            Uint32 tile_col = col >> TILE_WIDTH_BITS;
            Uint32 pixel_row = row & (TILE_WIDTH - 1);
            Uint32 pixel_col = col & (TILE_WIDTH - 1);

            Uint32 tile_index = (tile_row * mipmap.width_in_tiles) + tile_col;
            Uint32 tile_offset = tile_index * TILE_PIXELS;
            Uint32 local_morton = MORTON_LUT[pixel_row][pixel_col];

            // Write to the contiguous tiled pool
            mipmap.pixels[tile_offset + local_morton] = pixel_color;
        }
    }
}

TiledTexture texture_load_from_file(const char* filepath)
{
    TiledTexture texture = { 0 };
    if (!filepath) 
        return texture;
    
    int width = 0;
    int height = 0;
    int placeholder = 0;
    
    // Force RGBA so renderer uses a consistent format.
    Color* pixels = stbi_load(filepath, &width, &height, &placeholder, 4);

    if (!pixels) {
        SDL_LogError(1, "Failed to load texture: %s", filepath);
        return texture;
    }

    texture = create_tiled_texture(width, height);
    _populate_mipmap(texture.mipmaps[0], pixels, width);
    _generate_sub_mipmaps(texture);

    return texture;
}

// We assume square image in the power of 2
static void _generate_sub_mipmaps(TiledTexture texture) {
    Mipmap original_image = texture.mipmaps[0];
    for (Uint8 i = 1; i < texture.num_levels; i++) {
        Mipmap mipmap = texture.mipmaps[i];
        Uint16 new_width = mipmap.width;
        Uint16 kernel_width = 1u << i; // 2^i
        float kernel_size = (float)(kernel_width * kernel_width);

        for (Uint16 row = 0; row < new_width; row++) {
            for (Uint16 col = 0; col < new_width; col++) {
                float sum_r = 0.f;
                float sum_g = 0.f;
                float sum_b = 0.f;
                float sum_a = 0.f;

                // Map target (row,col) to the bounding box anchor in Mip 0
                Uint32 original_start_row = row * kernel_width;
                Uint32 original_start_col = col * kernel_width;

                for (Uint16 kernel_row = 0; kernel_row < kernel_width; kernel_row++) {
                    for (Uint16 kernel_col = 0; kernel_col < kernel_width; kernel_col++) {
                        Uint32 original_row = original_start_row + kernel_row;
                        Uint32 original_col = original_start_col + kernel_col;

                        Color original_pixel = _sample_tiled_pixel(&original_image, original_row, original_col);
                        sum_r += (float)original_pixel.r;
                        sum_g += (float)original_pixel.g;
                        sum_b += (float)original_pixel.b;
                        sum_a += (float)original_pixel.a;
                    }
                }
                Color average_color = {
                    .r = (Uint8)(sum_r / kernel_size),
                    .g = (Uint8)(sum_g / kernel_size),
                    .b = (Uint8)(sum_b / kernel_size),
                    .a = (Uint8)(sum_a / kernel_size)
                };
                // Compute destination tile parameters
                Uint32 mipmap_tile_row = row >> TILE_WIDTH_BITS;
                Uint32 mipmap_tile_col = col >> TILE_WIDTH_BITS;
                Uint32 mipmap_pixel_row = row & (TILE_WIDTH - 1);
                Uint32 mipmap_pixel_col = col & (TILE_WIDTH - 1);

                uint32_t mipmap_tile_idx = (mipmap_tile_row * mipmap.width_in_tiles) + mipmap_tile_col;
                uint32_t mipmap_tile_offset = mipmap_tile_idx * TILE_PIXELS;
                uint32_t mipmap_morton = MORTON_LUT[mipmap_pixel_row][mipmap_pixel_col];

                mipmap.pixels[mipmap_tile_offset + mipmap_morton] = average_color;
            }
        }
    }
}

void texture_free(TiledTexture* texture)
{
    if (!texture || texture->num_levels == 0)
        return;

    free(texture->mipmaps);
    _aligned_free(texture->contiguous_buffer);
    texture->mipmaps = NULL;
    texture->contiguous_buffer = NULL;
    texture->num_levels = 0;
}

TextureBank texture_bank_create(Uint32 capacity)
{
    TextureBank bank = { 0 };

    if (capacity == 0)
        capacity = 4;
    
    bank.capacity = capacity;
    bank.textures = malloc(sizeof(TiledTexture) * bank.capacity);
    if (!bank.textures) {
        bank.capacity = 0;
        return bank;
    }
    memset(bank.textures, 0, sizeof(TiledTexture) * bank.capacity);
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

static inline Uint32 _mipmap_padded_size(const Mipmap mipmap) {
    Uint32 padded_w = mipmap.width_in_tiles * TILE_WIDTH;
    Uint32 padded_h = ((mipmap.height + TILE_WIDTH - 1) & ~(TILE_WIDTH - 1));
    return padded_w * padded_h;
}

TiledTexture texture_clone(const TiledTexture src)
{
    TiledTexture copy = { 0 };
    if (!src.mipmaps || src.num_levels == 0)
        return copy;

    copy.num_levels = src.num_levels;
    copy.mipmaps = malloc(copy.num_levels * sizeof(Mipmap));
    if (!copy.mipmaps) {
        copy.num_levels = 0;
        return copy;
    }

    Uint32 total_pixels = 0;
    for (Uint8 i = 0; i < src.num_levels; i++)
        total_pixels += _mipmap_padded_size(src.mipmaps[i]);

    copy.contiguous_buffer = (Color*)_aligned_malloc(total_pixels * sizeof(Color), 64);
    if (!copy.contiguous_buffer) {
        free(copy.mipmaps);
        copy.mipmaps = NULL;
        copy.num_levels = 0;
        return copy;
    }
    memcpy(copy.contiguous_buffer, src.contiguous_buffer, total_pixels * sizeof(Color));

    Color* cursor = copy.contiguous_buffer;
    for (Uint8 i = 0; i < src.num_levels; i++) {
        copy.mipmaps[i].width = src.mipmaps[i].width;
        copy.mipmaps[i].height = src.mipmaps[i].height;
        copy.mipmaps[i].width_in_tiles = src.mipmaps[i].width_in_tiles; // <-- the actual fix
        copy.mipmaps[i].pixels = cursor;
        cursor += _mipmap_padded_size(src.mipmaps[i]);
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

    dst.textures = calloc(dst.capacity ? dst.capacity : dst.count, sizeof(TiledTexture));
    if (!dst.textures) return dst;

    for (Uint32 i = 0; i < src->count; i++) {
        dst.textures[i] = texture_clone(src->textures[i]);
    }

    return dst;
}

Uint32 texture_bank_add(TextureBank* bank, TiledTexture mipmap) {
    if (!bank->textures && bank->capacity == 0) {
        *bank = texture_bank_create(4);
    }
    if (bank->count + 1 >= bank->capacity) {
        Uint32 new_capacity = bank->capacity * 2;
        TiledTexture* resized = realloc(bank->textures, sizeof(TiledTexture) * new_capacity);
        if (!resized) {
            return TEXTURE_NONE;
        }

        bank->textures = resized;
        memset(&bank->textures[bank->capacity], 0, sizeof(TiledTexture) * (new_capacity - bank->capacity));
        bank->capacity = new_capacity;
    }
    bank->textures[bank->count++] = mipmap;
    return bank->count - 1;
}

Uint32 texture_bank_add_from_file(TextureBank* bank, const char* filepath)
{
    TiledTexture texture = texture_load_from_file(filepath);
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

    Uint32 col = (Uint32)(uu * (float)texture->width) % texture->width;
    Uint32 row = (Uint32)(vv * (float)texture->height) % texture->height;

    Uint32 index = row * texture->width + col;
    pixel.r = texture->pixels[index].r;
    pixel.g = texture->pixels[index].g;
    pixel.b = texture->pixels[index].b;
    pixel.a = texture->pixels[index].a;*/

    return pixel;
}

Color texture_sample_bilinear(const Mipmap mipmap, float u, float v)
{
    Color pixel = { 0, 0, 0, 255 };

    if (!mipmap.pixels || mipmap.width == 0 || mipmap.height == 0)
        return pixel;

    float uu = u - floorf(u);
    float vv = v - floorf(v);

    float row = vv * (float)(mipmap.height - 1);
    float col = uu * (float)(mipmap.width - 1);

    Uint32 row0 = (Uint32)floorf(row);
    Uint32 col0 = (Uint32)floorf(col);
    Uint32 row1 = clamp_u32(row0 + 1, 0, mipmap.height - 1);
    Uint32 col1 = clamp_u32(col0 + 1, 0, mipmap.width - 1);

    float t_row = row - (float)row0;
    float t_col = col - (float)col0;
    
    Color pixel00 = _sample_tiled_pixel(&mipmap, row0, col0);
    Color pixel01 = _sample_tiled_pixel(&mipmap, row0, col1);
    Color pixel10 = _sample_tiled_pixel(&mipmap, row1, col0);
    Color pixel11 = _sample_tiled_pixel(&mipmap, row1, col1);

    float r0 = glm_lerp(pixel00.r, pixel01.r, t_col);
    float g0 = glm_lerp(pixel00.g, pixel01.g, t_col);
    float b0 = glm_lerp(pixel00.b, pixel01.b, t_col);
    float a0 = glm_lerp(pixel00.a, pixel01.a, t_col);

    float r1 = glm_lerp(pixel10.r, pixel11.r, t_col);
    float g1 = glm_lerp(pixel10.g, pixel11.g, t_col);
    float b1 = glm_lerp(pixel10.b, pixel11.b, t_col);
    float a1 = glm_lerp(pixel10.a, pixel11.a, t_col);

    pixel.r = (Uint8)glm_lerp(r0, r1, t_row);
    pixel.g = (Uint8)glm_lerp(g0, g1, t_row);
    pixel.b = (Uint8)glm_lerp(b0, b1, t_row);
    pixel.a = (Uint8)glm_lerp(a0, a1, t_row);

    return pixel;
}

inline Uint32 clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value)
{
    if (value < min_value) return min_value;
    else if (value > max_value) return max_value;
    return value;
}

Color texture_sample_trilinear(const TiledTexture texture, float u, float v, vec2 duv_dx, vec2 duv_dy)
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