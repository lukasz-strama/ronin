#include "graphics/render.h"
#include "core/log.h"
#include "core/threads.h"
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>

static uint32_t *g_framebuffer = NULL;
static float *g_zbuffer = NULL;

void render_set_framebuffer(uint32_t *buffer)
{
    g_framebuffer = buffer;
    LOG_INFO("Framebuffer initialized (%dx%d)", RENDER_WIDTH, RENDER_HEIGHT);
}

void render_set_zbuffer(float *buffer)
{
    g_zbuffer = buffer;
    LOG_INFO("Z-buffer initialized");
}

static bool g_fog_enabled = false;
static float g_fog_start = 10.0f;
static float g_fog_end = 50.0f;
static uint32_t g_fog_color = 0xFF818181; // Grey

static uint32_t g_skybox_top = 0xFF0000AA;    // Deep blue
static uint32_t g_skybox_bottom = 0xFF808080; // Grey

#define MAX_RENDER_CMDS 65536

typedef struct
{
    int x0, y0, x1, y1, x2, y2;
    float z0, z1, z2;
    float w0, w1, w2;
    float u0, v0, u1, v1, u2, v2;
    const Texture *tex;
    float light;
    uint32_t color;
    bool textured;
} RenderCmd;

static RenderCmd g_cmd_buffer[MAX_RENDER_CMDS];
static int g_cmd_count = 0;
static bool g_threaded = false;

void render_set_fog(bool enabled, float start, float end, uint32_t color)
{
    g_fog_enabled = enabled;
    g_fog_start = start;
    g_fog_end = end;
    g_fog_color = color;
}

void render_get_fog(bool *enabled, float *start, float *end, uint32_t *color)
{
    if (enabled)
        *enabled = g_fog_enabled;
    if (start)
        *start = g_fog_start;
    if (end)
        *end = g_fog_end;
    if (color)
        *color = g_fog_color;
}

void render_set_skybox(uint32_t top, uint32_t bottom)
{
    g_skybox_top = top;
    g_skybox_bottom = bottom;
}

void render_get_skybox(uint32_t *top, uint32_t *bottom)
{
    if (top)
        *top = g_skybox_top;
    if (bottom)
        *bottom = g_skybox_bottom;
}

static uint32_t blend_colors(uint32_t c1, uint32_t c2, float t)
{
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;

    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = c1 & 0xFF;

    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = c2 & 0xFF;

    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void render_clear_gradient(void)
{
    for (int y = 0; y < RENDER_HEIGHT; y++)
    {
        float t = (float)y / (float)RENDER_HEIGHT;
        uint32_t color = blend_colors(g_skybox_top, g_skybox_bottom, t);
        for (int x = 0; x < RENDER_WIDTH; x++)
        {
            g_framebuffer[y * RENDER_WIDTH + x] = color;
        }
    }
}

void render_clear_zbuffer(void)
{
    if (g_zbuffer)
    {
        for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++)
        {
            g_zbuffer[i] = FLT_MAX;
        }
    }
}

void render_set_pixel(int x, int y, uint32_t color)
{
    if (x >= 0 && x < RENDER_WIDTH && y >= 0 && y < RENDER_HEIGHT)
    {
        g_framebuffer[y * RENDER_WIDTH + x] = color;
    }
}

// Bresenham's line algorithm
void render_draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        render_set_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void render_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    render_draw_line(x0, y0, x1, y1, color);
    render_draw_line(x1, y1, x2, y2, color);
    render_draw_line(x2, y2, x0, y0, color);
}

