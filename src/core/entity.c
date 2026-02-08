#include "core/entity.h"
#include "core/log.h"
#include <string.h>
#include <float.h>

static uint32_t s_transform_gen = 0;

void scene_init(Scene *scene)
{
    memset(scene, 0, sizeof(Scene));
    LOG_INFO("Scene initialized (capacity: %d entities)", MAX_ENTITIES);
}

int scene_add_mesh(Scene *scene, Mesh *mesh, Vec3 position, float scale)
{
    if (scene->count >= MAX_ENTITIES)
    {
        LOG_ERROR("Scene full (%d/%d), cannot add mesh entity", scene->count, MAX_ENTITIES);
        return -1;
    }

    int idx = scene->count++;
    Entity *ent = &scene->entities[idx];

    ent->type = ENTITY_MESH;
    ent->render_mode = RENDER_FLAT_SHADED;
    ent->mesh = mesh;
    ent->position = position;
    ent->rotation = (Vec3){0, 0, 0};
    ent->rotation_speed = (Vec3){0, 0, 0};
    ent->scale = scale;
    ent->texture = NULL;
    ent->uv_scale = 1.0f;
    ent->active = true;
    ent->pickable = true;
    ent->hit_timer = 0;

    return idx;
}

int scene_add_obj(Scene *scene, OBJMesh *obj, Vec3 position, float scale)
{
    if (scene->count >= MAX_ENTITIES)
    {
        LOG_ERROR("Scene full (%d/%d), cannot add OBJ entity", scene->count, MAX_ENTITIES);
        return -1;
    }

    int idx = scene->count++;
    Entity *ent = &scene->entities[idx];

    ent->type = ENTITY_OBJ_MESH;
    ent->render_mode = RENDER_FLAT_SHADED;
    ent->obj_mesh = obj;
    ent->position = position;
    ent->rotation = (Vec3){0, 0, 0};
    ent->rotation_speed = (Vec3){0, 0, 0};
    ent->scale = scale;
    ent->texture = NULL;
    ent->uv_scale = 1.0f;
    ent->active = true;
    ent->pickable = true;
    ent->hit_timer = 0;

    return idx;
}

void scene_set_rotation_speed(Scene *scene, int idx, Vec3 speed)
{
    if (idx < 0 || idx >= scene->count)
        return;
    scene->entities[idx].rotation_speed = speed;
}

void scene_set_texture(Scene *scene, int idx, Texture *tex, float uv_scale)
{
    if (idx < 0 || idx >= scene->count)
        return;
    scene->entities[idx].render_mode = RENDER_TEXTURED;
    scene->entities[idx].texture = tex;
    scene->entities[idx].uv_scale = uv_scale;
}

void scene_update(Scene *scene, float dt)
{
    for (int i = 0; i < scene->count; i++)
    {
        Entity *ent = &scene->entities[i];
        if (!ent->active)
            continue;

        ent->rotation.x += ent->rotation_speed.x * dt;
        ent->rotation.y += ent->rotation_speed.y * dt;
        ent->rotation.z += ent->rotation_speed.z * dt;

        if (ent->hit_timer > 0)
            ent->hit_timer -= dt;
    }
}

static Mat4 entity_model_matrix(const Entity *ent)
{
    Mat4 t = mat4_translate(ent->position.x, ent->position.y, ent->position.z);
    Mat4 ry = mat4_rotate_y(ent->rotation.y);
    Mat4 rx = mat4_rotate_x(ent->rotation.x);
    Mat4 rz = mat4_rotate_z(ent->rotation.z);
    Mat4 s = mat4_scale(ent->scale, ent->scale, ent->scale);

    // T * Ry * Rx * Rz * S
    return mat4_mul(t, mat4_mul(ry, mat4_mul(rx, mat4_mul(rz, s))));
}

// Compute world-space bounding sphere center and scaled radius for an entity
static void entity_bounding_sphere(const Entity *ent, Vec3 *center_out, float *radius_out)
{
    AABB local;
    float local_radius;
    if (ent->type == ENTITY_MESH)
    {
        local = ent->mesh->bounds;
        local_radius = ent->mesh->radius;
    }
    else
    {
        local = ent->obj_mesh->bounds;
        local_radius = ent->obj_mesh->radius;
    }

    Vec3 local_center = vec3_mul(vec3_add(local.min, local.max), 0.5f);
    *center_out = vec3_add(ent->position, vec3_mul(local_center, ent->scale));
    *radius_out = local_radius * ent->scale;
}

