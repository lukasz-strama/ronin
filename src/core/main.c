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
#include "core/entity.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define COLOR_BLACK 0xFF000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_DEBUG_AABB 0xFF00FF00

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

    Scene scene;
    scene_init(&scene);

    // Floor tiles (textured, static - vertices in world space)
    Mesh floor_tiles[MAX_FLOOR_TILES];
    int floor_tile_count = mesh_generate_floor(floor_tiles);

    Texture floor_tex;
    texture_create_checker(&floor_tex, 64, 8, 0xFFFF69B4, 0xFF808080);

    for (int i = 0; i < floor_tile_count; i++)
    {
        int idx = scene_add_mesh(&scene, &floor_tiles[i], (Vec3){0, 0, 0}, 1.0f);
        scene_set_texture(&scene, idx, &floor_tex, 0.5f);
    }

    // Corner cubes (shared base mesh, positioned via entity transform)
    Mesh cube_mesh = mesh_cube();
    cube_mesh.bounds = aabb_from_vertices(cube_mesh.vertices, cube_mesh.vertex_count);

    float margin = 2.0f;
    float half_floor = FLOOR_TOTAL_SIZE / 2.0f - margin;
    float cube_y = 1.1f;

    Vec3 cube_positions[4] = {
        {-half_floor, cube_y, -half_floor},
        {half_floor, cube_y, -half_floor},
        {-half_floor, cube_y, half_floor},
        {half_floor, cube_y, half_floor}};

    int cube_indices[4];
    for (int i = 0; i < 4; i++)
    {
        cube_indices[i] = scene_add_mesh(&scene, &cube_mesh, cube_positions[i], 1.0f);
        scene_set_rotation_speed(&scene, cube_indices[i], (Vec3){0, 0.8f, 0});
    }

    // Teapot (loaded OBJ)
    OBJMesh teapot;
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

    int teapot_idx = scene_add_obj(&scene, &teapot, (Vec3){3.0f, 0.0f, -3.0f}, 0.4f);
    scene_set_rotation_speed(&scene, teapot_idx, (Vec3){0, 0.8f, 0});

    LOG_INFO("Scene populated: %d entities", scene.count);

    // Camera & Colliders
    float aspect = (float)RENDER_WIDTH / (float)RENDER_HEIGHT;
    Mat4 proj = mat4_perspective(PI / 3.0f, aspect, 0.1f, 100.0f);

    Camera camera;
    camera_init(&camera, (Vec3){0, 2, 0}, 0.0f, 0.0f);

    // Register cube colliders from entity AABBs
    for (int i = 0; i < 4; i++)
    {
        camera_add_collider(&camera, entity_get_world_aabb(&scene.entities[cube_indices[i]]));
    }

    // Floor boundary walls
    float floor_half = FLOOR_TOTAL_SIZE / 2.0f;
    float wall_thick = 0.5f;
    camera_add_collider(&camera, (AABB){
                                     .min = {-floor_half, -1, -floor_half - wall_thick},
                                     .max = {floor_half, 10, -floor_half}});
    camera_add_collider(&camera, (AABB){
                                     .min = {-floor_half, -1, floor_half},
                                     .max = {floor_half, 10, floor_half + wall_thick}});
    camera_add_collider(&camera, (AABB){
                                     .min = {-floor_half - wall_thick, -1, -floor_half},
                                     .max = {-floor_half, 10, floor_half}});
    camera_add_collider(&camera, (AABB){
                                     .min = {floor_half, -1, -floor_half},
                                     .max = {floor_half + wall_thick, 10, floor_half}});

    // Teapot collider
    camera_add_collider(&camera, entity_get_world_aabb(&scene.entities[teapot_idx]));

    LOG_INFO("Registered %d colliders", camera.collider_count);

    Vec3 light_dir = vec3_normalize((Vec3){0.5f, 1.0f, -0.5f});

    Uint32 prev_time = SDL_GetTicks();

    bool key_w = false, key_s = false, key_a = false, key_d = false;
    bool debug_aabb = false;

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
                case SDLK_b:
                    debug_aabb = !debug_aabb;
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
        Vec3 move_delta = {0, 0, 0};
        if (key_w)
            move_delta = vec3_add(move_delta, vec3_mul(camera.direction, move_speed));
        if (key_s)
            move_delta = vec3_add(move_delta, vec3_mul(camera.direction, -move_speed));
        if (key_a)
            move_delta = vec3_sub(move_delta, vec3_mul(camera.right, move_speed));
        if (key_d)
            move_delta = vec3_add(move_delta, vec3_mul(camera.right, move_speed));

        if (move_delta.x != 0 || move_delta.y != 0 || move_delta.z != 0)
            camera_try_move(&camera, move_delta);

        scene_update(&scene, dt);

        Mat4 view = camera_get_view_matrix(&camera);
        Mat4 vp = mat4_mul(proj, view);

        framebuffer_clear(COLOR_BLACK);
        render_clear_zbuffer();

        scene_render(&scene, vp, camera.position, light_dir);

        // Debug: draw AABB wireframes for all colliders (toggle with B)
        if (debug_aabb)
        {
            for (int i = 0; i < camera.collider_count; i++)
            {
                render_draw_aabb(camera.colliders[i], vp, COLOR_DEBUG_AABB);
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
