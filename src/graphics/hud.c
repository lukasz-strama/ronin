#include "graphics/hud.h"
#include "graphics/render.h"
#include "core/entity.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 8x8 bitmap font for printable ASCII (32-126)
// Convention: bit 0 = leftmost pixel (x=0), bit 7 = rightmost (x=7)
static const uint8_t FONT_GLYPHS[95][8] = {
    // 32-47: Space, punctuation, digits prefix
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //   (space)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // #
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // %
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // &
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // (
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // )
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // *
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // ,
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // .
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // /

    // 48-57: Digits
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 0
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 1
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // 2
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // 3
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // 4
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // 5
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // 6
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // 7
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // 8
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // 9

    // 58-64: Punctuation
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // :
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // ;
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // <
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // =
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // >
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // ?
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // @

    // 65-90: Uppercase A-Z
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // A
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // B
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // C
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // D
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // E
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // F
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // G
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // H
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // I
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // J
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // K
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // L
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // M
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // O
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // P
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // Q
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // R
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // S
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // T
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // X
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // Y
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // Z

    // 91-96: Brackets, backslash, caret, underscore, backtick
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // [
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // backslash
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // ]
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // _
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // `

    // 97-122: Lowercase a-z
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // a
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // b
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // c
    {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00}, // d
    {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00}, // e
    {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00}, // f
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // g
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // h
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // i
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // j
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // k
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // l
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // m
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // n
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // o
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // p
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // q
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // r
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // s
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // t
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // u
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // v
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // w
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // x
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // y
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // z

    // 123-126: Braces, pipe, tilde
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // {
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // |
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // }
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~
};

int hud_font_init(Font *font)
{
    font->cols = 16;
    int glyph_count = FONT_LAST_CHAR - FONT_FIRST_CHAR + 1;
    int rows = (glyph_count + font->cols - 1) / font->cols;

    font->atlas.width = font->cols * FONT_GLYPH_W;
    font->atlas.height = rows * FONT_GLYPH_H;
    font->atlas.pixels = (uint32_t *)malloc(
        font->atlas.width * font->atlas.height * sizeof(uint32_t));

    if (!font->atlas.pixels)
    {
        LOG_ERROR("Failed to allocate font atlas");
        return 1;
    }

    memset(font->atlas.pixels, 0,
           font->atlas.width * font->atlas.height * sizeof(uint32_t));

    // Bake glyphs into atlas texture
    for (int i = 0; i < glyph_count; i++)
    {
        int col = i % font->cols;
        int row = i / font->cols;
        int ox = col * FONT_GLYPH_W;
        int oy = row * FONT_GLYPH_H;

        for (int y = 0; y < FONT_GLYPH_H; y++)
        {
            uint8_t bits = FONT_GLYPHS[i][y];
            for (int x = 0; x < FONT_GLYPH_W; x++)
            {
                if (bits & (1 << x))
                {
                    font->atlas.pixels[(oy + y) * font->atlas.width + (ox + x)] =
                        0xFFFFFFFF;
                }
            }
        }
    }

    LOG_INFO("Font atlas generated (%dx%d, %d glyphs)",
             font->atlas.width, font->atlas.height, glyph_count);
    return 0;
}

void hud_font_free(Font *font)
{
    texture_free(&font->atlas);
}

void hud_blit_rect(int x, int y, int w, int h, uint32_t color)
{
    for (int py = y; py < y + h; py++)
    {
        for (int px = x; px < x + w; px++)
        {
            render_set_pixel(px, py, color);
        }
    }
}

void hud_draw_char(const Font *font, int x, int y, char c, uint32_t color)
{
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR)
        return;

    int index = c - FONT_FIRST_CHAR;
    int col = index % font->cols;
    int row = index / font->cols;
    int ox = col * FONT_GLYPH_W;
    int oy = row * FONT_GLYPH_H;

    for (int py = 0; py < FONT_GLYPH_H; py++)
    {
        for (int px = 0; px < FONT_GLYPH_W; px++)
        {
            uint32_t atlas_pixel =
                font->atlas.pixels[(oy + py) * font->atlas.width + (ox + px)];
            if (atlas_pixel & 0x00FFFFFF)
            {
                render_set_pixel(x + px, y + py, color);
            }
        }
    }
}

void hud_draw_text(const Font *font, int x, int y, const char *text, uint32_t color)
{
    int cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++)
    {
        hud_draw_char(font, cursor_x, y, text[i], color);
        cursor_x += FONT_GLYPH_W;
    }
}

