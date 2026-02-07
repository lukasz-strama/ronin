#include "core/camera.h"
#include "core/log.h"
#include <math.h>

static const Vec3 WORLD_UP = { 0.0f, 1.0f, 0.0f };

void camera_init(Camera *cam, Vec3 position, float yaw, float pitch) {
    cam->position = position;
    cam->yaw      = yaw;
    cam->pitch    = pitch;
    camera_update_vectors(cam);
    LOG_INFO("Camera initialized at (%.2f, %.2f, %.2f) yaw=%.2f pitch=%.2f",
             position.x, position.y, position.z, yaw, pitch);
}

void camera_update_vectors(Camera *cam) {
    // Convert yaw/pitch to direction vector
    cam->direction.x = cosf(cam->pitch) * sinf(cam->yaw);
    cam->direction.y = sinf(cam->pitch);
    cam->direction.z = cosf(cam->pitch) * cosf(cam->yaw);
    cam->direction   = vec3_normalize(cam->direction);

    // Compute right vector (cross product order for left-handed system)
    cam->right = vec3_normalize(vec3_cross(WORLD_UP, cam->direction));
}

void camera_rotate(Camera *cam, float delta_yaw, float delta_pitch) {
    cam->yaw   += delta_yaw;
    cam->pitch += delta_pitch;

    if (cam->pitch > CAMERA_PITCH_LIMIT) {
        cam->pitch = CAMERA_PITCH_LIMIT;
    }
    if (cam->pitch < -CAMERA_PITCH_LIMIT) {
        cam->pitch = -CAMERA_PITCH_LIMIT;
    }

    camera_update_vectors(cam);
}

void camera_move_forward(Camera *cam, float delta) {
    cam->position = vec3_add(cam->position, vec3_mul(cam->direction, delta));
}

void camera_move_backward(Camera *cam, float delta) {
    cam->position = vec3_sub(cam->position, vec3_mul(cam->direction, delta));
}

void camera_strafe_left(Camera *cam, float delta) {
    cam->position = vec3_sub(cam->position, vec3_mul(cam->right, delta));
}

void camera_strafe_right(Camera *cam, float delta) {
    cam->position = vec3_add(cam->position, vec3_mul(cam->right, delta));
}

Mat4 camera_get_view_matrix(Camera *cam) {
    Vec3 target = vec3_add(cam->position, cam->direction);
    return mat4_look_at(cam->position, target, WORLD_UP);
}