// Render a Mesh entity with flat shading (cubes, etc.)
static void render_mesh_flat(const Entity *ent, Mat4 model, Mat4 vp,
                             Vec3 cam_pos, Vec3 light_dir,
                             bool backface_cull, int *bf_culled, int *tri_drawn,
                             int *clip_trivial)
{
    Mesh *m = ent->mesh;
    Mat4 mvp = mat4_mul(vp, model);

    for (int i = 0; i < m->face_count; i++)
    {
        Face face = m->faces[i];
        Vec3 lv0 = m->vertices[face.a];
        Vec3 lv1 = m->vertices[face.b];
        Vec3 lv2 = m->vertices[face.c];

        // World space
        Vec3 wv0 = vec3_from_vec4(mat4_mul_vec4(model, vec4_from_vec3(lv0, 1.0f)));
        Vec3 wv1 = vec3_from_vec4(mat4_mul_vec4(model, vec4_from_vec3(lv1, 1.0f)));
        Vec3 wv2 = vec3_from_vec4(mat4_mul_vec4(model, vec4_from_vec3(lv2, 1.0f)));

        Vec3 edge1 = vec3_sub(wv1, wv0);
        Vec3 edge2 = vec3_sub(wv2, wv0);
        Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

        // Backface culling
        Vec3 center = vec3_mul(vec3_add(vec3_add(wv0, wv1), wv2), 1.0f / 3.0f);
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
        intensity = 0.2f + intensity * 0.8f;
        uint32_t shaded = render_shade_color(face.color, intensity);

        Vec4 t0 = mat4_mul_vec4(mvp, vec4_from_vec3(lv0, 1.0f));
        Vec4 t1 = mat4_mul_vec4(mvp, vec4_from_vec3(lv1, 1.0f));
        Vec4 t2 = mat4_mul_vec4(mvp, vec4_from_vec3(lv2, 1.0f));

        ClipPolygon poly;
        poly.count = 3;
        poly.vertices[0] = (ClipVertex){t0, 0, 0, shaded};
        poly.vertices[1] = (ClipVertex){t1, 0, 0, shaded};
        poly.vertices[2] = (ClipVertex){t2, 0, 0, shaded};

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
                (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                poly.vertices[0].color);
            if (tri_drawn)
                (*tri_drawn)++;
        }
    }
}

// Render a Mesh entity with world-space UV texturing (floor tiles, etc.)
static void render_mesh_textured(const Entity *ent, Mat4 model, Mat4 vp,
                                 Vec3 cam_pos, Vec3 light_dir,
                                 bool backface_cull, int *bf_culled, int *tri_drawn,
                                 int *clip_trivial)
{
    Mesh *m = ent->mesh;

    for (int i = 0; i < m->face_count; i++)
    {
        Face face = m->faces[i];
        Vec3 lv0 = m->vertices[face.a];
        Vec3 lv1 = m->vertices[face.b];
        Vec3 lv2 = m->vertices[face.c];

        // World space
        Vec4 w0 = mat4_mul_vec4(model, vec4_from_vec3(lv0, 1.0f));
        Vec4 w1 = mat4_mul_vec4(model, vec4_from_vec3(lv1, 1.0f));
        Vec4 w2 = mat4_mul_vec4(model, vec4_from_vec3(lv2, 1.0f));
        Vec3 wv0 = vec3_from_vec4(w0);
        Vec3 wv1 = vec3_from_vec4(w1);
        Vec3 wv2 = vec3_from_vec4(w2);

        Vec3 edge1 = vec3_sub(wv1, wv0);
        Vec3 edge2 = vec3_sub(wv2, wv0);
        Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

        // Backface culling
        Vec3 center = vec3_mul(vec3_add(vec3_add(wv0, wv1), wv2), 1.0f / 3.0f);
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
        intensity = 0.3f + intensity * 0.7f;

        // UVs derived from world XZ position
        float us = ent->uv_scale;
        float u0 = wv0.x * us, vt0 = wv0.z * us;
        float u1 = wv1.x * us, vt1 = wv1.z * us;
        float u2 = wv2.x * us, vt2 = wv2.z * us;

        Vec4 t0 = mat4_mul_vec4(vp, vec4_from_vec3(wv0, 1.0f));
        Vec4 t1 = mat4_mul_vec4(vp, vec4_from_vec3(wv1, 1.0f));
        Vec4 t2 = mat4_mul_vec4(vp, vec4_from_vec3(wv2, 1.0f));

        ClipPolygon poly;
        poly.count = 3;
        poly.vertices[0] = (ClipVertex){t0, u0, vt0, 0};
        poly.vertices[1] = (ClipVertex){t1, u1, vt1, 0};
        poly.vertices[2] = (ClipVertex){t2, u2, vt2, 0};

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
                ent->texture, intensity);
            if (tri_drawn)
                (*tri_drawn)++;
        }
    }
}

