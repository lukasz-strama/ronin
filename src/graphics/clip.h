#ifndef CLIP_H
#define CLIP_H

#include "math/math.h"
#include <stdint.h>

#define MAX_CLIP_VERTICES 12

typedef struct {
    Vec4     position;  // Clip-space (x, y, z, w)
    float    u, v;      // Texture coordinates
    uint32_t color;
} ClipVertex;

typedef struct {
    ClipVertex vertices[MAX_CLIP_VERTICES];
    int        count;
} ClipPolygon;

// Clip polygon against all 6 frustum planes
// Returns the number of resulting vertices (0 if fully clipped)
int clip_polygon_against_frustum(ClipPolygon *poly);

#endif
