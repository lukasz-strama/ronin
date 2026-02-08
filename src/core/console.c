#include "core/console.h"
#include "core/level.h"
#include "core/log.h"
#include "graphics/render.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// Console overlay dimensions (in render-space pixels)
#define CON_MARGIN_X 6
#define CON_MARGIN_Y 6
#define CON_WIDTH (RENDER_WIDTH - CON_MARGIN_X * 2)
#define CON_HEIGHT 120
#define CON_BG_COLOR 0xDD0A0A0A
#define CON_BORDER 0xFF333333
#define CON_TEXT_CLR 0xFF00FF00
#define CON_PROMPT 0xFFFFFF00
#define CON_LOG_CLR 0xFFAAAAAA

void console_init(Console *con)
{
    memset(con, 0, sizeof(Console));
    con->wireframe = false;
    con->debug_rays = false;
    con->backface_cull = true;
    con->show_debug = true;
    LOG_INFO("Console initialized");
}

void console_clear_input(Console *con)
{
    memset(con->input, 0, CONSOLE_INPUT_MAX);
    con->cursor = 0;
}

void console_push_char(Console *con, char c)
{
    if (con->cursor < CONSOLE_INPUT_MAX - 1)
    {
        con->input[con->cursor++] = c;
        con->input[con->cursor] = '\0';
    }
}

void console_pop_char(Console *con)
{
    if (con->cursor > 0)
    {
        con->cursor--;
        con->input[con->cursor] = '\0';
    }
}

