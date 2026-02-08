#ifndef MESH_H
#define MESH_H

#include "math/math.h"
#include <stdint.h>

typedef struct
{
    float u, v;
} Vec2UV;

typedef struct
{
    int a, b, c;    // Vertex indices
    uint32_t color; // Per-face color
} Face;

typedef struct
{
    Vec3 *vertices;
    Face *faces;
    int vertex_count;
    int face_count;
    AABB bounds;
    float radius;
} Mesh;

// Arena constants
#define FLOOR_GRID_SIZE 8
#define FLOOR_TILE_SIZE 2.5f
#define FLOOR_TOTAL_SIZE (FLOOR_GRID_SIZE * FLOOR_TILE_SIZE) // 20 units
#define MAX_FLOOR_TILES (FLOOR_GRID_SIZE * FLOOR_GRID_SIZE)  // 64 tiles

#define COLOR_PINK 0xFFFF69B4
#define COLOR_GREY 0xFF808080

// Built-in meshes
Mesh mesh_cube(void);
Mesh mesh_cube_at(Vec3 position, float scale);

// Floor mesh generation
Mesh mesh_floor_tile(float x, float z, float size, uint32_t color);
int mesh_generate_floor(Mesh *tiles);

#endif