static void swap_int(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static void draw_scanline(int y, int x_start, int x_end, uint32_t color)
{
    if (x_start > x_end)
    {
        swap_int(&x_start, &x_end);
    }
    for (int x = x_start; x <= x_end; x++)
    {
        render_set_pixel(x, y, color);
    }
}

static void fill_flat_bottom(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    float inv_slope1 = (float)(x1 - x0) / (float)(y1 - y0);
    float inv_slope2 = (float)(x2 - x0) / (float)(y2 - y0);

    float cur_x1 = (float)x0;
    float cur_x2 = (float)x0;

    for (int y = y0; y <= y1; y++)
    {
        draw_scanline(y, (int)cur_x1, (int)cur_x2, color);
        cur_x1 += inv_slope1;
        cur_x2 += inv_slope2;
    }
}

// Flat-top triangle: v0 and v1 at top (same y), v2 at bottom
static void fill_flat_top(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    float inv_slope1 = (float)(x2 - x0) / (float)(y2 - y0);
    float inv_slope2 = (float)(x2 - x1) / (float)(y2 - y1);

    float cur_x1 = (float)x2;
    float cur_x2 = (float)x2;

    for (int y = y2; y >= y0; y--)
    {
        draw_scanline(y, (int)cur_x1, (int)cur_x2, color);
        cur_x1 -= inv_slope1;
        cur_x2 -= inv_slope2;
    }
}

void render_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    // Sort vertices by y-coordinate (y0 <= y1 <= y2)
    if (y0 > y1)
    {
        swap_int(&x0, &x1);
        swap_int(&y0, &y1);
    }
    if (y1 > y2)
    {
        swap_int(&x1, &x2);
        swap_int(&y1, &y2);
    }
    if (y0 > y1)
    {
        swap_int(&x0, &x1);
        swap_int(&y0, &y1);
    }

    // Check for trivial cases
    if (y1 == y2)
    {
        fill_flat_bottom(x0, y0, x1, y1, x2, y2, color);
    }
    else if (y0 == y1)
    {
        fill_flat_top(x0, y0, x1, y1, x2, y2, color);
    }
    else
    {
        // Split into flat-bottom and flat-top triangles
        int x3 = x0 + (int)(((float)(y1 - y0) / (float)(y2 - y0)) * (float)(x2 - x0));
        int y3 = y1;

        fill_flat_bottom(x0, y0, x1, y1, x3, y3, color);
        fill_flat_top(x1, y1, x3, y3, x2, y2, color);
    }
}

static int min3(int a, int b, int c)
{
    int m = a;
    if (b < m)
        m = b;
    if (c < m)
        m = c;
    return m;
}

static int max3(int a, int b, int c)
{
    int m = a;
    if (b > m)
        m = b;
    if (c > m)
        m = c;
    return m;
}

static float edge_func(int ax, int ay, int bx, int by, int px, int py)
{
    return (float)((px - ax) * (by - ay) - (py - ay) * (bx - ax));
}

static uint32_t apply_fog(uint32_t color, float w)
{
    if (!g_fog_enabled)
        return color;

    float factor = (w - g_fog_start) / (g_fog_end - g_fog_start);
    return blend_colors(color, g_fog_color, factor);
}

void render_fill_triangle_z(
    int x0, int y0, float z0, float w0,
    int x1, int y1, float z1, float w1,
    int x2, int y2, float z2, float w2,
    uint32_t color)
{
    if (g_threaded && threadpool_is_active())
    {
        if (g_cmd_count < MAX_RENDER_CMDS)
        {
            RenderCmd *cmd = &g_cmd_buffer[g_cmd_count++];
            cmd->x0 = x0;
            cmd->y0 = y0;
            cmd->x1 = x1;
            cmd->y1 = y1;
            cmd->x2 = x2;
            cmd->y2 = y2;
            cmd->z0 = z0;
            cmd->z1 = z1;
            cmd->z2 = z2;
            cmd->w0 = w0;
            cmd->w1 = w1;
            cmd->w2 = w2;
            cmd->color = color;
            cmd->textured = false;
        }
        return;
    }

    // Bounding box
    int min_x = min3(x0, x1, x2);
    int max_x = max3(x0, x1, x2);
    int min_y = min3(y0, y1, y2);
    int max_y = max3(y0, y1, y2);

    // Clip to screen
    if (min_x < 0)
        min_x = 0;
    if (min_y < 0)
        min_y = 0;
    if (max_x >= RENDER_WIDTH)
        max_x = RENDER_WIDTH - 1;
    if (max_y >= RENDER_HEIGHT)
        max_y = RENDER_HEIGHT - 1;

    float area = edge_func(x0, y0, x1, y1, x2, y2);
    if (area == 0)
        return;

    float inv_w0 = 1.0f / w0;
    float inv_w1 = 1.0f / w1;
    float inv_w2 = 1.0f / w2;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            float bary0 = edge_func(x1, y1, x2, y2, x, y);
            float bary1 = edge_func(x2, y2, x0, y0, x, y);
            float bary2 = edge_func(x0, y0, x1, y1, x, y);

            if ((bary0 >= 0 && bary1 >= 0 && bary2 >= 0) ||
                (bary0 <= 0 && bary1 <= 0 && bary2 <= 0))
            {
                bary0 /= area;
                bary1 /= area;
                bary2 /= area;

                // Early Z-rejection
                float z = bary0 * z0 + bary1 * z1 + bary2 * z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                // Fog calculation
                float interp_inv_w = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
                float w = 1.0f / interp_inv_w;
                uint32_t final_color = apply_fog(color, w);

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = final_color;
            }
        }
    }
}

// --- SIMD Implementation ---
#ifdef USE_SIMD
#include <immintrin.h>

