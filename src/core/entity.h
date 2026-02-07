#ifndef ENTITY_H
#define ENTITY_H

#include "math/math.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "graphics/clip.h"
#include "graphics/render.h"
#include "core/obj_loader.h"
#include <stdbool.h>

#define MAX_ENTITIES 128

typedef enum
{
    ENTITY_MESH,
    ENTITY_OBJ_MESH
} EntityType;

typedef enum
{
    RENDER_FLAT_SHADED,
    RENDER_TEXTURED
} RenderMode;

typedef struct
{
    EntityType type;
    RenderMode render_mode;
    union
    {
        Mesh *mesh;
        OBJMesh *obj_mesh;
    };
    Vec3 position;
    Vec3 rotation;
    Vec3 rotation_speed;
    float scale;
    Texture *texture;
    float uv_scale;
    bool active;
} Entity;

typedef struct
{
    Entity entities[MAX_ENTITIES];
    int count;
} Scene;

void scene_init(Scene *scene);
int scene_add_mesh(Scene *scene, Mesh *mesh, Vec3 position, float scale);
int scene_add_obj(Scene *scene, OBJMesh *obj, Vec3 position, float scale);
void scene_set_rotation_speed(Scene *scene, int idx, Vec3 speed);
void scene_set_texture(Scene *scene, int idx, Texture *tex, float uv_scale);
void scene_update(Scene *scene, float dt);
void scene_render(Scene *scene, Mat4 vp, Vec3 camera_pos, Vec3 light_dir);

AABB entity_get_world_aabb(const Entity *ent);

#endif
