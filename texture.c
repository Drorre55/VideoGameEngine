#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "stb_image.h"
#include "exr_loader.h"
#include <string.h>
#include <immintrin.h>

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
static inline Material _sample_tiled_pixel(const Mipmap* mipmap, Uint32 row, Uint32 col) {
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

TiledTexture texture_load_from_file(const char* color_filepath, const char* normal_filepath)
{
    TiledTexture texture = { 0 };
    if (!color_filepath) 
        return texture;
    
    int width = 0;
    int height = 0;
    int placeholder = 0;
    
    // Force RGBA so renderer uses a consistent format.
    Color* colors = stbi_load(color_filepath, &width, &height, &placeholder, 4);

    if (!colors) {
        SDL_LogError(1, "Failed to load texture: %s", color_filepath);
        return texture;
    }

    const char* err = NULL;
    vec3* normals = NULL;
    if (normal_filepath) {
        int normal_width, normal_height;
        normals = load_exr_normals(normal_filepath, &normal_width, &normal_height,
            /*encoded_01=*/1, /*flip_v=*/0);
        if (!normals) {
            normals = NULL;
        }
        else if (width != normal_width || height != normal_height) {
            free(normals);
            normals = NULL;
        }
    }
    texture = _create_tiled_texture(width, height);
    _populate_mipmap(texture.mipmaps[0], colors, normals, width);
    _generate_sub_mipmaps(texture);

    return texture;
}

static TiledTexture _create_tiled_texture(uint32_t width, uint32_t height) {
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
    texture.contiguous_buffer = (Material*)_aligned_malloc(total_pixels * sizeof(Material), 64);

    // Assign pointers inside the flat pool
    Material* current_ptr = texture.contiguous_buffer;
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

// Converts a flat, row-major linear Material buffers into Tiled / Morton structure for Mip 0.
static void _populate_mipmap(Mipmap mipmap, const Color* colors, const vec3* normals, Uint32 width) {
    // Walk through the linear source dimensions
    for (Uint32 row = 0; row < width; row++) {
        for (Uint32 col = 0; col < width; col++) {
            // Read linear pixel
            Color pixel_color = colors[row * width + col];
            vec3* pixel_normal = normals[row * width + col];
            glm_vec3_normalize(pixel_normal);

            // Calculate where it belongs in our tiled architecture
            Uint32 tile_row = row >> TILE_WIDTH_BITS;
            Uint32 tile_col = col >> TILE_WIDTH_BITS;
            Uint32 pixel_row = row & (TILE_WIDTH - 1);
            Uint32 pixel_col = col & (TILE_WIDTH - 1);

            Uint32 tile_index = (tile_row * mipmap.width_in_tiles) + tile_col;
            Uint32 tile_offset = tile_index * TILE_PIXELS;
            Uint32 local_morton = MORTON_LUT[pixel_row][pixel_col];

            // Write to the contiguous tiled pool
            mipmap.pixels[tile_offset + local_morton].color = pixel_color;
            if (!(normals[0])) {
                mipmap.pixels[tile_offset + local_morton].normal[0] = 0.;
                mipmap.pixels[tile_offset + local_morton].normal[1] = 0.;
                mipmap.pixels[tile_offset + local_morton].normal[2] = 0.;
            }
            else {
                mipmap.pixels[tile_offset + local_morton].normal[0] = *pixel_normal[0];
                mipmap.pixels[tile_offset + local_morton].normal[1] = *pixel_normal[1];
                mipmap.pixels[tile_offset + local_morton].normal[2] = *pixel_normal[2];
            }
        }
    }
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
                
                float sum_nx = 0.f;
                float sum_ny = 0.f;
                float sum_nz = 0.f;

                // Map target (row,col) to the bounding box anchor in Mip 0
                Uint32 original_start_row = row * kernel_width;
                Uint32 original_start_col = col * kernel_width;

                for (Uint16 kernel_row = 0; kernel_row < kernel_width; kernel_row++) {
                    for (Uint16 kernel_col = 0; kernel_col < kernel_width; kernel_col++) {
                        Uint32 original_row = original_start_row + kernel_row;
                        Uint32 original_col = original_start_col + kernel_col;

                        Material original_pixel = _sample_tiled_pixel(&original_image, original_row, original_col);
                        sum_r += (float)original_pixel.color.r;
                        sum_g += (float)original_pixel.color.g;
                        sum_b += (float)original_pixel.color.b;
                        sum_a += (float)original_pixel.color.a;

                        sum_nx += original_pixel.normal[0];
                        sum_ny += original_pixel.normal[1];
                        sum_nz += original_pixel.normal[2];
                    }
                }
                Material average = {
                    .color.r = (Uint8)(sum_r / kernel_size),
                    .color.g = (Uint8)(sum_g / kernel_size),
                    .color.b = (Uint8)(sum_b / kernel_size),
                    .color.a = (Uint8)(sum_a / kernel_size),
                    .normal = {
                        (sum_nx / kernel_size),
                        (sum_ny / kernel_size),
                        (sum_nz / kernel_size)
                    }
                };
                glm_vec3_normalize(average.normal);
                // Compute destination tile parameters
                Uint32 mipmap_tile_row = row >> TILE_WIDTH_BITS;
                Uint32 mipmap_tile_col = col >> TILE_WIDTH_BITS;
                Uint32 mipmap_pixel_row = row & (TILE_WIDTH - 1);
                Uint32 mipmap_pixel_col = col & (TILE_WIDTH - 1);

                uint32_t mipmap_tile_idx = (mipmap_tile_row * mipmap.width_in_tiles) + mipmap_tile_col;
                uint32_t mipmap_tile_offset = mipmap_tile_idx * TILE_PIXELS;
                uint32_t mipmap_morton = MORTON_LUT[mipmap_pixel_row][mipmap_pixel_col];

                mipmap.pixels[mipmap_tile_offset + mipmap_morton] = average;
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

    copy.contiguous_buffer = (Material*)_aligned_malloc(total_pixels * sizeof(Material), 64);
    if (!copy.contiguous_buffer) {
        free(copy.mipmaps);
        copy.mipmaps = NULL;
        copy.num_levels = 0;
        return copy;
    }
    memcpy(copy.contiguous_buffer, src.contiguous_buffer, total_pixels * sizeof(Material));

    Material* cursor = copy.contiguous_buffer;
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

Uint32 texture_bank_add_from_file(TextureBank* bank, const char* color_filepath, const char* normal_filepath)
{
    TiledTexture texture = texture_load_from_file(color_filepath, normal_filepath);
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

static inline Material _texture_sample_bilinear(const Mipmap mipmap, float u, float v)
{
    Material out_material = { 0 };

    if (!mipmap.pixels || mipmap.width == 0 || mipmap.height == 0)
        return out_material;

    float uu = u - floorf(u);
    float vv = v - floorf(v);

    float row = vv * (float)(mipmap.height - 1);
    float col = uu * (float)(mipmap.width - 1);

    Uint32 row0 = (Uint32)floorf(row);
    Uint32 col0 = (Uint32)floorf(col);

    Uint32 row1 = (row0 + 1) % mipmap.height;
    Uint32 col1 = (col0 + 1) % mipmap.width;

    row0 %= mipmap.height;
    col0 %= mipmap.width;

    float t_row = row - (float)row0;
    float t_col = col - (float)col0;
    
    Material pixel00 = _sample_tiled_pixel(&mipmap, row0, col0);
    Material pixel01 = _sample_tiled_pixel(&mipmap, row0, col1);
    Material pixel10 = _sample_tiled_pixel(&mipmap, row1, col0);
    Material pixel11 = _sample_tiled_pixel(&mipmap, row1, col1);

    // 3. Convert 8-bit RGBA channels to 32-bit floats inside SIMD registers
    // We unpack the bytes into floats so we can do precise blending math
    __m256 v_p00 = _mm256_setr_ps((float)pixel00.color.r, (float)pixel00.color.g, (float)pixel00.color.b, (float)pixel00.color.a, pixel00.normal[0], pixel00.normal[1], pixel00.normal[2], 0.);
    __m256 v_p01 = _mm256_setr_ps((float)pixel01.color.r, (float)pixel01.color.g, (float)pixel01.color.b, (float)pixel01.color.a, pixel01.normal[0], pixel01.normal[1], pixel01.normal[2], 0.);
    __m256 v_p10 = _mm256_setr_ps((float)pixel10.color.r, (float)pixel10.color.g, (float)pixel10.color.b, (float)pixel10.color.a, pixel10.normal[0], pixel10.normal[1], pixel10.normal[2], 0.);
    __m256 v_p11 = _mm256_setr_ps((float)pixel11.color.r, (float)pixel11.color.g, (float)pixel11.color.b, (float)pixel11.color.a, pixel11.normal[0], pixel11.normal[1], pixel11.normal[2], 0.);

    // 4. Setup interpolation weights as vector constants
    __m256 v_t_col = _mm256_set1_ps(t_col);
    __m256 v_t_row = _mm256_set1_ps(t_row);

    // 5. Perform Vectorized LERP across columns for all 4 channels at once
    // Formula: mixed = start + t * (end - start)
    __m256 v_m0 = _mm256_add_ps(v_p00, _mm256_mul_ps(v_t_col, _mm256_sub_ps(v_p01, v_p00))); // Top row LERP
    __m256 v_m1 = _mm256_add_ps(v_p10, _mm256_mul_ps(v_t_col, _mm256_sub_ps(v_p11, v_p10))); // Bottom row LERP

    // 6. Perform Vectorized LERP vertically between rows
    __m256 v_final = _mm256_add_ps(v_m0, _mm256_mul_ps(v_t_row, _mm256_sub_ps(v_m1, v_m0)));

    /// --- THE REGISTER OPTIMIZATION FIX ---
    // 1. Extract f4, f5, f6, f7 to 
    __m128 first_floats = _mm256_extractf128_ps(v_final, 1);

    // 2. Extract and convert f0, f1, f2, f3 to 32-bit integers
    __m128 last_floats = _mm256_castps256_ps128(v_final);
    __m128i v_int = _mm_cvtps_epi32(last_floats);

    // 2. Compress four 32-bit integers down to 8-bit bytes inside a single register
    __m128i ints16 = _mm_packus_epi32(v_int, _mm_setzero_si128()); // Down to 16-bit elements
    __m128i bytes8 = _mm_packus_epi16(ints16, _mm_setzero_si128()); // Down to contiguous 8-bit RGBA channels

    //// 4. Combine bytes8 (4 bytes) and first_floats (12 bytes of interest) into one register
    //// We want the final 128-bit register to look like: [ f6, f5, f4 | b3, b2, b1, b0 ]
    //// Shifting first_floats left by 4 bytes (32 bits) inserts zeros in the lowest slot
    //__m128i upper_as_int = _mm_castps_si128(first_floats);
    //__m128i shifted_ints = _mm_bslli_si128(upper_as_int, 4);      // [ f6, f5, f4, 0x00000000 ]

    //// 5. Bitwise OR them together to combine color bytes and normals
    //__m128i final_payload = _mm_or_si128(shifted_ints, bytes8);

    //// 6. 16-byte single store directly into the struct
    //_mm_storeu_ps((float*)&padded_material, _mm_castsi128_ps(final_payload));


    // Extract the lower 32 bits (4 bytes) and copy directly to the Color union
    union {
        Uint32 raw;
        Color color;
    } result_color;
    result_color.raw = (Uint32)_mm_cvtsi128_si32(bytes8);
    out_material.color = result_color.color;

    // 3. Store the 3 normal floats safely
    // A standard 128-bit store (_mm_storeu_ps) writes 16 bytes, which would 
    // overshoot your 12-byte vec3 and cause memory corruption.
    // Instead, store 8 bytes (X and Y), then the remaining 4 bytes (Z).
    _mm_storeu_si64((__m128i*)(out_material.normal), _mm_castps_si128(first_floats));

    // Extract lane 2 (Z) and write it down
    out_material.normal[2] = _mm_cvtss_f32(_mm_shuffle_ps(first_floats, first_floats, _MM_SHUFFLE(2, 2, 2, 2)));
    glm_vec3_normalize(out_material.normal);
    return out_material;
}

Material texture_sample_trilinear(const TiledTexture texture, float u, float v, vec2 duv_dx, vec2 duv_dy)
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

    low_mipmap = _clamp_u32(low_mipmap, 0, texture.num_levels - 1);
    high_mipmap = _clamp_u32(high_mipmap, 0, texture.num_levels - 1);
    
    if (low_mipmap == high_mipmap)
        return _texture_sample_bilinear(texture.mipmaps[low_mipmap], u, v);
    
    Material low_mipmap_material = _texture_sample_bilinear(texture.mipmaps[low_mipmap], u, v);
    Material high_mipmap_material = _texture_sample_bilinear(texture.mipmaps[high_mipmap], u, v);

    float t = lod - low_mipmap;
    Material pixel = { 
        .color.r = (Uint8)glm_lerp(low_mipmap_material.color.r, high_mipmap_material.color.r, t),
        .color.g = (Uint8)glm_lerp(low_mipmap_material.color.g, high_mipmap_material.color.g, t),
        .color.b = (Uint8)glm_lerp(low_mipmap_material.color.b, high_mipmap_material.color.b, t),
        .color.a = (Uint8)glm_lerp(low_mipmap_material.color.a, high_mipmap_material.color.a, t),
        .normal[0] = glm_lerp(low_mipmap_material.normal[0], high_mipmap_material.normal[0], t),
        .normal[1] = glm_lerp(low_mipmap_material.normal[1], high_mipmap_material.normal[1], t),
        .normal[2] = glm_lerp(low_mipmap_material.normal[2], high_mipmap_material.normal[2], t),
    };
    glm_vec3_normalize(pixel.normal);

    return pixel;
}

static inline Uint32 _clamp_u32(Uint32 value, Uint32 min_value, Uint32 max_value)
{
    if (value < min_value) return min_value;
    else if (value > max_value) return max_value;
    return value;
}

// THE UNIFIED BLOCK EXECUTION LOOP ENTRYPOINT
ColorSIMD texture_sample_trilinear_SIMD(int x, int y, int screen_width, float* depth_buffer, uint32_t* frame_buffer,
    __m256i int_mask, __m256 z_vec,
    __m256 u_vec, __m256 v_vec, TiledTexture* texture)
{
    // --- STAGE 1: PARALLEL QUAD DERIVATIVES & ANISOTROPIC FOOTPRINT ---
    // Scale normalized values up to Base Mip level dimensions
    float base_w = (float)texture->mipmaps[0].width;
    float base_h = (float)texture->mipmaps[0].height;

    __m256 u_frac = _mm256_sub_ps(u_vec, _mm256_floor_ps(u_vec));
    __m256 v_frac = _mm256_sub_ps(v_vec, _mm256_floor_ps(v_vec));

    __m256 u_tex = _mm256_mul_ps(u_frac, _mm256_set1_ps(base_w));
    __m256 v_tex = _mm256_mul_ps(v_frac, _mm256_set1_ps(base_h));

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

    // Multiply the normalized footprint bounds by the texture dimensions to map to texel-space delta footprints
    __m256 rho = _mm256_max_ps(_mm256_max_ps(dudx, dvdx), _mm256_max_ps(dudy, dvdy));

    // --- STAGE 2: LOG2 IEEE-754 BIT EXTRAPOLATION ---
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

    // --- STAGE 3: SCALED COORDINATE DISPATCH FOR TRILINEAR LAYERS ---
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
    __m128i lo = _mm256_castsi256_si128(mip_N);
    __m128i hi = _mm256_extracti128_si256(mip_N, 1);
    __m128i m = _mm_min_epi32(lo, hi);
    m = _mm_min_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(2, 3, 0, 1)));
    m = _mm_min_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(1, 0, 3, 2)));
    int current_mip_idx = _mm_extract_epi32(m, 0);

    if (current_mip_idx < 0) current_mip_idx = 0;
    if (current_mip_idx > texture->num_levels - 1) current_mip_idx = texture->num_levels - 1;
    int next_mip_idx = (current_mip_idx < texture->num_levels - 1) ? current_mip_idx + 1 : current_mip_idx;

    // --- STAGE 4: after current_mip_idx is calculated, calculate gamma ---
    __m256 base_level = _mm256_set1_ps((float)current_mip_idx);
    __m256 gamma = _mm256_sub_ps(lod, base_level);
    gamma = _mm256_max_ps(_mm256_min_ps(gamma, _mm256_set1_ps(1.0f)), _mm256_setzero_ps());

    // --- STAGE 5: bilinear sampling over current_mip_idx and next_mip_idx ---
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

    _sample_mip_level_soa_SIMD(texture->mipmaps[current_mip_idx].pixels, texture->mipmaps[current_mip_idx].width_in_tiles, u0_N, v0_N, mask_w_N, mask_h_N, int_mask, alpha_N, beta_N, &mipN_R, &mipN_G, &mipN_B, &mipN_A);
    _sample_mip_level_soa_SIMD(texture->mipmaps[next_mip_idx].pixels, texture->mipmaps[next_mip_idx].width_in_tiles, u0_Next, v0_Next, mask_w_Next, mask_h_Next, int_mask, alpha_Next, beta_Next, &mipNext_R, &mipNext_G, &mipNext_B, &mipNext_A);

    // --- STAGE 6: FINAL CROSS MIPMAPS BILINEAR BLENDING ---
    return (ColorSIMD) {
        .r = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_R, mipN_R), mipN_R),
            .g = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_G, mipN_G), mipN_G),
            .b = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_B, mipN_B), mipN_B),
            .a = _mm256_fmadd_ps(gamma, _mm256_sub_ps(mipNext_A, mipN_A), mipN_A)
    };
}

