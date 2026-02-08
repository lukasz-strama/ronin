#ifndef CHUNK_H
#define CHUNK_H

#include "core/obj_loader.h"
#include "math/math.h"
#include "graphics/clip.h"
#include "graphics/render.h"
#include <stdbool.h>

#define CHUNK_SIZE 50.0f
#define MAX_CHUNKS 16384

struct RenderStats;

typedef struct
{
    OBJVertex *vertices;
    OBJFace *faces;
    int vertex_count;
    int face_count;
    AABB bounds;
    Vec3 center;
    float radius;
    TransformCache *cache;
    int position_count;
} WorldChunk;

typedef struct
{
    WorldChunk *chunks;
    int count;
    int capacity;
    float cell_size;
    int nx, ny, nz;
    Vec3 origin;
} ChunkGrid;

// Split an OBJMesh into spatial chunks for frustum culling.
// The grid takes ownership of allocated memory and must be freed.
int chunk_grid_build(ChunkGrid *grid, const OBJMesh *mesh, float cell_size);
void chunk_grid_free(ChunkGrid *grid);

// Render all visible chunks (frustum-culled per chunk AABB).
void chunk_grid_render(const ChunkGrid *grid, Mat4 vp,
                       Vec3 camera_pos, Vec3 light_dir,
                       const Frustum *frustum, bool backface_cull,
                       struct RenderStats *stats_out);

// Wireframe variant
void chunk_grid_render_wireframe(const ChunkGrid *grid, Mat4 vp,
                                 Vec3 camera_pos,
                                 const Frustum *frustum, bool backface_cull,
                                 struct RenderStats *stats_out);

#endif