// Render an OBJMesh entity with flat shading
static void render_obj_flat(const Entity *ent, Mat4 model, Mat4 vp,
                            Vec3 cam_pos, Vec3 light_dir,
                            bool backface_cull, int *bf_culled, int *tri_drawn,
                            int *clip_trivial)
{
    OBJMesh *m = ent->obj_mesh;
    Mat4 mvp = mat4_mul(vp, model);
    uint32_t gen = ++s_transform_gen;
    TransformCache *cache = m->cache;

    for (int i = 0; i < m->face_count; i++)
    {
        OBJFace face = m->faces[i];
        int idx[3] = {face.a, face.b, face.c};

        Vec3 wv[3];
        Vec4 cv[3];
        for (int k = 0; k < 3; k++)
        {
            OBJVertex *vert = &m->vertices[idx[k]];
            TransformCache *tc = &cache[vert->pos_index];
            if (tc->gen != gen)
            {
                tc->world = mat4_mul_vec4(model, vec4_from_vec3(vert->position, 1.0f));
                tc->clip = mat4_mul_vec4(mvp, vec4_from_vec3(vert->position, 1.0f));
                tc->gen = gen;
            }
            wv[k] = vec3_from_vec4(tc->world);
            cv[k] = tc->clip;
        }

        Vec3 edge1 = vec3_sub(wv[1], wv[0]);
        Vec3 edge2 = vec3_sub(wv[2], wv[0]);
        Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

        // Backface culling
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
                (int)pv0.screen.x, (int)pv0.screen.y, pv0.z,
                (int)pv1.screen.x, (int)pv1.screen.y, pv1.z,
                (int)pv2.screen.x, (int)pv2.screen.y, pv2.z,
                poly.vertices[0].color);
            if (tri_drawn)
                (*tri_drawn)++;
        }
    }
}

void scene_render(Scene *scene, Mat4 vp, Vec3 camera_pos, Vec3 light_dir,
                  const Frustum *frustum, bool backface_cull,
                  RenderStats *stats_out)
{
    int culled = 0;
    int bf_culled = 0;
    int tri_drawn = 0;
    int clip_triv = 0;

    for (int i = 0; i < scene->count; i++)
    {
        Entity *ent = &scene->entities[i];
        if (!ent->active)
            continue;

        // Frustum culling via bounding sphere
        if (frustum)
        {
            Vec3 sphere_center;
            float sphere_radius;
            entity_bounding_sphere(ent, &sphere_center, &sphere_radius);
            if (!frustum_test_sphere(frustum, sphere_center, sphere_radius))
            {
                culled++;
                continue;
            }
        }

        Mat4 model = entity_model_matrix(ent);

        switch (ent->type)
        {
        case ENTITY_MESH:
            if (ent->render_mode == RENDER_TEXTURED && ent->texture)
                render_mesh_textured(ent, model, vp, camera_pos, light_dir,
                                     backface_cull, &bf_culled, &tri_drawn,
                                     &clip_triv);
            else
                render_mesh_flat(ent, model, vp, camera_pos, light_dir,
                                 backface_cull, &bf_culled, &tri_drawn,
                                 &clip_triv);
            break;
        case ENTITY_OBJ_MESH:
            render_obj_flat(ent, model, vp, camera_pos, light_dir,
                            backface_cull, &bf_culled, &tri_drawn,
                            &clip_triv);
            break;
        }
    }

    if (stats_out)
    {
        stats_out->entities_culled = culled;
        stats_out->backface_culled = bf_culled;
        stats_out->triangles_drawn = tri_drawn;
        stats_out->clip_trivial = clip_triv;
    }
}

