#include "math.h"
#include <math.h>
#include <stdbool.h>
#include <float.h>

// Vec2 operations

Vec2 vec2_add(Vec2 a, Vec2 b)
{
    return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 vec2_sub(Vec2 a, Vec2 b)
{
    return (Vec2){a.x - b.x, a.y - b.y};
}

Vec2 vec2_mul(Vec2 v, float s)
{
    return (Vec2){v.x * s, v.y * s};
}

float vec2_dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float vec2_length(Vec2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

// Vec3 operations

Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 vec3_mul(Vec3 v, float s)
{
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

float vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_length(Vec3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v)
{
    float len = vec3_length(v);
    if (len > 0.0f)
    {
        return vec3_mul(v, 1.0f / len);
    }
    return v;
}

// Vec4 operations

Vec4 vec4_from_vec3(Vec3 v, float w)
{
    return (Vec4){v.x, v.y, v.z, w};
}

Vec3 vec3_from_vec4(Vec4 v)
{
    return (Vec3){v.x, v.y, v.z};
}

// Mat4 operations

Mat4 mat4_identity(void)
{
    Mat4 m = {{{0}}};
    m.m[0][0] = 1.0f;
    m.m[1][1] = 1.0f;
    m.m[2][2] = 1.0f;
    m.m[3][3] = 1.0f;
    return m;
}

Mat4 mat4_mul(Mat4 a, Mat4 b)
{
    Mat4 result = {{{0}}};
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            result.m[i][j] = a.m[i][0] * b.m[0][j] +
                             a.m[i][1] * b.m[1][j] +
                             a.m[i][2] * b.m[2][j] +
                             a.m[i][3] * b.m[3][j];
        }
    }
    return result;
}

Vec4 mat4_mul_vec4(Mat4 m, Vec4 v)
{
    return (Vec4){
        m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * v.w,
        m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * v.w,
        m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * v.w,
        m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3] * v.w};
}

Mat4 mat4_translate(float x, float y, float z)
{
    Mat4 m = mat4_identity();
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    return m;
}

Mat4 mat4_scale(float x, float y, float z)
{
    Mat4 m = mat4_identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
    return m;
}

Mat4 mat4_rotate_x(float angle)
{
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[1][1] = c;
    m.m[1][2] = -s;
    m.m[2][1] = s;
    m.m[2][2] = c;
    return m;
}

Mat4 mat4_rotate_y(float angle)
{
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0][0] = c;
    m.m[0][2] = s;
    m.m[2][0] = -s;
    m.m[2][2] = c;
    return m;
}

Mat4 mat4_rotate_z(float angle)
{
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0][0] = c;
    m.m[0][1] = -s;
    m.m[1][0] = s;
    m.m[1][1] = c;
    return m;
}

Mat4 mat4_perspective(float fov, float aspect, float near, float far)
{
    Mat4 m = {{{0}}};
    float tan_half_fov = tanf(fov / 2.0f);
    m.m[0][0] = 1.0f / (aspect * tan_half_fov);
    m.m[1][1] = 1.0f / tan_half_fov;
    m.m[2][2] = far / (far - near);
    m.m[2][3] = -(far * near) / (far - near);
    m.m[3][2] = 1.0f;
    return m;
}

Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 z = vec3_normalize(vec3_sub(target, eye));
    Vec3 x = vec3_normalize(vec3_cross(up, z));
    Vec3 y = vec3_cross(z, x);

    Mat4 m = mat4_identity();
    m.m[0][0] = x.x;
    m.m[0][1] = x.y;
    m.m[0][2] = x.z;
    m.m[1][0] = y.x;
    m.m[1][1] = y.y;
    m.m[1][2] = y.z;
    m.m[2][0] = z.x;
    m.m[2][1] = z.y;
    m.m[2][2] = z.z;
    m.m[0][3] = -vec3_dot(x, eye);
    m.m[1][3] = -vec3_dot(y, eye);
    m.m[2][3] = -vec3_dot(z, eye);
    return m;
}

// AABB operations

AABB aabb_from_vertices(Vec3 *vertices, int count)
{
    AABB box;
    box.min = (Vec3){FLT_MAX, FLT_MAX, FLT_MAX};
    box.max = (Vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < count; i++)
    {
        Vec3 v = vertices[i];
        if (v.x < box.min.x)
            box.min.x = v.x;
        if (v.y < box.min.y)
            box.min.y = v.y;
        if (v.z < box.min.z)
            box.min.z = v.z;
        if (v.x > box.max.x)
            box.max.x = v.x;
        if (v.y > box.max.y)
            box.max.y = v.y;
        if (v.z > box.max.z)
            box.max.z = v.z;
    }
    return box;
}

