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
        if (x0 < 0) x0 = 0;
        if (x1 >= grid->nx) x1 = grid->nx - 1;
        if (y0 < 0) y0 = 0;
        if (y1 >= grid->ny) y1 = grid->ny - 1;
        if (z0 < 0) z0 = 0;
        if (z1 >= grid->nz) z1 = grid->nz - 1;

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


static bool ranges_overlap(float amin, float amax, float bmin, float bmax) {
    return amin <= bmax && bmin <= amax;
}


// Full SAT triangle-AABB intersection test with MTV (Minimum Translation Vector)
static bool triangle_aabb_mtv(Vec3 v0, Vec3 v1, Vec3 v2, AABB box, Vec3 *out_mtv) {
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

    float min_overlap = FLT_MAX;
    Vec3 best_axis = {0, 0, 0};

    // Test 9 cross products
    Vec3 axes[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    Vec3 edges[3] = {e0, e1, e2};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Vec3 axis = vec3_cross(axes[i], edges[j]);
            float len = vec3_length(axis);
            if (len < 0.0001f) continue;
            axis = vec3_normalize(axis);

            float t_min, t_max;
            project_triangle(v0, v1, v2, axis, &t_min, &t_max);
            float r = half.x * fabsf(axis.x) + half.y * fabsf(axis.y) + half.z * fabsf(axis.z);
            float b_min = -r, b_max = r;

            if (!ranges_overlap(t_min, t_max, b_min, b_max)) return false;

            // Calculate overlap
            float o1 = b_max - t_min;
            float o2 = t_max - b_min;
            float overlap = fminf(o1, o2);
            if (overlap < min_overlap) {
                min_overlap = overlap;
                best_axis = axis;
                // Correct direction to push box OUT of triangle
                // We want axis to point from triangle TO box
                // Center of box is 0,0,0 here. Triangle center approx via vertices.
                if (vec3_dot(axis, v0) > 0) best_axis = vec3_mul(best_axis, -1.0f);
            }
        }
    }

    // Test AABB face normals (Box axes)
    for (int i=0; i<3; i++) {
        Vec3 axis = axes[i];
        float t_min, t_max;
        project_triangle(v0, v1, v2, axis, &t_min, &t_max);
        float r = (i==0 ? half.x : (i==1 ? half.y : half.z));
        float b_min = -r, b_max = r;

        if (!ranges_overlap(t_min, t_max, b_min, b_max)) return false;

        float o1 = b_max - t_min;
        float o2 = t_max - b_min;
        float overlap = fminf(o1, o2);
        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_axis = axis;
            if (vec3_dot(axis, v0) > 0) best_axis = vec3_mul(best_axis, -1.0f);
        }
    }

    // Test triangle normal
    Vec3 normal = vec3_cross(e0, e1);
    if (vec3_length(normal) > 0.0001f) {
        normal = vec3_normalize(normal);
        float d = vec3_dot(normal, v0); // Distance of plane from center
        float r = half.x * fabsf(normal.x) + half.y * fabsf(normal.y) + half.z * fabsf(normal.z);
        // Box projection on normal is [-r, r]
        // Triangle projection is [d, d]
        
        if (!ranges_overlap(d, d, -r, r)) return false;

        float o1 = r - d;
        float o2 = d - -r;
        float overlap = fminf(o1, o2);
        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_axis = normal;
            // Ensure push is away from triangle plane
            // Triangle is at distance d. Box is at 0.
            if (d > 0) best_axis = vec3_mul(best_axis, -1.0f);
        }
    }

    if (min_overlap < FLT_MAX && out_mtv) {
        *out_mtv = vec3_mul(best_axis, min_overlap);
        // Safety: Ensure we push somewhat up if it's a floor collision
        if (best_axis.y > 0.7f) {
             // It's a floor, ensure we push UP
             if (out_mtv->y < 0) *out_mtv = vec3_mul(*out_mtv, -1.0f);
        }
    }

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


    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                int idx = grid_index(grid, x, y, z);
                GridCell *cell = &grid->cells[idx];

                for (int i = 0; i < cell->count; i++) {
                    int tri_idx = cell->triangle_indices[i];
                    Vec3 v0, v1, v2;
                    get_triangle_verts(grid->mesh, tri_idx, &v0, &v1, &v2);

                    Vec3 mtv;
                    if (triangle_aabb_mtv(v0, v1, v2, box, &mtv)) {
                        hit = true;

                        // Calculate triangle normal for slope check
                        Vec3 e1 = vec3_sub(v1, v0);
                        Vec3 e2 = vec3_sub(v2, v0);
                        Vec3 n = vec3_normalize(vec3_cross(e1, e2));
                        float up_dot = n.y;

                        // Prefer pushing OUT of floors (upward normal)
                        if (up_dot > best_dot) {
                            best_dot = up_dot;
                            best_push = mtv;
                            
                            // Extra epsilon for stability on floors
                            if (up_dot > 0.7f && best_push.y > 0) {
                                best_push.y += 0.001f;
                            }
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