void console_log(Console *con, const char *fmt, ...)
{
    // Shift lines up if full
    if (con->log_count >= CONSOLE_LOG_LINES)
    {
        for (int i = 0; i < CONSOLE_LOG_LINES - 1; i++)
        {
            memcpy(con->log[i], con->log[i + 1], CONSOLE_LOG_LINE_LEN);
        }
        con->log_count = CONSOLE_LOG_LINES - 1;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(con->log[con->log_count], CONSOLE_LOG_LINE_LEN, fmt, args);
    va_end(args);
    con->log_count++;

    // Reset scroll when new input comes in
    con->scroll_offset = 0;
}

#define CON_PAD 4
#define CON_MAX_VISIBLE ((CON_HEIGHT - FONT_GLYPH_H - CON_PAD * 3) / (FONT_GLYPH_H + 1))

void console_scroll(Console *con, int delta)
{
    con->scroll_offset += delta;
    if (con->scroll_offset < 0)
        con->scroll_offset = 0;

    int min_visible = con->log_count < CON_MAX_VISIBLE ? con->log_count : CON_MAX_VISIBLE;
    int max_offset = con->log_count - min_visible;

    if (con->scroll_offset > max_offset)
        con->scroll_offset = max_offset;
}

void console_draw(const Console *con, const Font *font)
{
    int x0 = CON_MARGIN_X;
    int y0 = CON_MARGIN_Y;

    // Semi-transparent background
    for (int py = y0; py < y0 + CON_HEIGHT; py++)
    {
        for (int px = x0; px < x0 + CON_WIDTH; px++)
        {
            render_set_pixel(px, py, CON_BG_COLOR);
        }
    }

    for (int px = x0; px < x0 + CON_WIDTH; px++)
    {
        render_set_pixel(px, y0, CON_BORDER);
        render_set_pixel(px, y0 + CON_HEIGHT - 1, CON_BORDER);
    }

    for (int py = y0; py < y0 + CON_HEIGHT; py++)
    {
        render_set_pixel(x0, py, CON_BORDER);
        render_set_pixel(x0 + CON_WIDTH - 1, py, CON_BORDER);
    }

    int pad = CON_PAD;
    int text_y = y0 + pad;

    int max_visible = CON_MAX_VISIBLE;

    int end_index = con->log_count - con->scroll_offset;
    if (end_index > con->log_count)
        end_index = con->log_count;

    int start_index = end_index - max_visible;
    if (start_index < 0)
        start_index = 0;

    static int last_offset = -1;
    if (con->scroll_offset != last_offset)
    {
        last_offset = con->scroll_offset;
    }

    for (int i = start_index; i < end_index; i++)
    {
        hud_draw_text(font, x0 + pad, text_y, con->log[i], CON_LOG_CLR);
        text_y += FONT_GLYPH_H + 1;
    }

    // Separator line above input
    int input_y = y0 + CON_HEIGHT - FONT_GLYPH_H - pad - 2;
    for (int px = x0 + 1; px < x0 + CON_WIDTH - 1; px++)
    {
        render_set_pixel(px, input_y - 2, CON_BORDER);
    }

    hud_draw_text(font, x0 + pad, input_y, ">", CON_PROMPT);

    hud_draw_text(font, x0 + pad + FONT_GLYPH_W + 2, input_y,
                  con->input, CON_TEXT_CLR);

    static int blink = 0;
    blink++;
    if ((blink / 20) % 2 == 0)
    {
        int cx = x0 + pad + FONT_GLYPH_W + 2 + con->cursor * FONT_GLYPH_W;
        hud_blit_rect(cx, input_y, FONT_GLYPH_W, FONT_GLYPH_H, CON_TEXT_CLR);
    }
}

// Tokenize input and dispatch commands
void console_execute(Console *con, CommandContext *ctx)
{
    if (con->cursor == 0)
        return;

    char cmd[CONSOLE_INPUT_MAX];
    strncpy(cmd, con->input, CONSOLE_INPUT_MAX);
    cmd[CONSOLE_INPUT_MAX - 1] = '\0';

    // Echo command to log (truncate gracefully)
    char echo[CONSOLE_LOG_LINE_LEN];
    snprintf(echo, sizeof(echo), "> %.60s", cmd);
    console_log(con, "%s", echo);

    LOG_INFO("Console command: %s", cmd);

    // Tokenize (space-separated)
    char *tokens[8] = {0};
    int ntokens = 0;
    char *tok = strtok(cmd, " \t");
    while (tok && ntokens < 8)
    {
        tokens[ntokens++] = tok;
        tok = strtok(NULL, " \t");
    }

    if (ntokens == 0)
    {
        console_clear_input(con);
        return;
    }

    // --- help ---
    if (strcmp(tokens[0], "help") == 0)
    {
        console_log(con, "Commands:");
        console_log(con, " help               - show this");
        console_log(con, " spawn teapot       - add teapot");
        console_log(con, " move <#|sel> x y z - move entity");
        console_log(con, " deselect           - clear sel");
        console_log(con, " fly                - toggle fly");
        console_log(con, " fly_speed <N>      - fly speed");
        console_log(con, " set speed <N>      - game speed");
        console_log(con, " toggle wireframe   - wireframe");
        console_log(con, " toggle backface    - backface cull");
        console_log(con, " toggle aabb        - bounding box");
        console_log(con, " toggle rays        - ray debug vis");
        console_log(con, " toggle debug       - toggle HUD");
        console_log(con, " load <file>        - load level/map");
        console_log(con, " save_level <file>  - save (.lvl)");
        console_log(con, " resume             - back to game");
        console_log(con, " quit               - exit");
    }
    // --- quit ---
    else if (strcmp(tokens[0], "quit") == 0 || strcmp(tokens[0], "exit") == 0)
    {
        *ctx->running = false;
    }
    // --- resume ---
    else if (strcmp(tokens[0], "resume") == 0 || strcmp(tokens[0], "close") == 0)
    {
        *ctx->state = GAME_STATE_PLAYING;
    }
    // --- spawn teapot ---
    else if (strcmp(tokens[0], "spawn") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "teapot") == 0)
    {
        // Spawn at camera position + 5 units forward, at camera height
        Vec3 spawn_pos = vec3_add(ctx->camera->position,
                                  vec3_mul(ctx->camera->direction, 5.0f));
        spawn_pos.y -= 1.0f; // Slightly below eye level

        int idx = scene_add_obj(ctx->scene, ctx->teapot, spawn_pos, 0.4f);
        if (idx >= 0)
        {
            scene_set_rotation_speed(ctx->scene, idx, (Vec3){0, 0.8f, 0});
            camera_add_collider(ctx->camera,
                                entity_get_world_aabb(&ctx->scene->entities[idx]));
            console_log(con, "Spawned teapot at (%.1f, %.1f, %.1f)",
                        spawn_pos.x, spawn_pos.y, spawn_pos.z);
        }
        else
        {
            console_log(con, "ERROR: scene full");
        }
    }
    // --- set speed <N> ---
    else if (strcmp(tokens[0], "set") == 0 && ntokens >= 3 &&
             strcmp(tokens[1], "speed") == 0)
    {
        float spd = (float)atof(tokens[2]);
        for (int i = 0; i < ctx->scene->count; i++)
        {
            Entity *ent = &ctx->scene->entities[i];
            Vec3 rs = ent->rotation_speed;
            if (rs.x != 0 || rs.y != 0 || rs.z != 0)
            {
                float len = vec3_length(rs);
                if (len > 0.001f)
                {
                    Vec3 dir = vec3_normalize(rs);
                    ent->rotation_speed = vec3_mul(dir, spd);
                }
            }
        }
        console_log(con, "Rotation speed set to %.2f", spd);
    }
    // --- toggle wireframe ---
    else if (strcmp(tokens[0], "toggle") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "wireframe") == 0)
    {
        con->wireframe = !con->wireframe;
        console_log(con, "Wireframe: %s", con->wireframe ? "ON" : "OFF");
    }
    // --- toggle backface ---
    else if (strcmp(tokens[0], "toggle") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "backface") == 0)
    {
        con->backface_cull = !con->backface_cull;
        console_log(con, "Backface culling: %s", con->backface_cull ? "ON" : "OFF");
    }
    // --- toggle aabb ---
    else if (strcmp(tokens[0], "toggle") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "aabb") == 0)
    {
        if (ctx->debug_aabb)
        {
            *ctx->debug_aabb = !(*ctx->debug_aabb);
            console_log(con, "AABB debug: %s", *ctx->debug_aabb ? "ON" : "OFF");
        }
    }
    // --- toggle rays ---
    else if (strcmp(tokens[0], "toggle") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "rays") == 0)
    {
        con->debug_rays = !con->debug_rays;
        console_log(con, "Ray debug: %s", con->debug_rays ? "ON" : "OFF");
    }
    // --- toggle debug ---
    else if (strcmp(tokens[0], "toggle") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "debug") == 0)
    {
        con->show_debug = !con->show_debug;
        console_log(con, "Debug info: %s", con->show_debug ? "ON" : "OFF");
    }
    // --- deselect ---
    else if (strcmp(tokens[0], "deselect") == 0)
    {
        if (ctx->selected_entity)
            *ctx->selected_entity = -1;
        console_log(con, "Selection cleared");
    }
    // --- load <filename> ---
    else if (strcmp(tokens[0], "load") == 0 && ntokens >= 2)
    {
        char *ext = strrchr(tokens[1], '.');
        if (ext && strcmp(ext, ".lvl") == 0)
        {
            // Call load_level logic
            if (level_load(tokens[1], ctx->scene, ctx->camera,
                           ctx->teapot, ctx->cube_mesh,
                           ctx->loaded_map, ctx->collision_grid,
                           ctx->chunk_grid,
                           ctx->current_map_path) == 0)
                console_log(con, "Loaded level: %s", tokens[1]);
            else
                console_log(con, "ERROR loading level: %s", tokens[1]);
        }
        else if (ext && strcmp(ext, ".obj") == 0)
        {
            // Call load_map logic
            if (level_load_map(tokens[1], ctx->scene, ctx->camera,
                               ctx->loaded_map, ctx->collision_grid,
                               ctx->chunk_grid) == 0)
            {
                if (ctx->current_map_path)
                {
                    strncpy(ctx->current_map_path, tokens[1], 255);
                    ctx->current_map_path[255] = '\0';
                }
                console_log(con, "Loaded map: %s", tokens[1]);
            }
            else
                console_log(con, "ERROR loading map: %s", tokens[1]);
        }
        else
        {
            console_log(con, "Unknown file type: %s (use .lvl or .obj)", tokens[1]);
        }
    }
    // --- load_level <filename> ---
    else if (strcmp(tokens[0], "load_level") == 0 && ntokens >= 2)
    {
        if (level_load(tokens[1], ctx->scene, ctx->camera,
                       ctx->teapot, ctx->cube_mesh,
                       ctx->loaded_map, ctx->collision_grid,
                       ctx->chunk_grid,
                       ctx->current_map_path) == 0)
        {
            console_log(con, "Loaded: %s", tokens[1]);
        }
        else
        {
            console_log(con, "ERROR loading: %s", tokens[1]);
        }
    }
    // --- save_level <filename> ---
    else if (strcmp(tokens[0], "save_level") == 0 && ntokens >= 2)
    {
        if (level_save(tokens[1], ctx->scene, ctx->camera,
                       ctx->current_map_path) == 0)
        {
            console_log(con, "Saved: %s", tokens[1]);
        }
        else
        {
            console_log(con, "ERROR saving: %s", tokens[1]);
        }
    }
    // --- load_map <obj_path> ---
    else if (strcmp(tokens[0], "load_map") == 0 && ntokens >= 2)
    {
        // Warn if loading .lvl with load_map
        char *ext = strrchr(tokens[1], '.');
        if (ext && strcmp(ext, ".lvl") == 0)
        {
            console_log(con, "ERROR: Use 'load_level' for .lvl files!");
        }
        else if (level_load_map(tokens[1], ctx->scene, ctx->camera,
                                ctx->loaded_map, ctx->collision_grid,
                                ctx->chunk_grid) == 0)
        {
            // Track the loaded map path for save
            if (ctx->current_map_path)
            {
                strncpy(ctx->current_map_path, tokens[1], 255);
                ctx->current_map_path[255] = '\0';
            }
            console_log(con, "Map loaded: %s", tokens[1]);
        }
        else
        {
            console_log(con, "ERROR loading map: %s", tokens[1]);
        }
    }
    // --- move <entity#|selected> <x> <y> <z> ---
    else if (strcmp(tokens[0], "move") == 0 && ntokens >= 4)
    {
        int ent_idx = -1;

        // Check if using selected entity or entity number
        if (strcmp(tokens[1], "selected") == 0 || strcmp(tokens[1], "sel") == 0)
        {
            ent_idx = *ctx->selected_entity;
        }
        else
        {
            ent_idx = atoi(tokens[1]);
        }

        if (ent_idx >= 0 && ent_idx < ctx->scene->count &&
            ctx->scene->entities[ent_idx].active)
        {
            float x = (float)atof(tokens[2]);
            float y = (float)atof(tokens[3]);
            float z = ntokens >= 5 ? (float)atof(tokens[4]) : 0.0f;
            ctx->scene->entities[ent_idx].position = (Vec3){x, y, z};
            console_log(con, "Entity %d -> (%.1f, %.1f, %.1f)", ent_idx, x, y, z);
        }
        else
        {
            console_log(con, "Invalid entity: %s", tokens[1]);
        }
    }
    // --- fly ---
    else if (strcmp(tokens[0], "fly") == 0)
    {
        ctx->camera->fly_mode = !ctx->camera->fly_mode;
        console_log(con, "Fly mode: %s", ctx->camera->fly_mode ? "ON" : "OFF");
    }
    // --- fly_speed <N> ---
    else if (strcmp(tokens[0], "fly_speed") == 0 && ntokens >= 2)
    {
        float spd = (float)atof(tokens[1]);
        if (spd < 0.1f) spd = 0.1f;
        ctx->camera->fly_speed = spd;
        console_log(con, "Fly speed set to %.2f", spd);
    }
    else
    {
        console_log(con, "Unknown command: %s", tokens[0]);
        console_log(con, "Type 'help' for commands.");
    }

    console_clear_input(con);
}
