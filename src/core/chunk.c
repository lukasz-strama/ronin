#include "core/chunk.h"
#include "core/entity.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

static inline int grid_index(const ChunkGrid *g, int cx, int cy, int cz)
{
    return cx + cy * g->nx + cz * g->nx * g->ny;
}

typedef struct
{
    int *face_indices;
    int count;
    int capacity;
} CellAccum;

// Render packet for front-to-back chunk sorting
typedef struct
{
    const WorldChunk *chunk;
    float dist_sq;
} RenderPacket;

static int compare_packets_asc(const void *a, const void *b)
{
    float da = ((const RenderPacket *)a)->dist_sq;
    float db = ((const RenderPacket *)b)->dist_sq;
    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

static void accum_push(CellAccum *a, int idx)
{
    if (a->count >= a->capacity)
    {
        a->capacity = a->capacity ? a->capacity * 2 : 64;
        a->face_indices = realloc(a->face_indices, (size_t)a->capacity * sizeof(int));
    }
    a->face_indices[a->count++] = idx;
}

int chunk_grid_build(ChunkGrid *grid, const OBJMesh *mesh, float cell_size)
{
    if (!mesh || mesh->face_count == 0)
        return 1;

    // Store texture reference from the source mesh
    grid->textures = mesh->textures;
    grid->texture_count = mesh->texture_count;

    // Determine grid dimensions from mesh AABB
    AABB b = mesh->bounds;
    Vec3 extent = vec3_sub(b.max, b.min);
    int nx = (int)ceilf(extent.x / cell_size);
    int ny = (int)ceilf(extent.y / cell_size);
    int nz = (int)ceilf(extent.z / cell_size);
    if (nx < 1)
        nx = 1;
    if (ny < 1)
        ny = 1;
    if (nz < 1)
        nz = 1;

    int total_cells = nx * ny * nz;

    grid->cell_size = cell_size;
    grid->nx = nx;
    grid->ny = ny;
    grid->nz = nz;
    grid->origin = b.min;

    CellAccum *acc = calloc((size_t)total_cells, sizeof(CellAccum));
    if (!acc)
    {
        LOG_ERROR("Chunk grid: failed to allocate accumulators (%d cells)", total_cells);
        return 1;
    }

    // Distribute faces by triangle center
    for (int i = 0; i < mesh->face_count; i++)
    {
        OBJFace f = mesh->faces[i];
        Vec3 v0 = mesh->vertices[f.a].position;
        Vec3 v1 = mesh->vertices[f.b].position;
        Vec3 v2 = mesh->vertices[f.c].position;
        Vec3 center = vec3_mul(vec3_add(vec3_add(v0, v1), v2), 1.0f / 3.0f);

        int cx = (int)((center.x - b.min.x) / cell_size);
        int cy = (int)((center.y - b.min.y) / cell_size);
        int cz = (int)((center.z - b.min.z) / cell_size);
        if (cx < 0)
            cx = 0;
        if (cx >= nx)
            cx = nx - 1;
        if (cy < 0)
            cy = 0;
        if (cy >= ny)
            cy = ny - 1;
        if (cz < 0)
            cz = 0;
        if (cz >= nz)
            cz = nz - 1;

        accum_push(&acc[grid_index(grid, cx, cy, cz)], i);
    }

    int chunk_count = 0;
    for (int i = 0; i < total_cells; i++)
    {
        if (acc[i].count > 0)
            chunk_count++;
    }

    grid->chunks = calloc((size_t)chunk_count, sizeof(WorldChunk));
    if (!grid->chunks)
    {
        for (int i = 0; i < total_cells; i++)
            free(acc[i].face_indices);
        free(acc);
        return 1;
    }
    grid->count = chunk_count;
    grid->capacity = chunk_count;

    int ci = 0;
    for (int i = 0; i < total_cells; i++)
    {
        CellAccum *a = &acc[i];
        if (a->count == 0)
            continue;

        WorldChunk *ch = &grid->chunks[ci++];
        ch->face_count = a->count;

        // Collect unique vertex indices referenced by this chunk's faces
        // Use a remap table from global vertex index -> local vertex index
        int *remap = calloc((size_t)mesh->vertex_count, sizeof(int));
        memset(remap, -1, (size_t)mesh->vertex_count * sizeof(int));

        int local_vert_count = 0;
        for (int f = 0; f < a->count; f++)
        {
            OBJFace face = mesh->faces[a->face_indices[f]];
            int idx[3] = {face.a, face.b, face.c};
            for (int k = 0; k < 3; k++)
            {
                if (remap[idx[k]] == -1)
                    remap[idx[k]] = local_vert_count++;
            }
        }

        ch->vertex_count = local_vert_count;
        ch->vertices = malloc((size_t)local_vert_count * sizeof(OBJVertex));
        ch->faces = malloc((size_t)a->count * sizeof(OBJFace));

        // Copy vertices (remap global -> local)
        for (int f = 0; f < a->count; f++)
        {
            OBJFace face = mesh->faces[a->face_indices[f]];
            int idx[3] = {face.a, face.b, face.c};
            for (int k = 0; k < 3; k++)
            {
                int gi = idx[k];
                int li = remap[gi];
                ch->vertices[li] = mesh->vertices[gi];
            }
        }

        int local_pos_count = 0;
        {
            int *seen = calloc((size_t)mesh->position_count, sizeof(int));
            memset(seen, -1, (size_t)mesh->position_count * sizeof(int));
            for (int v = 0; v < local_vert_count; v++)
            {
                int pi = ch->vertices[v].pos_index;
                if (seen[pi] == -1)
                    seen[pi] = local_pos_count++;
                ch->vertices[v].pos_index = seen[pi];
            }
            free(seen);
        }

        ch->position_count = local_pos_count;
        ch->cache = calloc((size_t)local_pos_count, sizeof(TransformCache));

        // Remap faces to local vertex indices
        for (int f = 0; f < a->count; f++)
        {
            OBJFace face = mesh->faces[a->face_indices[f]];
            ch->faces[f].a = remap[face.a];
            ch->faces[f].b = remap[face.b];
            ch->faces[f].c = remap[face.c];
            ch->faces[f].color = face.color;
            ch->faces[f].texture_id = face.texture_id;
        }

        // Compute AABB and bounding sphere from chunk vertices
        Vec3 mn = {FLT_MAX, FLT_MAX, FLT_MAX};
        Vec3 mx = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (int v = 0; v < local_vert_count; v++)
        {
            Vec3 p = ch->vertices[v].position;
            if (p.x < mn.x)
                mn.x = p.x;
            if (p.y < mn.y)
                mn.y = p.y;
            if (p.z < mn.z)
                mn.z = p.z;
            if (p.x > mx.x)
                mx.x = p.x;
            if (p.y > mx.y)
                mx.y = p.y;
            if (p.z > mx.z)
                mx.z = p.z;
        }
        ch->bounds.min = mn;
        ch->bounds.max = mx;
        ch->center = vec3_mul(vec3_add(mn, mx), 0.5f);
        ch->radius = bounding_radius_from_aabb(ch->bounds);

        free(remap);
    }

    // Free temporary accumulators
    for (int i = 0; i < total_cells; i++)
        free(acc[i].face_indices);
    free(acc);

    LOG_INFO("Chunk grid built: %d non-empty chunks (%dx%dx%d, cell=%.1f)",
             chunk_count, nx, ny, nz, cell_size);
    return 0;
}

void chunk_grid_free(ChunkGrid *grid)
{
    if (!grid->chunks)
        return;
    for (int i = 0; i < grid->count; i++)
    {
        free(grid->chunks[i].vertices);
        free(grid->chunks[i].faces);
        free(grid->chunks[i].cache);
    }
    free(grid->chunks);
    memset(grid, 0, sizeof(ChunkGrid));
}

static uint32_t s_chunk_gen = 0;

static void render_chunk_flat(const WorldChunk *ch, Mat4 vp,
                              Vec3 cam_pos, Vec3 light_dir,
                              bool backface_cull,
                              const Texture *textures, int texture_count,
                              int *bf_culled, int *tri_drawn,
                              int *clip_trivial)
{
    // Identity model matrix (map is at origin, scale 1)
    uint32_t gen = ++s_chunk_gen;
    TransformCache *cache = ch->cache;

    for (int i = 0; i < ch->face_count; i++)
    {
        OBJFace face = ch->faces[i];
        int idx[3] = {face.a, face.b, face.c};

        Vec3 wv[3];
        Vec4 cv[3];
        for (int k = 0; k < 3; k++)
        {
            OBJVertex *vert = &ch->vertices[idx[k]];
            TransformCache *tc = &cache[vert->pos_index];
            if (tc->gen != gen)
            {
                Vec4 pos4 = vec4_from_vec3(vert->position, 1.0f);
                // No model transform — vertices already in world space
                tc->world = pos4;
                tc->clip = mat4_mul_vec4(vp, pos4);
                tc->gen = gen;
            }
            wv[k] = vec3_from_vec4(tc->world);
            cv[k] = tc->clip;
        }

        Vec3 edge1 = vec3_sub(wv[1], wv[0]);
        Vec3 edge2 = vec3_sub(wv[2], wv[0]);
        Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

        Vec3 center = vec3_mul(vec3_add(vec3_add(wv[0], wv[1]), wv[2]), 1.0f / 3.0f);
        Vec3 view_dir = vec3_normalize(vec3_sub(cam_pos, center));
        if (backface_cull && vec3_dot(normal, view_dir) < 0)
        {
            if (bf_culled)
                (*bf_culled)++;
            continue;
        }

        float intensity = vec3_dot(normal, light_dir);
        if (intensity < 0)
            intensity = 0;
        intensity = 0.15f + intensity * 0.85f;

        bool has_texture = face.texture_id >= 0 && face.texture_id < texture_count && textures != NULL;

        if (has_texture)
        {
            // Textured rendering with per-vertex UVs
            const Texture *tex = &textures[face.texture_id];
            float u0 = ch->vertices[idx[0]].u;
            float v0 = ch->vertices[idx[0]].v;
            float u1 = ch->vertices[idx[1]].u;
            float v1 = ch->vertices[idx[1]].v;
            float u2 = ch->vertices[idx[2]].u;
            float v2 = ch->vertices[idx[2]].v;

            ClipPolygon poly;
            poly.count = 3;
            poly.vertices[0] = (ClipVertex){cv[0], u0, v0, 0};
            poly.vertices[1] = (ClipVertex){cv[1], u1, v1, 0};
            poly.vertices[2] = (ClipVertex){cv[2], u2, v2, 0};

            ClipResult cr = clip_classify(&poly);
            if (cr == CLIP_REJECT)
                continue;
            if (cr == CLIP_NEEDED && clip_polygon_against_frustum(&poly) < 3)
                continue;
            if (cr == CLIP_ACCEPT && clip_trivial)
                (*clip_trivial)++;

            ProjectedVertex pv0 = render_project_vertex(poly.vertices[0].position);
            for (int j = 1; j < poly.count - 1; j++)
            {
                ProjectedVertex pv1 = render_project_vertex(poly.vertices[j].position);
                ProjectedVertex pv2 = render_project_vertex(poly.vertices[j + 1].position);

                render_fill_triangle_textured(
                    (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                    poly.vertices[0].u, poly.vertices[0].v, poly.vertices[0].position.w,
                    (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                    poly.vertices[j].u, poly.vertices[j].v, poly.vertices[j].position.w,
                    (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                    poly.vertices[j + 1].u, poly.vertices[j + 1].v, poly.vertices[j + 1].position.w,
                    tex, intensity);
                if (tri_drawn)
                    (*tri_drawn)++;
            }
        }
        else
        {
            // Flat shaded fallback
            uint32_t shaded = render_shade_color(face.color, intensity);

            ClipPolygon poly;
            poly.count = 3;
            poly.vertices[0] = (ClipVertex){cv[0], 0, 0, shaded};
            poly.vertices[1] = (ClipVertex){cv[1], 0, 0, shaded};
            poly.vertices[2] = (ClipVertex){cv[2], 0, 0, shaded};

            ClipResult cr = clip_classify(&poly);
            if (cr == CLIP_REJECT)
                continue;
            if (cr == CLIP_NEEDED && clip_polygon_against_frustum(&poly) < 3)
                continue;
            if (cr == CLIP_ACCEPT && clip_trivial)
                (*clip_trivial)++;

            ProjectedVertex pv0 = render_project_vertex(poly.vertices[0].position);
            for (int j = 1; j < poly.count - 1; j++)
            {
                ProjectedVertex pv1 = render_project_vertex(poly.vertices[j].position);
                ProjectedVertex pv2 = render_project_vertex(poly.vertices[j + 1].position);

                render_fill_triangle_z(
                    (int)pv0.screen.x, (int)pv0.screen.y, pv0.z, poly.vertices[0].position.w,
                    (int)pv1.screen.x, (int)pv1.screen.y, pv1.z, poly.vertices[j].position.w,
                    (int)pv2.screen.x, (int)pv2.screen.y, pv2.z, poly.vertices[j + 1].position.w,
                    poly.vertices[0].color);
                if (tri_drawn)
                    (*tri_drawn)++;
            }
        }
    }
}

static void render_chunk_wireframe(const WorldChunk *ch, Mat4 vp,
                                   Vec3 cam_pos,
                                   bool backface_cull,
                                   int *bf_culled, int *tri_drawn,
                                   int *clip_trivial)
{
    uint32_t gen = ++s_chunk_gen;
    TransformCache *cache = ch->cache;

    for (int i = 0; i < ch->face_count; i++)
    {
        OBJFace face = ch->faces[i];
        int idx[3] = {face.a, face.b, face.c};

        Vec3 wv[3];
        Vec4 cv[3];
        for (int k = 0; k < 3; k++)
        {
            OBJVertex *vert = &ch->vertices[idx[k]];
            TransformCache *tc = &cache[vert->pos_index];
            if (tc->gen != gen)
            {
                Vec4 pos4 = vec4_from_vec3(vert->position, 1.0f);
                tc->world = pos4;
                tc->clip = mat4_mul_vec4(vp, pos4);
                tc->gen = gen;
            }
            wv[k] = vec3_from_vec4(tc->world);
            cv[k] = tc->clip;
        }

        if (backface_cull)
        {
            Vec3 edge1 = vec3_sub(wv[1], wv[0]);
            Vec3 edge2 = vec3_sub(wv[2], wv[0]);
            Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));
            Vec3 center = vec3_mul(vec3_add(vec3_add(wv[0], wv[1]), wv[2]), 1.0f / 3.0f);
            Vec3 view_dir = vec3_normalize(vec3_sub(cam_pos, center));
            if (vec3_dot(normal, view_dir) < 0)
            {
                if (bf_culled)
                    (*bf_culled)++;
                continue;
            }
        }

        uint32_t color = face.color;
        ClipPolygon poly;
        poly.count = 3;
        poly.vertices[0] = (ClipVertex){cv[0], 0, 0, color};
        poly.vertices[1] = (ClipVertex){cv[1], 0, 0, color};
        poly.vertices[2] = (ClipVertex){cv[2], 0, 0, color};

        ClipResult cr = clip_classify(&poly);
        if (cr == CLIP_REJECT)
            continue;
        if (cr == CLIP_NEEDED && clip_polygon_against_frustum(&poly) < 3)
            continue;
        if (cr == CLIP_ACCEPT && clip_trivial)
            (*clip_trivial)++;

        for (int j = 0; j < poly.count; j++)
        {
            int next = (j + 1) % poly.count;
            ProjectedVertex a = render_project_vertex(poly.vertices[j].position);
            ProjectedVertex b = render_project_vertex(poly.vertices[next].position);
            render_draw_line((int)a.screen.x, (int)a.screen.y,
                             (int)b.screen.x, (int)b.screen.y, color);
        }
        if (tri_drawn)
            (*tri_drawn)++;
    }
}

void chunk_grid_render(const ChunkGrid *grid, Mat4 vp,
                       Vec3 camera_pos, Vec3 light_dir,
                       const Frustum *frustum, bool backface_cull,
                       RenderStats *stats_out)
{
    int culled = 0;
    int bf_culled = 0;
    int tri_drawn = 0;
    int clip_triv = 0;

    // Collect visible chunks with distance for front-to-back sorting
    RenderPacket packets[MAX_CHUNKS];
    int packet_count = 0;

    for (int i = 0; i < grid->count; i++)
    {
        const WorldChunk *ch = &grid->chunks[i];

        if (frustum)
        {
            if (!frustum_test_sphere(frustum, ch->center, ch->radius))
            {
                culled++;
                continue;
            }
        }

        Vec3 diff = vec3_sub(ch->center, camera_pos);
        float dist_sq = vec3_dot(diff, diff);

        packets[packet_count].chunk = ch;
        packets[packet_count].dist_sq = dist_sq;
        packet_count++;
    }

    // Sort front-to-back (closest first) for Z-buffer efficiency
    qsort(packets, (size_t)packet_count, sizeof(RenderPacket), compare_packets_asc);

    // Draw in sorted order
    for (int i = 0; i < packet_count; i++)
    {
        render_chunk_flat(packets[i].chunk, vp, camera_pos, light_dir,
                          backface_cull,
                          grid->textures, grid->texture_count,
                          &bf_culled, &tri_drawn, &clip_triv);
    }

    if (stats_out)
    {
        stats_out->entities_culled += culled;
        stats_out->backface_culled += bf_culled;
        stats_out->triangles_drawn += tri_drawn;
        stats_out->clip_trivial += clip_triv;
    }
}

void chunk_grid_render_wireframe(const ChunkGrid *grid, Mat4 vp,
                                 Vec3 camera_pos,
                                 const Frustum *frustum, bool backface_cull,
                                 RenderStats *stats_out)
{
    int culled = 0;
    int bf_culled = 0;
    int tri_drawn = 0;
    int clip_triv = 0;

    // Collect visible chunks with distance for front-to-back sorting
    RenderPacket packets[MAX_CHUNKS];
    int packet_count = 0;

    for (int i = 0; i < grid->count; i++)
    {
        const WorldChunk *ch = &grid->chunks[i];

        if (frustum)
        {
            if (!frustum_test_sphere(frustum, ch->center, ch->radius))
            {
                culled++;
                continue;
            }
        }

        Vec3 diff = vec3_sub(ch->center, camera_pos);
        float dist_sq = vec3_dot(diff, diff);

        packets[packet_count].chunk = ch;
        packets[packet_count].dist_sq = dist_sq;
        packet_count++;
    }

    // Sort front-to-back (closest first)
    qsort(packets, (size_t)packet_count, sizeof(RenderPacket), compare_packets_asc);

    // Draw in sorted order
    for (int i = 0; i < packet_count; i++)
    {
        render_chunk_wireframe(packets[i].chunk, vp, camera_pos,
                               backface_cull, &bf_culled, &tri_drawn, &clip_triv);
    }

    if (stats_out)
    {
        stats_out->entities_culled += culled;
        stats_out->backface_culled += bf_culled;
        stats_out->triangles_drawn += tri_drawn;
        stats_out->clip_trivial += clip_triv;
    }
}
