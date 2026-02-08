#ifndef MATH_H
#define MATH_H

#include <stdbool.h>

typedef struct
{
    float x, y;
} Vec2;

typedef struct
{
    float x, y, z;
} Vec3;

typedef struct
{
    float x, y, z, w;
} Vec4;

typedef struct
{
    float m[4][4];
} Mat4;

// Vec2 operations
Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_sub(Vec2 a, Vec2 b);
Vec2 vec2_mul(Vec2 v, float s);
float vec2_dot(Vec2 a, Vec2 b);
float vec2_length(Vec2 v);

// Vec3 operations
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_mul(Vec3 v, float s);
Vec3 vec3_cross(Vec3 a, Vec3 b);
float vec3_dot(Vec3 a, Vec3 b);
float vec3_length(Vec3 v);
Vec3 vec3_normalize(Vec3 v);

// Vec4 operations
Vec4 vec4_from_vec3(Vec3 v, float w);
Vec3 vec3_from_vec4(Vec4 v);

// Mat4 operations
Mat4 mat4_identity(void);
Mat4 mat4_mul(Mat4 a, Mat4 b);
Vec4 mat4_mul_vec4(Mat4 m, Vec4 v);
Mat4 mat4_translate(float x, float y, float z);
Mat4 mat4_scale(float x, float y, float z);
Mat4 mat4_rotate_x(float angle);
Mat4 mat4_rotate_y(float angle);
Mat4 mat4_rotate_z(float angle);
Mat4 mat4_perspective(float fov, float aspect, float near, float far);
Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up);

// AABB (Axis-Aligned Bounding Box)
typedef struct
{
    Vec3 min;
    Vec3 max;
} AABB;

AABB aabb_from_vertices(Vec3 *vertices, int count);
AABB aabb_transform(AABB box, Mat4 m);
AABB aabb_from_center_size(Vec3 center, Vec3 half_extents);
bool aabb_overlap(AABB a, AABB b);

// Ray casting
typedef struct
{
    Vec3 origin;
    Vec3 direction;
} Ray;

Mat4 mat4_inverse(Mat4 m);
Ray ray_from_screen(int screen_x, int screen_y, int screen_w, int screen_h,
                    Mat4 inv_proj, Mat4 inv_view, Vec3 cam_pos);
bool ray_aabb_intersect(Ray ray, AABB box, float *t_out);

// Frustum culling
typedef struct
{
    float a, b, c, d; // Normal (a,b,c) and distance d: ax+by+cz+d=0
} Plane;

typedef struct
{
    Plane planes[6]; // Left, Right, Bottom, Top, Near, Far
} Frustum;

Frustum frustum_extract(Mat4 vp);
bool frustum_test_sphere(const Frustum *f, Vec3 center, float radius);

// Bounding radius from vertices (distance from center to furthest vertex)
float bounding_radius_from_vertices(Vec3 *vertices, int count);
float bounding_radius_from_aabb(AABB box);

#endif
