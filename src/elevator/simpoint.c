#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "elevator/elevator.h"

FN_EXTERN(VehiclePart_Delete);

#define VEHICLEPART_DELETE (FN(0x426860, VehiclePart_Delete))

ORIG(0x503BF0, uint16_t, SimpointObj_GetType, (void));
ORIG(0x503C00, void *, SimpointObjContainer_New, (int frames, const char *a, const char *b, float c, float d, float e,
                                              float f, Vec3f *g, int h));
ORIG(0x503CC0, Task *, SimpointObjContainer_Add, (void *group, uint16_t type, Vec3d *pos, Matrix *matrix, Vec3f *vel));

/* throws a new model object every few seconds */
typedef struct SimpointObjGenerator {
    RigidDynCubeObj obj;
    int timer;
    char model[16];
    char unk104[16];
    char unk114[16];
    int unk124;
    int interval;
    float unk12C;
    float unk130;
    float unk134;
    void *group;
    Vec3f vel;
    Vec3f unk148;
    int pad154;
} SimpointObjGenerator;

_Static_assert(offsetof(SimpointObjGenerator, group) == 0x138, "SimpointObjGenerator");
_Static_assert(sizeof(SimpointObjGenerator) == 0x158, "SimpointObjGenerator");

extern uint16_t gSimpointObjGeneratorType;
extern void *gSimpointObjGeneratorParams;
extern RangeF gSimpointObjGenerator12CRange;
extern RangeI gSimpointObjGeneratorIntervalRange;
extern RangeI gSimpointObjGenerator124Range;
extern RangeF gSimpointObjGenerator130Range;
extern RangeF gSimpointObjGenerator134Range;

static const float kUnits = 4096.0f;
static const float kSpread = 0x1.64ce9ep+5f;

void SimpointObjGenerator_Create(SimpointObjGenerator *g);
void SimpointObjGenerator_Run(SimpointObjGenerator *g);
void SimpointObjGenerator_GetParams(void *task, void **out);
void SimpointObjGenerator_Changed(SimpointObjGenerator *g);

void SimpointObjGenerator_Init(void)
{
    SimpointObjGenerator g;
    uint16_t t;

    t = TaskType_New("SimpointObjGenerator", sizeof(SimpointObjGenerator), RigidDynCubeObj_GetType(), 0);
    gSimpointObjGeneratorType = t;
    TaskType_SetHandler(t, 0, SimpointObjGenerator_Create);
    TaskType_SetHandler2(gSimpointObjGeneratorType, 1, SimpointObjGenerator_Run);
    TaskType_SetHandler2(gSimpointObjGeneratorType, 2, VEHICLEPART_DELETE);
    TaskType_SetHandler(gSimpointObjGeneratorType, Param_GetTableEvent(), SimpointObjGenerator_GetParams);
    TaskType_SetHandler(gSimpointObjGeneratorType, Param_GetChangedEvent(), SimpointObjGenerator_Changed);
    gSimpointObjGeneratorParams = ParamTable_New(13);
    gSimpointObjGenerator12CRange = (RangeF){0.0f, 1.0f};
    gSimpointObjGenerator130Range = (RangeF){0.0f, 1.0f};
    gSimpointObjGenerator134Range = (RangeF){0.0f, 1000.0f};
    gSimpointObjGenerator124Range = (RangeI){0, 600};
    gSimpointObjGeneratorIntervalRange = (RangeI){0, 300};
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.obj.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.obj.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, g.model, Param_String16(), 7, NULL);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, g.unk104, Param_String16(), 7, NULL);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, g.unk114, Param_String16(), 7, NULL);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.unk124, Param_Int32(), 7, &gSimpointObjGenerator124Range);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.interval, Param_Int32(), 7,
                   &gSimpointObjGeneratorIntervalRange);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.unk12C, Param_Real32(), 7, &gSimpointObjGenerator12CRange);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.unk130, Param_Real32(), 7, &gSimpointObjGenerator130Range);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.unk134, Param_Real32(), 7, &gSimpointObjGenerator134Range);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.vel, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gSimpointObjGeneratorParams, &g, &g.unk148, Param_Real32x3(), 7, NULL);
}
HOOK(0x43B850, SimpointObjGenerator_Init);

