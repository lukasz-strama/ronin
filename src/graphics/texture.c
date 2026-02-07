#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "graphics/texture.h"
#include "core/log.h"
#include <stdlib.h>

int texture_load(Texture *tex, const char *path) {
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 4);

    if (!data) {
        LOG_ERROR("Failed to load texture: %s", path);
        return 1;
    }

    tex->width  = width;
    tex->height = height;
    tex->pixels = (uint32_t *)malloc(width * height * sizeof(uint32_t));

    if (!tex->pixels) {
        LOG_ERROR("Failed to allocate texture memory");
        stbi_image_free(data);
        return 1;
    }

    // Convert RGBA to ARGB (stb loads as RGBA)
    for (int i = 0; i < width * height; i++) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        unsigned char a = data[i * 4 + 3];
        tex->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }

    stbi_image_free(data);

    LOG_INFO("Loaded texture: %s (%dx%d)", path, width, height);
    return 0;
}

void texture_free(Texture *tex) {
    if (tex->pixels) {
        free(tex->pixels);
        tex->pixels = NULL;
    }
    tex->width  = 0;
    tex->height = 0;
}

uint32_t texture_sample(const Texture *tex, float u, float v) {
    // Wrap UV coordinates
    u = u - (int)u;
    v = v - (int)v;
    if (u < 0) u += 1.0f;
    if (v < 0) v += 1.0f;

    int x = (int)(u * (tex->width  - 1));
    int y = (int)(v * (tex->height - 1));

    // Clamp to valid range
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= tex->width)  x = tex->width  - 1;
    if (y >= tex->height) y = tex->height - 1;

    return tex->pixels[y * tex->width + x];
}

int texture_create_checker(Texture *tex, int size, int tile_size,
                           uint32_t color1, uint32_t color2) {
    tex->width  = size;
    tex->height = size;
    tex->pixels = (uint32_t *)malloc(size * size * sizeof(uint32_t));

    if (!tex->pixels) {
        LOG_ERROR("Failed to allocate checker texture memory");
        return 1;
    }

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int tx = x / tile_size;
            int ty = y / tile_size;
            uint32_t color = ((tx + ty) % 2 == 0) ? color1 : color2;
            tex->pixels[y * size + x] = color;
        }
    }

    LOG_INFO("Created checker texture (%dx%d, tile=%d)", size, size, tile_size);
    return 0;
}

