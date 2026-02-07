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

    Mesh cube = mesh_cube();
    LOG_INFO("Loaded cube mesh: %d vertices, %d faces", cube.vertex_count, cube.face_count);

    float aspect = (float)RENDER_WIDTH / (float)RENDER_HEIGHT;
    Mat4 proj = mat4_perspective(PI / 3.0f, aspect, 0.1f, 100.0f);

    Vec3 camera_pos    = (Vec3){ 0, 0, -5 };
    Vec3 camera_target = (Vec3){ 0, 0, 0 };
    Vec3 camera_up     = (Vec3){ 0, 1, 0 };
    Mat4 view = mat4_look_at(camera_pos, camera_target, camera_up);

    Vec3 light_dir = vec3_normalize((Vec3){ 0, 0, -1 });

    float rotation = 0.0f;
    Uint32 prev_time = SDL_GetTicks();

    LOG_INFO("Entering main loop");

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
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        rotation += dt;

        Mat4 rot_y = mat4_rotate_y(rotation);
        Mat4 rot_x = mat4_rotate_x(rotation * 0.5f);
        Mat4 model = mat4_mul(rot_y, rot_x);

        Mat4 mv  = mat4_mul(view, model);
        Mat4 mvp = mat4_mul(proj, mv);

        framebuffer_clear(COLOR_BLACK);
        render_clear_zbuffer();

        for (int i = 0; i < cube.face_count; i++) {
            Face face = cube.faces[i];

            Vec3 v0 = cube.vertices[face.a];
            Vec3 v1 = cube.vertices[face.b];
            Vec3 v2 = cube.vertices[face.c];

            Vec4 w0 = mat4_mul_vec4(model, vec4_from_vec3(v0, 1.0f));
            Vec4 w1 = mat4_mul_vec4(model, vec4_from_vec3(v1, 1.0f));
            Vec4 w2 = mat4_mul_vec4(model, vec4_from_vec3(v2, 1.0f));

            Vec3 world_v0 = vec3_from_vec4(w0);
            Vec3 world_v1 = vec3_from_vec4(w1);
            Vec3 world_v2 = vec3_from_vec4(w2);

            Vec3 edge1  = vec3_sub(world_v1, world_v0);
            Vec3 edge2  = vec3_sub(world_v2, world_v0);
            Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

            // Backface culling
            Vec3 face_center = vec3_mul(vec3_add(vec3_add(world_v0, world_v1), world_v2), 1.0f / 3.0f);
            Vec3 view_dir = vec3_normalize(vec3_sub(camera_pos, face_center));
            if (vec3_dot(normal, view_dir) < 0) continue;

            // Flat shading intensity (compute before clipping)
            float intensity = vec3_dot(normal, light_dir);
            if (intensity < 0) intensity = 0;
            intensity = 0.2f + intensity * 0.8f;
            uint32_t shaded_color = shade_color(face.color, intensity);

            Vec4 t0 = mat4_mul_vec4(mvp, vec4_from_vec3(v0, 1.0f));
            Vec4 t1 = mat4_mul_vec4(mvp, vec4_from_vec3(v1, 1.0f));
            Vec4 t2 = mat4_mul_vec4(mvp, vec4_from_vec3(v2, 1.0f));

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
