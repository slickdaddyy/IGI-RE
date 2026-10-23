/* GlobalLight: the level's ambient light and fog for the terrain and four
   object categories, blended over time between GlobalLightKeyframe
   children. */
#pragma GCC optimize("excess-precision=standard")

#include <float.h>

#include "hook.h"
#include "animcam/animcam.h"

typedef struct LightColor {
    Vec3f ambient;
    Vec3f fog;
    float fogDensity;
} LightColor;

typedef struct LightSet {
    LightColor color;
    Vec3f matrix[3];    /* colour matrix, identity by default */
} LightSet;

/* "GlobalLight" (0x1B8 bytes) */
typedef struct GlobalLight {
    Task task;
    void *world;        /* 0x20 */
    Vec3f unk24[3];     /* 0x24 */
    float unk48;        /* 0x48 */
    LightSet sets[5];   /* 0x4C, terrain then object categories 1-4 */
    float minTime;      /* 0x18C */
    float maxTime;      /* 0x190 */
    float time;         /* 0x194 */
    float texParams[7]; /* 0x198 */
    char resetTex;      /* 0x1B4 */
    char applyTex;      /* 0x1B5 */
    char unk1B6[2];
} GlobalLight;

typedef struct KeySet {
    LightSet set;
    int link;           /* copy the colours of this set, -1 none */
} KeySet;

/* "GlobalLightKeyframe" (0x1A4 bytes) */
typedef struct GlobalLightKeyframe {
    Task task;
    Vec3f unk20[3];     /* 0x20 */
    float timeParam;    /* 0x44 */
    float time;         /* 0x48 */
    KeySet sets[5];     /* 0x4C */
    char linkAll;       /* 0x1A0 */
    char unk1A1[3];
} GlobalLightKeyframe;

_Static_assert(offsetof(GlobalLight, sets) == 0x4C, "sets");
_Static_assert(offsetof(GlobalLight, texParams) == 0x198, "texParams");
_Static_assert(sizeof(GlobalLight) == 0x1B8, "GlobalLight");
_Static_assert(offsetof(GlobalLightKeyframe, sets) == 0x4C, "sets");
_Static_assert(sizeof(GlobalLightKeyframe) == 0x1A4, "GlobalLightKeyframe");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401BE0, void *, Task_FindChild, (void *task, uint16_t type));
ORIG(0x401C40, void *, Task_FindNext, (void *task, uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B26C0, void, Texture_ReloadAll, (void));
ORIG(0x4B2720, void, sub_4B2720, (const float *p));
ORIG(0x4B2740, void, sub_4B2740, (float *p));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CE3D0, void, sub_4CE3D0, (void *world));
ORIG(0x4CEA30, void, sub_4CEA30, (void *world));
ORIG(0x4E8290, void, Viewport_AddCallback, (void *fn));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x50FD20, void, TerrainMesh_SetFog, (Vec3f *color, float density));
ORIG(0x50FE40, void, TerrainMesh_SetAmbient, (Vec3f *color));
ORIG(0x5196D0, void, sub_5196D0, (Vec3f *colors));
ORIG(0x52D400, void, sub_52D400, (Vec3f *colors));

#define gGlobalLightType VAR(uint16_t, 0xA8408C)
#define gGlobalLightParams VAR(void *, 0xA84088)
#define gKeyframeType VAR(uint16_t, 0xA84374)
#define gKeyframeParams VAR(void *, 0xA83F44)
#define gKeyframeTemplate ((GlobalLightKeyframe *)0xA84130)
#define gCurrentGlobalLight VAR(GlobalLight *, 0xA8437C)
#define gChangedKeyframe VAR(GlobalLightKeyframe *, 0xA84380)
#define gKeyframeTime VAR(float, 0xA83EA0)
#define gTexGammaRange ((void *)0x534CA0)

/* parameter names, one per light set */
#define gAmbientNames ((char (*)[32])0xA83E00)
#define gFogNames ((char (*)[32])0xA842D4)
#define gFogDensityNames ((char (*)[32])0xA83FE8)
#define gScaleRNames ((char (*)[32])0xA83EA4)
#define gScaleGNames ((char (*)[32])0xA83F48)
#define gScaleBNames ((char (*)[32])0xA84090)
#define gLinkNames ((char (*)[32])0xA83D60)

/* what the renderer uses, one per light set */
#define gAmbientColors ((Vec3f *)0xB818A0)
#define gFogColors ((Vec3f *)0xB818E0)
#define gFogDensities ((float *)0xB46D00)
#define gColorMatrix0 ((Vec3f *)0xB816C0)
#define gColorMatrix1 ((Vec3f *)0xB81720)
#define gColorMatrix2 ((Vec3f *)0xB81760)

static const float kFogScale = 0x1.0624dep-11f;

