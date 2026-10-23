#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "terredit/terredit.h"

/* "EditHazinglayer": the editor box that owns a haze layer */

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D9FC0, void *, HazeLayer_New, (const Vec3d *min, const Vec3d *max, const float *color, float density,
                                       double bottom, double top));
ORIG(0x4DA0A0, void, HazeLayer_Set, (void *layer, const Vec3d *min, const Vec3d *max, const float *color,
                                     float density, double bottom, double top));
ORIG(0x4DA160, void, HazeLayer_Delete, (void *layer));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

#define EditHazeLayer_Create ((void *)0x52AFE0)

void DiscardTerrain_MaskVisible(void *task, int *out);

typedef struct EditHazeLayer {
    Task task;
    void *layer;
    int unk24;
    Vec3d pos;
    Vec3d min;
    Vec3d max;
    int shift;          /* cell size is 2^31 >> shift */
    float density;
    double bottom;      /* above pos.z */
    double top;
} EditHazeLayer;

_Static_assert(sizeof(EditHazeLayer) == 0x88, "EditHazeLayer");

#define gEditHazeLayerType VAR(uint16_t, 0xA84398)
#define gEditHazeLayerParams VAR(void *, 0xA8439C)

void EditHazeLayer_Delete(EditHazeLayer *e);
void EditHazeLayer_Changed(EditHazeLayer *e);
void EditHazeLayer_GetParams(void *task, void **out);

void EditHazeLayer_Init(void)
{
    EditHazeLayer t;
    uint16_t type;

    type = TaskType_New("EditHazinglayer", sizeof(EditHazeLayer), EditorRigidObj_GetType(), 0, 0);
    gEditHazeLayerType = type;
    TaskType_SetHandler(type, 0, EditHazeLayer_Create);
    TaskType_SetHandler(gEditHazeLayerType, 7, NULL);
    TaskType_SetHandler2(gEditHazeLayerType, 2, EditHazeLayer_Delete);
    TaskType_SetHandler(gEditHazeLayerType, Param_GetTableEvent(), EditHazeLayer_GetParams);
    TaskType_SetHandler(gEditHazeLayerType, Param_GetChangedEvent(), EditHazeLayer_Changed);
    TaskType_SetHandler(gEditHazeLayerType, DynCubeObj_GetMaskVisibleEvent(), DiscardTerrain_MaskVisible);
    gEditHazeLayerParams = ParamTable_New(5);
    ParamTable_Add(gEditHazeLayerParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gEditHazeLayerParams, &t, &t.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gEditHazeLayerParams, &t, &t.density, Param_Real32(), 7, NULL);
    ParamTable_Add(gEditHazeLayerParams, &t, &t.bottom, Param_Real64(), 7, NULL);
    ParamTable_Add(gEditHazeLayerParams, &t, &t.top, Param_Real64(), 7, NULL);
}
HOOK(0x52B040, EditHazeLayer_Init);

void EditHazeLayer_Delete(EditHazeLayer *e)
{
    HazeLayer_Delete(e->layer);
}
HOOK(0x52B1C0, EditHazeLayer_Delete);

void EditHazeLayer_Changed(EditHazeLayer *e)
{
    int size = (0x40000000 >> (e->shift & 31)) * 2;
    int mask = -size;
    double s = size;
    double top, bottom;

    e->min.x = x87_fistp(e->pos.x) & mask;
    e->min.y = x87_fistp(e->pos.y) & mask;
    e->min.z = x87_fistp(e->pos.z) & mask;
    e->max.x = s + e->min.x;
    e->max.y = s + e->min.y;
    e->max.z = s + e->min.z;
    top = e->top + e->pos.z;
    bottom = e->bottom + e->pos.z;
    if (!e->layer) {
        e->layer = HazeLayer_New(&e->min, &e->max, NULL, e->density, bottom, top);
        return;
    }
    HazeLayer_Set(e->layer, &e->min, &e->max, NULL, e->density, bottom, top);
}
HOOK(0x52B1D0, EditHazeLayer_Changed);

void EditHazeLayer_GetParams(void *task, void **out)
{
    *out = gEditHazeLayerParams;
}
HOOK(0x52B2F0, EditHazeLayer_GetParams);

void EditHazeLayer_Shutdown(void)
{
    QTaskList_Free(gEditHazeLayerParams);
    TaskType_Free(gEditHazeLayerType);
}
HOOK(0x52B300, EditHazeLayer_Shutdown);
