#include "core/camera.h"
#include "core/collision_grid.h"
#include "core/log.h"
#include <math.h>
#include <stdbool.h>

static const Vec3 WORLD_UP = {0.0f, 1.0f, 0.0f};

void camera_init(Camera *cam, Vec3 position, float yaw, float pitch)
{
    cam->position = position;
    cam->yaw = yaw;
    cam->pitch = pitch;
    cam->velocity_y = 0.0f;
    cam->grounded = false;
    cam->collider_count = 0;
    cam->fly_mode = false;
    cam->map_grid = NULL;
    camera_update_vectors(cam);
    LOG_INFO("Camera initialized at (%.2f, %.2f, %.2f) yaw=%.2f pitch=%.2f",
             position.x, position.y, position.z, yaw, pitch);
}

void camera_update_vectors(Camera *cam)
{
    // Convert yaw/pitch to direction vector
    cam->direction.x = cosf(cam->pitch) * sinf(cam->yaw);
    cam->direction.y = sinf(cam->pitch);
    cam->direction.z = cosf(cam->pitch) * cosf(cam->yaw);
    cam->direction = vec3_normalize(cam->direction);

    // Compute right vector (cross product order for left-handed system)
    cam->right = vec3_normalize(vec3_cross(WORLD_UP, cam->direction));
}

void camera_rotate(Camera *cam, float delta_yaw, float delta_pitch)
{
    cam->yaw += delta_yaw;
    cam->pitch += delta_pitch;

    if (cam->pitch > CAMERA_PITCH_LIMIT)
    {
        cam->pitch = CAMERA_PITCH_LIMIT;
    }
    if (cam->pitch < -CAMERA_PITCH_LIMIT)
    {
        cam->pitch = -CAMERA_PITCH_LIMIT;
    }

    camera_update_vectors(cam);
}

void camera_move_forward(Camera *cam, float delta)
{
    cam->position = vec3_add(cam->position, vec3_mul(cam->direction, delta));
}

void camera_move_backward(Camera *cam, float delta)
{
    cam->position = vec3_sub(cam->position, vec3_mul(cam->direction, delta));
}

void camera_strafe_left(Camera *cam, float delta)
{
    cam->position = vec3_sub(cam->position, vec3_mul(cam->right, delta));
}

void camera_strafe_right(Camera *cam, float delta)
{
    cam->position = vec3_add(cam->position, vec3_mul(cam->right, delta));
}

Mat4 camera_get_view_matrix(Camera *cam)
{
    Vec3 target = vec3_add(cam->position, cam->direction);
    return mat4_look_at(cam->position, target, WORLD_UP);
}

void camera_add_collider(Camera *cam, AABB box)
{
    if (cam->collider_count >= MAX_COLLIDERS)
    {
        LOG_WARN("Max colliders reached (%d)", MAX_COLLIDERS);
        return;
    }
    cam->colliders[cam->collider_count++] = box;
}

AABB camera_get_aabb(Camera *cam)
{
    Vec3 half = {CAMERA_HALF_W, CAMERA_HALF_H, CAMERA_HALF_D};
    return aabb_from_center_size(cam->position, half);
}

bool camera_try_move(Camera *cam, Vec3 delta)
{
    // Fly mode skips all collision and floor constraints
    if (cam->fly_mode)
    {
        cam->position = vec3_add(cam->position, delta);
        return true;
    }

    Vec3 half = {CAMERA_HALF_W, CAMERA_HALF_H, CAMERA_HALF_D};
    Vec3 new_pos = cam->position;
    bool blocked = false;

    // Iterative step: move X, check collision
    Vec3 test_x = {cam->position.x + delta.x, cam->position.y, cam->position.z};
    AABB box_x = aabb_from_center_size(test_x, half);
    bool blocked_x = false;
    for (int i = 0; i < cam->collider_count; i++)
    {
        if (aabb_overlap(box_x, cam->colliders[i]))
        {
            blocked_x = true;
            break;
        }
    }
    if (!blocked_x)
        new_pos.x = test_x.x;
    else
        blocked = true;

    // Iterative step: move Z, check collision
    Vec3 test_z = {new_pos.x, cam->position.y, cam->position.z + delta.z};
    AABB box_z = aabb_from_center_size(test_z, half);
    bool blocked_z = false;
    for (int i = 0; i < cam->collider_count; i++)
    {
        if (aabb_overlap(box_z, cam->colliders[i]))
        {
            blocked_z = true;
            break;
        }
    }
    if (!blocked_z)
        new_pos.z = test_z.z;
    else
        blocked = true;

    // Check map grid collision
    if (cam->map_grid)
    {
        AABB test_box = aabb_from_center_size(new_pos, half);
        Vec3 push;
        if (grid_check_aabb(cam->map_grid, test_box, &push))
        {
            new_pos = vec3_add(new_pos, push);
            blocked = true;
        }
    }

    // Floor constraint (only if no map grid)
    if (!cam->map_grid && new_pos.y < CAMERA_EYE_HEIGHT)
        new_pos.y = CAMERA_EYE_HEIGHT;

    cam->position = new_pos;
    return !blocked;
}

void camera_apply_gravity(Camera *cam, float dt)
{
    if (cam->fly_mode)
        return;

    // Apply gravity
    cam->velocity_y -= GRAVITY * dt;

    // Clamp terminal velocity
    if (cam->velocity_y < -30.0f)
        cam->velocity_y = -30.0f;

    // Calculate vertical movement
    float dy = cam->velocity_y * dt;
    Vec3 half = {CAMERA_HALF_W, CAMERA_HALF_H, CAMERA_HALF_D};

    // Test new Y position
    Vec3 test_pos = cam->position;
    test_pos.y += dy;

    bool hit_floor = false;
    bool hit_ceiling = false;

    // Check map grid collision for vertical movement
    if (cam->map_grid)
    {
        AABB test_box = aabb_from_center_size(test_pos, half);
        Vec3 push;
        if (grid_check_aabb(cam->map_grid, test_box, &push))
        {
            test_pos = vec3_add(test_pos, push);
            // Determine if floor or ceiling hit based on push direction
            if (push.y > 0.01f)
                hit_floor = true;
            else if (push.y < -0.01f)
                hit_ceiling = true;
        }
    }

    // Fallback floor constraint when no map grid
    if (!cam->map_grid && test_pos.y < CAMERA_EYE_HEIGHT)
    {
        test_pos.y = CAMERA_EYE_HEIGHT;
        hit_floor = true;
    }

    // Ground detection: cast a small box below feet
    if (!hit_floor && cam->map_grid)
    {
        Vec3 feet_pos = cam->position;
        feet_pos.y -= CAMERA_HALF_H + 0.05f;
        Vec3 probe_half = {CAMERA_HALF_W * 0.8f, 0.05f, CAMERA_HALF_D * 0.8f};
        AABB probe = aabb_from_center_size(feet_pos, probe_half);
        Vec3 push;
        if (grid_check_aabb(cam->map_grid, probe, &push))
            hit_floor = true;
    }

    // Update grounded state
    if (hit_floor && cam->velocity_y <= 0)
    {
        cam->grounded = true;
        cam->velocity_y = 0.0f;
    }
    else if (hit_ceiling && cam->velocity_y > 0)
    {
        cam->velocity_y = 0.0f;
    }
    else
    {
        cam->grounded = false;
    }

    cam->position = test_pos;
}

void camera_jump(Camera *cam)
{
    if (cam->fly_mode)
        return;

    if (cam->grounded)
    {
        cam->velocity_y = JUMP_VELOCITY;
        cam->grounded = false;
        LOG_INFO("Jump!");
    }
}
