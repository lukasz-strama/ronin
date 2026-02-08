#ifndef LEVEL_H
#define LEVEL_H

#include "core/entity.h"
#include "core/obj_loader.h"
#include "core/camera.h"
#include "core/collision_grid.h"
#include "core/chunk.h"
#include <stdbool.h>

#define ENTITY_TYPE_MAX_LEN 32
#define LEVEL_MESH_PATH_MAX 256
#define MAX_ENTITY_DEFS 64

typedef struct
{
    char type[ENTITY_TYPE_MAX_LEN];
    float x, y, z;
    float rot_y;
} EntityDef;

typedef struct
{
    char mesh_path[LEVEL_MESH_PATH_MAX];
    EntityDef entities[MAX_ENTITY_DEFS];
    int entity_count;
    bool has_mesh;
} LevelData;

// Load a level file and populate the scene.
// If the level has a mesh defined, it loads the map and collision grid.
int level_load(const char *path, Scene *scene, Camera *camera,
               OBJMesh *teapot, Mesh *cube_mesh,
               OBJMesh *map_out, CollisionGrid *grid_out,
               ChunkGrid *chunk_grid_out,
               char *map_path_out);

// Load an OBJ file as world geometry, clearing current scene.
// The loaded mesh is stored in map_out for rendering.
// The collision grid is built and assigned to camera.
int level_load_map(const char *obj_path, Scene *scene, Camera *camera,
                   OBJMesh *map_out, CollisionGrid *grid_out,
                   ChunkGrid *chunk_grid_out);

int level_save(const char *path, const Scene *scene, const Camera *camera,
               const char *map_path);

#endif
