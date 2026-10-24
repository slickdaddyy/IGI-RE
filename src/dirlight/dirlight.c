/* Dirlight: a directional light whose direction and colours are blended
   between DirlightKeyframe children by time. */
#pragma GCC optimize("excess-precision=standard")
#pragma GCC optimize("rounding-math")

#include <float.h>

#include "hook.h"
#include "dirlight/dirlight.h"

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401BE0, Task *, Task_FindChild, (void *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (void *task, uint16_t type));
ORIG(0x401E70, void, Task_BroadcastType, (void *task, uint8_t slot, uint16_t type, void *arg));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B2760, long double, Math_ACosF, (float x));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3EF0, void, Matrix_SetAxisAngle, (Matrix *m, const Vec3f *axis, float angle));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F1CB0, void *, Param_Range0100, (void));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F17C0, void *, Level_Get, (void));
ORIG(0x4CE3D0, void, Terrain_InvalidateLighting, (void *world));
ORIG(0x4CEA30, void, Terrain_UpdateLighting, (void *world));
ORIG(0x4DAA60, Light *, Light_Add, (const Vec3f *dir, const Vec3f *color, const Vec3f *ambient, char unk24,
                                    char on));
ORIG(0x4DAAF0, void, Light_Remove, (Light *l));
ORIG(0x4E8290, void, Viewport_AddCallback, (void *fn));
ORIG(0x521030, uint16_t, Lensflare_GetType, (void));
ORIG(0x527460, void, DirlightKeyframe_GetParams, (void *kf, void **out));
ORIG(0x527470, void, DirlightKeyframe_Delete, (void *kf));
ORIG(0x5274D0, void, Dirlight_SetTime, (float time));

#define gCurrentWorld VAR(void *, 0xA44344)
#define gDirlightParams VAR(void *, 0xA83D30)
#define gDirlightType VAR(uint16_t, 0xA83D34)
#define gDirlightRandom ((void *)0xA83D38)
#define gDirlightKeyframeTime VAR(float, 0xA83D44)
#define gDirlightKeyframeParams VAR(void *, 0xA83D48)
#define gDirlightKeyframeType VAR(uint16_t, 0xA83D4C)
#define gDirlightKeyframeChanging VAR(DirlightKeyframe *, 0xA83D54)
#define gDirlightPool VAR(Pool, 0xBA1E00)

static const float kTwoPi = 0x1.921fb6p+2f;

#define FIRST_KEY(dl) ((DirlightKeyframe *)Task_FindChild(dl, gDirlightKeyframeType))
#define NEXT_KEY(k) ((DirlightKeyframe *)Task_FindNext(k, gDirlightKeyframeType))

/* blends the keyframes around dl->time into the light */
void Dirlight_Update(Dirlight *dl)
{
    DirlightKeyframe *k, *lo, *hi, *src;

    Terrain_InvalidateLighting(dl->world);
    if (!FIRST_KEY(dl)) {
        dl->color.x = dl->color.y = dl->color.z = 0;
        dl->ambient.x = dl->ambient.y = dl->ambient.z = 0;
        dl->dir.x = 0;
        dl->dir.y = 0;
        dl->dir.z = 1.0f;
        goto set_light;
    }
    if (feq(dl->maxTime, dl->minTime)) {
        src = FIRST_KEY(dl);
        goto copy;
    }
    lo = hi = NULL;
    for (k = FIRST_KEY(dl); k; k = NEXT_KEY(k)) {
        if (feq(k->time, dl->time)) {
            lo = hi = k;
            break;
        }
        if (k->time < dl->time && (!lo || k->time > lo->time))
            lo = k;
        if (k->time > dl->time && (!hi || k->time < hi->time))
            hi = k;
    }
    if (!lo) {
        src = hi;
        goto copy;
    }
    if (hi && lo != hi && !feq(lo->time, hi->time)) {
        float f = (dl->time - lo->time) / (hi->time - lo->time);
        long double angle;
        Vec3f axis, dir;
        Matrix m;

        dl->color.x = (hi->color.x - lo->color.x) * f + lo->color.x;
        dl->color.y = (hi->color.y - lo->color.y) * f + lo->color.y;
        dl->color.z = (hi->color.z - lo->color.z) * f + lo->color.z;
        dl->ambient.x = (hi->ambient.x - lo->ambient.x) * f + lo->ambient.x;
        dl->ambient.y = (hi->ambient.y - lo->ambient.y) * f + lo->ambient.y;
        dl->ambient.z = (hi->ambient.z - lo->ambient.z) * f + lo->ambient.z;
        angle = Math_ACosF(hi->dir.x * lo->dir.x + lo->dir.y * hi->dir.y + lo->dir.z * hi->dir.z);
        axis.x = hi->dir.z * lo->dir.y - lo->dir.z * hi->dir.y;
        axis.y = -(hi->dir.z * lo->dir.x - hi->dir.x * lo->dir.z);
        axis.z = hi->dir.y * lo->dir.x - hi->dir.x * lo->dir.y;
        Matrix_SetAxisAngle(&m, &axis, angle * f);
        dir.x = m.m[2] * lo->dir.z + m.m[1] * lo->dir.y + m.m[0] * lo->dir.x;
        dir.y = m.m[5] * lo->dir.z + m.m[4] * lo->dir.y + m.m[3] * lo->dir.x;
        dir.z = m.m[8] * lo->dir.z + m.m[7] * lo->dir.y + m.m[6] * lo->dir.x;
        dl->dir = dir;
        goto set_light;
    }
    src = lo;
copy:
    dl->color = src->color;
    dl->ambient = src->ambient;
    dl->dir = src->dir;
set_light:
    if (!dl->light) {
        dl->light = Light_Add(&dl->dir, &dl->color, &dl->ambient, dl->unk4C, dl->on);
    } else {
        dl->light->dir = dl->dir;
        dl->light->color = dl->color;
        dl->light->ambient = dl->ambient;
        dl->light->unk24 = dl->unk4C;
        dl->light->on = dl->on;
    }
    Terrain_UpdateLighting(dl->world);
}
HOOK(0x5269C0, Dirlight_Update);

