#include "core/obj_loader.h"
#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#define OBJ_INITIAL_CAP 1024

// Read an entire file into a heap-allocated buffer.
// Returns NULL on failure. Caller must free() the result.
static char *obj_read_file(const char *path, long *out_size)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        LOG_ERROR("Cannot open file: %s", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0)
    {
        LOG_ERROR("File is empty or unreadable: %s", path);
        fclose(fp);
        return NULL;
    }

    char *buffer = (char *)malloc(size + 1);
    if (!buffer)
    {
        LOG_ERROR("Failed to allocate %ld bytes for file: %s", size, path);
        fclose(fp);
        return NULL;
    }

    long bytes_read = (long)fread(buffer, 1, size, fp);
    fclose(fp);

    buffer[bytes_read] = '\0';
    if (out_size)
        *out_size = bytes_read;

    return buffer;
}

// Grow a dynamic array when capacity is reached
static void *obj_grow(void *ptr, int *cap, int elem_size)
{
    int new_cap = (*cap) * 2;
    void *new_ptr = realloc(ptr, new_cap * elem_size);
    if (!new_ptr)
    {
        LOG_ERROR("Failed to grow buffer from %d to %d elements", *cap, new_cap);
        return ptr;
    }
    *cap = new_cap;
    return new_ptr;
}

// Parse a face index group: "v", "v/vt", "v/vt/vn", or "v//vn"
// All indices are converted from 1-based to 0-based.
// Missing components are set to -1.
static void obj_parse_face_index(const char *token, int *vi, int *ti, int *ni)
{
    *vi = -1;
    *ti = -1;
    *ni = -1;

    // v
    *vi = atoi(token) - 1;

    const char *slash1 = strchr(token, '/');
    if (!slash1)
        return;

    // v/vt or v//vn
    slash1++;
    if (*slash1 != '/')
    {
        *ti = atoi(slash1) - 1;
    }

    const char *slash2 = strchr(slash1, '/');
    if (!slash2)
        return;

    // vn
    slash2++;
    *ni = atoi(slash2) - 1;
}

// Extract directory portion of a file path into dir_buf
static void obj_get_dir(const char *path, char *dir_buf, int buf_size)
{
    strncpy(dir_buf, path, buf_size - 1);
    dir_buf[buf_size - 1] = '\0';

    char *last_slash = strrchr(dir_buf, '/');
    if (last_slash)
        *(last_slash + 1) = '\0';
    else
        dir_buf[0] = '\0';
}

// Convert Kd RGB floats to 0xAARRGGBB
static uint32_t kd_to_argb(float r, float g, float b)
{
    int ri = (int)(r * 255.0f);
    int gi = (int)(g * 255.0f);
    int bi = (int)(b * 255.0f);
    if (ri > 255)
        ri = 255;
    if (gi > 255)
        gi = 255;
    if (bi > 255)
        bi = 255;
    return 0xFF000000 | (ri << 16) | (gi << 8) | bi;
}

// Parse a .mtl file and populate materials in the mesh.
// dir is the directory prefix for relative texture paths.
static int obj_load_mtl(OBJMesh *mesh, const char *mtl_path, const char *dir)
{
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", dir, mtl_path);

    long mtl_size = 0;
    char *mtl_data = obj_read_file(full_path, &mtl_size);
    if (!mtl_data)
    {
        LOG_WARN("Cannot open MTL file: %s", full_path);
        return 1;
    }

    OBJMaterial *cur = NULL;
    char *line = mtl_data;
    while (line && *line)
    {
        char *eol = strchr(line, '\n');
        if (eol)
            *eol = '\0';

        while (*line == ' ' || *line == '\t')
            line++;

        // Remove trailing whitespace/carriage return
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' '))
            line[--len] = '\0';

        if (strncmp(line, "newmtl ", 7) == 0)
        {
            if (mesh->material_count >= OBJ_MAX_MATERIALS)
            {
                LOG_WARN("Max materials reached (%d)", OBJ_MAX_MATERIALS);
            }
            else
            {
                cur = &mesh->materials[mesh->material_count++];
                memset(cur, 0, sizeof(OBJMaterial));
                cur->texture_id = -1;
                cur->color = 0xFFCCCCCC;
                strncpy(cur->name, line + 7, OBJ_MTL_NAME_MAX - 1);
            }
        }
        else if (cur && strncmp(line, "map_Kd ", 7) == 0)
        {
            strncpy(cur->diffuse_path, line + 7, sizeof(cur->diffuse_path) - 1);
        }
        else if (cur && strncmp(line, "Kd ", 3) == 0)
        {
            float r = 0, g = 0, b = 0;
            sscanf(line + 3, "%f %f %f", &r, &g, &b);
            cur->color = kd_to_argb(r, g, b);
        }

        line = eol ? eol + 1 : NULL;
    }

    free(mtl_data);
    LOG_INFO("MTL loaded: %s (%d materials)", full_path, mesh->material_count);
    return 0;
}