void hud_draw_crosshair(uint32_t color)
{
    int center_x = RENDER_WIDTH / 2;
    int center_y = RENDER_HEIGHT / 2;
    render_set_pixel(center_x, center_y, color);
}

void hud_draw_fps(const Font *font, float dt)
{
    static float smoothed_fps = -1.0f;

    float fps = (dt > 0.0001f) ? 1.0f / dt : 0.0f;
    if (smoothed_fps < 0.0f)
    {
        smoothed_fps = fps;
    }
    else
    {
        smoothed_fps = smoothed_fps * 0.95f + fps * 0.05f;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "FPS:%4.0f", smoothed_fps);

    int text_w = (int)strlen(buf) * FONT_GLYPH_W;

    // Dark background for readability
    hud_blit_rect(2, 2, text_w + 4, FONT_GLYPH_H + 4, 0xFF0A0A0A);

    // Drop shadow + text
    hud_draw_text(font, 5, 5, buf, 0xFF000000);
    hud_draw_text(font, 4, 4, buf, 0xFF00FF00);
}

static bool hud_button(const Font *font, int x, int y, int w, int h, const char *text, int mx, int my, bool clicked)
{
    bool hover = (mx >= x && mx < x + w && my >= y && my < y + h);
    uint32_t bg_color = hover ? 0xFF666666 : 0xFF333333;
    uint32_t text_col = hover ? 0xFFFFFFFF : 0xFFAAAAAA;
    uint32_t border_col = 0xFF888888;

    hud_blit_rect(x, y, w, h, bg_color);
    
    // Border
    for (int px = x; px < x + w; px++) { render_set_pixel(px, y, border_col); render_set_pixel(px, y + h - 1, border_col); }
    for (int py = y; py < y + h; py++) { render_set_pixel(x, py, border_col); render_set_pixel(x + w - 1, py, border_col); }

    int tw = (int)strlen(text) * FONT_GLYPH_W;
    int tx = x + (w - tw) / 2;
    int ty = y + (h - FONT_GLYPH_H) / 2;

    hud_draw_text(font, tx, ty, text, text_col);
    return hover && clicked;
}

static bool hud_checkbox(const Font *font, int x, int y, int w, int h, const char *text, bool *value, int mx, int my, bool clicked)
{
    bool hover = (mx >= x && mx < x + w && my >= y && my < y + h);
    uint32_t bg_color = hover ? 0xFF444444 : 0xFF222222;
    uint32_t text_col = hover ? 0xFFFFFFFF : 0xFFAAAAAA;
    uint32_t border_col = 0xFF888888;

    if (hover && clicked) *value = !(*value);

    // Draw box
    hud_blit_rect(x, y, h, h, bg_color);
    // Draw Border
    for (int px = x; px < x + h; px++) { render_set_pixel(px, y, border_col); render_set_pixel(px, y + h - 1, border_col); }
    for (int py = y; py < y + h; py++) { render_set_pixel(x, py, border_col); render_set_pixel(x + h - 1, py, border_col); }

    // Checkmark
    if (*value)
    {
        int pad = 3;
        hud_blit_rect(x + pad, y + pad, h - pad * 2, h - pad * 2, 0xFF00FF00);
    }

    // Label
    hud_draw_text(font, x + h + 8, y + (h - FONT_GLYPH_H)/2, text, text_col);
    
    return hover && clicked;
}

