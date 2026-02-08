#include "graphics/clip.h"

// Interpolate between two clip vertices
static ClipVertex lerp_vertex(ClipVertex *a, ClipVertex *b, float t)
{
    ClipVertex out;

    out.position.x = a->position.x + t * (b->position.x - a->position.x);
    out.position.y = a->position.y + t * (b->position.y - a->position.y);
    out.position.z = a->position.z + t * (b->position.z - a->position.z);
    out.position.w = a->position.w + t * (b->position.w - a->position.w);

    // Interpolate UV coordinates
    out.u = a->u + t * (b->u - a->u);
    out.v = a->v + t * (b->v - a->v);

    // Interpolate color components
    uint8_t ra = (a->color >> 16) & 0xFF;
    uint8_t ga = (a->color >> 8) & 0xFF;
    uint8_t ba = a->color & 0xFF;

    uint8_t rb = (b->color >> 16) & 0xFF;
    uint8_t gb = (b->color >> 8) & 0xFF;
    uint8_t bb = b->color & 0xFF;

    uint8_t r = (uint8_t)(ra + t * (rb - ra));
    uint8_t g = (uint8_t)(ga + t * (gb - ga));
    uint8_t b_out = (uint8_t)(ba + t * (bb - ba));

    out.color = 0xFF000000 | (r << 16) | (g << 8) | b_out;

    return out;
}

typedef enum
{
    PLANE_LEFT,
    PLANE_RIGHT,
    PLANE_BOTTOM,
    PLANE_TOP,
    PLANE_NEAR,
    PLANE_FAR
} FrustumPlane;

// Returns signed distance from vertex to plane (positive = inside)
static float plane_distance(ClipVertex *v, FrustumPlane plane)
{
    float x = v->position.x;
    float y = v->position.y;
    float z = v->position.z;
    float w = v->position.w;

    switch (plane)
    {
    case PLANE_LEFT:
        return x + w; // x >= -w
    case PLANE_RIGHT:
        return w - x; // x <= w
    case PLANE_BOTTOM:
        return y + w; // y >= -w
    case PLANE_TOP:
        return w - y; // y <= w
    case PLANE_NEAR:
        return z; // z >= 0
    case PLANE_FAR:
        return w - z; // z <= w
    }
    return 0;
}

// Clip polygon against a single plane using Sutherland-Hodgman
static void clip_against_plane(ClipPolygon *poly, FrustumPlane plane)
{
    if (poly->count < 3)
        return;

    ClipVertex out[MAX_CLIP_VERTICES];
    int out_count = 0;

    ClipVertex *prev = &poly->vertices[poly->count - 1];
    float prev_dist = plane_distance(prev, plane);

    for (int i = 0; i < poly->count; i++)
    {
        ClipVertex *curr = &poly->vertices[i];
        float curr_dist = plane_distance(curr, plane);

        // Crossing from inside to outside or vice versa
        if ((prev_dist >= 0) != (curr_dist >= 0))
        {
            float t = prev_dist / (prev_dist - curr_dist);
            out[out_count++] = lerp_vertex(prev, curr, t);
        }

        // Current vertex is inside
        if (curr_dist >= 0)
        {
            out[out_count++] = *curr;
        }

        prev = curr;
        prev_dist = curr_dist;
    }

    // Copy result back
    for (int i = 0; i < out_count; i++)
    {
        poly->vertices[i] = out[i];
    }
    poly->count = out_count;
}

// Compute 6-bit outcode for a single clip-space vertex.
// Each bit indicates the vertex is outside one frustum plane.
static inline uint8_t clip_outcode(Vec4 v)
{
    uint8_t code = 0;
    if (v.x < -v.w)
        code |= 0x01; // LEFT
    if (v.x > v.w)
        code |= 0x02; // RIGHT
    if (v.y < -v.w)
        code |= 0x04; // BOTTOM
    if (v.y > v.w)
        code |= 0x08; // TOP
    if (v.z < 0)
        code |= 0x10; // NEAR
    if (v.z > v.w)
        code |= 0x20; // FAR
    return code;
}

ClipResult clip_classify(const ClipPolygon *poly)
{
    uint8_t and_codes = 0xFF; // intersection of outcodes
    uint8_t or_codes = 0x00;  // union of outcodes

    for (int i = 0; i < poly->count; i++)
    {
        uint8_t c = clip_outcode(poly->vertices[i].position);
        and_codes &= c;
        or_codes |= c;
    }

    if (and_codes != 0)
        return CLIP_REJECT; // all verts outside same plane
    if (or_codes == 0)
        return CLIP_ACCEPT; // all verts inside all planes
    return CLIP_NEEDED;
}

int clip_polygon_against_frustum(ClipPolygon *poly)
{
    // Fast path: trivial accept/reject via outcodes
    ClipResult cr = clip_classify(poly);
    if (cr == CLIP_REJECT)
    {
        poly->count = 0;
        return 0;
    }
    if (cr == CLIP_ACCEPT)
        return poly->count;

    clip_against_plane(poly, PLANE_NEAR);
    if (poly->count < 3)
        return 0;

    clip_against_plane(poly, PLANE_FAR);
    if (poly->count < 3)
        return 0;

    clip_against_plane(poly, PLANE_LEFT);
    if (poly->count < 3)
        return 0;

    clip_against_plane(poly, PLANE_RIGHT);
    if (poly->count < 3)
        return 0;

    clip_against_plane(poly, PLANE_BOTTOM);
    if (poly->count < 3)
        return 0;

    clip_against_plane(poly, PLANE_TOP);
    return poly->count;
}
