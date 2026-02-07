#include "graphics/clip.h"

// Interpolate between two clip vertices
static ClipVertex lerp_vertex(ClipVertex *a, ClipVertex *b, float t) {
    ClipVertex out;

    out.position.x = a->position.x + t * (b->position.x - a->position.x);
    out.position.y = a->position.y + t * (b->position.y - a->position.y);
    out.position.z = a->position.z + t * (b->position.z - a->position.z);
    out.position.w = a->position.w + t * (b->position.w - a->position.w);

    // Interpolate color components
    uint8_t ra = (a->color >> 16) & 0xFF;
    uint8_t ga = (a->color >> 8)  & 0xFF;
    uint8_t ba =  a->color        & 0xFF;

    uint8_t rb = (b->color >> 16) & 0xFF;
    uint8_t gb = (b->color >> 8)  & 0xFF;
    uint8_t bb =  b->color        & 0xFF;

    uint8_t r = (uint8_t)(ra + t * (rb - ra));
    uint8_t g = (uint8_t)(ga + t * (gb - ga));
    uint8_t b_out = (uint8_t)(ba + t * (bb - ba));

    out.color = 0xFF000000 | (r << 16) | (g << 8) | b_out;

    return out;
}

typedef enum {
    PLANE_LEFT,
    PLANE_RIGHT,
    PLANE_BOTTOM,
    PLANE_TOP,
    PLANE_NEAR,
    PLANE_FAR
} FrustumPlane;

// Returns signed distance from vertex to plane (positive = inside)
static float plane_distance(ClipVertex *v, FrustumPlane plane) {
    float x = v->position.x;
    float y = v->position.y;
    float z = v->position.z;
    float w = v->position.w;

    switch (plane) {
        case PLANE_LEFT:   return x + w;   // x >= -w
        case PLANE_RIGHT:  return w - x;   // x <= w
        case PLANE_BOTTOM: return y + w;   // y >= -w
        case PLANE_TOP:    return w - y;   // y <= w
        case PLANE_NEAR:   return z;       // z >= 0
        case PLANE_FAR:    return w - z;   // z <= w
    }
    return 0;
}

// Clip polygon against a single plane using Sutherland-Hodgman
static void clip_against_plane(ClipPolygon *poly, FrustumPlane plane) {
    if (poly->count < 3) return;

    ClipVertex out[MAX_CLIP_VERTICES];
    int out_count = 0;

    ClipVertex *prev = &poly->vertices[poly->count - 1];
    float prev_dist = plane_distance(prev, plane);

    for (int i = 0; i < poly->count; i++) {
        ClipVertex *curr = &poly->vertices[i];
        float curr_dist = plane_distance(curr, plane);

        // Crossing from inside to outside or vice versa
        if ((prev_dist >= 0) != (curr_dist >= 0)) {
            float t = prev_dist / (prev_dist - curr_dist);
            out[out_count++] = lerp_vertex(prev, curr, t);
        }

        // Current vertex is inside
        if (curr_dist >= 0) {
            out[out_count++] = *curr;
        }

        prev = curr;
        prev_dist = curr_dist;
    }

    // Copy result back
    for (int i = 0; i < out_count; i++) {
        poly->vertices[i] = out[i];
    }
    poly->count = out_count;
}

int clip_polygon_against_frustum(ClipPolygon *poly) {
    clip_against_plane(poly, PLANE_NEAR);
    if (poly->count < 3) return 0;

    clip_against_plane(poly, PLANE_FAR);
    if (poly->count < 3) return 0;

    clip_against_plane(poly, PLANE_LEFT);
    if (poly->count < 3) return 0;

    clip_against_plane(poly, PLANE_RIGHT);
    if (poly->count < 3) return 0;

    clip_against_plane(poly, PLANE_BOTTOM);
    if (poly->count < 3) return 0;

    clip_against_plane(poly, PLANE_TOP);
    return poly->count;
}