void SimpointObjGenerator_Create(SimpointObjGenerator *g)
{
    Vec3d zero = {0, 0, 0};
    float radius;
    int i;

    RigidDynCubeObj_Init(Game_GetWorld(), g, NULL);
    radius = obj_radius(&g->obj);
    DynCube_Add(Game_GetWorld(), g, &zero, radius, 2);
    for (i = 0; i < 9; i++)
        g->obj.matrix.m[i] = i % 4 == 0 ? 1.0f : 0.0f;
    g->obj.matrix.count = 0;
    g->timer = 0;
    g->model[0] = 0;
    g->unk104[0] = 0;
    g->unk114[0] = 0;
    g->unk124 = 30;
    g->interval = 15;
    g->unk12C = 0x1.f5c29p-1f;
    g->unk130 = 1.0f;
    g->unk134 = 10.0f;
    g->group = NULL;
    g->vel.x = 0.0f;
    g->vel.y = 0.0f;
    g->vel.z = 1.0f;
    g->unk148.x = 0.0f;
    g->unk148.y = 0.0f;
    g->unk148.z = 0.0f;
}
HOOK(0x43BB20, SimpointObjGenerator_Create);

void SimpointObjGenerator_Run(SimpointObjGenerator *g)
{
    Vec3f v;

    if (!g->group)
        g->group = SimpointObjContainer_New(g->unk124 * 30, g->unk104, g->unk114, g->unk12C, g->unk130, kSpread, g->unk134,
                                        &g->unk148, 0);
    if (g->timer <= 0) {
        v.x = g->vel.x * kUnits;
        v.y = g->vel.y * kUnits;
        v.z = g->vel.z * kUnits;
        SimpointObjContainer_Add(g->group, SimpointObj_GetType(), &g->obj.pos, &g->obj.matrix, &v);
        g->timer = g->interval * 30;
    }
    g->timer--;
}
HOOK(0x43BCA0, SimpointObjGenerator_Run);

void SimpointObjGenerator_Changed(SimpointObjGenerator *g)
{
    void **model = VirModel_Find(g->model);
    char msg[128];
    char name[128];

    if (!model && g->model[1] != '*') {
        crt_sprintf(msg, "Model \"%s\" not available", g->model);
        strcpy(name, g->model);
        crt_sprintf(g->model, "** %s **", name);
    }
    RigidDynCubeObj_SetModel(g, model);
    g->timer = 0;
}
HOOK(0x43BDA0, SimpointObjGenerator_Changed);

void SimpointObjGenerator_GetParams(void *task, void **out)
{
    *out = gSimpointObjGeneratorParams;
}
HOOK(0x43BE50, SimpointObjGenerator_GetParams);

void SimpointObjGenerator_Close(void)
{
    QTaskList_Free(gSimpointObjGeneratorParams);
    TaskType_Free(gSimpointObjGeneratorType);
}
HOOK(0x43BE60, SimpointObjGenerator_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53C31C) uint16_t gSimpointObjGeneratorType INIT(385);
PAD(igd, 53C31C, 53C31E, 0x2);
BSS(57BF48) RangeF gSimpointObjGenerator12CRange;
BSS(57BF48) RangeI gSimpointObjGeneratorIntervalRange;
BSS(57BF48) RangeI gSimpointObjGenerator124Range;
BSS(57BF48) void *gSimpointObjGeneratorParams;
PAD(igb, 57BF48, 57BF64, 0x4);
BSS(57BF48) RangeF gSimpointObjGenerator130Range;
BSS(57BF48) RangeF gSimpointObjGenerator134Range;