void Dirlight_Create(Dirlight *dl);
void Dirlight_Delete(Dirlight *dl);
void Dirlight_GetParams(Dirlight *dl, void **out);
void Dirlight_Changed(Dirlight *dl);
void DirlightKeyframe_Create(DirlightKeyframe *k);
void DirlightKeyframe_Changed(DirlightKeyframe *k);

void Dirlight_Init(void)
{
    DirlightKeyframe k;
    Dirlight d;
    void *range;

    gDirlightType = TaskType_New("Dirlight", sizeof(Dirlight), Task_GetRootType(), 0, 0);
    gDirlightKeyframeType = TaskType_New("DirlightKeyframe", sizeof(DirlightKeyframe), Task_GetRootType(), 0, 0);
    TaskType_SetHandler(gDirlightType, 0, Dirlight_Create);
    TaskType_SetHandler2(gDirlightType, 1, Task_Nothing);
    TaskType_SetHandler2(gDirlightType, 2, Dirlight_Delete);
    TaskType_SetHandler(gDirlightType, Param_GetTableEvent(), Dirlight_GetParams);
    TaskType_SetHandler(gDirlightType, Param_GetChangedEvent(), Dirlight_Changed);
    TaskType_SetHandler(gDirlightKeyframeType, 0, DirlightKeyframe_Create);
    TaskType_SetHandler2(gDirlightKeyframeType, 2, DirlightKeyframe_Delete);
    TaskType_SetHandler(gDirlightKeyframeType, Param_GetTableEvent(), DirlightKeyframe_GetParams);
    TaskType_SetHandler(gDirlightKeyframeType, Param_GetChangedEvent(), DirlightKeyframe_Changed);
    gDirlightParams = ParamTable_New(4);
    gDirlightKeyframeParams = ParamTable_New(6);
    ParamTable_Add(gDirlightParams, &d, &d.unk4C, Param_Bool8(), 7, NULL);
    ParamTable_Add(gDirlightParams, &d, &d.on, Param_Bool8(), 7, NULL);
    range = Param_Range0100();
    ParamTable_Add(gDirlightParams, &d, &d.unk50, Param_Real32(), 7, range);
    ParamTable_Add(gDirlightKeyframeParams, &k, &k.angleY, Param_Angle(), 7, NULL);
    ParamTable_Add(gDirlightKeyframeParams, &k, &k.angleZ, Param_Angle(), 7, NULL);
    ParamTable_Add(gDirlightKeyframeParams, &k, &k.color, Param_RGB(), 7, NULL);
    ParamTable_Add(gDirlightKeyframeParams, &k, &k.ambient, Param_RGB(), 7, NULL);
    ParamTable_Add(gDirlightKeyframeParams, &k, &k.editTime, Param_Real32(), 7, NULL);
    Pool_Init(&gDirlightPool, 4, 3, 4);
    Random_Init(gDirlightRandom);
    Viewport_AddCallback(Task_Nothing);
}
HOOK(0x526EE0, Dirlight_Init);

