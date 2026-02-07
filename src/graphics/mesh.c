#include "mesh.h"
#include "core/log.h"

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

static Vec3 positioned_cube_verts[4][8];
static Face positioned_cube_faces[4][12];
static int  positioned_cube_count = 0;

Mesh mesh_cube_at(Vec3 position, float scale) {
    if (positioned_cube_count >= 4) {
        LOG_ERROR("Max positioned cubes reached (4)");
        return mesh_cube();
    }

    int idx = positioned_cube_count++;
    Vec3 *verts = positioned_cube_verts[idx];
    Face *faces = positioned_cube_faces[idx];

    // Transform base cube vertices
    for (int i = 0; i < 8; i++) {
        verts[i].x = cube_vertices[i].x * scale + position.x;
        verts[i].y = cube_vertices[i].y * scale + position.y;
        verts[i].z = cube_vertices[i].z * scale + position.z;
    }

    // Copy faces (indices stay the same, but vary colors)
    uint32_t colors[6] = {
        0xFFE74C3C,  // Red
        0xFF2ECC71,  // Green
        0xFF3498DB,  // Blue
        0xFFF39C12,  // Orange
        0xFF9B59B6,  // Purple
        0xFF1ABC9C   // Teal
    };
    for (int i = 0; i < 12; i++) {
        faces[i] = cube_faces[i];
        faces[i].color = colors[i / 2];
    }

    LOG_INFO("Created cube at (%.1f, %.1f, %.1f) scale=%.1f",
             position.x, position.y, position.z, scale);

    Mesh m = {
        .vertices     = verts,
        .faces        = faces,
        .vertex_count = 8,
        .face_count   = 12
    };
    return m;
}

static Vec3 floor_tile_verts[MAX_FLOOR_TILES][4];
static Face floor_tile_faces[MAX_FLOOR_TILES][2];

Mesh mesh_floor_tile(float x, float z, float size, uint32_t color) {
    static int tile_idx = 0;
    if (tile_idx >= MAX_FLOOR_TILES) {
        LOG_ERROR("Max floor tiles reached");
        tile_idx = 0;
    }

    int idx = tile_idx++;
    Vec3 *verts = floor_tile_verts[idx];
    Face *faces = floor_tile_faces[idx];

    float half = size / 2.0f;
    verts[0] = (Vec3){ x - half, 0.0f, z - half };
    verts[1] = (Vec3){ x - half, 0.0f, z + half };
    verts[2] = (Vec3){ x + half, 0.0f, z + half };
    verts[3] = (Vec3){ x + half, 0.0f, z - half };

    // Two triangles for quad (facing up: +Y)
    faces[0] = (Face){ 0, 1, 2, color };
    faces[1] = (Face){ 0, 2, 3, color };

    Mesh m = {
        .vertices     = verts,
        .faces        = faces,
        .vertex_count = 4,
        .face_count   = 2
    };
    return m;
}

int mesh_generate_floor(Mesh *tiles) {
    float half_total = FLOOR_TOTAL_SIZE / 2.0f;
    float half_tile  = FLOOR_TILE_SIZE / 2.0f;
    int count = 0;

    for (int row = 0; row < FLOOR_GRID_SIZE; row++) {
        for (int col = 0; col < FLOOR_GRID_SIZE; col++) {
            float x = -half_total + half_tile + col * FLOOR_TILE_SIZE;
            float z = -half_total + half_tile + row * FLOOR_TILE_SIZE;

            // Checkerboard pattern
            uint32_t color = ((row + col) % 2 == 0) ? COLOR_PINK : COLOR_GREY;

            tiles[count++] = mesh_floor_tile(x, z, FLOOR_TILE_SIZE, color);
        }
    }

    LOG_INFO("Generated floor: %d tiles (%dx%d grid, %.1f units total)",
             count, FLOOR_GRID_SIZE, FLOOR_GRID_SIZE, FLOOR_TOTAL_SIZE);

    return count;
}

