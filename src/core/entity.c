#include "core/entity.h"
#include "core/log.h"
#include <string.h>

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

// Render a Mesh entity with flat shading (cubes, etc.)
static void render_mesh_flat(const Entity *ent, Mat4 model, Mat4 vp,
                             Vec3 cam_pos, Vec3 light_dir)
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
        if (vec3_dot(normal, view_dir) < 0)
            continue;

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

        if (clip_polygon_against_frustum(&poly) < 3)
            continue;

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
        }
    }
}

// Render a Mesh entity with world-space UV texturing (floor tiles, etc.)
static void render_mesh_textured(const Entity *ent, Mat4 model, Mat4 vp,
                                 Vec3 cam_pos, Vec3 light_dir)
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
        if (vec3_dot(normal, view_dir) < 0)
            continue;

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

        if (clip_polygon_against_frustum(&poly) < 3)
            continue;

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
        }
    }
}

// Render an OBJMesh entity with flat shading
static void render_obj_flat(const Entity *ent, Mat4 model, Mat4 vp,
                            Vec3 cam_pos, Vec3 light_dir)
{
    OBJMesh *m = ent->obj_mesh;
    Mat4 mvp = mat4_mul(vp, model);

    for (int i = 0; i < m->face_count; i++)
    {
        OBJFace face = m->faces[i];
        OBJVertex ov0 = m->vertices[face.a];
        OBJVertex ov1 = m->vertices[face.b];
        OBJVertex ov2 = m->vertices[face.c];

        // World space for lighting
        Vec3 wv0 = vec3_from_vec4(mat4_mul_vec4(model, vec4_from_vec3(ov0.position, 1.0f)));
        Vec3 wv1 = vec3_from_vec4(mat4_mul_vec4(model, vec4_from_vec3(ov1.position, 1.0f)));
        Vec3 wv2 = vec3_from_vec4(mat4_mul_vec4(model, vec4_from_vec3(ov2.position, 1.0f)));

        Vec3 edge1 = vec3_sub(wv1, wv0);
        Vec3 edge2 = vec3_sub(wv2, wv0);
        Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

        // Backface culling
        Vec3 center = vec3_mul(vec3_add(vec3_add(wv0, wv1), wv2), 1.0f / 3.0f);
        Vec3 view_dir = vec3_normalize(vec3_sub(cam_pos, center));
        if (vec3_dot(normal, view_dir) < 0)
            continue;

        float intensity = vec3_dot(normal, light_dir);
        if (intensity < 0)
            intensity = 0;
        intensity = 0.15f + intensity * 0.85f;
        uint32_t shaded = render_shade_color(face.color, intensity);

        Vec4 t0 = mat4_mul_vec4(mvp, vec4_from_vec3(ov0.position, 1.0f));
        Vec4 t1 = mat4_mul_vec4(mvp, vec4_from_vec3(ov1.position, 1.0f));
        Vec4 t2 = mat4_mul_vec4(mvp, vec4_from_vec3(ov2.position, 1.0f));

        ClipPolygon poly;
        poly.count = 3;
        poly.vertices[0] = (ClipVertex){t0, 0, 0, shaded};
        poly.vertices[1] = (ClipVertex){t1, 0, 0, shaded};
        poly.vertices[2] = (ClipVertex){t2, 0, 0, shaded};

        if (clip_polygon_against_frustum(&poly) < 3)
            continue;

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
        }
    }
}

void scene_render(Scene *scene, Mat4 vp, Vec3 camera_pos, Vec3 light_dir)
{
    for (int i = 0; i < scene->count; i++)
    {
        Entity *ent = &scene->entities[i];
        if (!ent->active)
            continue;

        Mat4 model = entity_model_matrix(ent);

        switch (ent->type)
        {
        case ENTITY_MESH:
            if (ent->render_mode == RENDER_TEXTURED && ent->texture)
                render_mesh_textured(ent, model, vp, camera_pos, light_dir);
            else
                render_mesh_flat(ent, model, vp, camera_pos, light_dir);
            break;
        case ENTITY_OBJ_MESH:
            render_obj_flat(ent, model, vp, camera_pos, light_dir);
            break;
        }
    }
}

void scene_render_wireframe(Scene *scene, Mat4 vp)
{
    for (int i = 0; i < scene->count; i++)
    {
        Entity *ent = &scene->entities[i];
        if (!ent->active)
            continue;

        Mat4 model = entity_model_matrix(ent);
        Mat4 mvp = mat4_mul(vp, model);

        int face_count = 0;
        if (ent->type == ENTITY_MESH)
            face_count = ent->mesh->face_count;
        else
            face_count = ent->obj_mesh->face_count;

        for (int f = 0; f < face_count; f++)
        {
            Vec3 v[3];
            uint32_t color = 0xFF00FF00;

            if (ent->type == ENTITY_MESH)
            {
                Face face = ent->mesh->faces[f];
                v[0] = ent->mesh->vertices[face.a];
                v[1] = ent->mesh->vertices[face.b];
                v[2] = ent->mesh->vertices[face.c];
                color = face.color;
            }
            else
            {
                OBJFace face = ent->obj_mesh->faces[f];
                v[0] = ent->obj_mesh->vertices[face.a].position;
                v[1] = ent->obj_mesh->vertices[face.b].position;
                v[2] = ent->obj_mesh->vertices[face.c].position;
                color = face.color;
            }

            // Clip-space transform
            Vec4 cv[3];
            for (int k = 0; k < 3; k++)
                cv[k] = mat4_mul_vec4(mvp, vec4_from_vec3(v[k], 1.0f));

            ClipPolygon poly;
            poly.count = 3;
            poly.vertices[0] = (ClipVertex){cv[0], 0, 0, color};
            poly.vertices[1] = (ClipVertex){cv[1], 0, 0, color};
            poly.vertices[2] = (ClipVertex){cv[2], 0, 0, color};

            if (clip_polygon_against_frustum(&poly) < 3)
                continue;

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
        }
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
