#ifndef MESH_H
#define MESH_H

#include "math/math.h"
#include <stdint.h>

typedef struct {
    int      a, b, c;   // Vertex indices
    uint32_t color;     // Per-face color
} Face;

typedef struct {
    Vec3* vertices;
    Face* faces;
    int   vertex_count;
    int   face_count;
} Mesh;

// Built-in meshes
Mesh mesh_cube(void);

#endif
