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
#include "core/camera.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

#define COLOR_BLACK  0xFF000000
#define COLOR_WHITE  0xFFFFFFFF

#define PI 3.14159265358979323846f

static uint32_t framebuffer[RENDER_WIDTH * RENDER_HEIGHT];
static float    zbuffer[RENDER_WIDTH * RENDER_HEIGHT];

static void framebuffer_clear(uint32_t color) {
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++) {
        framebuffer[i] = color;
    }
}

static uint32_t shade_color(uint32_t base_color, float intensity) {
    if (intensity < 0.1f) intensity = 0.1f;
    if (intensity > 1.0f) intensity = 1.0f;

    uint8_t r = (uint8_t)(((base_color >> 16) & 0xFF) * intensity);
    uint8_t g = (uint8_t)(((base_color >> 8)  & 0xFF) * intensity);
    uint8_t b = (uint8_t)(( base_color        & 0xFF) * intensity);

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

typedef struct {
    Vec2  screen;
    float z;
} ProjectedVertex;

static ProjectedVertex project_vertex(Vec4 v) {
    ProjectedVertex pv;
    float x = v.x / v.w;
    float y = v.y / v.w;
    float z = v.z / v.w;

    pv.screen.x = (x + 1.0f) * 0.5f * RENDER_WIDTH;
    pv.screen.y = (1.0f - y) * 0.5f * RENDER_HEIGHT;
    pv.z = z;
    return pv;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    LOG_INFO("Initializing engine...");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
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
        0
    );

    if (!window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
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
        RENDER_HEIGHT
    );
    if (!texture) {
        LOG_ERROR("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    render_set_framebuffer(framebuffer);
    render_set_zbuffer(zbuffer);

    // Generate floor tiles
    Mesh floor_tiles[MAX_FLOOR_TILES];
    int floor_tile_count = mesh_generate_floor(floor_tiles);

    // Create 4 cubes at corners (with margin from edge)
    float margin = 2.0f;
    float half_floor = FLOOR_TOTAL_SIZE / 2.0f - margin;
    float cube_y = 1.1f;  // Cubes sit slightly above floor

    Mesh corner_cubes[4];
    corner_cubes[0] = mesh_cube_at((Vec3){ -half_floor, cube_y, -half_floor }, 1.0f);
    corner_cubes[1] = mesh_cube_at((Vec3){  half_floor, cube_y, -half_floor }, 1.0f);
    corner_cubes[2] = mesh_cube_at((Vec3){ -half_floor, cube_y,  half_floor }, 1.0f);
    corner_cubes[3] = mesh_cube_at((Vec3){  half_floor, cube_y,  half_floor }, 1.0f);

    float aspect = (float)RENDER_WIDTH / (float)RENDER_HEIGHT;
    Mat4 proj = mat4_perspective(PI / 3.0f, aspect, 0.1f, 100.0f);

    // Camera starts at center of floor, eye-level above ground
    Camera camera;
    camera_init(&camera, (Vec3){ 0, 2, 0 }, 0.0f, 0.0f);

    Vec3 light_dir = vec3_normalize((Vec3){ 0.5f, 1.0f, -0.5f });

    Uint32 prev_time = SDL_GetTicks();

    // Input state
    bool key_w = false, key_s = false, key_a = false, key_d = false;

    SDL_SetRelativeMouseMode(SDL_TRUE);
    LOG_INFO("Entering main loop (WASD + Mouse to move, ESC to quit)");

    bool running = true;
    while (running) {
        Uint32 curr_time = SDL_GetTicks();
        float dt = (curr_time - prev_time) / 1000.0f;
        prev_time = curr_time;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_w: key_w = true; break;
                    case SDLK_s: key_s = true; break;
                    case SDLK_a: key_a = true; break;
                    case SDLK_d: key_d = true; break;
                }
            }
            if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: key_w = false; break;
                    case SDLK_s: key_s = false; break;
                    case SDLK_a: key_a = false; break;
                    case SDLK_d: key_d = false; break;
                }
            }
            if (event.type == SDL_MOUSEMOTION) {
                float delta_yaw   =  event.motion.xrel * CAMERA_SENSITIVITY;
                float delta_pitch = -event.motion.yrel * CAMERA_SENSITIVITY;
                camera_rotate(&camera, delta_yaw, delta_pitch);
            }
        }

        float move_speed = CAMERA_SPEED * dt;
        if (key_w) camera_move_forward(&camera, move_speed);
        if (key_s) camera_move_backward(&camera, move_speed);
        if (key_a) camera_strafe_left(&camera, move_speed);
        if (key_d) camera_strafe_right(&camera, move_speed);

        Mat4 view = camera_get_view_matrix(&camera);
        Mat4 vp = mat4_mul(proj, view);

        framebuffer_clear(COLOR_BLACK);
        render_clear_zbuffer();

        // Render floor tiles (static, model = identity)
        for (int t = 0; t < floor_tile_count; t++) {
            Mesh *tile = &floor_tiles[t];
            for (int i = 0; i < tile->face_count; i++) {
                Face face = tile->faces[i];

                Vec3 v0 = tile->vertices[face.a];
                Vec3 v1 = tile->vertices[face.b];
                Vec3 v2 = tile->vertices[face.c];

                // Floor is static - world coords = local coords
                Vec3 edge1  = vec3_sub(v1, v0);
                Vec3 edge2  = vec3_sub(v2, v0);
                Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

                // Backface culling
                Vec3 face_center = vec3_mul(vec3_add(vec3_add(v0, v1), v2), 1.0f / 3.0f);
                Vec3 view_dir = vec3_normalize(vec3_sub(camera.position, face_center));
                if (vec3_dot(normal, view_dir) < 0) continue;

                // Flat shading
                float intensity = vec3_dot(normal, light_dir);
                if (intensity < 0) intensity = 0;
                intensity = 0.3f + intensity * 0.7f;
                uint32_t shaded_color = shade_color(face.color, intensity);

                Vec4 t0 = mat4_mul_vec4(vp, vec4_from_vec3(v0, 1.0f));
                Vec4 t1 = mat4_mul_vec4(vp, vec4_from_vec3(v1, 1.0f));
                Vec4 t2 = mat4_mul_vec4(vp, vec4_from_vec3(v2, 1.0f));

                ClipPolygon poly;
                poly.count = 3;
                poly.vertices[0] = (ClipVertex){ t0, shaded_color };
                poly.vertices[1] = (ClipVertex){ t1, shaded_color };
                poly.vertices[2] = (ClipVertex){ t2, shaded_color };

                if (clip_polygon_against_frustum(&poly) < 3) continue;

                ProjectedVertex pv0 = project_vertex(poly.vertices[0].position);
                for (int j = 1; j < poly.count - 1; j++) {
                    ProjectedVertex pv1 = project_vertex(poly.vertices[j].position);
                    ProjectedVertex pv2 = project_vertex(poly.vertices[j + 1].position);

                    render_fill_triangle_z(
                        (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                        (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                        (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                        poly.vertices[0].color
                    );
                }
            }
        }

        // Render corner cubes (static, vertices already in world space)
        for (int c = 0; c < 4; c++) {
            Mesh *cube = &corner_cubes[c];
            for (int i = 0; i < cube->face_count; i++) {
                Face face = cube->faces[i];

                Vec3 v0 = cube->vertices[face.a];
                Vec3 v1 = cube->vertices[face.b];
                Vec3 v2 = cube->vertices[face.c];

                Vec3 edge1  = vec3_sub(v1, v0);
                Vec3 edge2  = vec3_sub(v2, v0);
                Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

                // Backface culling
                Vec3 face_center = vec3_mul(vec3_add(vec3_add(v0, v1), v2), 1.0f / 3.0f);
                Vec3 view_dir = vec3_normalize(vec3_sub(camera.position, face_center));
                if (vec3_dot(normal, view_dir) < 0) continue;

                // Flat shading
                float intensity = vec3_dot(normal, light_dir);
                if (intensity < 0) intensity = 0;
                intensity = 0.2f + intensity * 0.8f;
                uint32_t shaded_color = shade_color(face.color, intensity);

                Vec4 t0 = mat4_mul_vec4(vp, vec4_from_vec3(v0, 1.0f));
                Vec4 t1 = mat4_mul_vec4(vp, vec4_from_vec3(v1, 1.0f));
                Vec4 t2 = mat4_mul_vec4(vp, vec4_from_vec3(v2, 1.0f));

                ClipPolygon poly;
                poly.count = 3;
                poly.vertices[0] = (ClipVertex){ t0, shaded_color };
                poly.vertices[1] = (ClipVertex){ t1, shaded_color };
                poly.vertices[2] = (ClipVertex){ t2, shaded_color };

                if (clip_polygon_against_frustum(&poly) < 3) continue;

                ProjectedVertex pv0 = project_vertex(poly.vertices[0].position);
                for (int j = 1; j < poly.count - 1; j++) {
                    ProjectedVertex pv1 = project_vertex(poly.vertices[j].position);
                    ProjectedVertex pv2 = project_vertex(poly.vertices[j + 1].position);

                    render_fill_triangle_z(
                        (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                        (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                        (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                        poly.vertices[0].color
                    );
                }
            }
        }

        SDL_UpdateTexture(texture, NULL, framebuffer, RENDER_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    LOG_INFO("Shutting down...");

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    LOG_INFO("Goodbye!");
    return 0;
}