static void render_fill_triangle_simd(
    int x0, int y0, float z0, float u0, float v0, float w0_clip,
    int x1, int y1, float z1, float u1, float v1, float w1_clip,
    int x2, int y2, float z2, float u2, float v2, float w2_clip,
    const Texture *tex, float light_intensity)
{
    int min_x = min3(x0, x1, x2);
    int max_x = max3(x0, x1, x2);
    int min_y = min3(y0, y1, y2);
    int max_y = max3(y0, y1, y2);

    if (min_x < 0)
        min_x = 0;
    if (min_y < 0)
        min_y = 0;
    if (max_x >= RENDER_WIDTH)
        max_x = RENDER_WIDTH - 1;
    if (max_y >= RENDER_HEIGHT)
        max_y = RENDER_HEIGHT - 1;

    float area = edge_func(x0, y0, x1, y1, x2, y2);
    if (area == 0)
        return;
    float inv_area = 1.0f / area;

    float inv_w0 = 1.0f / w0_clip;
    float inv_w1 = 1.0f / w1_clip;
    float inv_w2 = 1.0f / w2_clip;

    float u0_over_w = u0 * inv_w0;
    float v0_over_w = v0 * inv_w0;
    float u1_over_w = u1 * inv_w1;
    float v1_over_w = v1 * inv_w1;
    float u2_over_w = u2 * inv_w2;
    float v2_over_w = v2 * inv_w2;

    __m128 v_inv_area = _mm_set1_ps(inv_area);

    __m128 v_z0 = _mm_set1_ps(z0);
    __m128 v_z1 = _mm_set1_ps(z1);
    __m128 v_z2 = _mm_set1_ps(z2);

    __m128 v_u0w = _mm_set1_ps(u0_over_w);
    __m128 v_u1w = _mm_set1_ps(u1_over_w);
    __m128 v_u2w = _mm_set1_ps(u2_over_w);

    __m128 v_v0w = _mm_set1_ps(v0_over_w);
    __m128 v_v1w = _mm_set1_ps(v1_over_w);
    __m128 v_v2w = _mm_set1_ps(v2_over_w);

    __m128 v_inv_w0 = _mm_set1_ps(inv_w0);
    __m128 v_inv_w1 = _mm_set1_ps(inv_w1);
    __m128 v_inv_w2 = _mm_set1_ps(inv_w2);

    __m128 v_zeros = _mm_setzero_ps();
    __m128 v_inc_x = _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f);

    for (int y = min_y; y <= max_y; y++)
    {
        float row_bary0 = edge_func(x1, y1, x2, y2, min_x, y);
        float row_bary1 = edge_func(x2, y2, x0, y0, min_x, y);
        float row_bary2 = edge_func(x0, y0, x1, y1, min_x, y);

        int x = min_x;
        for (; x <= max_x - 3; x += 4)
        {
            __m128 v_bary0 = _mm_add_ps(_mm_set1_ps(row_bary0), _mm_mul_ps(v_inc_x, _mm_set1_ps(y2 - y1)));
            __m128 v_bary1 = _mm_add_ps(_mm_set1_ps(row_bary1), _mm_mul_ps(v_inc_x, _mm_set1_ps(y0 - y2)));
            __m128 v_bary2 = _mm_add_ps(_mm_set1_ps(row_bary2), _mm_mul_ps(v_inc_x, _mm_set1_ps(y1 - y0)));

            row_bary0 += 4.0f * (y2 - y1);
            row_bary1 += 4.0f * (y0 - y2);
            row_bary2 += 4.0f * (y1 - y0);

            __m128 mask0 = _mm_cmpge_ps(v_bary0, v_zeros);
            __m128 mask1 = _mm_cmpge_ps(v_bary1, v_zeros);
            __m128 mask2 = _mm_cmpge_ps(v_bary2, v_zeros);

            __m128 mask_pos = _mm_and_ps(_mm_and_ps(mask0, mask1), mask2);

            __m128 mask0_neg = _mm_cmple_ps(v_bary0, v_zeros);
            __m128 mask1_neg = _mm_cmple_ps(v_bary1, v_zeros);
            __m128 mask2_neg = _mm_cmple_ps(v_bary2, v_zeros);
            __m128 mask_neg = _mm_and_ps(_mm_and_ps(mask0_neg, mask1_neg), mask2_neg);

            __m128 mask = _mm_or_ps(mask_pos, mask_neg);

            int mask_bits = _mm_movemask_ps(mask);
            if (mask_bits == 0)
                continue;

            v_bary0 = _mm_mul_ps(v_bary0, v_inv_area);
            v_bary1 = _mm_mul_ps(v_bary1, v_inv_area);
            v_bary2 = _mm_mul_ps(v_bary2, v_inv_area);

            // Z = b0*z0 + b1*z1 + b2*z2
            __m128 v_z = _mm_add_ps(
                _mm_add_ps(_mm_mul_ps(v_bary0, v_z0), _mm_mul_ps(v_bary1, v_z1)),
                _mm_mul_ps(v_bary2, v_z2));

            int idx = y * RENDER_WIDTH + x;
            __m128 v_zbuf = _mm_loadu_ps(&g_zbuffer[idx]);
            __m128 z_mask = _mm_cmplt_ps(v_z, v_zbuf);

            mask = _mm_and_ps(mask, z_mask);
            mask_bits = _mm_movemask_ps(mask);
            if (mask_bits == 0)
                continue;

            __m128 v_interp_inv_w = _mm_add_ps(
                _mm_add_ps(_mm_mul_ps(v_bary0, v_inv_w0), _mm_mul_ps(v_bary1, v_inv_w1)),
                _mm_mul_ps(v_bary2, v_inv_w2));

            __m128 v_interp_u = _mm_add_ps(
                _mm_add_ps(_mm_mul_ps(v_bary0, v_u0w), _mm_mul_ps(v_bary1, v_u1w)),
                _mm_mul_ps(v_bary2, v_u2w));

            __m128 v_interp_v = _mm_add_ps(
                _mm_add_ps(_mm_mul_ps(v_bary0, v_v0w), _mm_mul_ps(v_bary1, v_v1w)),
                _mm_mul_ps(v_bary2, v_v2w));

            __m128 v_u = _mm_div_ps(v_interp_u, v_interp_inv_w);
            __m128 v_v = _mm_div_ps(v_interp_v, v_interp_inv_w);
            __m128 v_w = _mm_rcp_ps(v_interp_inv_w);

            float zs[4], us[4], vs[4], ws[4];
            _mm_storeu_ps(zs, v_z);
            _mm_storeu_ps(us, v_u);
            _mm_storeu_ps(vs, v_v);
            _mm_storeu_ps(ws, v_w);

            for (int i = 0; i < 4; i++)
            {
                if ((mask_bits >> i) & 1)
                {
                    uint32_t tex_color = texture_sample(tex, us[i], vs[i]);

                    uint8_t r = (uint8_t)(((tex_color >> 16) & 0xFF) * light_intensity);
                    uint8_t g = (uint8_t)(((tex_color >> 8) & 0xFF) * light_intensity);
                    uint8_t b = (uint8_t)((tex_color & 0xFF) * light_intensity);

                    uint32_t lit_color = 0xFF000000 | (r << 16) | (g << 8) | b;
                    uint32_t final_color = apply_fog(lit_color, ws[i]);

                    g_zbuffer[idx + i] = zs[i];
                    g_framebuffer[idx + i] = final_color;
                }
            }
        }

        for (; x <= max_x; x++)
        {
            float bary0 = edge_func(x1, y1, x2, y2, x, y);
            float bary1 = edge_func(x2, y2, x0, y0, x, y);
            float bary2 = edge_func(x0, y0, x1, y1, x, y);

            if ((bary0 >= 0 && bary1 >= 0 && bary2 >= 0) ||
                (bary0 <= 0 && bary1 <= 0 && bary2 <= 0))
            {
                bary0 /= area;
                bary1 /= area;
                bary2 /= area;

                float z = bary0 * z0 + bary1 * z1 + bary2 * z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                float interp_inv_w = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
                float interp_u_over_w = bary0 * u0_over_w + bary1 * u1_over_w + bary2 * u2_over_w;
                float interp_v_over_w = bary0 * v0_over_w + bary1 * v1_over_w + bary2 * v2_over_w;
                float u = interp_u_over_w / interp_inv_w;
                float v = interp_v_over_w / interp_inv_w;

                uint32_t tex_color = texture_sample(tex, u, v);
                uint8_t r = (uint8_t)(((tex_color >> 16) & 0xFF) * light_intensity);
                uint8_t g = (uint8_t)(((tex_color >> 8) & 0xFF) * light_intensity);
                uint8_t b = (uint8_t)((tex_color & 0xFF) * light_intensity);
                uint32_t lit_color = 0xFF000000 | (r << 16) | (g << 8) | b;
                float w = 1.0f / interp_inv_w;
                uint32_t final_color = apply_fog(lit_color, w);

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = final_color;
            }
        }
    }
}
#endif // USE_SIMD

