#include "core/level.h"
#include "core/log.h"
#include "core/collision_grid.h"
#include "core/chunk.h"
#include "graphics/mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim_line(char *line)
{
    char *end = line + strlen(line) - 1;
    while (end > line && (*end == '\n' || *end == '\r' || isspace(*end)))
        *end-- = '\0';
}

static int parse_level_file(const char *path, LevelData *data)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        LOG_ERROR("Failed to open level file: %s", path);
        return 1;
    }

    memset(data, 0, sizeof(LevelData));
    char line[512];

    while (fgets(line, sizeof(line), fp))
    {
        trim_line(line);

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0')
            continue;

        char keyword[32];
        if (sscanf(line, "%31s", keyword) != 1)
            continue;

        if (strcmp(keyword, "mesh") == 0)
        {
            if (sscanf(line, "mesh %255s", data->mesh_path) == 1)
            {
                data->has_mesh = true;
                LOG_INFO("Level mesh: %s", data->mesh_path);
            }
        }
        else if (strcmp(keyword, "entity") == 0)
        {
            if (data->entity_count >= MAX_ENTITY_DEFS)
            {
                LOG_WARN("Max entity definitions reached, skipping");
                continue;
            }

            EntityDef *def = &data->entities[data->entity_count];
            int n = sscanf(line, "entity %31s %f %f %f %f",
                           def->type, &def->x, &def->y, &def->z, &def->rot_y);
            if (n >= 4)
            {
                if (n == 4)
                    def->rot_y = 0.0f;
                data->entity_count++;
                LOG_INFO("Entity def: %s at (%.1f, %.1f, %.1f)",
                         def->type, def->x, def->y, def->z);
            }
        }
        else if (strcmp(keyword, "player_start") == 0)
        {
            // Handle player_start as a special entity without the "entity" prefix
            if (data->entity_count >= MAX_ENTITY_DEFS)
                continue;

            EntityDef *def = &data->entities[data->entity_count];
            strcpy(def->type, "player_start");
            int n = sscanf(line, "player_start %f %f %f %f",
                           &def->x, &def->y, &def->z, &def->rot_y);
            if (n >= 3)
            {
                if (n == 3)
                    def->rot_y = 0.0f;
                data->entity_count++;
                LOG_INFO("Player start: (%.1f, %.1f, %.1f) yaw=%.1f",
                         def->x, def->y, def->z, def->rot_y);
            }
        }
    }

    fclose(fp);
    LOG_INFO("Parsed level: %d entity definitions", data->entity_count);
    return 0;
}

int level_load(const char *path, Scene *scene, Camera *camera,
               OBJMesh *teapot, Mesh *cube_mesh,
               OBJMesh *map_out, CollisionGrid *grid_out,
               ChunkGrid *chunk_grid_out,
               char *map_path_out)
{
    LevelData data;
    if (parse_level_file(path, &data) != 0)
        return 1;

    // If level has a mesh, load it first (this clears scene)
    if (data.has_mesh && map_out)
    {
        if (level_load_map(data.mesh_path, scene, camera, map_out, grid_out, chunk_grid_out) == 0)
        {
            // Track the map path
            if (map_path_out)
            {
                strncpy(map_path_out, data.mesh_path, 255);
                map_path_out[255] = '\0';
            }
        }
        else
        {
            LOG_ERROR("Failed to load level mesh: %s", data.mesh_path);
        }
    }
    else
    {
        // No mesh - just deactivate pickable entities
        for (int i = 0; i < scene->count; i++)
        {
            if (scene->entities[i].pickable)
            {
                scene->entities[i].active = false;
            }
        }
    }

    // Spawn entities from definitions
    for (int i = 0; i < data.entity_count; i++)
    {
        EntityDef *def = &data.entities[i];
        Vec3 pos = {def->x, def->y, def->z};

        if (strcmp(def->type, "player_start") == 0)
        {
            camera->position = pos;
            camera->yaw = def->rot_y * (3.14159265f / 180.0f);
            camera_update_vectors(camera);
            LOG_INFO("Player start at (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
        }
        else if (strcmp(def->type, "teapot") == 0)
        {
            if (teapot)
            {
                int idx = scene_add_obj(scene, teapot, pos, 0.4f);
                if (idx >= 0)
                {
                    scene_set_rotation_speed(scene, idx, (Vec3){0, 0.8f, 0});
                    camera_add_collider(camera, entity_get_world_aabb(&scene->entities[idx]));
                }
            }
        }
        else if (strcmp(def->type, "cube") == 0)
        {
            if (cube_mesh)
            {
                int idx = scene_add_mesh(scene, cube_mesh, pos, 1.0f);
                if (idx >= 0)
                {
                    scene_set_rotation_speed(scene, idx, (Vec3){0, 0.8f, 0});
                    camera_add_collider(camera, entity_get_world_aabb(&scene->entities[idx]));
                }
            }
        }
        else if (strcmp(def->type, "light_point") == 0)
        {
            LOG_INFO("Light point at (%.1f, %.1f, %.1f) - not implemented yet",
                     pos.x, pos.y, pos.z);
        }
        else
        {
            LOG_WARN("Unknown entity type: %s", def->type);
        }
    }

    LOG_INFO("Level loaded: %s", path);
    return 0;
}

int level_save(const char *path, const Scene *scene, const Camera *camera,
               const char *map_path)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
    {
        LOG_ERROR("Failed to open file for writing: %s", path);
        return 1;
    }

    fprintf(fp, "# Level saved by engine\n\n");

    // Save map path if set
    if (map_path && map_path[0] != '\0')
    {
        fprintf(fp, "mesh %s\n\n", map_path);
    }

    // Save camera position and rotation
    if (camera)
    {
        fprintf(fp, "# Camera format: player_start <x> <y> <z> <yaw>\n");
        fprintf(fp, "player_start  %.2f  %.2f  %.2f   %.2f\n\n",
                camera->position.x, camera->position.y, camera->position.z,
                camera->yaw * (180.0f / 3.14159265f));
    }

    fprintf(fp, "# Entity format: entity <type> <x> <y> <z> <rot_y>\n");

    int saved = 0;
    for (int i = 0; i < scene->count; i++)
    {
        const Entity *ent = &scene->entities[i];
        if (!ent->active || !ent->pickable)
            continue;

        const char *type_str = NULL;
        if (ent->type == ENTITY_OBJ_MESH)
        {
            type_str = "teapot";
        }
        else if (ent->type == ENTITY_MESH && ent->render_mode == RENDER_FLAT_SHADED)
        {
            type_str = "cube";
        }

        if (type_str)
        {
            fprintf(fp, "entity %s  %.2f  %.2f  %.2f   %.1f\n",
                    type_str,
                    ent->position.x, ent->position.y, ent->position.z,
                    ent->rotation.y * (180.0f / 3.14159265f));
            saved++;
        }
    }

    fclose(fp);
    LOG_INFO("Level saved: %s (%d entities)", path, saved);
    return 0;
}

