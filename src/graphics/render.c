#include "graphics/render.h"
#include <stdlib.h>
#include <float.h>

static uint32_t *g_framebuffer = NULL;
static float    *g_zbuffer     = NULL;

void render_set_framebuffer(uint32_t *buffer) {
    g_framebuffer = buffer;
}

void render_set_zbuffer(float *buffer) {
    g_zbuffer = buffer;
}

void render_clear_zbuffer(void) {
    if (g_zbuffer) {
        for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++) {
            g_zbuffer[i] = FLT_MAX;
        }
    }
}

void render_set_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < RENDER_WIDTH && y >= 0 && y < RENDER_HEIGHT) {
        g_framebuffer[y * RENDER_WIDTH + x] = color;
    }
}

static void render_set_pixel_z(int x, int y, float z, uint32_t color) {
    if (x >= 0 && x < RENDER_WIDTH && y >= 0 && y < RENDER_HEIGHT) {
        int idx = y * RENDER_WIDTH + x;
        if (z < g_zbuffer[idx]) {
            g_zbuffer[idx] = z;
            g_framebuffer[idx] = color;
        }
    }
}

// Bresenham's line algorithm
void render_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        render_set_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void render_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    render_draw_line(x0, y0, x1, y1, color);
    render_draw_line(x1, y1, x2, y2, color);
    render_draw_line(x2, y2, x0, y0, color);
}

static void swap_int(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

// Horizontal scanline
static void draw_scanline(int y, int x_start, int x_end, uint32_t color) {
    if (x_start > x_end) {
        swap_int(&x_start, &x_end);
    }
    for (int x = x_start; x <= x_end; x++) {
        render_set_pixel(x, y, color);
    }
}

// Flat-bottom triangle: v0 at top, v1 and v2 at bottom (same y)
static void fill_flat_bottom(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    float inv_slope1 = (float)(x1 - x0) / (float)(y1 - y0);
    float inv_slope2 = (float)(x2 - x0) / (float)(y2 - y0);

    float cur_x1 = (float)x0;
    float cur_x2 = (float)x0;

    for (int y = y0; y <= y1; y++) {
        draw_scanline(y, (int)cur_x1, (int)cur_x2, color);
        cur_x1 += inv_slope1;
        cur_x2 += inv_slope2;
    }
}

// Flat-top triangle: v0 and v1 at top (same y), v2 at bottom
static void fill_flat_top(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    float inv_slope1 = (float)(x2 - x0) / (float)(y2 - y0);
    float inv_slope2 = (float)(x2 - x1) / (float)(y2 - y1);

    float cur_x1 = (float)x2;
    float cur_x2 = (float)x2;

    for (int y = y2; y >= y0; y--) {
        draw_scanline(y, (int)cur_x1, (int)cur_x2, color);
        cur_x1 -= inv_slope1;
        cur_x2 -= inv_slope2;
    }
}

void render_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    // Sort vertices by y-coordinate (y0 <= y1 <= y2)
    if (y0 > y1) { swap_int(&x0, &x1); swap_int(&y0, &y1); }
    if (y1 > y2) { swap_int(&x1, &x2); swap_int(&y1, &y2); }
    if (y0 > y1) { swap_int(&x0, &x1); swap_int(&y0, &y1); }

    // Check for trivial cases
    if (y1 == y2) {
        fill_flat_bottom(x0, y0, x1, y1, x2, y2, color);
    } else if (y0 == y1) {
        fill_flat_top(x0, y0, x1, y1, x2, y2, color);
    } else {
        // Split into flat-bottom and flat-top triangles
        int x3 = x0 + (int)(((float)(y1 - y0) / (float)(y2 - y0)) * (float)(x2 - x0));
        int y3 = y1;

        fill_flat_bottom(x0, y0, x1, y1, x3, y3, color);
        fill_flat_top(x1, y1, x3, y3, x2, y2, color);
    }
}

static int min3(int a, int b, int c) {
    int m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

static int max3(int a, int b, int c) {
    int m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

// Edge function for barycentric coordinates
static float edge_func(int ax, int ay, int bx, int by, int px, int py) {
    return (float)((px - ax) * (by - ay) - (py - ay) * (bx - ax));
}

void render_fill_triangle_z(
    int x0, int y0, float z0,
    int x1, int y1, float z1,
    int x2, int y2, float z2,
    uint32_t color
) {
    // Bounding box
    int min_x = min3(x0, x1, x2);
    int max_x = max3(x0, x1, x2);
    int min_y = min3(y0, y1, y2);
    int max_y = max3(y0, y1, y2);

    // Clip to screen
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= RENDER_WIDTH)  max_x = RENDER_WIDTH - 1;
    if (max_y >= RENDER_HEIGHT) max_y = RENDER_HEIGHT - 1;

    float area = edge_func(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float w0 = edge_func(x1, y1, x2, y2, x, y);
            float w1 = edge_func(x2, y2, x0, y0, x, y);
            float w2 = edge_func(x0, y0, x1, y1, x, y);

            // Check if point is inside triangle
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                w0 /= area;
                w1 /= area;
                w2 /= area;

                float z = w0 * z0 + w1 * z1 + w2 * z2;
                render_set_pixel_z(x, y, z, color);
            }
        }
    }
}