// SIMD Flag (run-time toggle)
static bool g_simd_enabled = false;

void render_set_simd(bool enabled)
{
    g_simd_enabled = enabled;
    LOG_INFO("SIMD Rasterizer: %s", enabled ? "ON" : "OFF");
}

void render_fill_triangle_textured(
    int x0, int y0, float z0, float u0, float v0, float w0_clip,
    int x1, int y1, float z1, float u1, float v1, float w1_clip,
    int x2, int y2, float z2, float u2, float v2, float w2_clip,
    const Texture *tex, float light_intensity)
{
    if (g_threaded && threadpool_is_active())
    {
        if (g_cmd_count < MAX_RENDER_CMDS)
        {
            RenderCmd *cmd = &g_cmd_buffer[g_cmd_count++];
            cmd->x0 = x0;
            cmd->y0 = y0;
            cmd->x1 = x1;
            cmd->y1 = y1;
            cmd->x2 = x2;
            cmd->y2 = y2;
            cmd->z0 = z0;
            cmd->z1 = z1;
            cmd->z2 = z2;
            cmd->w0 = w0_clip;
            cmd->w1 = w1_clip;
            cmd->w2 = w2_clip;
            cmd->u0 = u0;
            cmd->v0 = v0;
            cmd->u1 = u1;
            cmd->v1 = v1;
            cmd->u2 = u2;
            cmd->v2 = v2;
            cmd->tex = tex;
            cmd->light = light_intensity;
            cmd->textured = true;
        }
        return;
    }

#ifdef USE_SIMD
    if (g_simd_enabled)
    {
        render_fill_triangle_simd(x0, y0, z0, u0, v0, w0_clip,
                                  x1, y1, z1, u1, v1, w1_clip,
                                  x2, y2, z2, u2, v2, w2_clip,
                                  tex, light_intensity);
        return;
    }
#endif

    int min_x = min3(x0, x1, x2);
    int max_x = max3(x0, x1, x2);

    int min_y = min3(y0, y1, y2);
    int max_y = max3(y0, y1, y2);

    if (min_x < 0)
        min_x = 0;
    if (min_y < 0)
        min_y = 0;
    if (max_x >= RENDER_WIDTH)
        max_x = RENDER_WIDTH - 1;
    if (max_y >= RENDER_HEIGHT)
        max_y = RENDER_HEIGHT - 1;

    float area = edge_func(x0, y0, x1, y1, x2, y2);
    if (area == 0)
        return;

    // Pre-compute 1/w for perspective correction
    float inv_w0 = 1.0f / w0_clip;
    float inv_w1 = 1.0f / w1_clip;
    float inv_w2 = 1.0f / w2_clip;

    // Pre-compute u/w and v/w for perspective-correct interpolation
    float u0_over_w = u0 * inv_w0;
    float v0_over_w = v0 * inv_w0;
    float u1_over_w = u1 * inv_w1;
    float v1_over_w = v1 * inv_w1;
    float u2_over_w = u2 * inv_w2;
    float v2_over_w = v2 * inv_w2;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            float bary0 = edge_func(x1, y1, x2, y2, x, y);
            float bary1 = edge_func(x2, y2, x0, y0, x, y);
            float bary2 = edge_func(x0, y0, x1, y1, x, y);

            if ((bary0 >= 0 && bary1 >= 0 && bary2 >= 0) ||
                (bary0 <= 0 && bary1 <= 0 && bary2 <= 0))
            {
                bary0 /= area;
                bary1 /= area;
                bary2 /= area;

                // Early Z-rejection
                float z = bary0 * z0 + bary1 * z1 + bary2 * z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                float interp_inv_w = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
                float interp_u_over_w = bary0 * u0_over_w + bary1 * u1_over_w + bary2 * u2_over_w;
                float interp_v_over_w = bary0 * v0_over_w + bary1 * v1_over_w + bary2 * v2_over_w;
                float u = interp_u_over_w / interp_inv_w;
                float v = interp_v_over_w / interp_inv_w;

                uint32_t tex_color = texture_sample(tex, u, v);

                uint8_t r = (uint8_t)(((tex_color >> 16) & 0xFF) * light_intensity);
                uint8_t g = (uint8_t)(((tex_color >> 8) & 0xFF) * light_intensity);
                uint8_t b = (uint8_t)((tex_color & 0xFF) * light_intensity);

                uint32_t lit_color = 0xFF000000 | (r << 16) | (g << 8) | b;

                // Fog
                float w = 1.0f / interp_inv_w;
                uint32_t final_color = apply_fog(lit_color, w);

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = final_color;
            }
        }
    }
}

