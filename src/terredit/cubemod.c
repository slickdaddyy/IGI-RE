#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "terredit/terredit.h"

/* "CubeModifier": a named editor box snapped to a cell grid of the
   terrain. Every live one is kept in gCubeModifiers. */

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

typedef struct CubeModifier {
    Task task;
    Terrain *world;
    int unk24;
    Vec3d pos;
    Vec3d min;          /* relative to the terrain offset */
    Vec3d max;
    int shift;          /* cell size is 2^31 >> shift */
    char name[256];
    int unk174;
} CubeModifier;

_Static_assert(sizeof(CubeModifier) == 0x178, "CubeModifier");

#define CUBEMOD_MAX 100

#define gCubeModifierParams VAR(void *, 0xA843A4)
#define gCubeModifierType VAR(uint16_t, 0xA843A8)
#define gCubeModifiers ((CubeModifier **)0xA843AC)
#define gScriptEmptyString ((const char *)0x567C74)

static const float kRadiusScale = 4.0f;

void CubeModifier_Create(CubeModifier *cm)
{
    int i = 0;

    cm->pos.x = cm->pos.y = cm->pos.z = 0;
    cm->min.x = cm->min.y = cm->min.z = 0;
    cm->max.x = cm->max.y = cm->max.z = 0;
    cm->world = gCurrentWorld;
    cm->shift = 10;
    crt_sprintf(cm->name, gScriptEmptyString);
    while (gCubeModifiers[i])
        i++;
    gCubeModifiers[i] = cm;
}
HOOK(0x52B320, CubeModifier_Create);

void CubeModifier_Delete(CubeModifier *cm);
void CubeModifier_GetParams(void *task, void **out);
void CubeModifier_Changed(CubeModifier *cm);
void CubeModifier_GetInfo(CubeModifier *cm, ObjInfo *info);

void CubeModifier_Init(void)
{
    CubeModifier t;
    uint16_t type;
    int i;

    for (i = 0; i < CUBEMOD_MAX; i++)
        gCubeModifiers[i] = NULL;
    type = TaskType_New("CubeModifier", sizeof(CubeModifier), EditorRigidObj_GetType(), 0, 0);
    gCubeModifierType = type;
    TaskType_SetHandler(type, 0, CubeModifier_Create);
    TaskType_SetHandler(gCubeModifierType, 7, CubeModifier_GetInfo);
    TaskType_SetHandler2(gCubeModifierType, 2, CubeModifier_Delete);
    TaskType_SetHandler(gCubeModifierType, Param_GetTableEvent(), CubeModifier_GetParams);
    TaskType_SetHandler(gCubeModifierType, Param_GetChangedEvent(), CubeModifier_Changed);
    gCubeModifierParams = ParamTable_New(3);
    ParamTable_Add(gCubeModifierParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gCubeModifierParams, &t, &t.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gCubeModifierParams, &t, &t.name, Param_String256(), 7, NULL);
}
HOOK(0x52B3B0, CubeModifier_Init);

void CubeModifier_Delete(CubeModifier *cm)
{
    int i = 0;

    while (gCubeModifiers[i] != cm)
        i++;
    gCubeModifiers[i] = NULL;
}
HOOK(0x52B4D0, CubeModifier_Delete);

void CubeModifier_Changed(CubeModifier *cm)
{
    int size = (0x40000000 >> (cm->shift & 31)) * 2;
    int mask = -size;
    Terrain *w = cm->world;
    double ox = w->offset[0], oy = w->offset[1], oz = w->offset[2];
    long double px = cm->pos.x + ox;
    double py = cm->pos.y + oy;
    double pz = cm->pos.z + oz;
    double fx = x87_fistp(px) & mask;
    double fy = x87_fistp(py) & mask;
    double fz = x87_fistp(pz) & mask;
    double s = size;

    cm->min.x = fx - ox;
    cm->min.y = fy - oy;
    cm->min.z = fz - oz;
    cm->max.x = (s + fx) - ox;
    cm->max.y = (s + fy) - oy;
    cm->max.z = (s + fz) - oz;
}
HOOK(0x52B510, CubeModifier_Changed);

void CubeModifier_GetParams(void *task, void **out)
{
    *out = gCubeModifierParams;
}
HOOK(0x52B620, CubeModifier_GetParams);

void CubeModifier_GetInfo(CubeModifier *cm, ObjInfo *info)
{
    info->pos = cm->pos;
    obj_info_identity(info);
    info->radius = (0x40000000 >> (cm->shift & 31)) * kRadiusScale;
    info->name = NULL;
}
HOOK(0x52B630, CubeModifier_GetInfo);

void CubeModifier_Shutdown(void)
{
    QTaskList_Free(gCubeModifierParams);
    TaskType_Free(gCubeModifierType);
}
HOOK(0x52B6A0, CubeModifier_Shutdown);
