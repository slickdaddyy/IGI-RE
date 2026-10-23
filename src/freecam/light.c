#include "freecam/freecam.h"
#include "freecam/light.h"

/* Point and spot lights placed in the world as DynCube objects. */

typedef struct SpotLightObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    float color[3];
    float range;
    float dir[3];
    float cone;
    char unk88[8];
} SpotLightObj;

/* filled by the slot 7 handler */
typedef struct LightInfo {
    Vec3d pos;
    float unk18[13];
    float range;
    char *name;
} LightInfo;

_Static_assert(sizeof(PointLightObj) == 0x80, "PointLightObj");
_Static_assert(sizeof(SpotLightObj) == 0x90, "SpotLightObj");
_Static_assert(offsetof(LightInfo, name) == 0x50, "LightInfo");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, int align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, const Vec3d *pos, float radius));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4DA8D0, void, Light_PushPoint, (const Vec3d *pos, const float *color, float range));
ORIG(0x4DA950, void, Light_PushSpot, (const Vec3d *pos, const float *color, float range, const float *dir,
                                      float cone));
ORIG(0x4DAA00, void, Light_Pop, (void));

#define gPointLightType VAR(uint16_t, 0x54F800)
#define gSpotLightType VAR(uint16_t, 0x54F802)
#define gPointLightPool VAR(Pool, 0xAFA800)
#define gSpotLightPool VAR(Pool, 0xAFA820)

void PointLightObj_Push(PointLightObj *l)
{
    Light_PushPoint(&l->pos, l->color, l->range);
}
HOOK(0x531200, PointLightObj_Push);

void SpotLightObj_Push(SpotLightObj *l)
{
    Light_PushSpot(&l->pos, l->color, l->range, l->dir, l->cone);
}
HOOK(0x531220, SpotLightObj_Push);

/* takes the light's item out of the pool, then deletes the task */
void PointLightObj_Delete(void *world, PointLightObj *l)
{
    Pool *p = &gPointLightPool;
    int slot, last, keep;
    char *item;

    DynCube_Remove(world, l);
    slot = *(int *)((char *)l->poolItem + p->index_offset);
    if (slot != --p->used) {
        item = p->data + p->slots[p->used] * p->stride;
        keep = p->slots[slot];
        last = *(int *)(item + p->index_offset);
        p->slots[slot] = p->slots[last];
        p->slots[last] = keep;
        *(int *)(item + p->index_offset) = slot;
    }
    Task_Delete(l);
}
HOOK(0x531250, PointLightObj_Delete);

void PointLightObj_SetRange(void *world, PointLightObj *l, const Vec3d *pos, float range)
{
    l->range = range;
    DynCube_Move(world, l, pos, range);
}
HOOK(0x5312D0, PointLightObj_SetRange);

void PointLightObj_GetInfo(PointLightObj *l, LightInfo *out);
void SpotLightObj_GetInfo(SpotLightObj *l, LightInfo *out);
void LightObj_Pop(void);

void LightObj_Init(void)
{
    gPointLightType = TaskType_New("PointlightDynCubeObj", sizeof(PointLightObj), gDynCubeObjType, 0, 0);
    gSpotLightType = TaskType_New("SpotlightDynCubeObj", sizeof(SpotLightObj), gDynCubeObjType, 0, 0);
    TaskType_SetHandler(gPointLightType, 4, PointLightObj_Push);
    TaskType_SetHandler(gPointLightType, 5, LightObj_Pop);
    TaskType_SetHandler(gPointLightType, 7, PointLightObj_GetInfo);
    TaskType_SetHandler(gSpotLightType, 4, SpotLightObj_Push);
    TaskType_SetHandler(gSpotLightType, 5, LightObj_Pop);
    TaskType_SetHandler(gSpotLightType, 7, SpotLightObj_GetInfo);
    Pool_Init(&gPointLightPool, 4, 10, 4);
    Pool_Init(&gSpotLightPool, 4, 10, 4);
}
HOOK(0x531300, LightObj_Init);

static void light_info(const Vec3d *pos, const float *range, LightInfo *out)
{
    static const float m[13] = { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 };

    memcpy(&out->pos, pos, sizeof(Vec3d));
    memcpy(out->unk18, m, sizeof(m));
    memcpy(&out->range, range, 4);
}

void PointLightObj_GetInfo(PointLightObj *l, LightInfo *out)
{
    light_info(&l->pos, &l->range, out);
    if (out->name)
        memcpy(out->name, "Pointlight", 11);
}
HOOK(0x5313F0, PointLightObj_GetInfo);

void LightObj_Pop(void)
{
    Light_Pop();
}
HOOK(0x531470, LightObj_Pop);

void SpotLightObj_GetInfo(SpotLightObj *l, LightInfo *out)
{
    light_info(&l->pos, &l->range, out);
    if (out->name)
        memcpy(out->name, "Spotlight", 10);
}
HOOK(0x531480, SpotLightObj_GetInfo);

void LightObj_Shutdown(void)
{
    Pool_Free(&gPointLightPool);
    Pool_Free(&gSpotLightPool);
    TaskType_Free(gPointLightType);
    gPointLightType = 0x181;
    TaskType_Free(gSpotLightType);
    gSpotLightType = 0x181;
}
HOOK(0x5314F0, LightObj_Shutdown);