// Evaluates texture filtering for a single mipmap pointer target
static inline void _sample_mip_level_soa_SIMD(const Color* mip_ptr, Uint32 width_in_tiles, __m256i u0, __m256i v0,
    __m256i tex_w_mask, __m256i tex_h_mask, __m256i int_mask,
    __m256 alpha, __m256 beta,
    __m256* out_R, __m256* out_G, __m256* out_B, __m256* out_A)
{
    // -------------------------------------------------------------------------
   // 1. GL_REPEAT wrapping
   // -------------------------------------------------------------------------
    __m256i u0_w = _mm256_and_si256(u0, tex_w_mask);
    __m256i v0_w = _mm256_and_si256(v0, tex_h_mask);
    __m256i u1_w = _mm256_and_si256(_mm256_add_epi32(u0, _mm256_set1_epi32(1)), tex_w_mask);
    __m256i v1_w = _mm256_and_si256(_mm256_add_epi32(v0, _mm256_set1_epi32(1)), tex_h_mask);

    // -------------------------------------------------------------------------
    // 2. Split each coordinate into:
    //
    //     tile coordinate : coordinate / 8
    //     local coordinate: coordinate % 8
    //
    // The texture is ROW-MAJOR tiled, with Morton ordering only inside
    // each 8x8 tile.
    // -------------------------------------------------------------------------

    __m256i tile_x0 = _mm256_srli_epi32(u0_w, 3);
    __m256i tile_x1 = _mm256_srli_epi32(u1_w, 3);
    __m256i tile_y0 = _mm256_srli_epi32(v0_w, 3);
    __m256i tile_y1 = _mm256_srli_epi32(v1_w, 3);

    __m256i local_x0 = _mm256_and_si256(u0_w, _mm256_set1_epi32(7));
    __m256i local_x1 = _mm256_and_si256(u1_w, _mm256_set1_epi32(7));
    __m256i local_y0 = _mm256_and_si256(v0_w, _mm256_set1_epi32(7));
    __m256i local_y1 = _mm256_and_si256(v1_w, _mm256_set1_epi32(7));

    // -------------------------------------------------------------------------
    // 3. Row-major tile addresses
    //
    //     tile_index = tile_y * width_in_tiles + tile_x
    // -------------------------------------------------------------------------

    __m256i tiles_per_row = _mm256_set1_epi32(width_in_tiles);

    __m256i tile_index_TL = _mm256_add_epi32(_mm256_mullo_epi32(tile_y0, tiles_per_row), tile_x0);
    __m256i tile_index_TR = _mm256_add_epi32(_mm256_mullo_epi32(tile_y0, tiles_per_row), tile_x1);
    __m256i tile_index_BL = _mm256_add_epi32(_mm256_mullo_epi32(tile_y1, tiles_per_row), tile_x0);
    __m256i tile_index_BR = _mm256_add_epi32(_mm256_mullo_epi32(tile_y1, tiles_per_row), tile_x1);

    // -------------------------------------------------------------------------
    // 4. Morton order INSIDE each 8x8 tile
    //
    // local coordinates are only 0..7, so your existing bit_interleave()
    // produces exactly the required 3-bit Morton spreading.
    // -------------------------------------------------------------------------

    __m256i x_TL = _bit_interleave_SIMD(local_x0);
    __m256i x_TR = _bit_interleave_SIMD(local_x1);
    __m256i y_TL = _mm256_slli_epi32(_bit_interleave_SIMD(local_y0), 1);
    __m256i y_BL = _mm256_slli_epi32(_bit_interleave_SIMD(local_y1), 1);

    __m256i local_morton_TL = _mm256_or_si256(x_TL, y_TL);
    __m256i local_morton_TR = _mm256_or_si256(x_TR, y_TL);
    __m256i local_morton_BL = _mm256_or_si256(x_TL, y_BL);
    __m256i local_morton_BR = _mm256_or_si256(x_TR, y_BL);

    // -------------------------------------------------------------------------
    // 5. Convert tile index + local Morton index into the final pixel address
    //
    // Each 8x8 tile contains exactly 64 Color elements.
    // -------------------------------------------------------------------------

    __m256i tile_pixel_count = _mm256_set1_epi32(64);
    __m256i morton_TL = _mm256_add_epi32(_mm256_mullo_epi32(tile_index_TL, tile_pixel_count), local_morton_TL);
    __m256i morton_TR = _mm256_add_epi32(_mm256_mullo_epi32(tile_index_TR, tile_pixel_count), local_morton_TR);
    __m256i morton_BL = _mm256_add_epi32(_mm256_mullo_epi32(tile_index_BL, tile_pixel_count), local_morton_BL);
    __m256i morton_BR = _mm256_add_epi32(_mm256_mullo_epi32(tile_index_BR, tile_pixel_count), local_morton_BR);

    // -------------------------------------------------------------------------
    // 6. Gather the four bilinear pixels
    // -------------------------------------------------------------------------

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

    _unpack_rgba_to_float_soa_SIMD(packed_TL, &tl_R, &tl_G, &tl_B, &tl_A);
    _unpack_rgba_to_float_soa_SIMD(packed_TR, &tr_R, &tr_G, &tr_B, &tr_A);
    _unpack_rgba_to_float_soa_SIMD(packed_BL, &bl_R, &bl_G, &bl_B, &bl_A);
    _unpack_rgba_to_float_soa_SIMD(packed_BR, &br_R, &br_G, &br_B, &br_A);

    // Run parallel linear filters for all 4 channels
    *out_R = _bilinear_filter_channel_SIMD(tl_R, tr_R, bl_R, br_R, alpha, beta);
    *out_G = _bilinear_filter_channel_SIMD(tl_G, tr_G, bl_G, br_G, alpha, beta);
    *out_B = _bilinear_filter_channel_SIMD(tl_B, tr_B, bl_B, br_B, alpha, beta);
    *out_A = _bilinear_filter_channel_SIMD(tl_A, tr_A, bl_A, br_A, alpha, beta);
}