AABB aabb_transform(AABB box, Mat4 m)
{
    Vec3 corners[8] = {
        {box.min.x, box.min.y, box.min.z},
        {box.min.x, box.min.y, box.max.z},
        {box.min.x, box.max.y, box.min.z},
        {box.min.x, box.max.y, box.max.z},
        {box.max.x, box.min.y, box.min.z},
        {box.max.x, box.min.y, box.max.z},
        {box.max.x, box.max.y, box.min.z},
        {box.max.x, box.max.y, box.max.z},
    };

    AABB result;
    result.min = (Vec3){FLT_MAX, FLT_MAX, FLT_MAX};
    result.max = (Vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 8; i++)
    {
        Vec4 t = mat4_mul_vec4(m, vec4_from_vec3(corners[i], 1.0f));
        Vec3 p = vec3_from_vec4(t);
        if (p.x < result.min.x)
            result.min.x = p.x;
        if (p.y < result.min.y)
            result.min.y = p.y;
        if (p.z < result.min.z)
            result.min.z = p.z;
        if (p.x > result.max.x)
            result.max.x = p.x;
        if (p.y > result.max.y)
            result.max.y = p.y;
        if (p.z > result.max.z)
            result.max.z = p.z;
    }
    return result;
}

AABB aabb_from_center_size(Vec3 center, Vec3 half_extents)
{
    AABB box;
    box.min = vec3_sub(center, half_extents);
    box.max = vec3_add(center, half_extents);
    return box;
}

bool aabb_overlap(AABB a, AABB b)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x)
        return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y)
        return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z)
        return false;
    return true;
}

// 4x4 matrix inverse via cofactor expansion
Mat4 mat4_inverse(Mat4 mat)
{
    float m[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            m[r * 4 + c] = mat.m[r][c];

    float inv[16];

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabsf(det) < 1e-8f)
        return mat4_identity();

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float inv_det = 1.0f / det;
    Mat4 result;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            result.m[r][c] = inv[r * 4 + c] * inv_det;

    return result;
}

Ray ray_from_screen(int screen_x, int screen_y, int screen_w, int screen_h,
                    Mat4 inv_proj, Mat4 inv_view, Vec3 cam_pos)
{
    // Screen to NDC (matching render_project_vertex inverse)
    float ndc_x = (2.0f * screen_x / screen_w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / screen_h);

    // Unproject near and far points through inverse projection
    Vec4 clip_near = {ndc_x, ndc_y, 0.0f, 1.0f};
    Vec4 clip_far = {ndc_x, ndc_y, 1.0f, 1.0f};

    Vec4 view_near = mat4_mul_vec4(inv_proj, clip_near);
    Vec4 view_far = mat4_mul_vec4(inv_proj, clip_far);

    // Perspective divide to recover view-space positions
    Vec3 vn = vec3_mul(vec3_from_vec4(view_near), 1.0f / view_near.w);
    Vec3 vf = vec3_mul(vec3_from_vec4(view_far), 1.0f / view_far.w);

    // View space to world space
    Vec3 wn = vec3_from_vec4(mat4_mul_vec4(inv_view, vec4_from_vec3(vn, 1.0f)));
    Vec3 wf = vec3_from_vec4(mat4_mul_vec4(inv_view, vec4_from_vec3(vf, 1.0f)));

    Ray ray;
    ray.origin = cam_pos;
    ray.direction = vec3_normalize(vec3_sub(wf, wn));
    return ray;
}

// Slab method for ray-AABB intersection
bool ray_aabb_intersect(Ray ray, AABB box, float *t_out)
{
    float tmin = -FLT_MAX;
    float tmax = FLT_MAX;

    // X slab
    if (fabsf(ray.direction.x) > 1e-8f)
    {
        float t1 = (box.min.x - ray.origin.x) / ray.direction.x;
        float t2 = (box.max.x - ray.origin.x) / ray.direction.x;
        if (t1 > t2)
        {
            float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tmin)
            tmin = t1;
        if (t2 < tmax)
            tmax = t2;
        if (tmin > tmax)
            return false;
    }
    else
    {
        if (ray.origin.x < box.min.x || ray.origin.x > box.max.x)
            return false;
    }

    // Y slab
    if (fabsf(ray.direction.y) > 1e-8f)
    {
        float t1 = (box.min.y - ray.origin.y) / ray.direction.y;
        float t2 = (box.max.y - ray.origin.y) / ray.direction.y;
        if (t1 > t2)
        {
            float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tmin)
            tmin = t1;
        if (t2 < tmax)
            tmax = t2;
        if (tmin > tmax)
            return false;
    }
    else
    {
        if (ray.origin.y < box.min.y || ray.origin.y > box.max.y)
            return false;
    }

    // Z slab
    if (fabsf(ray.direction.z) > 1e-8f)
    {
        float t1 = (box.min.z - ray.origin.z) / ray.direction.z;
        float t2 = (box.max.z - ray.origin.z) / ray.direction.z;
        if (t1 > t2)
        {
            float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tmin)
            tmin = t1;
        if (t2 < tmax)
            tmax = t2;
        if (tmin > tmax)
            return false;
    }
    else
    {
        if (ray.origin.z < box.min.z || ray.origin.z > box.max.z)
            return false;
    }

    if (tmax < 0)
        return false;

    if (t_out)
        *t_out = (tmin >= 0) ? tmin : tmax;
    return true;
}
