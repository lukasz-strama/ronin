#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include "graphics/hud.h"
#include "core/entity.h"
#include "core/camera.h"
#include "core/obj_loader.h"
#include "core/collision_grid.h"
#include "graphics/mesh.h"

typedef enum
{
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_CONSOLE
} GameState;

#define CONSOLE_INPUT_MAX 128
#define CONSOLE_LOG_LINES 128
#define CONSOLE_LOG_LINE_LEN 64

typedef struct
{
    char input[CONSOLE_INPUT_MAX];
    int cursor;

    char log[CONSOLE_LOG_LINES][CONSOLE_LOG_LINE_LEN];
    int log_count;
    int scroll_offset; // 0 = standard (bottom), > 0 = lines scrolled up

    bool wireframe;
    bool debug_rays;
} Console;

void console_init(Console *con);
void console_clear_input(Console *con);
void console_push_char(Console *con, char c);
void console_pop_char(Console *con);
void console_log(Console *con, const char *fmt, ...);
void console_scroll(Console *con, int delta);
void console_draw(const Console *con, const Font *font);

// Command execution (needs access to scene + camera for mutation)
typedef struct
{
    Scene *scene;
    Camera *camera;
    OBJMesh *teapot;
    Mesh *cube_mesh;
    OBJMesh *loaded_map;
    CollisionGrid *collision_grid;
    char *current_map_path;
    bool *running;
    Console *console;
    GameState *state;
    int *selected_entity;
} CommandContext;

void console_execute(Console *con, CommandContext *ctx);

#endif
