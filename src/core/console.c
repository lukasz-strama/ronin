#include "core/console.h"
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

    int pad = 4;
    int text_y = y0 + pad;

    int max_visible = (CON_HEIGHT - FONT_GLYPH_H - pad * 3) / (FONT_GLYPH_H + 1);
    int start = con->log_count - max_visible;
    if (start < 0)
        start = 0;

    for (int i = start; i < con->log_count; i++)
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
        console_log(con, " help             - show this");
        console_log(con, " spawn teapot     - add teapot");
        console_log(con, " set speed <N>    - rotation spd");
        console_log(con, " toggle wireframe - wireframe");
        console_log(con, " toggle aabb      - bounding box");
        console_log(con, " resume           - back to game");
        console_log(con, " quit             - exit engine");
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
        // Spawn at camera position + 5 units forward
        Vec3 spawn_pos = vec3_add(ctx->camera->position,
                                  vec3_mul(ctx->camera->direction, 5.0f));
        spawn_pos.y = 0.0f;

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
    // --- toggle aabb ---
    else if (strcmp(tokens[0], "toggle") == 0 && ntokens >= 2 &&
             strcmp(tokens[1], "aabb") == 0)
    {
        // Signal handled via return; main reads console.wireframe-like flag
        // Reuse a simple approach: log and let main handle via a flag
        console_log(con, "Use 'B' key to toggle AABB");
    }
    else
    {
        console_log(con, "Unknown command: %s", tokens[0]);
        console_log(con, "Type 'help' for commands.");
    }

    console_clear_input(con);
}
