#ifndef CAMERA_H
#define CAMERA_H

#include "math/math.h"
#include <stdbool.h>

struct CollisionGrid;

#define CAMERA_WALK_SPEED 5.0f
#define CAMERA_DEFAULT_FLY_SPEED 20.0f
#define CAMERA_SENSITIVITY 0.002f
#define CAMERA_PITCH_LIMIT 1.55f // ~89 degrees

#define CAMERA_HALF_W 0.3f
#define CAMERA_HALF_H 0.9f
#define CAMERA_HALF_D 0.3f

#define GRAVITY 15.0f
#define JUMP_VELOCITY 7.0f

#define CAMERA_EYE_HEIGHT 2.0f

#define MAX_COLLIDERS 16

typedef struct
{
    Vec3 position;
    Vec3 direction; // Forward vector (computed from yaw/pitch)
    Vec3 right;     // Right vector (computed)
    float yaw;      // Horizontal rotation (radians)
    float pitch;    // Vertical rotation (radians)

    float fly_speed;

    float velocity_y;
    bool grounded;

    // Collision world
    AABB colliders[MAX_COLLIDERS];
    int collider_count;
    bool fly_mode;
    struct CollisionGrid *map_grid;
} Camera;

void camera_init(Camera *cam, Vec3 position, float yaw, float pitch);
void camera_update_vectors(Camera *cam);
void camera_rotate(Camera *cam, float delta_yaw, float delta_pitch);
void camera_move_forward(Camera *cam, float delta);
void camera_move_backward(Camera *cam, float delta);
void camera_strafe_left(Camera *cam, float delta);
void camera_strafe_right(Camera *cam, float delta);
Mat4 camera_get_view_matrix(Camera *cam);

void camera_add_collider(Camera *cam, AABB box);
AABB camera_get_aabb(Camera *cam);
bool camera_try_move(Camera *cam, Vec3 delta);

void camera_apply_gravity(Camera *cam, float dt);
void camera_jump(Camera *cam);

#endif
