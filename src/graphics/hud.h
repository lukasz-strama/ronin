#ifndef HUD_H
#define HUD_H

#include <stdint.h>
#include "graphics/texture.h"

#define FONT_GLYPH_W 8
#define FONT_GLYPH_H 8
#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126

typedef struct
{
    Texture atlas; // Font atlas texture (16 cols x 6 rows of 8x8 glyphs)
    int cols;      // Characters per row in atlas
} Font;

struct RenderStats;

// Font atlas
int hud_font_init(Font *font);
void hud_font_free(Font *font);

// Sprite blitting (2D rect directly to framebuffer, bypasses 3D pipeline)
void hud_blit_rect(int x, int y, int w, int h, uint32_t color);

// Text rendering
void hud_draw_char(const Font *font, int x, int y, char c, uint32_t color);
void hud_draw_text(const Font *font, int x, int y, const char *text, uint32_t color);

// UI elements
void hud_draw_crosshair(uint32_t color);
void hud_draw_fps(const Font *font, float dt);
// Returns: 0=None, 1=Resume, 2=Console, 3=Quit
int hud_draw_pause_menu(const Font *font, int mx, int my, bool clicked);
void hud_draw_cull_stats(const Font *font, const struct RenderStats *stats, int total_entities);

#endif
