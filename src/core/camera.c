#include "core/camera.h"
#include "core/log.h"
#include <math.h>
#include <stdbool.h>

static const Vec3 WORLD_UP = {0.0f, 1.0f, 0.0f};

void camera_init(Camera *cam, Vec3 position, float yaw, float pitch)
{
    cam->position = position;
    cam->yaw = yaw;
    cam->pitch = pitch;
    cam->collider_count = 0;
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
    Vec3 new_pos = vec3_add(cam->position, delta);

    // Floor constraint
    if (new_pos.y < CAMERA_EYE_HEIGHT)
        new_pos.y = CAMERA_EYE_HEIGHT;

    Vec3 half = {CAMERA_HALF_W, CAMERA_HALF_H, CAMERA_HALF_D};
    AABB new_box = aabb_from_center_size(new_pos, half);

    for (int i = 0; i < cam->collider_count; i++)
    {
        if (aabb_overlap(new_box, cam->colliders[i]))
        {
            // Slide: try each axis independently
            Vec3 slide_x = {cam->position.x + delta.x, cam->position.y, cam->position.z};
            Vec3 slide_z = {cam->position.x, cam->position.y, cam->position.z + delta.z};

            AABB box_x = aabb_from_center_size(slide_x, half);
            AABB box_z = aabb_from_center_size(slide_z, half);

            bool blocked_x = false;
            bool blocked_z = false;

            for (int j = 0; j < cam->collider_count; j++)
            {
                if (aabb_overlap(box_x, cam->colliders[j]))
                    blocked_x = true;
                if (aabb_overlap(box_z, cam->colliders[j]))
                    blocked_z = true;
            }

            if (!blocked_x)
            {
                cam->position.x = slide_x.x;
            }
            if (!blocked_z)
            {
                cam->position.z = slide_z.z;
            }
            if (cam->position.y < CAMERA_EYE_HEIGHT)
                cam->position.y = CAMERA_EYE_HEIGHT;
            return false;
        }
    }

    cam->position = new_pos;
    return true;
}
