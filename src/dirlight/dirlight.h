#ifndef DIRLIGHT_DIRLIGHT_H
#define DIRLIGHT_DIRLIGHT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "litmap/litmaptypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

extern TaskHandler gTaskHandlers[];
#define TASK_HANDLER(event, type) gTaskHandlers[(event) * 0x180 + (type)]

/* a directional light from the level's light sources (0x28 bytes) */
typedef struct Light {
    Vec3f dir;
    Vec3f color;
    Vec3f ambient;
    char unk24;
    char on;
} Light;

typedef struct Dirlight {
    Task task;
    void *world;
    Vec3f dir;
    Vec3f color;
    Vec3f ambient;
    Light *light;
    char unk4C;
    char on;
    char unk4E[2];
    float unk50;
    float minTime;
    float maxTime;
    float time;
    void **slot;
} Dirlight;

typedef struct DirlightKeyframe {
    Task task;
    float angleY;
    float angleZ;
    Vec3f dir;
    Vec3f color;
    Vec3f ambient;
    float editTime;
    float time;
} DirlightKeyframe;

_Static_assert(sizeof(Dirlight) == 0x64, "Dirlight");
_Static_assert(sizeof(DirlightKeyframe) == 0x54, "DirlightKeyframe");

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