ProjectedVertex render_project_vertex(Vec4 v)
{
    ProjectedVertex pv;
    float x = v.x / v.w;
    float y = v.y / v.w;
    float z = v.z / v.w;

    pv.screen.x = (x + 1.0f) * 0.5f * RENDER_WIDTH;
    pv.screen.y = (1.0f - y) * 0.5f * RENDER_HEIGHT;
    pv.z = z;
    return pv;
}

uint32_t render_shade_color(uint32_t base_color, float intensity)
{
    if (intensity < 0.1f)
        intensity = 0.1f;
    if (intensity > 1.0f)
        intensity = 1.0f;

    uint8_t r = (uint8_t)(((base_color >> 16) & 0xFF) * intensity);
    uint8_t g = (uint8_t)(((base_color >> 8) & 0xFF) * intensity);
    uint8_t b = (uint8_t)((base_color & 0xFF) * intensity);

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Clip a line segment in clip-space against the near plane (w = NEAR_W).
// Returns false if the segment is entirely behind the near plane.
static bool clip_line_near(Vec4 *a, Vec4 *b)
{
#define NEAR_W 0.1f
    bool a_in = (a->w >= NEAR_W);
    bool b_in = (b->w >= NEAR_W);

    if (a_in && b_in)
        return true;
    if (!a_in && !b_in)
        return false;

    // Interpolate: find t where w = NEAR_W along the edge
    float t = (NEAR_W - a->w) / (b->w - a->w);
    Vec4 clipped = {
        a->x + t * (b->x - a->x),
        a->y + t * (b->y - a->y),
        a->z + t * (b->z - a->z),
        a->w + t * (b->w - a->w),
    };

    if (!a_in)
        *a = clipped;
    else
        *b = clipped;
    return true;
#undef NEAR_W
}

void render_draw_aabb(AABB box, Mat4 vp, uint32_t color)
{
    Vec3 corners[8] = {
        {box.min.x, box.min.y, box.min.z},
        {box.max.x, box.min.y, box.min.z},
        {box.max.x, box.max.y, box.min.z},
        {box.min.x, box.max.y, box.min.z},
        {box.min.x, box.min.y, box.max.z},
        {box.max.x, box.min.y, box.max.z},
        {box.max.x, box.max.y, box.max.z},
        {box.min.x, box.max.y, box.max.z},
    };

    Vec4 clip_verts[8];
    for (int i = 0; i < 8; i++)
        clip_verts[i] = mat4_mul_vec4(vp, vec4_from_vec3(corners[i], 1.0f));

    int edges[12][2] = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    for (int i = 0; i < 12; i++)
    {
        Vec4 a = clip_verts[edges[i][0]];
        Vec4 b = clip_verts[edges[i][1]];

        if (!clip_line_near(&a, &b))
            continue;

        float inv_wa = 1.0f / a.w;
        float inv_wb = 1.0f / b.w;
        int x0 = (int)((a.x * inv_wa + 1.0f) * 0.5f * RENDER_WIDTH);
        int y0 = (int)((1.0f - a.y * inv_wa) * 0.5f * RENDER_HEIGHT);
        int x1 = (int)((b.x * inv_wb + 1.0f) * 0.5f * RENDER_WIDTH);
        int y1 = (int)((1.0f - b.y * inv_wb) * 0.5f * RENDER_HEIGHT);

        render_draw_line(x0, y0, x1, y1, color);
    }
}

void render_draw_3d_line(Vec3 start, Vec3 end, Mat4 vp, uint32_t color)
{
    Vec4 a = mat4_mul_vec4(vp, vec4_from_vec3(start, 1.0f));
    Vec4 b = mat4_mul_vec4(vp, vec4_from_vec3(end, 1.0f));

    if (!clip_line_near(&a, &b))
        return;

    float inv_wa = 1.0f / a.w;
    float inv_wb = 1.0f / b.w;
    int x0 = (int)((a.x * inv_wa + 1.0f) * 0.5f * RENDER_WIDTH);
    int y0 = (int)((1.0f - a.y * inv_wa) * 0.5f * RENDER_HEIGHT);
    int x1 = (int)((b.x * inv_wb + 1.0f) * 0.5f * RENDER_WIDTH);
    int y1 = (int)((1.0f - b.y * inv_wb) * 0.5f * RENDER_HEIGHT);

    render_draw_line(x0, y0, x1, y1, color);
}

static void tile_fill_z(const RenderCmd *cmd,
                        int tx, int ty, int tw, int th)
{
    int min_x = min3(cmd->x0, cmd->x1, cmd->x2);
    int max_x = max3(cmd->x0, cmd->x1, cmd->x2);
    int min_y = min3(cmd->y0, cmd->y1, cmd->y2);
    int max_y = max3(cmd->y0, cmd->y1, cmd->y2);

    if (min_x < tx)
        min_x = tx;
    if (min_y < ty)
        min_y = ty;
    if (max_x > tx + tw - 1)
        max_x = tx + tw - 1;
    if (max_y > ty + th - 1)
        max_y = ty + th - 1;

    if (min_x > max_x || min_y > max_y)
        return;

    float area = edge_func(cmd->x0, cmd->y0, cmd->x1, cmd->y1, cmd->x2, cmd->y2);
    if (area == 0)
        return;

    float inv_w0 = 1.0f / cmd->w0;
    float inv_w1 = 1.0f / cmd->w1;
    float inv_w2 = 1.0f / cmd->w2;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            float bary0 = edge_func(cmd->x1, cmd->y1, cmd->x2, cmd->y2, x, y);
            float bary1 = edge_func(cmd->x2, cmd->y2, cmd->x0, cmd->y0, x, y);
            float bary2 = edge_func(cmd->x0, cmd->y0, cmd->x1, cmd->y1, x, y);

            if ((bary0 >= 0 && bary1 >= 0 && bary2 >= 0) ||
                (bary0 <= 0 && bary1 <= 0 && bary2 <= 0))
            {
                bary0 /= area;
                bary1 /= area;
                bary2 /= area;

                float z = bary0 * cmd->z0 + bary1 * cmd->z1 + bary2 * cmd->z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                float interp_inv_w = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
                float w = 1.0f / interp_inv_w;
                uint32_t final_color = apply_fog(cmd->color, w);

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = final_color;
            }
        }
    }
}