static bool hud_slider(const Font *font, int x, int y, int w, int h, const char *text, float *value, float min, float max, int mx, int my, bool mouse_down)
{
    bool hover = (mx >= x && mx < x + w && my >= y && my < y + h);
    uint32_t bg_color = 0xFF222222;
    uint32_t fill_color = 0xFF44AA44;
    uint32_t empty_color = 0xFF444444;
    uint32_t handle_color = hover ? 0xFFFFFFFF : 0xFFCCCCCC;
    uint32_t text_col = hover ? 0xFFFFFFFF : 0xFFAAAAAA;
    uint32_t border_col = 0xFF888888;

    // Allow dragging if mouse is down while hovering
    if (mouse_down && hover)
    {
        float t = (float)(mx - x) / (float)w;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        *value = min + t * (max - min);
    }

    // Text Label (Centered above)
    int label_w = (int)strlen(text) * FONT_GLYPH_W;
    int label_x = x + (w - label_w) / 2;
    hud_draw_text(font, label_x, y - FONT_GLYPH_H - 4, text, text_col);

    // Value Text (Top Right)
    char val_buf[16];
    snprintf(val_buf, sizeof(val_buf), "%.0f", *value);
    int val_w = (int)strlen(val_buf) * FONT_GLYPH_W;
    hud_draw_text(font, x + w - val_w, y - FONT_GLYPH_H - 4, val_buf, text_col);

    // Background Container
    hud_blit_rect(x, y, w, h, bg_color);
    
    // Border
    for (int px = x; px < x + w; px++) { render_set_pixel(px, y, border_col); render_set_pixel(px, y + h - 1, border_col); }
    for (int py = y; py < y + h; py++) { render_set_pixel(x, py, border_col); render_set_pixel(x + w - 1, py, border_col); }

    // Bar visualization
    int pad = 4;
    int bar_x = x + pad;
    int bar_y = y + h/2 - 2;
    int bar_w = w - pad*2;
    int bar_h = 4;
    
    float norm = (*value - min) / (max - min);
    int fill_w = (int)(norm * bar_w);
    
    // Empty part
    hud_blit_rect(bar_x, bar_y, bar_w, bar_h, empty_color);
    // Filled part
    hud_blit_rect(bar_x, bar_y, fill_w, bar_h, fill_color);

    // Handle
    int handle_w = 8;
    int handle_h = h - 6;
    int handle_x = bar_x + fill_w - handle_w / 2;
    
    // Clamp handle within bar area
    if (handle_x < bar_x) handle_x = bar_x;
    if (handle_x > bar_x + bar_w - handle_w) handle_x = bar_x + bar_w - handle_w;
    
    int handle_y = y + 3;
    hud_blit_rect(handle_x, handle_y, handle_w, handle_h, handle_color);

    return hover && mouse_down;
}

int hud_draw_pause_menu(const Font *font, int mx, int my, bool clicked, bool mouse_down, MenuState *state, MenuData *data)
{
    // Dim overlay
    for (int py = 0; py < RENDER_HEIGHT; py++)
        for (int px = 0; px < RENDER_WIDTH; px++)
            if ((px + py) % 2 == 0) render_set_pixel(px, py, 0xFF000000);

    int cx = RENDER_WIDTH / 2;
    int cy = RENDER_HEIGHT / 2;
    int btn_w = 120;
    int btn_h = 20;
    int spacing = 6;

    if (*state == MENU_MAIN)
    {
        hud_draw_text(font, cx - (6 * FONT_GLYPH_W) / 2, cy - 60, "PAUSED", 0xFFFFFFFF);
        int start_y = cy - 20;
        
        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "RESUME", mx, my, clicked)) return 1;
        start_y += btn_h + spacing;
        
        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "SETTINGS", mx, my, clicked)) *state = MENU_SETTINGS;
        start_y += btn_h + spacing;

        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "CONSOLE", mx, my, clicked)) return 2;
        start_y += btn_h + spacing;
        
        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "QUIT", mx, my, clicked)) return 3;
    }
    else if (*state == MENU_SETTINGS)
    {
        hud_draw_text(font, cx - (8 * FONT_GLYPH_W) / 2, cy - 60, "SETTINGS", 0xFFFFFFFF);
        int start_y = cy - 20;

        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "VIDEO", mx, my, clicked)) *state = MENU_SETTINGS_VIDEO;
        start_y += btn_h + spacing;

        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "GRAPHICS", mx, my, clicked)) *state = MENU_SETTINGS_GRAPHICS;
        start_y += btn_h + spacing;

        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "AUDIO", mx, my, clicked)) *state = MENU_SETTINGS_AUDIO;
        start_y += btn_h + spacing;

        if (hud_button(font, cx - btn_w / 2, start_y, btn_w, btn_h, "BACK", mx, my, clicked)) *state = MENU_MAIN;
    }
    else if (*state == MENU_SETTINGS_GRAPHICS)
    {
        hud_draw_text(font, cx - (8 * FONT_GLYPH_W) / 2, cy - 60, "GRAPHICS", 0xFFFFFFFF);
        int start_y = cy - 20;
        
        int check_h = 16;
        int check_w = 200;  // ample width for text
        int ox = cx - 60;   // offset left for checkbox

        hud_checkbox(font, ox, start_y, check_w, check_h, "Frustum Culling", data->frustum_cull, mx, my, clicked);
        start_y += check_h + spacing;

        hud_checkbox(font, ox, start_y, check_w, check_h, "Backface Culling", data->backface_cull, mx, my, clicked);
        start_y += check_h + spacing;

        hud_checkbox(font, ox, start_y, check_w, check_h, "Wireframe Mode", data->wireframe, mx, my, clicked);
        start_y += check_h + spacing;

        if (data->fog_end)
        {
            start_y += 10;
            int slider_x = cx - check_w / 2;
            hud_slider(font, slider_x, start_y, check_w, check_h, "Fog Distance", data->fog_end, 50.0f, 1000.0f, mx, my, mouse_down);
            start_y += check_h + spacing + 10;
        }

        if (hud_button(font, cx - btn_w / 2, start_y + 10, btn_w, btn_h, "BACK", mx, my, clicked)) *state = MENU_SETTINGS;
    }
    else if (*state == MENU_SETTINGS_VIDEO)
    {
        hud_draw_text(font, cx - (5 * FONT_GLYPH_W) / 2, cy - 60, "VIDEO", 0xFFFFFFFF);
        int start_y = cy - 20;
        
        int check_h = 16;
        int check_w = 200;
        int ox = cx - 60;

        if (data->vsync)
        {
            hud_checkbox(font, ox, start_y, check_w, check_h, "VSync", data->vsync, mx, my, clicked);
            start_y += check_h + spacing;
        }

        if (hud_button(font, cx - btn_w / 2, start_y + 30, btn_w, btn_h, "BACK", mx, my, clicked)) *state = MENU_SETTINGS;
    }
    else if (*state == MENU_SETTINGS_AUDIO)
    {
        hud_draw_text(font, cx - (5 * FONT_GLYPH_W) / 2, cy - 60, "AUDIO", 0xFFFFFFFF);
        int start_y = cy;
        hud_draw_text(font, cx - 40, start_y, "(Empty)", 0xFFAAAAAA);
        if (hud_button(font, cx - btn_w / 2, start_y + 30, btn_w, btn_h, "BACK", mx, my, clicked)) *state = MENU_SETTINGS;
    }

    return 0;
}

