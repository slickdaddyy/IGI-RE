#pragma GCC optimize("excess-precision=standard")
#include <stddef.h>

#include "hook.h"
#include "igi.h"
#include "animtrig/animtrig.h"
#include "param/paramtypes.h"
#include "config/configvars.h"
#include "lod/lodvars.h"
#include "physobj/physobjvars.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4CEC10, void *, VirModel_Find, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0E50, void, ScriptInit, (const char *path, int a, int b, int c));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int max));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* level wide LOD settings, read from lod.qsc */
typedef struct LODSettings {
    Task task;
    float splineDetail;
    float rigidDetail;
    float boneDetail;
    float splineCull;
    float rigidCull;
    float boneCull;
    float blend;
    float range;
} LODSettings;

/* LOD switch distances of one model */
typedef struct ModelLODSettings {
    Task task;
    char name[16];
    float dist[5];
} ModelLODSettings;

_Static_assert(sizeof(LODSettings) == 0x40, "LODSettings");
_Static_assert(sizeof(ModelLODSettings) == 0x44, "ModelLODSettings");

extern uint16_t gLODSettingsType;
extern uint16_t gModelLODSettingsType;
extern uint16_t gModelLODSettingsContainerType;
extern ParamRange gModelLodDistRange;
extern ParamRange gLodBlendRange;
extern ParamRange gLodCullRange;
extern ParamRange gLodDetailRange;
extern void *gLODSettingsParams;
extern void *gModelLODSettingsParams;

static const float kPercent = 0x1.47ae14p-6f; /* 0.02 */
static const float k4096 = 4096.0f;

void LODSettings_Create(LODSettings *s);
void LODSettings_Changed(LODSettings *s);
void LODSettings_GetParams(LODSettings *s, void **out);
void ModelLODSettings_Create(ModelLODSettings *s);
void ModelLODSettings_Changed(ModelLODSettings *s);
void ModelLODSettings_Delete(ModelLODSettings *s);
void ModelLODSettings_GetParams(ModelLODSettings *s, void **out);
void ModelLODSettingsContainer_Create(Task *t);
void ModelLODSettingsContainer_PreDelete(Task *t);

void LODSettings_Init(void)
{
    LODSettings s;
    ModelLODSettings m;

    gModelLodDistRange.f.min = 0;
    gModelLodDistRange.f.max = 1000.0f;
    gLodDetailRange.f.min = 0;
    gLodDetailRange.f.max = 1500.0f;
    gLodCullRange.f.min = 0;
    gLodCullRange.f.max = 10000.0f;
    gLodBlendRange.f.min = 0;
    gLodBlendRange.f.max = 1.0f;
    gLODSettingsType = TaskType_New("LODSettings", 0x40, Task_GetRootType(), 0);
    gModelLODSettingsType = TaskType_New("ModelLODSettings", 0x44, Task_GetRootType(), 0);
    gModelLODSettingsContainerType = TaskType_New("ModelLODSettingsContainer", 0x20, Task_GetRootType(), 0);
    TaskType_SetHandler(gLODSettingsType, 0, LODSettings_Create);
    TaskType_SetHandler(gLODSettingsType, Param_GetChangedEvent(), LODSettings_Changed);
    TaskType_SetHandler(gLODSettingsType, Param_GetTableEvent(), LODSettings_GetParams);
    TaskType_SetHandler(gModelLODSettingsType, 0, ModelLODSettings_Create);
    TaskType_SetHandler2(gModelLODSettingsType, 2, ModelLODSettings_Delete);
    TaskType_SetHandler(gModelLODSettingsType, Param_GetChangedEvent(), ModelLODSettings_Changed);
    TaskType_SetHandler(gModelLODSettingsType, Param_GetTableEvent(), ModelLODSettings_GetParams);
    TaskType_SetHandler(gModelLODSettingsContainerType, 0, ModelLODSettingsContainer_Create);
    TaskType_SetHandler2(gModelLODSettingsContainerType, 3, ModelLODSettingsContainer_PreDelete);
    gLODSettingsParams = ParamTable_New(8);
    gModelLODSettingsParams = ParamTable_New(6);
    ParamTable_Add(gLODSettingsParams, &s, &s.splineDetail, Param_Real32(), 7, &gLodDetailRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.rigidDetail, Param_Real32(), 7, &gLodDetailRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.boneDetail, Param_Real32(), 7, &gLodDetailRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.splineCull, Param_Real32(), 7, &gLodCullRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.rigidCull, Param_Real32(), 7, &gLodCullRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.boneCull, Param_Real32(), 7, &gLodCullRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.blend, Param_Real32(), 7, &gLodBlendRange);
    ParamTable_Add(gLODSettingsParams, &s, &s.range, Param_Real32(), 7, NULL);
    ParamTable_Add(gModelLODSettingsParams, &m, m.name, Param_String16(), 7, NULL);
    ParamTable_Add(gModelLODSettingsParams, &m, &m.dist[0], Param_Real32(), 7, &gModelLodDistRange);
    ParamTable_Add(gModelLODSettingsParams, &m, &m.dist[1], Param_Real32(), 7, &gModelLodDistRange);
    ParamTable_Add(gModelLODSettingsParams, &m, &m.dist[2], Param_Real32(), 7, &gModelLodDistRange);
    ParamTable_Add(gModelLODSettingsParams, &m, &m.dist[3], Param_Real32(), 7, &gModelLodDistRange);
    ParamTable_Add(gModelLODSettingsParams, &m, &m.dist[4], Param_Real32(), 7, &gModelLodDistRange);
}
HOOK(0x4ED6B0, LODSettings_Init);

