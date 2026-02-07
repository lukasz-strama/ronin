#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>

typedef struct {
    uint32_t *pixels;   // ARGB format (0xAARRGGBB)
    int       width;
    int       height;
} Texture;

int      texture_load(Texture *tex, const char *path);
void     texture_free(Texture *tex);
uint32_t texture_sample(const Texture *tex, float u, float v);

// Create a procedural checkerboard texture
int texture_create_checker(Texture *tex, int size, int tile_size, 
                           uint32_t color1, uint32_t color2);

#endif
