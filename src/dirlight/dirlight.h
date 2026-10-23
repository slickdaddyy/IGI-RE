#ifndef DIRLIGHT_DIRLIGHT_H
#define DIRLIGHT_DIRLIGHT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskHandler)(void *task, void *arg);

#define gTaskHandlers ((TaskHandler *)0xA96AE0)
#define TASK_HANDLER(event, type) gTaskHandlers[(event) * 0x180 + (type)]

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

/* fixed size slots; the slot number of an item is kept at item + size */
typedef struct Pool {
    char *data;
    int *free;
    int used;
    int count;
    int stride;
    int size;
} Pool;

/* one lightmap texture of a LOD (0x2C bytes) */
typedef struct LightmapTex {
    char texture[0x24];
    void *pixels;
    short width;
    short height;
} LightmapTex;

/* what the builder keeps per lightmap texture (0x28 bytes) */
typedef struct LightmapTexInfo {
    Vec3f *normals;
    Vec3f *positions;
    Vec3f *unk8;
    Vec3f *unkC;
    float *weights;
    uint8_t *unk14;
    uint8_t *unk18;
    int firstTexel;
    int litCount;
    int *texelIds;
} LightmapTexInfo;

/* the lightmaps of one LOD; also the layout of a .olm file */
typedef struct Lightmaps {
    int magic;
    float version;
    int time[7];
    int loaded;
    int built;
    int count;
    int texels;
    int texels2;
    LightmapTexInfo *info;
    LightmapTex tex[];
} Lightmaps;

/* one LOD of an object in a lightmap set (0x24 bytes) */
typedef struct LightmapRef {
    Lightmaps *lightmaps;
    void *lod;
    Task *obj;
    float *matrix;
    Vec3d *pos;
    int unk14;
    int lodIndex;
    int lodCount;
    int uploaded;
} LightmapRef;

typedef struct LightmapSet {
    int count;
    void *model;
    float radius;
    Vec3f center;
    LightmapRef refs[];
} LightmapSet;

/* per vertex weights of one light source */
typedef struct VertexLight {
    float *weights;
    int *verts;
    int count;
} VertexLight;

typedef struct VertexLights {
    int count;
    VertexLight lights[];
} VertexLights;

typedef struct LightmapInfo {
    Task task;
    char unk20[0x48];
    char unk68, unk69, unk6A, unk6B, unk6C, unk6D, unk6E;
    char unk6F, unk70, unk71;
    char unk72[6];
    char name[0x10];
    LightmapSet *set;
    float scale;
    float unk90;
    float unk94;
    float lastScale;
    int unk9C;
    int unkA0;
    int unkA4;
    int texelsA8;
    int texelsAC;
    int vertexLightSize;
    int unkB4;
    int unkB8;
    void *unkBC;
    int unkC0;
    int unkC4;
    VertexLights *vertexLights;
    int unkCC;
    Vec3f unkD0;
    int unkDC;
} LightmapInfo;

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

_Static_assert(sizeof(LightmapTex) == 0x2C, "LightmapTex");
_Static_assert(offsetof(Lightmaps, tex) == 0x3C, "Lightmaps");
_Static_assert(sizeof(LightmapTexInfo) == 0x28, "LightmapTexInfo");
_Static_assert(sizeof(LightmapRef) == 0x24, "LightmapRef");
_Static_assert(sizeof(LightmapSet) == 0x18, "LightmapSet");
_Static_assert(sizeof(LightmapInfo) == 0xE0, "LightmapInfo");
_Static_assert(offsetof(LightmapInfo, name) == 0x78, "LightmapInfo.name");
_Static_assert(offsetof(LightmapInfo, unkD0) == 0xD0, "LightmapInfo.unkD0");
_Static_assert(sizeof(Dirlight) == 0x64, "Dirlight");
_Static_assert(sizeof(DirlightKeyframe) == 0x54, "DirlightKeyframe");

#define gLightSourcePool VAR(Pool, 0xA83CF8)

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