void LODSettings_Create(LODSettings *s)
{
    s->splineDetail = 0;
    s->rigidDetail = 0;
    s->boneDetail = 0;
    s->splineCull = 0;
    s->rigidCull = 0;
    s->boneCull = 0;
    s->blend = 1.0f;
    s->range = 4.0f;
}
HOOK(0x4EDA70, LODSettings_Create);

/* percentages to factors, range in units of 4096 */
void LODSettings_Changed(LODSettings *s)
{
    gLodSplineDetail = s->splineDetail * kPercent;
    gLodRigidDetail = s->rigidDetail * kPercent;
    gLodBoneDetail = s->boneDetail * kPercent;
    gLodSplineCull = s->splineCull * kPercent;
    gLodRigidCull = s->rigidCull * kPercent;
    gLodBoneCull = s->boneCull * kPercent;
    gLodBlend = s->blend;
    gLodRange = s->range * k4096;
}
HOOK(0x4EDAA0, LODSettings_Changed);

void LODSettings_GetParams(LODSettings *s, void **out)
{
    *out = gLODSettingsParams;
}
HOOK(0x4EDB20, LODSettings_GetParams);

void ModelLODSettings_Create(ModelLODSettings *s)
{
    s->dist[0] = 0;
    s->dist[1] = 0;
    s->dist[2] = 0;
    s->dist[3] = 0;
    crt_sprintf(s->name, gEmptyString);
}
HOOK(0x4EDB30, ModelLODSettings_Create);

/* makes the distances ascending and hands them to the model */
void ModelLODSettings_Changed(ModelLODSettings *s)
{
    char *model;
    int i;

    for (i = 0; i < 5; i++) {
        if (i && s->dist[i] < s->dist[i - 1])
            s->dist[i] = s->dist[i - 1];
    }
    model = VirModel_Find(s->name);
    if (model) {
        *(int *)(model + 0x24) = 1;
        for (i = 0; i < 5; i++)
            ((float *)(model + 0x28))[i] = s->dist[i] * k4096;
    }
}
HOOK(0x4EDB60, ModelLODSettings_Changed);

void ModelLODSettings_Delete(ModelLODSettings *s)
{
    char *model = VirModel_Find(s->name);

    if (model)
        *(int *)(model + 0x24) = 0;
}
HOOK(0x4EDBD0, ModelLODSettings_Delete);

void ModelLODSettings_GetParams(ModelLODSettings *s, void **out)
{
    *out = gModelLODSettingsParams;
}
HOOK(0x4EDBF0, ModelLODSettings_GetParams);

/* runs LOCAL:lod.qsc, which makes the settings tasks inside this one */
void ModelLODSettingsContainer_Create(Task *t)
{
    void *script;

    gModelLODSettingsContainer = t;
    Script_SetSymbolContext("Task_New", t);
    script = Script_Load("LOCAL:lod.qsc");
    if (script) {
        ScriptInit("LOCAL:lod.qsc", 1, 0, 0);
        Script_Run(script);
        Script_Free(script);
    }
}
HOOK(0x4EDC00, ModelLODSettingsContainer_Create);

void ModelLODSettingsContainer_PreDelete(Task *t)
{
    gModelLODSettingsContainer = NULL;
}
HOOK(0x4EDC50, ModelLODSettingsContainer_PreDelete);

void LODSettings_Shutdown(void)
{
    QTaskList_Free(gModelLODSettingsParams);
    QTaskList_Free(gLODSettingsParams);
    TaskType_Free(gModelLODSettingsContainerType);
    TaskType_Free(gModelLODSettingsType);
    TaskType_Free(gLODSettingsType);
}
HOOK(0x4EDC60, LODSettings_Shutdown);

uint16_t ModelLODSettingsContainer_GetType(void)
{
    return gModelLODSettingsContainerType;
}
HOOK(0x4EDCB0, ModelLODSettingsContainer_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DBDC) uint16_t gLODSettingsType INIT(385);
DATA(54DBDC) uint16_t gModelLODSettingsType INIT(385);
DATA(54DBDC) uint16_t gModelLODSettingsContainerType INIT(385);
PAD(igd, 54DBDC, 54DBE2, 0x2);
BSS(A5EFA8) ParamRange gModelLodDistRange;
BSS(A5EFA8) ParamRange gLodBlendRange;
BSS(A5EFA8) ParamRange gLodCullRange;
BSS(A5EFA8) ParamRange gLodDetailRange;
BSS(A5EFA8) void *gLODSettingsParams;
BSS(A5EFA8) void *gModelLODSettingsParams;
