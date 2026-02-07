#ifndef MESH_H
#define MESH_H

#include "math.h"
#include <stdint.h>

#define MAX_MESH_VERTICES 256
#define MAX_MESH_FACES    256

typedef struct {
    int a, b, c;
} Face;

typedef struct {
    Vec3   vertices[MAX_MESH_VERTICES];
    Face   faces[MAX_MESH_FACES];
    int    vertex_count;
    int    face_count;
} Mesh;

// Built-in meshes
Mesh mesh_cube(void);

#endif
