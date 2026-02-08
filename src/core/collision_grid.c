#include "core/collision_grid.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

static inline int grid_index(const CollisionGrid *g, int x, int y, int z) {
    return x + y * g->nx + z * g->nx * g->ny;
}

static inline void cell_add(GridCell *cell, int tri_idx) {
    if (cell->count >= cell->capacity) {
        int new_cap = cell->capacity == 0 ? 32 : cell->capacity * 2;
        cell->triangle_indices = realloc(cell->triangle_indices, new_cap * sizeof(int));
        cell->capacity = new_cap;
    }
    cell->triangle_indices[cell->count++] = tri_idx;
}

static void get_triangle_verts(const OBJMesh *mesh, int face_idx, Vec3 *v0, Vec3 *v1, Vec3 *v2) {
    OBJFace f = mesh->faces[face_idx];
    *v0 = mesh->vertices[f.a].position;
    *v1 = mesh->vertices[f.b].position;
    *v2 = mesh->vertices[f.c].position;
}

static void triangle_aabb(Vec3 v0, Vec3 v1, Vec3 v2, AABB *out) {
    out->min.x = fminf(v0.x, fminf(v1.x, v2.x));
    out->min.y = fminf(v0.y, fminf(v1.y, v2.y));
    out->min.z = fminf(v0.z, fminf(v1.z, v2.z));
    out->max.x = fmaxf(v0.x, fmaxf(v1.x, v2.x));
    out->max.y = fmaxf(v0.y, fmaxf(v1.y, v2.y));
    out->max.z = fmaxf(v0.z, fmaxf(v1.z, v2.z));
}

int grid_build(CollisionGrid *grid, OBJMesh *mesh, float cell_size) {
    memset(grid, 0, sizeof(CollisionGrid));
    grid->mesh = mesh;
    grid->cell_size = cell_size;
    grid->origin = mesh->bounds.min;

    float wx = mesh->bounds.max.x - mesh->bounds.min.x;
    float wy = mesh->bounds.max.y - mesh->bounds.min.y;
    float wz = mesh->bounds.max.z - mesh->bounds.min.z;

    grid->nx = (int)ceilf(wx / cell_size) + 1;
    grid->ny = (int)ceilf(wy / cell_size) + 1;
    grid->nz = (int)ceilf(wz / cell_size) + 1;

    int total_cells = grid->nx * grid->ny * grid->nz;
    grid->cells = calloc(total_cells, sizeof(GridCell));
    if (!grid->cells) {
        LOG_ERROR("Failed to allocate grid cells");
        return 1;
    }

    LOG_INFO("Building collision grid: %dx%dx%d cells (%.1f unit)", 
             grid->nx, grid->ny, grid->nz, cell_size);

    // Assign each triangle to cells it overlaps
    for (int i = 0; i < mesh->face_count; i++) {
        Vec3 v0, v1, v2;
        get_triangle_verts(mesh, i, &v0, &v1, &v2);

        AABB tri_box;
        triangle_aabb(v0, v1, v2, &tri_box);

        // Convert to cell coordinates
        int x0 = (int)floorf((tri_box.min.x - grid->origin.x) / cell_size);
        int y0 = (int)floorf((tri_box.min.y - grid->origin.y) / cell_size);
        int z0 = (int)floorf((tri_box.min.z - grid->origin.z) / cell_size);
        int x1 = (int)floorf((tri_box.max.x - grid->origin.x) / cell_size);
        int y1 = (int)floorf((tri_box.max.y - grid->origin.y) / cell_size);
        int z1 = (int)floorf((tri_box.max.z - grid->origin.z) / cell_size);

        // Clamp
        if (x0 < 0) x0 = 0; if (x1 >= grid->nx) x1 = grid->nx - 1;
        if (y0 < 0) y0 = 0; if (y1 >= grid->ny) y1 = grid->ny - 1;
        if (z0 < 0) z0 = 0; if (z1 >= grid->nz) z1 = grid->nz - 1;

        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    int idx = grid_index(grid, x, y, z);
                    cell_add(&grid->cells[idx], i);
                }
            }
        }
    }

    LOG_INFO("Collision grid built: %d triangles distributed", mesh->face_count);
    return 0;
}

void grid_free(CollisionGrid *grid) {
    if (grid->cells) {
        int total = grid->nx * grid->ny * grid->nz;
        for (int i = 0; i < total; i++) {
            free(grid->cells[i].triangle_indices);
        }
        free(grid->cells);
        grid->cells = NULL;
    }
}

// SAT test: project triangle onto axis, return min/max
static void project_triangle(Vec3 v0, Vec3 v1, Vec3 v2, Vec3 axis, float *out_min, float *out_max) {
    float p0 = vec3_dot(v0, axis);
    float p1 = vec3_dot(v1, axis);
    float p2 = vec3_dot(v2, axis);
    *out_min = fminf(p0, fminf(p1, p2));
    *out_max = fmaxf(p0, fmaxf(p1, p2));
}

// SAT test: project AABB onto axis
static void project_aabb(AABB box, Vec3 axis, float *out_min, float *out_max) {
    Vec3 center = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };
    Vec3 half = {
        (box.max.x - box.min.x) * 0.5f,
        (box.max.y - box.min.y) * 0.5f,
        (box.max.z - box.min.z) * 0.5f
    };
    float c = vec3_dot(center, axis);
    float r = half.x * fabsf(axis.x) + half.y * fabsf(axis.y) + half.z * fabsf(axis.z);
    *out_min = c - r;
    *out_max = c + r;
}

