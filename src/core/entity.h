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
    bool pickable;
    float hit_timer;
} Entity;

typedef struct
{
    Entity entities[MAX_ENTITIES];
    int count;
} Scene;

typedef struct RenderStats
{
    int entities_culled; // Frustum-culled entities
    int backface_culled; // Triangles discarded by backface test
    int triangles_drawn; // Triangles sent to rasterizer
    int clip_trivial;    // Triangles that skipped clipping (trivial accept)
} RenderStats;

void scene_init(Scene *scene);
int scene_add_mesh(Scene *scene, Mesh *mesh, Vec3 position, float scale);
int scene_add_obj(Scene *scene, OBJMesh *obj, Vec3 position, float scale);
void scene_set_rotation_speed(Scene *scene, int idx, Vec3 speed);
void scene_set_texture(Scene *scene, int idx, Texture *tex, float uv_scale);
void scene_update(Scene *scene, float dt);
void scene_render(Scene *scene, Mat4 vp, Vec3 camera_pos, Vec3 light_dir,
                  const Frustum *frustum, bool backface_cull,
                  RenderStats *stats_out);
void scene_render_wireframe(Scene *scene, Mat4 vp, Vec3 camera_pos,
                            const Frustum *frustum, bool backface_cull,
                            RenderStats *stats_out);

AABB entity_get_world_aabb(const Entity *ent);
int scene_ray_pick(const Scene *scene, Ray ray, float *t_out);

// Projectile system
#define MAX_PROJECTILES 32
#define PROJECTILE_SPEED 20.0f
#define PROJECTILE_LIFETIME 5.0f
#define PROJECTILE_HALF_SIZE 0.15f
#define HIT_FLASH_DURATION 0.5f

typedef struct
{
    Vec3 position;
    Vec3 direction;
    float lifetime;
    bool active;
} Projectile;

#endif
