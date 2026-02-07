#ifndef CAMERA_H
#define CAMERA_H

#include "math/math.h"

#define CAMERA_SPEED       5.0f
#define CAMERA_SENSITIVITY 0.002f
#define CAMERA_PITCH_LIMIT 1.55f  // ~89 degrees

typedef struct {
    Vec3  position;
    Vec3  direction;   // Forward vector (computed from yaw/pitch)
    Vec3  right;       // Right vector (computed)
    float yaw;         // Horizontal rotation (radians)
    float pitch;       // Vertical rotation (radians)
} Camera;

void camera_init(Camera *cam, Vec3 position, float yaw, float pitch);
void camera_update_vectors(Camera *cam);
void camera_rotate(Camera *cam, float delta_yaw, float delta_pitch);
void camera_move_forward(Camera *cam, float delta);
void camera_move_backward(Camera *cam, float delta);
void camera_strafe_left(Camera *cam, float delta);
void camera_strafe_right(Camera *cam, float delta);
Mat4 camera_get_view_matrix(Camera *cam);

#endif