void hud_draw_cull_stats(const Font *font, const RenderStats *stats, int total_entities)
{
    // Line 1: visible entities
    char buf1[32];
    int visible = total_entities - stats->entities_culled;
    snprintf(buf1, sizeof(buf1), "ENT:%d/%d", visible, total_entities);

    // Line 2: triangles drawn / backface-culled
    char buf2[32];
    snprintf(buf2, sizeof(buf2), "TRI:%d BF:%d", stats->triangles_drawn, stats->backface_culled);

    // Line 3: clip-skipped triangles (trivially accepted)
    char buf3[32];
    snprintf(buf3, sizeof(buf3), "CL:%d skip", stats->clip_trivial);

    // Line 4: chunk stats (only shown when chunks are active)
    char buf4[32];
    int num_lines = 3;
    if (stats->chunks_total > 0)
    {
        int ch_visible = stats->chunks_total - stats->chunks_culled;
        snprintf(buf4, sizeof(buf4), "CHK:%d/%d", ch_visible, stats->chunks_total);
        num_lines = 4;
    }

    int len1 = (int)strlen(buf1) * FONT_GLYPH_W;
    int len2 = (int)strlen(buf2) * FONT_GLYPH_W;
    int len3 = (int)strlen(buf3) * FONT_GLYPH_W;
    int text_w = len1 > len2 ? len1 : len2;
    if (len3 > text_w)
        text_w = len3;
    if (num_lines == 4)
    {
        int len4 = (int)strlen(buf4) * FONT_GLYPH_W;
        if (len4 > text_w)
            text_w = len4;
    }
    int x = RENDER_WIDTH - text_w - 6;
    int y = 2;
    int line_h = FONT_GLYPH_H + 2;

    hud_blit_rect(x - 2, y, text_w + 4, line_h * num_lines + 4, 0xFF0A0A0A);

    hud_draw_text(font, x + 1, y + 3, buf1, 0xFF000000);
    hud_draw_text(font, x, y + 2, buf1, 0xFF00CCFF);

    hud_draw_text(font, x + 1, y + 3 + line_h, buf2, 0xFF000000);
    hud_draw_text(font, x, y + 2 + line_h, buf2, 0xFF00CCFF);

    hud_draw_text(font, x + 1, y + 3 + line_h * 2, buf3, 0xFF000000);
    hud_draw_text(font, x, y + 2 + line_h * 2, buf3, 0xFF00CCFF);

    if (num_lines == 4)
    {
        hud_draw_text(font, x + 1, y + 3 + line_h * 3, buf4, 0xFF000000);
        hud_draw_text(font, x, y + 2 + line_h * 3, buf4, 0xFF88FF88);
    }
}
