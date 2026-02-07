#include "mesh.h"

Mesh mesh_cube(void) {
    Mesh m = {0};

    // Cube vertices (unit cube centered at origin)
    m.vertices[0] = (Vec3){ -1, -1, -1 };
    m.vertices[1] = (Vec3){ -1,  1, -1 };
    m.vertices[2] = (Vec3){  1,  1, -1 };
    m.vertices[3] = (Vec3){  1, -1, -1 };
    m.vertices[4] = (Vec3){ -1, -1,  1 };
    m.vertices[5] = (Vec3){ -1,  1,  1 };
    m.vertices[6] = (Vec3){  1,  1,  1 };
    m.vertices[7] = (Vec3){  1, -1,  1 };
    m.vertex_count = 8;

    // Cube faces (12 triangles, 2 per side)
    // Front face
    m.faces[0]  = (Face){ 0, 1, 2 };
    m.faces[1]  = (Face){ 0, 2, 3 };
    // Back face
    m.faces[2]  = (Face){ 4, 6, 5 };
    m.faces[3]  = (Face){ 4, 7, 6 };
    // Left face
    m.faces[4]  = (Face){ 0, 5, 1 };
    m.faces[5]  = (Face){ 0, 4, 5 };
    // Right face
    m.faces[6]  = (Face){ 3, 2, 6 };
    m.faces[7]  = (Face){ 3, 6, 7 };
    // Top face
    m.faces[8]  = (Face){ 1, 5, 6 };
    m.faces[9]  = (Face){ 1, 6, 2 };
    // Bottom face
    m.faces[10] = (Face){ 0, 3, 7 };
    m.faces[11] = (Face){ 0, 7, 4 };
    m.face_count = 12;

    return m;
}