// Load all referenced textures from parsed materials.
// Sets texture_id on each material that has a valid diffuse map.
static void obj_load_textures(OBJMesh *mesh, const char *dir)
{
    // Count how many materials have a diffuse map
    int tex_needed = 0;
    for (int i = 0; i < mesh->material_count; i++)
    {
        if (mesh->materials[i].diffuse_path[0] != '\0')
            tex_needed++;
    }

    if (tex_needed == 0)
        return;

    mesh->textures = (Texture *)calloc(tex_needed, sizeof(Texture));
    if (!mesh->textures)
    {
        LOG_ERROR("Failed to allocate %d textures", tex_needed);
        return;
    }

    int loaded = 0;
    for (int i = 0; i < mesh->material_count; i++)
    {
        OBJMaterial *mat = &mesh->materials[i];
        if (mat->diffuse_path[0] == '\0')
            continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", dir, mat->diffuse_path);

        if (texture_load(&mesh->textures[loaded], full_path) == 0)
        {
            mat->texture_id = loaded;
            loaded++;
        }
        else
        {
            LOG_WARN("Failed to load texture: %s", full_path);
        }
    }

    mesh->texture_count = loaded;
    LOG_INFO("Loaded %d/%d textures", loaded, tex_needed);
}

// Find material index by name, returns -1 if not found
static int obj_find_material(const OBJMesh *mesh, const char *name)
{
    for (int i = 0; i < mesh->material_count; i++)
    {
        if (strcmp(mesh->materials[i].name, name) == 0)
            return i;
    }
    return -1;
}

