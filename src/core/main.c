#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/log.h"
#include "math/math.h"
#include "graphics/mesh.h"
#include "graphics/render.h"
#include "graphics/clip.h"
#include "graphics/texture.h"
#include "graphics/hud.h"
#include "core/camera.h"
#include "core/obj_loader.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define COLOR_BLACK 0xFF000000
#define COLOR_WHITE 0xFFFFFFFF

#define PI 3.14159265358979323846f

static uint32_t framebuffer[RENDER_WIDTH * RENDER_HEIGHT];
static float zbuffer[RENDER_WIDTH * RENDER_HEIGHT];

static void framebuffer_clear(uint32_t color)
{
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++)
    {
        framebuffer[i] = color;
    }
}

static uint32_t shade_color(uint32_t base_color, float intensity)
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

typedef struct
{
    Vec2 screen;
    float z;
} ProjectedVertex;

static ProjectedVertex project_vertex(Vec4 v)
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

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    LOG_INFO("Initializing engine...");

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    LOG_INFO("Creating window (%dx%d)", WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_Window *window = SDL_CreateWindow(
        "Software Renderer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0);

    if (!window)
    {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        RENDER_WIDTH,
        RENDER_HEIGHT);
    if (!texture)
    {
        LOG_ERROR("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    render_set_framebuffer(framebuffer);
    render_set_zbuffer(zbuffer);

    Mesh floor_tiles[MAX_FLOOR_TILES];
    int floor_tile_count = mesh_generate_floor(floor_tiles);

    float margin = 2.0f;
    float half_floor = FLOOR_TOTAL_SIZE / 2.0f - margin;
    float cube_y = 1.1f;

    Mesh corner_cubes[4];
    corner_cubes[0] = mesh_cube_at((Vec3){-half_floor, cube_y, -half_floor}, 1.0f);
    corner_cubes[1] = mesh_cube_at((Vec3){half_floor, cube_y, -half_floor}, 1.0f);
    corner_cubes[2] = mesh_cube_at((Vec3){-half_floor, cube_y, half_floor}, 1.0f);
    corner_cubes[3] = mesh_cube_at((Vec3){half_floor, cube_y, half_floor}, 1.0f);

    float aspect = (float)RENDER_WIDTH / (float)RENDER_HEIGHT;
    Mat4 proj = mat4_perspective(PI / 3.0f, aspect, 0.1f, 100.0f);

    Camera camera;
    camera_init(&camera, (Vec3){0, 2, 0}, 0.0f, 0.0f);

    Texture floor_tex;
    texture_create_checker(&floor_tex, 64, 8, 0xFFFF69B4, 0xFF808080);

    Font hud_font;
    if (hud_font_init(&hud_font) != 0)
    {
        LOG_ERROR("Failed to initialize HUD font");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    OBJMesh teapot;
    if (obj_load(&teapot, "assets/utah_teapot.obj") != 0)
    {
        LOG_ERROR("Failed to load teapot model");
        hud_font_free(&hud_font);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Teapot transform: placed next to camera start, scaled down
    Vec3 teapot_pos = {3.0f, 0.0f, -3.0f};
    float teapot_scale = 0.4f;
    float teapot_angle = 0.0f;

    Vec3 light_dir = vec3_normalize((Vec3){0.5f, 1.0f, -0.5f});

    Uint32 prev_time = SDL_GetTicks();

    bool key_w = false, key_s = false, key_a = false, key_d = false;

    SDL_SetRelativeMouseMode(SDL_TRUE);
    LOG_INFO("Entering main loop (WASD + Mouse to move, ESC to quit)");

    bool running = true;
    while (running)
    {
        Uint32 curr_time = SDL_GetTicks();
        float dt = (curr_time - prev_time) / 1000.0f;
        prev_time = curr_time;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            if (event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_w:
                    key_w = true;
                    break;
                case SDLK_s:
                    key_s = true;
                    break;
                case SDLK_a:
                    key_a = true;
                    break;
                case SDLK_d:
                    key_d = true;
                    break;
                }
            }
            if (event.type == SDL_KEYUP)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_w:
                    key_w = false;
                    break;
                case SDLK_s:
                    key_s = false;
                    break;
                case SDLK_a:
                    key_a = false;
                    break;
                case SDLK_d:
                    key_d = false;
                    break;
                }
            }
            if (event.type == SDL_MOUSEMOTION)
            {
                float delta_yaw = event.motion.xrel * CAMERA_SENSITIVITY;
                float delta_pitch = -event.motion.yrel * CAMERA_SENSITIVITY;
                camera_rotate(&camera, delta_yaw, delta_pitch);
            }
        }

        float move_speed = CAMERA_SPEED * dt;
        if (key_w)
            camera_move_forward(&camera, move_speed);
        if (key_s)
            camera_move_backward(&camera, move_speed);
        if (key_a)
            camera_strafe_left(&camera, move_speed);
        if (key_d)
            camera_strafe_right(&camera, move_speed);

        Mat4 view = camera_get_view_matrix(&camera);
        Mat4 vp = mat4_mul(proj, view);

        framebuffer_clear(COLOR_BLACK);
        render_clear_zbuffer();

        // Render floor tiles with texture
        for (int t = 0; t < floor_tile_count; t++)
        {
            Mesh *tile = &floor_tiles[t];
            for (int i = 0; i < tile->face_count; i++)
            {
                Face face = tile->faces[i];

                Vec3 v0 = tile->vertices[face.a];
                Vec3 v1 = tile->vertices[face.b];
                Vec3 v2 = tile->vertices[face.c];

                // Floor is static - world coords = local coords
                Vec3 edge1 = vec3_sub(v1, v0);
                Vec3 edge2 = vec3_sub(v2, v0);
                Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

                // Backface culling
                Vec3 face_center = vec3_mul(vec3_add(vec3_add(v0, v1), v2), 1.0f / 3.0f);
                Vec3 view_dir = vec3_normalize(vec3_sub(camera.position, face_center));
                if (vec3_dot(normal, view_dir) < 0)
                    continue;

                // Flat shading intensity
                float intensity = vec3_dot(normal, light_dir);
                if (intensity < 0)
                    intensity = 0;
                intensity = 0.3f + intensity * 0.7f;

                // Calculate UVs from world position (tile repeating pattern)
                float uv_scale = 0.5f; // Controls texture tiling frequency
                float u0 = v0.x * uv_scale;
                float vt0 = v0.z * uv_scale;
                float u1 = v1.x * uv_scale;
                float vt1 = v1.z * uv_scale;
                float u2 = v2.x * uv_scale;
                float vt2 = v2.z * uv_scale;

                Vec4 t0 = mat4_mul_vec4(vp, vec4_from_vec3(v0, 1.0f));
                Vec4 t1 = mat4_mul_vec4(vp, vec4_from_vec3(v1, 1.0f));
                Vec4 t2 = mat4_mul_vec4(vp, vec4_from_vec3(v2, 1.0f));

                ClipPolygon poly;
                poly.count = 3;
                poly.vertices[0] = (ClipVertex){t0, u0, vt0, 0};
                poly.vertices[1] = (ClipVertex){t1, u1, vt1, 0};
                poly.vertices[2] = (ClipVertex){t2, u2, vt2, 0};

                if (clip_polygon_against_frustum(&poly) < 3)
                    continue;

                ProjectedVertex pv0 = project_vertex(poly.vertices[0].position);
                for (int j = 1; j < poly.count - 1; j++)
                {
                    ProjectedVertex pv1 = project_vertex(poly.vertices[j].position);
                    ProjectedVertex pv2 = project_vertex(poly.vertices[j + 1].position);

                    render_fill_triangle_textured(
                        (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                        poly.vertices[0].u, poly.vertices[0].v, poly.vertices[0].position.w,
                        (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                        poly.vertices[j].u, poly.vertices[j].v, poly.vertices[j].position.w,
                        (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                        poly.vertices[j + 1].u, poly.vertices[j + 1].v, poly.vertices[j + 1].position.w,
                        &floor_tex, intensity);
                }
            }
        }

        // Render corner cubes (static, vertices already in world space)
        for (int c = 0; c < 4; c++)
        {
            Mesh *cube = &corner_cubes[c];
            for (int i = 0; i < cube->face_count; i++)
            {
                Face face = cube->faces[i];

                Vec3 v0 = cube->vertices[face.a];
                Vec3 v1 = cube->vertices[face.b];
                Vec3 v2 = cube->vertices[face.c];

                Vec3 edge1 = vec3_sub(v1, v0);
                Vec3 edge2 = vec3_sub(v2, v0);
                Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

                // Backface culling
                Vec3 face_center = vec3_mul(vec3_add(vec3_add(v0, v1), v2), 1.0f / 3.0f);
                Vec3 view_dir = vec3_normalize(vec3_sub(camera.position, face_center));
                if (vec3_dot(normal, view_dir) < 0)
                    continue;

                // Flat shading
                float intensity = vec3_dot(normal, light_dir);
                if (intensity < 0)
                    intensity = 0;
                intensity = 0.2f + intensity * 0.8f;
                uint32_t shaded_color = shade_color(face.color, intensity);

                Vec4 t0 = mat4_mul_vec4(vp, vec4_from_vec3(v0, 1.0f));
                Vec4 t1 = mat4_mul_vec4(vp, vec4_from_vec3(v1, 1.0f));
                Vec4 t2 = mat4_mul_vec4(vp, vec4_from_vec3(v2, 1.0f));

                ClipPolygon poly;
                poly.count = 3;
                poly.vertices[0] = (ClipVertex){t0, 0, 0, shaded_color};
                poly.vertices[1] = (ClipVertex){t1, 0, 0, shaded_color};
                poly.vertices[2] = (ClipVertex){t2, 0, 0, shaded_color};

                if (clip_polygon_against_frustum(&poly) < 3)
                    continue;

                ProjectedVertex pv0 = project_vertex(poly.vertices[0].position);
                for (int j = 1; j < poly.count - 1; j++)
                {
                    ProjectedVertex pv1 = project_vertex(poly.vertices[j].position);
                    ProjectedVertex pv2 = project_vertex(poly.vertices[j + 1].position);

                    render_fill_triangle_z(
                        (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                        (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                        (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                        poly.vertices[0].color);
                }
            }
        }

        // Rotate teapot
        teapot_angle += 0.8f * dt;
        Mat4 teapot_t = mat4_translate(teapot_pos.x, teapot_pos.y, teapot_pos.z);
        Mat4 teapot_r = mat4_rotate_y(teapot_angle);
        Mat4 teapot_s = mat4_scale(teapot_scale, teapot_scale, teapot_scale);
        Mat4 teapot_model = mat4_mul(teapot_t, mat4_mul(teapot_r, teapot_s));
        Mat4 teapot_mvp = mat4_mul(vp, teapot_model);

        for (int i = 0; i < teapot.face_count; i++)
        {
            OBJFace face = teapot.faces[i];

            OBJVertex ov0 = teapot.vertices[face.a];
            OBJVertex ov1 = teapot.vertices[face.b];
            OBJVertex ov2 = teapot.vertices[face.c];

            // Transform to world space for lighting
            Vec4 w0 = mat4_mul_vec4(teapot_model, vec4_from_vec3(ov0.position, 1.0f));
            Vec4 w1 = mat4_mul_vec4(teapot_model, vec4_from_vec3(ov1.position, 1.0f));
            Vec4 w2 = mat4_mul_vec4(teapot_model, vec4_from_vec3(ov2.position, 1.0f));
            Vec3 wv0 = vec3_from_vec4(w0);
            Vec3 wv1 = vec3_from_vec4(w1);
            Vec3 wv2 = vec3_from_vec4(w2);

            Vec3 edge1 = vec3_sub(wv1, wv0);
            Vec3 edge2 = vec3_sub(wv2, wv0);
            Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

            // Backface culling
            Vec3 face_center = vec3_mul(vec3_add(vec3_add(wv0, wv1), wv2), 1.0f / 3.0f);
            Vec3 view_dir = vec3_normalize(vec3_sub(camera.position, face_center));
            if (vec3_dot(normal, view_dir) < 0)
                continue;

            // Flat shading
            float intensity = vec3_dot(normal, light_dir);
            if (intensity < 0)
                intensity = 0;
            intensity = 0.15f + intensity * 0.85f;
            uint32_t shaded_color = shade_color(face.color, intensity);

            Vec4 t0 = mat4_mul_vec4(teapot_mvp, vec4_from_vec3(ov0.position, 1.0f));
            Vec4 t1 = mat4_mul_vec4(teapot_mvp, vec4_from_vec3(ov1.position, 1.0f));
            Vec4 t2 = mat4_mul_vec4(teapot_mvp, vec4_from_vec3(ov2.position, 1.0f));

            ClipPolygon poly;
            poly.count = 3;
            poly.vertices[0] = (ClipVertex){t0, 0, 0, shaded_color};
            poly.vertices[1] = (ClipVertex){t1, 0, 0, shaded_color};
            poly.vertices[2] = (ClipVertex){t2, 0, 0, shaded_color};

            if (clip_polygon_against_frustum(&poly) < 3)
                continue;

            ProjectedVertex pv0 = project_vertex(poly.vertices[0].position);
            for (int j = 1; j < poly.count - 1; j++)
            {
                ProjectedVertex pv1 = project_vertex(poly.vertices[j].position);
                ProjectedVertex pv2 = project_vertex(poly.vertices[j + 1].position);

                render_fill_triangle_z(
                    (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                    (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                    (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                    poly.vertices[0].color);
            }
        }

        // 2D overlay (drawn on top of 3D scene)
        hud_draw_crosshair(0xFFFFFFFF);
        hud_draw_fps(&hud_font, dt);

        SDL_UpdateTexture(texture, NULL, framebuffer, RENDER_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    LOG_INFO("Shutting down...");

    obj_mesh_free(&teapot);
    hud_font_free(&hud_font);
    texture_free(&floor_tex);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    LOG_INFO("Goodbye!");
    return 0;
}