// Single-Channel Parallel Bilinear Interpolation using Fused Multiply-Add
static inline __m256 _bilinear_filter_channel_SIMD(__m256 tl, __m256 tr, __m256 bl, __m256 br, __m256 alpha, __m256 beta) {
    __m256 top = _mm256_fmadd_ps(alpha, _mm256_sub_ps(tr, tl), tl);
    __m256 bot = _mm256_fmadd_ps(alpha, _mm256_sub_ps(br, bl), bl);
    return _mm256_fmadd_ps(beta, _mm256_sub_ps(bot, top), top);
}

// Unpack raw RRGGBBAA 32-bit integer blobs into 4 separate float registers (SoA format)
static inline void _unpack_rgba_to_float_soa_SIMD(__m256i packed_rgba, __m256* r, __m256* g, __m256* b, __m256* a) {
    __m256i mask_R = _mm256_setr_epi8(0, -1, -1, -1, 4, -1, -1, -1, 8, -1, -1, -1, 12, -1, -1, -1,
        0, -1, -1, -1, 4, -1, -1, -1, 8, -1, -1, -1, 12, -1, -1, -1);
    __m256i mask_G = _mm256_setr_epi8(1, -1, -1, -1, 5, -1, -1, -1, 9, -1, -1, -1, 13, -1, -1, -1,
        1, -1, -1, -1, 5, -1, -1, -1, 9, -1, -1, -1, 13, -1, -1, -1);
    __m256i mask_B = _mm256_setr_epi8(2, -1, -1, -1, 6, -1, -1, -1, 10, -1, -1, -1, 14, -1, -1, -1,
        2, -1, -1, -1, 6, -1, -1, -1, 10, -1, -1, -1, 14, -1, -1, -1);
    __m256i mask_A = _mm256_setr_epi8(3, -1, -1, -1, 7, -1, -1, -1, 11, -1, -1, -1, 15, -1, -1, -1,
        3, -1, -1, -1, 7, -1, -1, -1, 11, -1, -1, -1, 15, -1, -1, -1);

    *r = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_R));
    *g = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_G));
    *b = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_B));
    *a = _mm256_cvtepi32_ps(_mm256_shuffle_epi8(packed_rgba, mask_A));
}

// Simulated PDEP/Interleave bit math across 8 lanes: xxxxxxxxABCDEFGH -> 0A0B0C0D0E0F0G0H
static inline __m256i _bit_interleave_SIMD(__m256i x) {
    x = _mm256_and_si256(x, _mm256_set1_epi32(0x0000FFFF));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 8)), _mm256_set1_epi32(0x00FF00FF));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 4)), _mm256_set1_epi32(0x0F0F0F0F));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 2)), _mm256_set1_epi32(0x33333333));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 1)), _mm256_set1_epi32(0x55555555));
    return x;
}
