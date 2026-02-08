#include "graphics/render.h"
#include "core/log.h"
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

// Horizontal scanline
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

// Flat-bottom triangle: v0 at top, v1 and v2 at bottom (same y)
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

// Edge function for barycentric coordinates
static float edge_func(int ax, int ay, int bx, int by, int px, int py)
{
    return (float)((px - ax) * (by - ay) - (py - ay) * (bx - ax));
}

void render_fill_triangle_z(
    int x0, int y0, float z0,
    int x1, int y1, float z1,
    int x2, int y2, float z2,
    uint32_t color)
{
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

                // Early Z-rejection: compute depth first, skip pixel if occluded
                float z = bary0 * z0 + bary1 * z1 + bary2 * z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = color;
            }
        }
    }
}

void render_fill_triangle_textured(
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

                // Early Z-rejection: compute depth first, skip pixel if occluded
                float z = bary0 * z0 + bary1 * z1 + bary2 * z2;
                int idx = y * RENDER_WIDTH + x;
                if (z >= g_zbuffer[idx])
                    continue;

                // Pixel passed depth test — now do the expensive work
                float interp_inv_w = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
                float interp_u_over_w = bary0 * u0_over_w + bary1 * u1_over_w + bary2 * u2_over_w;
                float interp_v_over_w = bary0 * v0_over_w + bary1 * v1_over_w + bary2 * v2_over_w;
                float u = interp_u_over_w / interp_inv_w;
                float v = interp_v_over_w / interp_inv_w;

                uint32_t tex_color = texture_sample(tex, u, v);

                uint8_t r = (uint8_t)(((tex_color >> 16) & 0xFF) * light_intensity);
                uint8_t g = (uint8_t)(((tex_color >> 8) & 0xFF) * light_intensity);
                uint8_t b = (uint8_t)((tex_color & 0xFF) * light_intensity);

                g_zbuffer[idx] = z;
                g_framebuffer[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
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
