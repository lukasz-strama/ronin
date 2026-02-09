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

int hud_font_init(Font *font);
void hud_font_free(Font *font);
void hud_blit_rect(int x, int y, int w, int h, uint32_t color);
void hud_draw_char(const Font *font, int x, int y, char c, uint32_t color);
void hud_draw_text(const Font *font, int x, int y, const char *text, uint32_t color);
void hud_draw_crosshair(uint32_t color);
void hud_draw_fps(const Font *font, float dt);

typedef enum
{
    MENU_MAIN = 0,
    MENU_SETTINGS,
    MENU_SETTINGS_GRAPHICS,
    MENU_SETTINGS_AUDIO,
    MENU_SETTINGS_VIDEO,
} MenuState;

typedef struct
{
    bool *backface_cull;
    bool *frustum_cull;
    bool *wireframe;
    bool *debug_info;
    bool *draw_aabb;
    float *fog_end;
    bool *vsync;
} MenuData;

int hud_draw_pause_menu(const Font *font, int mx, int my, bool clicked, bool mouse_down, MenuState *state, MenuData *data);
void hud_draw_cull_stats(const Font *font, const struct RenderStats *stats, int total_entities);

#endif
