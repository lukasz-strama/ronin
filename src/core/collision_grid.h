#ifndef COLLISION_GRID_H
#define COLLISION_GRID_H

#include "math/math.h"
#include "core/obj_loader.h"
#include <stdbool.h>

#define GRID_CELL_SIZE 5.0f
#define MAX_TRIS_PER_CELL 512

typedef struct {
    int *triangle_indices;
    int count;
    int capacity;
} GridCell;

typedef struct CollisionGrid {
    GridCell *cells;
    int nx, ny, nz;
    Vec3 origin;
    float cell_size;
    OBJMesh *mesh;
} CollisionGrid;

int  grid_build(CollisionGrid *grid, OBJMesh *mesh, float cell_size);
void grid_free(CollisionGrid *grid);
bool grid_check_aabb(const CollisionGrid *grid, AABB box, Vec3 *push_out);

#endif
