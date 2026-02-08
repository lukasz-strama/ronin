#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "math/math.h"
#include <stdint.h>

// Per-vertex data unrolled from OBJ indices
typedef struct
{
    Vec3 position;
    Vec3 normal;
    float u, v;
} OBJVertex;

typedef struct
{
    int a, b, c;    // Indices into the vertex array
    uint32_t color; // Per-face color (computed from normals or default)
} OBJFace;

typedef struct
{
    OBJVertex *vertices;
    OBJFace *faces;
    int vertex_count;
    int face_count;
    AABB bounds;
    float radius;
} OBJMesh;

// The caller must free the mesh with obj_mesh_free().
int obj_load(OBJMesh *mesh, const char *path);

void obj_mesh_free(OBJMesh *mesh);

#endif