void scene_render_wireframe(Scene *scene, Mat4 vp, Vec3 camera_pos,
                            const Frustum *frustum, bool backface_cull,
                            RenderStats *stats_out)
{
    int culled = 0;
    int bf_culled = 0;
    int tri_drawn = 0;
    int clip_triv = 0;

    for (int i = 0; i < scene->count; i++)
    {
        Entity *ent = &scene->entities[i];
        if (!ent->active)
            continue;

        // Frustum culling via bounding sphere
        if (frustum)
        {
            Vec3 sphere_center;
            float sphere_radius;
            entity_bounding_sphere(ent, &sphere_center, &sphere_radius);
            if (!frustum_test_sphere(frustum, sphere_center, sphere_radius))
            {
                culled++;
                continue;
            }
        }

        Mat4 model = entity_model_matrix(ent);
        Mat4 mvp = mat4_mul(vp, model);

        int face_count = 0;
        if (ent->type == ENTITY_MESH)
            face_count = ent->mesh->face_count;
        else
            face_count = ent->obj_mesh->face_count;

        // Per-entity cache generation for OBJ vertex deduplication
        uint32_t gen = 0;
        TransformCache *tcache = NULL;
        if (ent->type == ENTITY_OBJ_MESH)
        {
            gen = ++s_transform_gen;
            tcache = ent->obj_mesh->cache;
        }

        for (int f = 0; f < face_count; f++)
        {
            Vec3 wv[3];
            Vec4 cv[3];
            uint32_t color = 0xFF00FF00;

            if (ent->type == ENTITY_MESH)
            {
                Face face = ent->mesh->faces[f];
                Vec3 v[3] = {
                    ent->mesh->vertices[face.a],
                    ent->mesh->vertices[face.b],
                    ent->mesh->vertices[face.c]};
                color = face.color;
                for (int k = 0; k < 3; k++)
                {
                    Vec4 h = vec4_from_vec3(v[k], 1.0f);
                    wv[k] = vec3_from_vec4(mat4_mul_vec4(model, h));
                    cv[k] = mat4_mul_vec4(mvp, h);
                }
            }
            else
            {
                OBJFace face = ent->obj_mesh->faces[f];
                int idx[3] = {face.a, face.b, face.c};
                color = face.color;
                for (int k = 0; k < 3; k++)
                {
                    OBJVertex *vert = &ent->obj_mesh->vertices[idx[k]];
                    TransformCache *tc = &tcache[vert->pos_index];
                    if (tc->gen != gen)
                    {
                        tc->world = mat4_mul_vec4(model, vec4_from_vec3(vert->position, 1.0f));
                        tc->clip = mat4_mul_vec4(mvp, vec4_from_vec3(vert->position, 1.0f));
                        tc->gen = gen;
                    }
                    wv[k] = vec3_from_vec4(tc->world);
                    cv[k] = tc->clip;
                }
            }

            // Backface culling in wireframe
            if (backface_cull)
            {
                Vec3 edge1 = vec3_sub(wv[1], wv[0]);
                Vec3 edge2 = vec3_sub(wv[2], wv[0]);
                Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

                Vec3 center = vec3_mul(vec3_add(vec3_add(wv[0], wv[1]), wv[2]), 1.0f / 3.0f);
                Vec3 view_dir = vec3_normalize(vec3_sub(camera_pos, center));
                if (vec3_dot(normal, view_dir) < 0)
                {
                    bf_culled++;
                    continue;
                }
            }

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
            if (cr == CLIP_ACCEPT)
                clip_triv++;

            // Draw wireframe edges of the clipped polygon
            for (int j = 0; j < poly.count; j++)
            {
                int next = (j + 1) % poly.count;
                ProjectedVertex a = render_project_vertex(poly.vertices[j].position);
                ProjectedVertex b = render_project_vertex(poly.vertices[next].position);

                render_draw_line(
                    (int)a.screen.x, (int)a.screen.y,
                    (int)b.screen.x, (int)b.screen.y,
                    color);
            }
            tri_drawn++;
        }
    }

    if (stats_out)
    {
        stats_out->entities_culled = culled;
        stats_out->backface_culled = bf_culled;
        stats_out->triangles_drawn = tri_drawn;
        stats_out->clip_trivial = clip_triv;
    }
}

AABB entity_get_world_aabb(const Entity *ent)
{
    AABB local;
    if (ent->type == ENTITY_MESH)
        local = ent->mesh->bounds;
    else
        local = ent->obj_mesh->bounds;

    Vec3 scaled_min = vec3_mul(local.min, ent->scale);
    Vec3 scaled_max = vec3_mul(local.max, ent->scale);

    // Half-extents of the scaled box
    Vec3 he = vec3_mul(vec3_sub(scaled_max, scaled_min), 0.5f);
    Vec3 center_local = vec3_mul(vec3_add(scaled_min, scaled_max), 0.5f);

    // Worst-case radius for Y-rotation: max of XZ extent
    float rx = (he.x > he.z) ? he.x : he.z;
    Vec3 worst_half = {rx, he.y, rx};

    Vec3 world_center = vec3_add(ent->position, center_local);
    return aabb_from_center_size(world_center, worst_half);
}

int scene_ray_pick(const Scene *scene, Ray ray, float *t_out)
{
    int closest = -1;
    float closest_t = FLT_MAX;

    for (int i = 0; i < scene->count; i++)
    {
        const Entity *ent = &scene->entities[i];
        if (!ent->active || !ent->pickable)
            continue;

        AABB world_box = entity_get_world_aabb(ent);
        float t;
        if (ray_aabb_intersect(ray, world_box, &t) && t > 0 && t < closest_t)
        {
            closest_t = t;
            closest = i;
        }
    }

    if (t_out)
        *t_out = closest_t;
    return closest;
}
