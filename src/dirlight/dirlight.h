#ifndef DIRLIGHT_DIRLIGHT_H
#define DIRLIGHT_DIRLIGHT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "litmap/litmaptypes.h"
#include "dirlight/dirlighttypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

extern TaskHandler gTaskHandlers[];
#define TASK_HANDLER(event, type) gTaskHandlers[(event) * 0x180 + (type)]

extern Pool gLightSourcePool;

/* x87 compares that treat unordered as equal, like fcomp + test ah,0x40 */
static inline int feq(float a, float b)
{
    return !__builtin_islessgreater(a, b);
}

/* what the lightmap builder hands each light source's trace handler */
typedef struct TraceArg {
    VertexLights *lights;
    int *index;
    int *ids;
    float **areas;
    float *weights;
    float **normals;
    LightmapInfo *info;
} TraceArg;

void Dirlight_TraceVertices(Dirlight *dl, TraceArg *arg);
void LightmapTex_Alloc(LightmapTex *t);
void Line2_Intersect(float *out, const float *a0, const float *a1, const float *b0, const float *b1);
Lightmaps *Lightmap_BuildLod(void *lod, int lodIndex, float scale, const char *name, int index, int full, int flags,
                             int unk);
void LightSource_Add(void *light);
void LightSource_Remove(void *light);

#endif