static void tile_fill_textured(const RenderCmd *cmd,
                               int tx, int ty, int tw, int th)
{
    int min_x = min3(cmd->x0, cmd->x1, cmd->x2);
    int max_x = max3(cmd->x0, cmd->x1, cmd->x2);
    int min_y = min3(cmd->y0, cmd->y1, cmd->y2);
    int max_y = max3(cmd->y0, cmd->y1, cmd->y2);

    if (min_x < tx)
        min_x = tx;
    if (min_y < ty)
        min_y = ty;
    if (max_x > tx + tw - 1)
        max_x = tx + tw - 1;
    if (max_y > ty + th - 1)
        max_y = ty + th - 1;

    if (min_x > max_x || min_y > max_y)
        return;

    float area = edge_func(cmd->x0, cmd->y0, cmd->x1, cmd->y1, cmd->x2, cmd->y2);
    if (area == 0)
        return;

    float inv_w0 = 1.0f / cmd->w0;
    float inv_w1 = 1.0f / cmd->w1;
    float inv_w2 = 1.0f / cmd->w2;

    float u0_over_w = cmd->u0 * inv_w0;
    float v0_over_w = cmd->v0 * inv_w0;
    float u1_over_w = cmd->u1 * inv_w1;
    float v1_over_w = cmd->v1 * inv_w1;
    float u2_over_w = cmd->u2 * inv_w2;
    float v2_over_w = cmd->v2 * inv_w2;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            float bary0 = edge_func(cmd->x1, cmd->y1, cmd->x2, cmd->y2, x, y);
            float bary1 = edge_func(cmd->x2, cmd->y2, cmd->x0, cmd->y0, x, y);
            float bary2 = edge_func(cmd->x0, cmd->y0, cmd->x1, cmd->y1, x, y);

            if ((bary0 >= 0 && bary1 >= 0 && bary2 >= 0) ||
                (bary0 <= 0 && bary1 <= 0 && bary2 <= 0))
            {
                bary0 /= area;
                bary1 /= area;
                bary2 /= area;

                float z = bary0 * cmd->z0 + bary1 * cmd->z1 + bary2 * cmd->z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                float interp_inv_w = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
                float interp_u_over_w = bary0 * u0_over_w + bary1 * u1_over_w + bary2 * u2_over_w;
                float interp_v_over_w = bary0 * v0_over_w + bary1 * v1_over_w + bary2 * v2_over_w;
                float u = interp_u_over_w / interp_inv_w;
                float v = interp_v_over_w / interp_inv_w;

                uint32_t tex_color = texture_sample(cmd->tex, u, v);
                uint8_t r = (uint8_t)(((tex_color >> 16) & 0xFF) * cmd->light);
                uint8_t g = (uint8_t)(((tex_color >> 8) & 0xFF) * cmd->light);
                uint8_t b = (uint8_t)((tex_color & 0xFF) * cmd->light);
                uint32_t lit_color = 0xFF000000 | (r << 16) | (g << 8) | b;

                float w = 1.0f / interp_inv_w;
                uint32_t final_color = apply_fog(lit_color, w);

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = final_color;
            }
        }
    }
}