int level_load_map(const char *obj_path, Scene *scene, Camera *camera,
                   OBJMesh *map_out, CollisionGrid *grid_out,
                   ChunkGrid *chunk_grid_out)
{
    LOG_INFO("Loading map: %s", obj_path);

    // Clear all entities
    for (int i = 0; i < scene->count; i++)
    {
        scene->entities[i].active = false;
    }
    scene->count = 0;

    // Clear all colliders and map grid
    camera->collider_count = 0;
    camera->map_grid = NULL;
    if (grid_out)
        grid_free(grid_out);
    if (chunk_grid_out)
        chunk_grid_free(chunk_grid_out);

    // Free previous map data (textures, vertices, etc.)
    if (map_out && map_out->vertices)
        obj_mesh_free(map_out);

    // Load the OBJ file
    if (obj_load(map_out, obj_path) != 0)
    {
        LOG_ERROR("Failed to load map: %s", obj_path);
        return 1;
    }

    // Add as a static non-pickable entity at origin
    int idx = scene_add_obj(scene, map_out, (Vec3){0, 0, 0}, 1.0f);
    if (idx >= 0)
    {
        scene->entities[idx].pickable = false;
        scene->entities[idx].rotation_speed = (Vec3){0, 0, 0};
        scene->entities[idx].chunked = true;
        LOG_INFO("Map added as entity %d", idx);
    }

    // Build spatial chunk grid for the map mesh
    if (chunk_grid_out)
    {
        if (chunk_grid_build(chunk_grid_out, map_out, CHUNK_SIZE) == 0)
        {
            LOG_INFO("Chunk grid ready: %d chunks", chunk_grid_out->count);
        }
        else
        {
            LOG_WARN("Chunk grid build failed");
        }
    }

    // Build collision grid
    if (grid_out)
    {
        if (grid_build(grid_out, map_out, GRID_CELL_SIZE) == 0)
        {
            camera->map_grid = grid_out;
            camera->fly_mode = false;
            LOG_INFO("Collision grid ready - fly mode OFF");
        }
        else
        {
            camera->fly_mode = true;
            LOG_WARN("Collision grid failed - fly mode ON");
        }
    }
    else
    {
        camera->fly_mode = true;
        LOG_INFO("No collision grid - fly mode ON");
    }

    // Simple spawn at origin
    camera->position = (Vec3){0, 2, -5};
    camera->yaw = 0;
    camera->pitch = 0;
    camera_update_vectors(camera);

    LOG_INFO("Camera at (%.1f, %.1f, %.1f)",
             camera->position.x, camera->position.y, camera->position.z);
    LOG_INFO("Map bounds: min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)",
             map_out->bounds.min.x, map_out->bounds.min.y, map_out->bounds.min.z,
             map_out->bounds.max.x, map_out->bounds.max.y, map_out->bounds.max.z);

    LOG_INFO("Map loaded: %s (%d verts, %d faces)",
             obj_path, map_out->vertex_count, map_out->face_count);
    return 0;
}
