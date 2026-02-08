#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "graphics/texture.h"
#include "math/math.h"

// Must match main.c
#define RENDER_WIDTH 320
#define RENDER_HEIGHT 240

void render_set_framebuffer(uint32_t *buffer);
void render_set_zbuffer(float *buffer);
void render_clear_zbuffer(void);

// Pixel operations
void render_set_pixel(int x, int y, uint32_t color);

// Fog & Skybox
void render_set_fog(bool enabled, float start, float end, uint32_t color);
void render_get_fog(bool *enabled, float *start, float *end, uint32_t *color);
void render_set_skybox(uint32_t top_color, uint32_t bottom_color);
void render_get_skybox(uint32_t *top_color, uint32_t *bottom_color);
void render_clear_gradient(void);

// Drawing primitives
void render_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void render_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);
void render_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);

// Depth-aware triangle fill (with per-vertex Z and W values)
void render_fill_triangle_z(
    int x0, int y0, float z0, float w0,
    int x1, int y1, float z1, float w1,
    int x2, int y2, float z2, float w2,
    uint32_t color);

// Textured triangle with perspective-correct UV interpolation
void render_fill_triangle_textured(
    int x0, int y0, float z0, float u0, float v0, float w0,
    int x1, int y1, float z1, float u1, float v1, float w1,
    int x2, int y2, float z2, float u2, float v2, float w2,
    const Texture *tex, float light_intensity);

// Debug: draw wireframe AABB projected through view-projection matrix
void render_draw_aabb(AABB box, Mat4 vp, uint32_t color);

// Draw a 3D line segment projected through view-projection matrix
void render_draw_3d_line(Vec3 start, Vec3 end, Mat4 vp, uint32_t color);

// Shared vertex projection (clip-space -> screen-space)
typedef struct
{
    Vec2 screen;
    float z;
} ProjectedVertex;

ProjectedVertex render_project_vertex(Vec4 v);
uint32_t render_shade_color(uint32_t base_color, float intensity);

#endif
