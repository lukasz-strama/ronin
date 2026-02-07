#include "mesh.h"

static Vec3 cube_vertices[8] = {
    { -1, -1, -1 },
    { -1,  1, -1 },
    {  1,  1, -1 },
    {  1, -1, -1 },
    { -1, -1,  1 },
    { -1,  1,  1 },
    {  1,  1,  1 },
    {  1, -1,  1 }
};

static Face cube_faces[12] = {
    // Front (Red)
    { 0, 1, 2, 0xFFFF0000 },
    { 0, 2, 3, 0xFFFF0000 },
    // Back (Green)
    { 4, 6, 5, 0xFF00FF00 },
    { 4, 7, 6, 0xFF00FF00 },
    // Left (Blue)
    { 0, 5, 1, 0xFF0000FF },
    { 0, 4, 5, 0xFF0000FF },
    // Right (Yellow)
    { 3, 2, 6, 0xFFFFFF00 },
    { 3, 6, 7, 0xFFFFFF00 },
    // Top (Magenta)
    { 1, 5, 6, 0xFFFF00FF },
    { 1, 6, 2, 0xFFFF00FF },
    // Bottom (Cyan)
    { 0, 3, 7, 0xFF00FFFF },
    { 0, 7, 4, 0xFF00FFFF }
};

Mesh mesh_cube(void) {
    Mesh m = {
        .vertices     = cube_vertices,
        .faces        = cube_faces,
        .vertex_count = 8,
        .face_count   = 12
    };
    return m;
}
