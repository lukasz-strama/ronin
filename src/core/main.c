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
#include "core/console.h"
#include "core/level.h"
#include "core/collision_grid.h"
#include "core/chunk.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define COLOR_BLACK 0xFF000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_DEBUG_AABB 0xFF00FF00
#define COLOR_HOVER_AABB 0xFF00FF00
#define COLOR_SELECT_AABB 0xFF00CC00
#define COLOR_HIT_FLASH 0xFFFF0000
#define COLOR_PROJECTILE 0xFFFF4444
#define COLOR_DEBUG_RAY 0xFFFFFF00

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
        scene.entities[idx].pickable = false;
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
    Mat4 proj = mat4_perspective(PI / 3.0f, aspect, 0.1f, 10000.0f);

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

    // Game state machine
    GameState game_state = GAME_STATE_PLAYING;
    Console console;
    console_init(&console);
    console_log(&console, "Engine console ready. Type 'help'.");

    bool running = true;
    int selected_entity = -1;
    static OBJMesh loaded_map = {0};
    static CollisionGrid collision_grid = {0};
    static ChunkGrid chunk_grid = {0};
    static char current_map_path[256] = {0};

    CommandContext cmd_ctx;
    cmd_ctx.scene = &scene;
    cmd_ctx.camera = &camera;
    cmd_ctx.teapot = &teapot;
    cmd_ctx.cube_mesh = &cube_mesh;
    cmd_ctx.loaded_map = &loaded_map;
    cmd_ctx.collision_grid = &collision_grid;
    cmd_ctx.chunk_grid = &chunk_grid;
    cmd_ctx.current_map_path = current_map_path;
    cmd_ctx.running = &running;
    cmd_ctx.console = &console;
    cmd_ctx.state = &game_state;
    cmd_ctx.selected_entity = &selected_entity;

    Uint32 prev_time = SDL_GetTicks();

    bool key_w = false, key_s = false, key_a = false, key_d = false;
    bool key_space = false;
    bool key_shift = false;
    bool debug_aabb = false;
    bool shoot_requested = false;
    bool select_requested = false;

    int hovered_entity = -1;

    Projectile projectiles[MAX_PROJECTILES] = {0};

    // Debug ray visualization
    Vec3 debug_ray_start = {0, 0, 0};
    Vec3 debug_ray_end = {0, 0, 0};
    float debug_ray_timer = 0;

    SDL_SetRelativeMouseMode(SDL_TRUE);
    LOG_INFO("Entering main loop (WASD + Mouse, ESC=Pause, ~=Console)");

    while (running)
    {
        Uint32 curr_time = SDL_GetTicks();
        float dt = (curr_time - prev_time) / 1000.0f;
        prev_time = curr_time;

        bool menu_clicked = false;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
                break;
            }

            // --- Console mode input ---
            if (game_state == GAME_STATE_CONSOLE)
            {
                if (event.type == SDL_KEYDOWN)
                {
                    SDL_Keycode key = event.key.keysym.sym;
                    if (key == SDLK_BACKQUOTE)
                    {
                        game_state = GAME_STATE_PLAYING;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        LOG_INFO("Console closed");
                    }
                    else if (key == SDLK_ESCAPE)
                    {
                        game_state = GAME_STATE_PAUSED;
                    }
                    else if (key == SDLK_RETURN)
                    {
                        console_execute(&console, &cmd_ctx);
                    }
                    else if (key == SDLK_BACKSPACE)
                    {
                        console_pop_char(&console);
                    }
                    else if (key == SDLK_PAGEUP || key == SDLK_UP)
                    {
                        console_scroll(&console, 1);
                    }
                    else if (key == SDLK_PAGEDOWN || key == SDLK_DOWN)
                    {
                        console_scroll(&console, -1);
                    }
                }
                else if (event.type == SDL_TEXTINPUT)
                {
                    // Filter out backtick from text input
                    char c = event.text.text[0];
                    if (c != '`' && c != '~')
                    {
                        console_push_char(&console, c);
                    }
                }
                continue;
            }

            // --- Paused mode input ---
            if (game_state == GAME_STATE_PAUSED)
            {
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    menu_clicked = true;
                }

                if (event.type == SDL_KEYDOWN)
                {
                    SDL_Keycode key = event.key.keysym.sym;
                    if (key == SDLK_ESCAPE)
                    {
                        game_state = GAME_STATE_PLAYING;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        LOG_INFO("Resumed");
                    }
                    else if (key == SDLK_BACKQUOTE)
                    {
                        game_state = GAME_STATE_CONSOLE;
                        LOG_INFO("Console opened from pause");
                    }
                    else if (key == SDLK_q)
                    {
                        running = false;
                    }
                }
                continue;
            }

            // --- Playing mode input ---
            if (event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    game_state = GAME_STATE_PAUSED;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    key_w = key_s = key_a = key_d = false;
                    LOG_INFO("Paused");
                    break;
                case SDLK_BACKQUOTE:
                    game_state = GAME_STATE_CONSOLE;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    key_w = key_s = key_a = key_d = false;
                    LOG_INFO("Console opened");
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
                case SDLK_SPACE:
                    key_space = true;
                    break;
                case SDLK_LSHIFT:
                case SDLK_RSHIFT:
                    key_shift = true;
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
                case SDLK_SPACE:
                    key_space = false;
                    break;
                case SDLK_LSHIFT:
                case SDLK_RSHIFT:
                    key_shift = false;
                    break;
                }
            }
            if (event.type == SDL_MOUSEMOTION)
            {
                float delta_yaw = event.motion.xrel * CAMERA_SENSITIVITY;
                float delta_pitch = -event.motion.yrel * CAMERA_SENSITIVITY;
                camera_rotate(&camera, delta_yaw, delta_pitch);
            }
            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                    shoot_requested = true;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    select_requested = true;
            }
        }

        // --- Update (only while playing) ---
        if (game_state == GAME_STATE_PLAYING)
        {
            float current_speed = camera.fly_mode ? camera.fly_speed : CAMERA_WALK_SPEED;
            if (key_shift)
                current_speed *= 2.0f;

            float move_speed = current_speed * dt;
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

            // Apply gravity and handle jumping
            camera_apply_gravity(&camera, dt);
            if (key_space)
                camera_jump(&camera);

            scene_update(&scene, dt);

            // Projectile movement and collision
            for (int i = 0; i < MAX_PROJECTILES; i++)
            {
                Projectile *p = &projectiles[i];
                if (!p->active)
                    continue;

                p->lifetime -= dt;
                if (p->lifetime <= 0)
                {
                    p->active = false;
                    continue;
                }

                p->position = vec3_add(p->position,
                                       vec3_mul(p->direction, PROJECTILE_SPEED * dt));

                // Check collision against pickable entities
                AABB pbox = aabb_from_center_size(p->position,
                                                  (Vec3){PROJECTILE_HALF_SIZE, PROJECTILE_HALF_SIZE, PROJECTILE_HALF_SIZE});

                for (int j = 0; j < scene.count; j++)
                {
                    Entity *ent = &scene.entities[j];
                    if (!ent->active || !ent->pickable)
                        continue;

                    AABB ent_box = entity_get_world_aabb(ent);
                    if (aabb_overlap(pbox, ent_box))
                    {
                        p->active = false;
                        ent->hit_timer = HIT_FLASH_DURATION;
                        LOG_INFO("Projectile hit entity %d!", j);
                        console_log(&console, "Hit entity %d!", j);
                        break;
                    }
                }
            }

            if (debug_ray_timer > 0)
                debug_ray_timer -= dt;
        }

        // --- Render (always) ---
        Mat4 view = camera_get_view_matrix(&camera);
        Mat4 vp = mat4_mul(proj, view);

        // Extract frustum planes for culling
        Frustum frustum = frustum_extract(vp);
        RenderStats render_stats = {0};

        // Screen-to-world ray from crosshair (center of screen)
        Mat4 inv_proj = mat4_inverse(proj);
        Mat4 inv_view = mat4_inverse(view);
        Ray center_ray = ray_from_screen(
            RENDER_WIDTH / 2, RENDER_HEIGHT / 2,
            RENDER_WIDTH, RENDER_HEIGHT,
            inv_proj, inv_view, camera.position);

        // Hover detection and queued actions (only while playing)
        if (game_state == GAME_STATE_PLAYING)
        {
            float t;
            hovered_entity = scene_ray_pick(&scene, center_ray, &t);

            if (shoot_requested)
            {
                // Find a free projectile slot
                for (int i = 0; i < MAX_PROJECTILES; i++)
                {
                    if (!projectiles[i].active)
                    {
                        projectiles[i].position = vec3_add(camera.position,
                                                           vec3_mul(center_ray.direction, 0.5f));
                        projectiles[i].direction = center_ray.direction;
                        projectiles[i].lifetime = PROJECTILE_LIFETIME;
                        projectiles[i].active = true;
                        LOG_INFO("Projectile fired");
                        break;
                    }
                }

                if (console.debug_rays)
                {
                    debug_ray_start = camera.position;
                    debug_ray_end = vec3_add(camera.position,
                                             vec3_mul(center_ray.direction, 50.0f));
                    debug_ray_timer = 3.0f;
                }
                shoot_requested = false;
            }

            if (select_requested)
            {
                if (hovered_entity >= 0)
                {
                    selected_entity = hovered_entity;
                    LOG_INFO("Selected entity %d", hovered_entity);
                    console_log(&console, "Selected entity %d", hovered_entity);
                }
                else
                {
                    selected_entity = -1;
                }

                if (console.debug_rays)
                {
                    debug_ray_start = camera.position;
                    debug_ray_end = vec3_add(camera.position,
                                             vec3_mul(center_ray.direction, 50.0f));
                    debug_ray_timer = 3.0f;
                }
                select_requested = false;
            }
        }
        else
        {
            hovered_entity = -1;
        }

        framebuffer_clear(COLOR_BLACK);
        render_clear_zbuffer();

        // Render chunked map geometry first (frustum-culled per chunk)
        if (chunk_grid.count > 0)
        {
            render_stats.chunks_total = chunk_grid.count;
            if (console.wireframe)
                chunk_grid_render_wireframe(&chunk_grid, vp, camera.position,
                                            &frustum, console.backface_cull,
                                            &render_stats);
            else
                chunk_grid_render(&chunk_grid, vp, camera.position, light_dir,
                                  &frustum, console.backface_cull,
                                  &render_stats);
            render_stats.chunks_culled = render_stats.entities_culled;
            render_stats.entities_culled = 0;
        }

        if (console.wireframe)
            scene_render_wireframe(&scene, vp, camera.position,
                                   &frustum, console.backface_cull,
                                   &render_stats);
        else
            scene_render(&scene, vp, camera.position, light_dir,
                         &frustum, console.backface_cull,
                         &render_stats);

        if (debug_aabb)
        {
            for (int i = 0; i < camera.collider_count; i++)
            {
                render_draw_aabb(camera.colliders[i], vp, COLOR_DEBUG_AABB);
            }
        }

        if (hovered_entity >= 0)
        {
            AABB box = entity_get_world_aabb(&scene.entities[hovered_entity]);
            render_draw_aabb(box, vp, COLOR_HOVER_AABB);
        }

        // Selection highlight (darker green, persistent until deselected)
        if (selected_entity >= 0 && selected_entity < scene.count &&
            selected_entity != hovered_entity)
        {
            AABB box = entity_get_world_aabb(&scene.entities[selected_entity]);
            render_draw_aabb(box, vp, COLOR_SELECT_AABB);
        }

        for (int i = 0; i < scene.count; i++)
        {
            if (scene.entities[i].hit_timer > 0)
            {
                AABB box = entity_get_world_aabb(&scene.entities[i]);
                render_draw_aabb(box, vp, COLOR_HIT_FLASH);
            }
        }

        for (int i = 0; i < MAX_PROJECTILES; i++)
        {
            if (!projectiles[i].active)
                continue;

            AABB pbox = aabb_from_center_size(projectiles[i].position,
                                              (Vec3){PROJECTILE_HALF_SIZE, PROJECTILE_HALF_SIZE, PROJECTILE_HALF_SIZE});
            render_draw_aabb(pbox, vp, COLOR_PROJECTILE);

            // Draw a bright center dot for visibility at distance
            Vec4 cc = mat4_mul_vec4(vp,
                                    vec4_from_vec3(projectiles[i].position, 1.0f));
            if (cc.w > 0.1f)
            {
                ProjectedVertex pv = render_project_vertex(cc);
                int px = (int)pv.screen.x;
                int py = (int)pv.screen.y;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        render_set_pixel(px + dx, py + dy, 0xFFFF0000);
            }
        }

        // Debug ray visualization
        if (console.debug_rays && debug_ray_timer > 0)
        {
            render_draw_3d_line(debug_ray_start, debug_ray_end, vp, COLOR_DEBUG_RAY);
        }

        // --- HUD overlays ---
        hud_draw_crosshair(0xFFFFFFFF);
        if (console.show_debug)
        {
            hud_draw_fps(&hud_font, dt);
            hud_draw_cull_stats(&hud_font, &render_stats, scene.count);
        }

        if (game_state == GAME_STATE_PAUSED)
        {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            int rmx = (int)(mx * ((float)RENDER_WIDTH / WINDOW_WIDTH));
            int rmy = (int)(my * ((float)RENDER_HEIGHT / WINDOW_HEIGHT));

            int action = hud_draw_pause_menu(&hud_font, rmx, rmy, menu_clicked);
            if (action == 1) // Resume
            {
                game_state = GAME_STATE_PLAYING;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                LOG_INFO("Resumed");
            }
            else if (action == 2) // Console
            {
                game_state = GAME_STATE_CONSOLE;
                LOG_INFO("Console opened from pause");
            }
            else if (action == 3) // Quit
            {
                running = false;
            }
        }
        else if (game_state == GAME_STATE_CONSOLE)
        {
            console_draw(&console, &hud_font);
        }

        SDL_UpdateTexture(texture, NULL, framebuffer, RENDER_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    LOG_INFO("Shutting down...");

    chunk_grid_free(&chunk_grid);
    grid_free(&collision_grid);
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