void Dirlight_Create(Dirlight *dl)
{
    Pool *pool = &gDirlightPool;
    void **item;

    dl->world = gCurrentWorld;
    dl->dir.x = 0;
    dl->dir.y = 0;
    dl->color.x = dl->color.y = dl->color.z = 0;
    dl->ambient.x = dl->ambient.y = dl->ambient.z = 0;
    dl->light = NULL;
    dl->dir.z = 1.0f;
    dl->unk4C = 1;
    dl->on = 1;
    item = (void **)(pool->data + pool->free[pool->used] * pool->stride);
    *(int *)((char *)item + pool->size) = pool->used;
    pool->used++;
    dl->slot = item;
    *item = dl;
    LightSource_Add(dl);
}
HOOK(0x527160, Dirlight_Create);

void Dirlight_Delete(Dirlight *dl)
{
    Pool *pool = &gDirlightPool;
    int idx, used, last, tmp;
    char *end;

    if (dl->light)
        Light_Remove(dl->light);
    idx = *(int *)((char *)dl->slot + pool->size);
    used = --pool->used;
    if (idx != used) {
        end = pool->data + pool->free[used] * pool->stride;
        tmp = pool->free[idx];
        last = *(int *)(end + pool->size);
        pool->free[idx] = pool->free[last];
        pool->free[last] = tmp;
        *(int *)(end + pool->size) = idx;
    }
    LightSource_Remove(dl);
}
HOOK(0x5271E0, Dirlight_Delete);

void Dirlight_GetParams(Dirlight *dl, void **out)
{
    *out = gDirlightParams;
}
HOOK(0x527260, Dirlight_GetParams);

void Dirlight_Changed(Dirlight *dl)
{
    DirlightKeyframe *k;

    dl->time = -FLT_MAX;
    dl->minTime = FLT_MAX;
    dl->maxTime = -FLT_MAX;
    for (k = FIRST_KEY(dl); k; k = NEXT_KEY(k)) {
        if (k->time > dl->maxTime)
            dl->maxTime = k->time;
        if (!(k->time >= dl->minTime))
            dl->minTime = k->time;
    }
    Dirlight_SetTime(0);
}
HOOK(0x527270, Dirlight_Changed);

void DirlightKeyframe_Create(DirlightKeyframe *k)
{
    k->editTime = 0;
    k->time = 0;
    k->color.x = k->color.y = k->color.z = 1.0f;
    k->ambient.x = k->ambient.y = k->ambient.z = 0;
    k->dir.x = 0;
    k->dir.y = 0;
    k->dir.z = 1.0f;
}
HOOK(0x5272F0, DirlightKeyframe_Create);

void DirlightKeyframe_Changed(DirlightKeyframe *k)
{
    Task *parent;
    Matrix m;
    float x, y, z;
    uint16_t type;
    uint8_t ev;

    if (!gDirlightKeyframeChanging)
        gDirlightKeyframeChanging = k;
    k->dir.x = 1.0f;
    k->dir.y = 0;
    k->dir.z = 0;
    Matrix_SetEuler(&m, 0, kTwoPi - k->angleY, kTwoPi - k->angleZ);
    x = m.m[2] * k->dir.z + m.m[1] * k->dir.y + m.m[0] * k->dir.x;
    y = m.m[5] * k->dir.z + m.m[4] * k->dir.y + m.m[3] * k->dir.x;
    z = m.m[8] * k->dir.z + m.m[7] * k->dir.y + m.m[6] * k->dir.x;
    k->dir.x = x;
    k->dir.y = y;
    k->dir.z = z;
    k->time = k->editTime;
    gDirlightKeyframeTime = k->editTime;
    parent = k->task.parent;
    if (parent && parent->type == gDirlightType)
        TASK_HANDLER(Param_GetChangedEvent(), parent->type)(parent, k);
    type = Lensflare_GetType();
    ev = Param_GetChangedEvent();
    Task_BroadcastType(Level_Get(), ev, type, NULL);
    if (gDirlightKeyframeChanging == k)
        gDirlightKeyframeChanging = NULL;
}
HOOK(0x527320, DirlightKeyframe_Changed);
