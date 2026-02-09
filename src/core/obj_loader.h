#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "math/math.h"
#include "graphics/texture.h"
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
    int texture_id; // Index into OBJMesh::textures, -1 = none
} OBJFace;

typedef struct
{
    Vec4 world;
    Vec4 clip;
    uint32_t gen;
} TransformCache;

#define OBJ_MAX_MATERIALS 128
#define OBJ_MTL_NAME_MAX 64

typedef struct
{
    char name[OBJ_MTL_NAME_MAX];
    char diffuse_path[256]; // map_Kd relative to OBJ dir
    uint32_t color;         // Kd fallback as 0xAARRGGBB
    int texture_id;         // Index into OBJMesh::textures after load, -1 = none
} OBJMaterial;

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

    // Materials & textures
    OBJMaterial materials[OBJ_MAX_MATERIALS];
    int material_count;
    Texture *textures;
    int texture_count;
} OBJMesh;

// The caller must free the mesh with obj_mesh_free().
int obj_load(OBJMesh *mesh, const char *path);

void obj_mesh_free(OBJMesh *mesh);

#endif
