#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "math/math.h"
#include <stdint.h>

typedef struct
{
    Vec3 position;
    Vec3 normal;
    float u, v;
    int pos_index;
} OBJVertex;

typedef struct
{
    int a, b, c;
    uint32_t color;
} OBJFace;

typedef struct
{
    Vec4 world;
    Vec4 clip;
    uint32_t gen;
} TransformCache;

typedef struct
{
    OBJVertex *vertices;
    OBJFace *faces;
    int vertex_count;
    int face_count;
    AABB bounds;
    float radius;
    TransformCache *cache;
    int position_count;
} OBJMesh;

// The caller must free the mesh with obj_mesh_free().
int obj_load(OBJMesh *mesh, const char *path);

void obj_mesh_free(OBJMesh *mesh);

#endif