int obj_load(OBJMesh *mesh, const char *path)
{
    memset(mesh, 0, sizeof(*mesh));

    long file_size = 0;
    char *file_data = obj_read_file(path, &file_size);
    if (!file_data)
        return 1;

    LOG_INFO("Parsing OBJ: %s (%ld bytes)", path, file_size);

    // Extract directory for resolving relative MTL/texture paths
    char obj_dir[256];
    obj_get_dir(path, obj_dir, sizeof(obj_dir));

    // Temporary arrays for raw OBJ data
    int v_cap = OBJ_INITIAL_CAP, v_count = 0;
    int vt_cap = OBJ_INITIAL_CAP, vt_count = 0;
    int vn_cap = OBJ_INITIAL_CAP, vn_count = 0;
    int f_cap = OBJ_INITIAL_CAP, f_count = 0;

    Vec3 *positions = (Vec3 *)malloc(v_cap * sizeof(Vec3));
    Vec3 *normals = (Vec3 *)malloc(vn_cap * sizeof(Vec3));
    float *texcoords = (float *)malloc(vt_cap * 2 * sizeof(float));

    // Unrolled output vertices (3 per triangle face)
    int out_cap = OBJ_INITIAL_CAP;
    OBJVertex *out_verts = (OBJVertex *)malloc(out_cap * sizeof(OBJVertex));
    OBJFace *out_faces = (OBJFace *)malloc(f_cap * sizeof(OBJFace));

    if (!positions || !normals || !texcoords || !out_verts || !out_faces)
    {
        LOG_ERROR("Failed to allocate OBJ parse buffers");
        free(file_data);
        free(positions);
        free(normals);
        free(texcoords);
        free(out_verts);
        free(out_faces);
        return 1;
    }

    int out_vert_count = 0;
    int current_material = -1; // Active material index

    // First pass: scan for mtllib to load materials and textures before parsing faces
    {
        char *scan = file_data;
        while (scan && *scan)
        {
            char *eol = strchr(scan, '\n');
            if (eol)
                *eol = '\0';

            while (*scan == ' ' || *scan == '\t')
                scan++;

            if (strncmp(scan, "mtllib ", 7) == 0)
            {
                char mtl_name[256];
                if (sscanf(scan + 7, "%255s", mtl_name) == 1)
                {
                    obj_load_mtl(mesh, mtl_name, obj_dir);
                    obj_load_textures(mesh, obj_dir);
                }
            }

            if (eol)
            {
                *eol = '\n'; // Restore newline for second pass
                scan = eol + 1;
            }
            else
            {
                break;
            }
        }
    }

    // Parse line by line
    char *line = file_data;
    while (line && *line)
    {
        // Find end of line
        char *eol = strchr(line, '\n');
        if (eol)
            *eol = '\0';

        // Skip leading whitespace
        while (*line == ' ' || *line == '\t')
            line++;

        if (strncmp(line, "v ", 2) == 0)
        {
            // Vertex position
            Vec3 p = {0};
            sscanf(line + 2, "%f %f %f", &p.x, &p.y, &p.z);
            if (v_count >= v_cap)
                positions = (Vec3 *)obj_grow(positions, &v_cap, sizeof(Vec3));
            positions[v_count++] = p;
        }
        else if (strncmp(line, "vt ", 3) == 0)
        {
            // Texture coordinate
            float u = 0, v = 0;
            sscanf(line + 3, "%f %f", &u, &v);
            if (vt_count >= vt_cap)
            {
                texcoords = (float *)obj_grow(texcoords, &vt_cap, 2 * sizeof(float));
            }
            texcoords[vt_count * 2 + 0] = u;
            texcoords[vt_count * 2 + 1] = v;
            vt_count++;
        }
        else if (strncmp(line, "vn ", 3) == 0)
        {
            // Vertex normal
            Vec3 n = {0};
            sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z);
            if (vn_count >= vn_cap)
                normals = (Vec3 *)obj_grow(normals, &vn_cap, sizeof(Vec3));
            normals[vn_count++] = n;
        }
        else if (strncmp(line, "usemtl ", 7) == 0)
        {
            char mtl_name[OBJ_MTL_NAME_MAX];
            if (sscanf(line + 7, "%63s", mtl_name) == 1)
                current_material = obj_find_material(mesh, mtl_name);
        }
        else if (line[0] == 'f' && line[1] == ' ')
        {
            // Face: triangulate fan-style for polygons with >3 verts
            char *tokens[64];
            int token_count = 0;
            char *tok = line + 2;

            while (*tok && token_count < 64)
            {
                while (*tok == ' ')
                    tok++;
                if (*tok == '\0')
                    break;
                tokens[token_count++] = tok;
                while (*tok && *tok != ' ')
                    tok++;
                if (*tok)
                    *tok++ = '\0';
            }

            if (token_count < 3)
            {
                line = eol ? eol + 1 : NULL;
                continue;
            }

            // Determine face color and texture from current material
            uint32_t face_color = 0xFFCCCCCC;
            int face_tex_id = -1;
            if (current_material >= 0 && current_material < mesh->material_count)
            {
                face_color = mesh->materials[current_material].color;
                face_tex_id = mesh->materials[current_material].texture_id;
            }

            // Parse first vertex (pivot for fan triangulation)
            int vi0, ti0, ni0;
            obj_parse_face_index(tokens[0], &vi0, &ti0, &ni0);

            for (int i = 1; i < token_count - 1; i++)
            {
                int vi1, ti1, ni1;
                int vi2, ti2, ni2;
                obj_parse_face_index(tokens[i], &vi1, &ti1, &ni1);
                obj_parse_face_index(tokens[i + 1], &vi2, &ti2, &ni2);

                // Grow output arrays if needed
                while (out_vert_count + 3 > out_cap)
                {
                    out_verts = (OBJVertex *)obj_grow(out_verts, &out_cap, sizeof(OBJVertex));
                }
                while (f_count >= f_cap)
                {
                    int old_cap = f_cap;
                    out_faces = (OBJFace *)obj_grow(out_faces, &f_cap, sizeof(OBJFace));
                    (void)old_cap;
                }

                // Unroll vertex 0
                int idx_base = out_vert_count;

                int indices[3] = {vi0, vi1, vi2};
                int tex_idx[3] = {ti0, ti1, ti2};
                int norm_idx[3] = {ni0, ni1, ni2};

                for (int k = 0; k < 3; k++)
                {
                    OBJVertex ov = {0};

                    if (indices[k] >= 0 && indices[k] < v_count)
                        ov.position = positions[indices[k]];

                    if (norm_idx[k] >= 0 && norm_idx[k] < vn_count)
                        ov.normal = normals[norm_idx[k]];

                    if (tex_idx[k] >= 0 && tex_idx[k] < vt_count)
                    {
                        ov.u = texcoords[tex_idx[k] * 2 + 0];
                        ov.v = texcoords[tex_idx[k] * 2 + 1];
                    }

                    ov.pos_index = indices[k];

                    out_verts[out_vert_count++] = ov;
                }

                out_faces[f_count++] = (OBJFace){
                    .a = idx_base,
                    .b = idx_base + 1,
                    .c = idx_base + 2,
                    .color = face_color,
                    .texture_id = face_tex_id};
            }
        }

        line = eol ? eol + 1 : NULL;
    }

    // Shrink to fit
    OBJVertex *shrunk_verts = (OBJVertex *)realloc(out_verts, out_vert_count * sizeof(OBJVertex));
    OBJFace *shrunk_faces = (OBJFace *)realloc(out_faces, f_count * sizeof(OBJFace));

    mesh->vertices = shrunk_verts ? shrunk_verts : out_verts;
    mesh->faces = shrunk_faces ? shrunk_faces : out_faces;
    mesh->vertex_count = out_vert_count;
    mesh->face_count = f_count;
    mesh->position_count = v_count;
    mesh->cache = (TransformCache *)calloc(v_count, sizeof(TransformCache));

    // Compute AABB from all vertex positions
    mesh->bounds.min = (Vec3){FLT_MAX, FLT_MAX, FLT_MAX};
    mesh->bounds.max = (Vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (int i = 0; i < out_vert_count; i++)
    {
        Vec3 p = mesh->vertices[i].position;
        if (p.x < mesh->bounds.min.x)
            mesh->bounds.min.x = p.x;
        if (p.y < mesh->bounds.min.y)
            mesh->bounds.min.y = p.y;
        if (p.z < mesh->bounds.min.z)
            mesh->bounds.min.z = p.z;
        if (p.x > mesh->bounds.max.x)
            mesh->bounds.max.x = p.x;
        if (p.y > mesh->bounds.max.y)
            mesh->bounds.max.y = p.y;
        if (p.z > mesh->bounds.max.z)
            mesh->bounds.max.z = p.z;
    }

    LOG_INFO("OBJ AABB: min(%.2f, %.2f, %.2f) max(%.2f, %.2f, %.2f)",
             mesh->bounds.min.x, mesh->bounds.min.y, mesh->bounds.min.z,
             mesh->bounds.max.x, mesh->bounds.max.y, mesh->bounds.max.z);

    // Bounding sphere radius from AABB center to furthest vertex
    Vec3 center = vec3_mul(vec3_add(mesh->bounds.min, mesh->bounds.max), 0.5f);
    float max_dist_sq = 0.0f;
    for (int i = 0; i < out_vert_count; i++)
    {
        Vec3 d = vec3_sub(mesh->vertices[i].position, center);
        float dist_sq = vec3_dot(d, d);
        if (dist_sq > max_dist_sq)
            max_dist_sq = dist_sq;
    }
    mesh->radius = sqrtf(max_dist_sq);

    LOG_INFO("OBJ bounding radius: %.2f", mesh->radius);

    LOG_INFO("OBJ loaded: %d positions, %d texcoords, %d normals -> %d triangles (%d unrolled verts)",
             v_count, vt_count, vn_count, f_count, out_vert_count);

    free(positions);
    free(normals);
    free(texcoords);
    free(file_data);

    return 0;
}

void obj_mesh_free(OBJMesh *mesh)
{
    if (mesh->vertices)
    {
        free(mesh->vertices);
        mesh->vertices = NULL;
    }
    if (mesh->faces)
    {
        free(mesh->faces);
        mesh->faces = NULL;
    }
    if (mesh->cache)
    {
        free(mesh->cache);
        mesh->cache = NULL;
    }
    if (mesh->textures)
    {
        for (int i = 0; i < mesh->texture_count; i++)
            texture_free(&mesh->textures[i]);
        free(mesh->textures);
        mesh->textures = NULL;
    }
    mesh->vertex_count = 0;
    mesh->face_count = 0;
    mesh->position_count = 0;
    mesh->material_count = 0;
    mesh->texture_count = 0;
}