void GlobalLight_SetTime(float time);

/* hands the current colours to the terrain and the renderer */
static void publish(GlobalLight *g, int matrix)
{
    int i;

    TerrainMesh_SetAmbient(&g->sets[0].color.ambient);
    TerrainMesh_SetFog(&g->sets[0].color.fog, g->sets[0].color.fogDensity * kFogScale);
    for (i = 0; i < 5; i++) {
        LightSet *s = &g->sets[i];

        gAmbientColors[i] = s->color.ambient;
        gFogColors[i] = s->color.fog;
        gFogDensities[i] = s->color.fogDensity * kFogScale;
        if (matrix) {
            gColorMatrix0[i] = s->matrix[0];
            gColorMatrix1[i] = s->matrix[1];
            gColorMatrix2[i] = s->matrix[2];
        }
    }
    sub_52D400(g->unk24);
    sub_5196D0(g->unk24);
    sub_4CEA30(g->world);
}

static void copy_key(GlobalLight *g, const GlobalLightKeyframe *k, int matrix)
{
    int i;

    for (i = 0; i < 5; i++) {
        if (matrix)
            g->sets[i] = k->sets[i].set;
        else
            g->sets[i].color = k->sets[i].set.color;
    }
    g->unk24[0] = k->unk20[0];
    g->unk24[1] = k->unk20[1];
    g->unk24[2] = k->unk20[2];
}

static float lerp(float a, float b, long double t)
{
    return ((long double)b - a) * t + a;
}

/* only a time before the first keyframe copies the colour matrices */
void GlobalLight_Apply(GlobalLight *g)
{
    GlobalLightKeyframe *k, *lo, *hi;
    long double t;
    int i, j;

    sub_4CE3D0(g->world);
    if (!Task_FindChild(g, gKeyframeType)) {
        for (i = 0; i < 5; i++)
            memset(&g->sets[i].color, 0, sizeof(LightColor));
        memset(g->unk24, 0, sizeof(g->unk24));
        publish(g, 0);
        return;
    }
    if (!(g->maxTime < g->minTime || g->maxTime > g->minTime)) {
        copy_key(g, Task_FindChild(g, gKeyframeType), 0);
        publish(g, 0);
        return;
    }
    lo = hi = NULL;
    for (k = Task_FindChild(g, gKeyframeType); k; k = Task_FindNext(k, gKeyframeType)) {
        if (!(k->time < g->time || k->time > g->time)) {
            lo = hi = k;
            break;
        }
        if (!(k->time >= g->time) && (!lo || k->time > lo->time))
            lo = k;
        if (k->time > g->time && (!hi || !(k->time >= hi->time)))
            hi = k;
    }
    if (!lo) {
        copy_key(g, hi, 1);
        publish(g, 1);
        return;
    }
    if (!hi || lo == hi || !(lo->time < hi->time || lo->time > hi->time)) {
        copy_key(g, lo, 0);
        publish(g, 0);
        return;
    }
    t = ((long double)g->time - lo->time) / ((long double)hi->time - lo->time);
    for (i = 0; i < 5; i++) {
        const LightColor *a = &lo->sets[i].set.color, *b = &hi->sets[i].set.color;
        LightColor *c = &g->sets[i].color;

        c->ambient.x = lerp(a->ambient.x, b->ambient.x, t);
        c->ambient.y = lerp(a->ambient.y, b->ambient.y, t);
        c->ambient.z = lerp(a->ambient.z, b->ambient.z, t);
        c->fog.x = lerp(a->fog.x, b->fog.x, t);
        c->fog.y = lerp(a->fog.y, b->fog.y, t);
        c->fog.z = lerp(a->fog.z, b->fog.z, t);
        c->fogDensity = lerp(a->fogDensity, b->fogDensity, t);
    }
    for (j = 0; j < 3; j++) {
        g->unk24[j].x = lerp(lo->unk20[j].x, hi->unk20[j].x, t);
        g->unk24[j].y = lerp(lo->unk20[j].y, hi->unk20[j].y, t);
        g->unk24[j].z = lerp(lo->unk20[j].z, hi->unk20[j].z, t);
    }
    publish(g, 0);
}
HOOK(0x527520, GlobalLight_Apply);

void GlobalLight_Create(GlobalLight *g);
void GlobalLight_Delete(GlobalLight *g);
void GlobalLight_GetParams(void *task, void **out);
void GlobalLight_ParamsChanged(GlobalLight *g);
void GlobalLightKeyframe_Create(GlobalLightKeyframe *k);
void GlobalLightKeyframe_ParamsChanged(GlobalLightKeyframe *k);
void GlobalLightKeyframe_GetParams(void *task, void **out);
void GlobalLightKeyframe_Delete(GlobalLightKeyframe *k);

