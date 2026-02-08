#ifndef CLIP_H
#define CLIP_H

#include "math/math.h"
#include <stdint.h>

#define MAX_CLIP_VERTICES 12

typedef struct
{
    Vec4 position;
    float u, v;
    uint32_t color;
} ClipVertex;

typedef struct
{
    ClipVertex vertices[MAX_CLIP_VERTICES];
    int count;
} ClipPolygon;

typedef enum
{
    CLIP_ACCEPT, // All vertices inside frustum – skip clipping entirely
    CLIP_REJECT, // All vertices outside the same plane – discard
    CLIP_NEEDED  // Triangle straddles frustum edge – must clip
} ClipResult;

ClipResult clip_classify(const ClipPolygon *poly);

// Clip polygon against all 6 frustum planes
// Returns the number of resulting vertices (0 if fully clipped)
int clip_polygon_against_frustum(ClipPolygon *poly);

#endif