static void tile_rasterize(int tile_x, int tile_y, int tile_w, int tile_h,
                           void *userdata)
{
    (void)userdata;
    for (int i = 0; i < g_cmd_count; i++)
    {
        const RenderCmd *cmd = &g_cmd_buffer[i];
        if (cmd->textured)
            tile_fill_textured(cmd, tile_x, tile_y, tile_w, tile_h);
        else
            tile_fill_z(cmd, tile_x, tile_y, tile_w, tile_h);
    }
}

void render_set_threaded(bool enabled)
{
    g_threaded = enabled;
    LOG_INFO("Threaded rasterizer: %s", enabled ? "ON" : "OFF");
}

bool render_get_threaded(void)
{
    return g_threaded;
}

void render_begin_commands(void)
{
    g_cmd_count = 0;
}

void render_flush_commands(void)
{
    if (g_cmd_count == 0)
        return;

    int tiles_x = (RENDER_WIDTH + TILE_SIZE - 1) / TILE_SIZE;
    int tiles_y = (RENDER_HEIGHT + TILE_SIZE - 1) / TILE_SIZE;

    threadpool_dispatch(tiles_x, tiles_y, TILE_SIZE,
                        RENDER_WIDTH, RENDER_HEIGHT,
                        tile_rasterize, NULL);
}