void GlobalLight_Open(void)
{
    GlobalLight g;
    GlobalLightKeyframe *t = gKeyframeTemplate;
    char set[32];
    int i;

    gGlobalLightType = TaskType_New("GlobalLight", sizeof(GlobalLight), Task_GetRootType(), 0, 0);
    gKeyframeType = TaskType_New("GlobalLightKeyframe", sizeof(GlobalLightKeyframe), Task_GetRootType(), 0, 0);
    TaskType_SetHandler(gGlobalLightType, 0, GlobalLight_Create);
    TaskType_SetHandler2(gGlobalLightType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gGlobalLightType, 2, GlobalLight_Delete);
    TaskType_SetHandler(gGlobalLightType, Param_GetTableEvent(), GlobalLight_GetParams);
    TaskType_SetHandler(gGlobalLightType, Param_GetChangedEvent(), GlobalLight_ParamsChanged);
    TaskType_SetHandler(gKeyframeType, 0, GlobalLightKeyframe_Create);
    TaskType_SetHandler2(gKeyframeType, 2, GlobalLightKeyframe_Delete);
    TaskType_SetHandler(gKeyframeType, Param_GetTableEvent(), GlobalLightKeyframe_GetParams);
    TaskType_SetHandler(gKeyframeType, Param_GetChangedEvent(), GlobalLightKeyframe_ParamsChanged);
    gGlobalLightParams = ParamTable_New(6);
    ParamTable_Add(gGlobalLightParams, &g, &g.unk48, Param_Real32(), 7, NULL);
    ParamTable_Add(gGlobalLightParams, &g, &g.texParams[0], Param_RGB(), 7, NULL);
    ParamTable_Add(gGlobalLightParams, &g, &g.texParams[3], Param_RGB(), 7, NULL);
    ParamTable_Add(gGlobalLightParams, &g, &g.texParams[6], Param_Real32(), 7, gTexGammaRange);
    ParamTable_Add(gGlobalLightParams, &g, &g.applyTex, Param_PushButton(), 6, NULL);
    ParamTable_Add(gGlobalLightParams, &g, &g.resetTex, Param_PushButton(), 6, NULL);
    gKeyframeParams = ParamTable_New(40);
    ParamTable_Add(gKeyframeParams, t, &t->linkAll, Param_PushButton(), 7, NULL);
    for (i = 0; i < 5; i++) {
        if (i == 0)
            strcpy(set, "terrain");
        else
            crt_sprintf(set, "object category %d", i);
        crt_sprintf(gAmbientNames[i], "Ambient color %s", set);
        crt_sprintf(gFogNames[i], "Fog color %s", set);
        crt_sprintf(gFogDensityNames[i], "Fog density %s", set);
        crt_sprintf(gScaleRNames[i], "Scale R %s", set);
        crt_sprintf(gScaleGNames[i], "Scale G %s", set);
        crt_sprintf(gScaleBNames[i], "Scale B %s", set);
        crt_sprintf(gLinkNames[i], "Link setting %s", set);
        ParamTable_Add(gKeyframeParams, t, &t->sets[i].set.color.ambient, Param_RGB(), 7, NULL);
        ParamTable_Add(gKeyframeParams, t, &t->sets[i].set.color.fog, Param_RGB(), 7, NULL);
        ParamTable_Add(gKeyframeParams, t, &t->sets[i].set.color.fogDensity, Param_Real32(), 7, NULL);
        ParamTable_Add(gKeyframeParams, t, &t->sets[i].link, Param_Int32(), 7, NULL);
    }
    ParamTable_Add(gKeyframeParams, t, &t->unk20[0], Param_RGB(), 7, NULL);
    ParamTable_Add(gKeyframeParams, t, &t->unk20[1], Param_RGB(), 7, NULL);
    ParamTable_Add(gKeyframeParams, t, &t->unk20[2], Param_RGB(), 7, NULL);
    ParamTable_Add(gKeyframeParams, t, &t->timeParam, Param_Real32(), 7, NULL);
    Viewport_AddCallback(TASK_NOTHING);
}
HOOK(0x527C90, GlobalLight_Open);

void GlobalLight_Create(GlobalLight *g)
{
    int i;

    g->world = gCurrentWorld;
    for (i = 0; i < 5; i++) {
        LightSet *s = &g->sets[i];

        s->color.ambient.x = s->color.ambient.y = s->color.ambient.z = 0.0f;
        s->color.fog.x = s->color.fog.y = s->color.fog.z = 1.0f;
        s->color.fogDensity = 0.0f;
        s->matrix[0].x = 1.0f;
        s->matrix[0].y = s->matrix[0].z = 0.0f;
        s->matrix[1].x = 0.0f;
        s->matrix[1].y = 1.0f;
        s->matrix[1].z = 0.0f;
        s->matrix[2].x = s->matrix[2].y = 0.0f;
        s->matrix[2].z = 1.0f;
    }
    sub_4B2740(g->texParams);
    g->applyTex = 1;
    g->resetTex = 0;
    gCurrentGlobalLight = g;
}
HOOK(0x5280E0, GlobalLight_Create);