static bool ranges_overlap(float amin, float amax, float bmin, float bmax) {
    return amin <= bmax && bmin <= amax;
}

// Full SAT triangle-AABB intersection test
static bool triangle_aabb_intersect(Vec3 v0, Vec3 v1, Vec3 v2, AABB box) {
    // Translate triangle to AABB center
    Vec3 center = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };
    v0 = vec3_sub(v0, center);
    v1 = vec3_sub(v1, center);
    v2 = vec3_sub(v2, center);

    Vec3 half = {
        (box.max.x - box.min.x) * 0.5f,
        (box.max.y - box.min.y) * 0.5f,
        (box.max.z - box.min.z) * 0.5f
    };

    // Triangle edges
    Vec3 e0 = vec3_sub(v1, v0);
    Vec3 e1 = vec3_sub(v2, v1);
    Vec3 e2 = vec3_sub(v0, v2);

    // Test 9 cross products
    Vec3 axes[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    Vec3 edges[3] = {e0, e1, e2};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Vec3 axis = vec3_cross(axes[i], edges[j]);
            float len = vec3_length(axis);
            if (len < 0.0001f) continue;
            axis = vec3_normalize(axis);

            float t_min, t_max, b_min, b_max;
            project_triangle(v0, v1, v2, axis, &t_min, &t_max);
            float r = half.x * fabsf(axis.x) + half.y * fabsf(axis.y) + half.z * fabsf(axis.z);
            b_min = -r; b_max = r;

            if (!ranges_overlap(t_min, t_max, b_min, b_max))
                return false;
        }
    }

    // Test AABB face normals
    if (fmaxf(v0.x, fmaxf(v1.x, v2.x)) < -half.x || fminf(v0.x, fminf(v1.x, v2.x)) > half.x) return false;
    if (fmaxf(v0.y, fmaxf(v1.y, v2.y)) < -half.y || fminf(v0.y, fminf(v1.y, v2.y)) > half.y) return false;
    if (fmaxf(v0.z, fmaxf(v1.z, v2.z)) < -half.z || fminf(v0.z, fminf(v1.z, v2.z)) > half.z) return false;

    // Test triangle normal
    Vec3 normal = vec3_cross(e0, e1);
    float d = vec3_dot(normal, v0);
    float r = half.x * fabsf(normal.x) + half.y * fabsf(normal.y) + half.z * fabsf(normal.z);
    if (fabsf(d) > r) return false;

    return true;
}

bool grid_check_aabb(const CollisionGrid *grid, AABB box, Vec3 *push_out) {
    if (!grid->cells) return false;

    // Find cells the AABB overlaps
    int x0 = (int)floorf((box.min.x - grid->origin.x) / grid->cell_size);
    int y0 = (int)floorf((box.min.y - grid->origin.y) / grid->cell_size);
    int z0 = (int)floorf((box.min.z - grid->origin.z) / grid->cell_size);
    int x1 = (int)floorf((box.max.x - grid->origin.x) / grid->cell_size);
    int y1 = (int)floorf((box.max.y - grid->origin.y) / grid->cell_size);
    int z1 = (int)floorf((box.max.z - grid->origin.z) / grid->cell_size);

    if (x0 < 0) x0 = 0;
    if (x1 >= grid->nx) x1 = grid->nx - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= grid->ny) y1 = grid->ny - 1;
    if (z0 < 0) z0 = 0;
    if (z1 >= grid->nz) z1 = grid->nz - 1;

    bool hit = false;
    Vec3 best_push = {0, 0, 0};
    float best_dot = -1.0f;

    // AABB center for penetration calculation
    Vec3 box_center = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                int idx = grid_index(grid, x, y, z);
                GridCell *cell = &grid->cells[idx];

                for (int i = 0; i < cell->count; i++) {
                    int tri_idx = cell->triangle_indices[i];
                    Vec3 v0, v1, v2;
                    get_triangle_verts(grid->mesh, tri_idx, &v0, &v1, &v2);

                    if (triangle_aabb_intersect(v0, v1, v2, box)) {
                        hit = true;

                        // Calculate triangle normal
                        Vec3 e1 = vec3_sub(v1, v0);
                        Vec3 e2 = vec3_sub(v2, v0);
                        Vec3 n = vec3_normalize(vec3_cross(e1, e2));

                        // Prefer upward-facing normals (floors) for stability
                        float up_dot = n.y;
                        if (up_dot > best_dot) {
                            best_dot = up_dot;

                            // Calculate push distance (how far to move out)
                            // Use triangle centroid to box center distance
                            Vec3 tri_center = {
                                (v0.x + v1.x + v2.x) / 3.0f,
                                (v0.y + v1.y + v2.y) / 3.0f,
                                (v0.z + v1.z + v2.z) / 3.0f
                            };

                            // Push amount based on AABB half-height for floors
                            float push_dist = 0.15f;
                            if (up_dot > 0.7f) {
                                // Floor - push up more aggressively
                                push_dist = box_center.y - tri_center.y + 0.1f;
                                if (push_dist < 0.1f) push_dist = 0.1f;
                                if (push_dist > 0.5f) push_dist = 0.5f;
                            }

                            best_push = vec3_mul(n, push_dist);
                        }
                    }
                }
            }
        }
    }

    if (hit && push_out) {
        *push_out = best_push;
    }
    return hit;
}