int render_get_cmd_count(void)
{
    return g_cmd_count;
}

static const uint32_t s_tile_colors[] = {
    0x40FF0000,
    0x4000FF00,
    0x400000FF,
    0x40FFFF00,
    0x40FF00FF,
    0x4000FFFF,
    0x40FF8000,
    0x4080FF00,
    0x408000FF,
    0x40FF0080,
    0x4000FF80,
    0x400080FF,
    0x40FF4040,
    0x4040FF40,
    0x404040FF,
    0x40FFAA00,
};

static uint32_t blend_tile_color(uint32_t base, uint32_t overlay)
{
    uint8_t oa = (overlay >> 24) & 0xFF;
    float a = oa / 255.0f;
    float ia = 1.0f - a;

    uint8_t br = (base >> 16) & 0xFF;
    uint8_t bg = (base >> 8) & 0xFF;
    uint8_t bb = base & 0xFF;

    uint8_t or_ = (overlay >> 16) & 0xFF;
    uint8_t og = (overlay >> 8) & 0xFF;
    uint8_t ob = overlay & 0xFF;

    uint8_t fr = (uint8_t)(br * ia + or_ * a);
    uint8_t fg = (uint8_t)(bg * ia + og * a);
    uint8_t fb = (uint8_t)(bb * ia + ob * a);

    return 0xFF000000 | (fr << 16) | (fg << 8) | fb;
}

void render_draw_tile_debug(void)
{
    int tiles_x = threadpool_get_tiles_x();
    int tiles_y = threadpool_get_tiles_y();
    const int *owners = threadpool_get_tile_owners();
    int num_colors = (int)(sizeof(s_tile_colors) / sizeof(s_tile_colors[0]));

    if (tiles_x <= 0 || tiles_y <= 0)
        return;

    for (int ty = 0; ty < tiles_y; ty++)
    {
        for (int tx = 0; tx < tiles_x; tx++)
        {
            int tile_idx = ty * tiles_x + tx;
            int owner = (tile_idx < 1024) ? owners[tile_idx] : 0;
            uint32_t tint = s_tile_colors[owner % num_colors];

            int px = tx * TILE_SIZE;
            int py = ty * TILE_SIZE;
            int pw = TILE_SIZE;
            int ph = TILE_SIZE;
            if (px + pw > RENDER_WIDTH)
                pw = RENDER_WIDTH - px;
            if (py + ph > RENDER_HEIGHT)
                ph = RENDER_HEIGHT - py;

            for (int y = py; y < py + ph; y++)
            {
                for (int x = px; x < px + pw; x++)
                {
                    int idx = y * RENDER_WIDTH + x;
                    g_framebuffer[idx] = blend_tile_color(g_framebuffer[idx], tint);
                }
            }

            uint32_t grid_color = 0xFFFFFFFF;
            for (int x = px; x < px + pw; x++)
            {
                if (py >= 0 && py < RENDER_HEIGHT)
                    g_framebuffer[py * RENDER_WIDTH + x] = grid_color;
            }
            for (int y = py; y < py + ph; y++)
            {
                if (px >= 0 && px < RENDER_WIDTH)
                    g_framebuffer[y * RENDER_WIDTH + px] = grid_color;
            }
        }
    }
}