void GlobalLight_Delete(GlobalLight *g)
{
    gCurrentGlobalLight = NULL;
}
HOOK(0x528160, GlobalLight_Delete);

void GlobalLight_GetParams(void *task, void **out)
{
    *out = gGlobalLightParams;
}
HOOK(0x528170, GlobalLight_GetParams);

void GlobalLight_ParamsChanged(GlobalLight *g)
{
    GlobalLightKeyframe *k;

    if (g->resetTex) {
        sub_4B2740(g->texParams);
        g->applyTex = 1;
        g->resetTex = 0;
    }
    if (g->applyTex) {
        sub_4B2720(g->texParams);
        Texture_ReloadAll();
        g->applyTex = 0;
    }
    g->minTime = FLT_MAX;
    g->time = -FLT_MAX;
    g->maxTime = -FLT_MAX;
    for (k = Task_FindChild(g, gKeyframeType); k; k = Task_FindNext(k, gKeyframeType)) {
        if (k->time > g->maxTime)
            fcopy(&g->maxTime, &k->time);
        if (!(k->time >= g->minTime))
            fcopy(&g->minTime, &k->time);
    }
    GlobalLight_SetTime(0.0f);
}
HOOK(0x528180, GlobalLight_ParamsChanged);

void GlobalLightKeyframe_Create(GlobalLightKeyframe *k)
{
    int i;

    for (i = 0; i < 5; i++) {
        LightColor *c = &k->sets[i].set.color;

        c->ambient.x = c->ambient.y = c->ambient.z = 0.0f;
        c->fog.x = c->fog.y = c->fog.z = 1.0f;
        c->fogDensity = 0.0f;
        k->sets[i].link = -1;
    }
    k->unk20[1].x = k->unk20[1].y = k->unk20[1].z = 0.0f;
    k->time = 0.0f;
    k->timeParam = 0.0f;
    k->linkAll = 0;
    k->unk20[0].x = k->unk20[0].y = k->unk20[0].z = 1.0f;
    k->unk20[2].x = k->unk20[2].y = k->unk20[2].z = 1.0f;
}
HOOK(0x528260, GlobalLightKeyframe_Create);

void GlobalLightKeyframe_ParamsChanged(GlobalLightKeyframe *k)
{
    Task *p;
    long double time;
    int i, n;

    if (k->linkAll) {
        if (k->sets[1].link) {
            for (i = 0; i < 5; i++)
                k->sets[i].link = i - 1;
        } else {
            for (i = 0; i < 5; i++)
                k->sets[i].link = -1;
        }
        k->linkAll = 0;
    }
    if (!gChangedKeyframe)
        gChangedKeyframe = k;
    time = k->timeParam;
    k->time = time;
    gKeyframeTime = time;
    if (k->task.parent && k->task.parent->type == gGlobalLightType) {
        p = k->task.parent;
        HANDLER(Param_GetChangedEvent(), p->type)(p, k);
    }
    /* repeated so that chains of links resolve */
    for (n = 5; n; n--) {
        for (i = 0; i < 5; i++) {
            int l = k->sets[i].link;

            if (l >= 0 && l < 5)
                k->sets[i].set.color = k->sets[l].set.color;
        }
    }
    if (gChangedKeyframe == k)
        gChangedKeyframe = NULL;
}
HOOK(0x5282C0, GlobalLightKeyframe_ParamsChanged);

void GlobalLightKeyframe_GetParams(void *task, void **out)
{
    *out = gKeyframeParams;
}
HOOK(0x5283F0, GlobalLightKeyframe_GetParams);

void GlobalLightKeyframe_Delete(GlobalLightKeyframe *k)
{
    ((GlobalLight *)k->task.parent)->time = -FLT_MAX;
}
HOOK(0x528400, GlobalLightKeyframe_Delete);

void GlobalLight_Close(void)
{
    QTaskList_Free(gKeyframeParams);
    QTaskList_Free(gGlobalLightParams);
    TaskType_Free(gKeyframeType);
    TaskType_Free(gGlobalLightType);
}
HOOK(0x528420, GlobalLight_Close);

void GlobalLight_SetTime(float time)
{
    GlobalLight *g = gCurrentGlobalLight;

    if (g && (time < g->time || time > g->time)) {
        g->time = time;
        GlobalLight_Apply(gCurrentGlobalLight);
    }
}
HOOK(0x528460, GlobalLight_SetTime);
