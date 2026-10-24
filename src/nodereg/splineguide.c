/* SplinePathGuideQTask: moves a target object along a SplinePath at the
   speeds set on the path nodes. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "nodereg/splinepath.h"

/* "SplinePathGuideQTask" (0x44) */
typedef struct SplinePathGuide {
    Task task;
    int pathId;
    int targetId;
    int segment;
    unsigned flags;     /* 1: moving */
    int loop;
    int active;
    float u;            /* on the segment */
    float speed;
    float start;        /* where to start, 0 to 1 of the whole path */
} SplinePathGuide;

/* the parts of the moved object used here */
typedef struct GuideTarget {
    Task task;
    Vec3d pos;
    char unk38[0xC];
    uint32_t radius;
    char unk48[0x28];
    Matrix matrix;
} GuideTarget;

_Static_assert(sizeof(SplinePathGuide) == 0x44, "SplinePathGuide");
_Static_assert(offsetof(GuideTarget, matrix) == 0x70, "GuideTarget");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, uint32_t radius));
ORIG(0x4F0F50, int, Param_Real32, (void));
ORIG(0x51A4A0, void, sub_51A4A0, (SplinePath *p, float *out, int i, float u));
ORIG(0x4F0FA0, int, Param_Int32, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));

extern uint16_t gSplinePathGuideType;
extern uint8_t gGuideMoveEvent;
extern uint8_t gGuideStartEvent;
extern void *gSplinePathGuideParams;
extern float gFrameRate;
extern int gUnkA8375C;
extern void *gSplinePathGuides;

static const double kSecondsPerHour = 3600.0;
static const double kSpeedScale = 4096000.0;
static const float kZeroF = 0.0f;
static const float kOneF = 1.0f;

void SplinePathGuide_Create(SplinePathGuide *g)
{
    g->pathId = -1;
    g->targetId = -1;
    g->speed = 1;
    g->segment = 0;
    g->flags = 0;
    g->u = 0;
    g->start = 0;
    g->loop = 0;
    g->active = 0;
    QTaskList_Add(gSplinePathGuides, g);
}
HOOK(0x51AE50, SplinePathGuide_Create);

void SplinePathGuide_Delete(SplinePathGuide *g)
{
    QTaskList_Remove(gSplinePathGuides, g);
}
HOOK(0x51AE90, SplinePathGuide_Delete);

void SplinePathGuide_Run(SplinePathGuide *g)
{
    SplinePath *p;
    GuideTarget *t;
    SplinePathNode *a, *b;
    long double v;
    float k, u;
    Vec3d pos, tmp;
    Vec3f delta;
    TaskFn fn;

    if (!(g->flags & 1) || g->pathId < 0 || !(p = (SplinePath *)Task_FindById(g->pathId)))
        return;
    if (g->targetId < 0 || !(t = (GuideTarget *)Task_FindById(g->targetId)))
        return;
    a = p->nodes[g->segment];
    if (!a || !(b = p->nodes[g->segment + 1]) || p->count < 3)
        return;
    u = g->u;
    k = kSpeedScale / (gFrameRate * kSecondsPerHour) * ((b->speed - a->speed) * u + a->speed) * g->speed;
    v = SplinePathNode_GetStepScale(a, u) * k + g->u;
    g->u = v;
    if (v > kOneF) {
        int i = g->segment + 1;

        v = v - kOneF;
        g->u = v;
        g->segment = i;
        g->u = v * p->nodes[i - 1]->length / p->nodes[i]->length;
        if (i >= p->count - 1) {
            if (!g->loop) {
                g->u = 1;
                g->segment = p->count - 2;
                g->flags &= ~1;
            } else {
                g->u = 0;
                g->segment = 0;
            }
        }
    }
    u = !(g->u >= kOneF) ? g->u : kOneF;
    g->u = u;
    sub_51A4A0(p, t->matrix.m, g->segment, u);
    pos = *SplinePath_GetPos(&tmp, p, g->segment, g->u);
    delta.x = pos.x - t->pos.x;
    delta.y = pos.y - t->pos.y;
    delta.z = pos.z - t->pos.z;
    DynCube_Move(gCurrentWorld, t, &pos, t->radius);
    fn = gTaskHandlers[gGuideMoveEvent][t->task.type];
    if (fn)
        fn(t, &delta);
}
HOOK(0x51AEB0, SplinePathGuide_Run);

/* puts the guide at fraction start of the whole path length */
void SplinePathGuide_SetStart(SplinePathGuide *g)
{
    SplinePath *p = (SplinePath *)Task_FindById(g->pathId);
    long double d;
    int n, i;

    if (!p || p->count < 3)
        return;
    n = p->count - 1;
    d = kZeroF;
    for (i = 0; i < n; i++)
        d += p->nodes[i]->length;
    d *= g->start;
    for (i = 0; i < n; i++) {
        SplinePathNode *a = p->nodes[i];

        if (!(d > a->length)) {
            g->segment = i;
            g->u = d / a->length;
            return;
        }
        d -= a->length;
    }
}
HOOK(0x51B0B0, SplinePathGuide_SetStart);

void SplinePathGuide_ParamsChanged(SplinePathGuide *g);
void SplinePathGuide_GetParamTable(SplinePathGuide *g, ParamTable **out);

void SplinePathGuide_Open(void)
{
    SplinePathGuide p;

    gSplinePathGuideType = TaskType_New("SplinePathGuideQTask", sizeof(SplinePathGuide), Task_GetRootType(), 0, 0);
    gGuideMoveEvent = TaskEvent_Alloc(0);
    gGuideStartEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler2(gSplinePathGuideType, 1, SplinePathGuide_Run);
    TaskType_SetHandler(gSplinePathGuideType, 0, SplinePathGuide_Create);
    TaskType_SetHandler(gSplinePathGuideType, Param_GetTableEvent(), SplinePathGuide_GetParamTable);
    TaskType_SetHandler(gSplinePathGuideType, Param_GetChangedEvent(), SplinePathGuide_ParamsChanged);
    TaskType_SetHandler2(gSplinePathGuideType, 2, SplinePathGuide_Delete);
    gSplinePathGuideParams = ParamTable_New(5);
    ParamTable_Add(gSplinePathGuideParams, &p, &p.pathId, Param_Int32(), 7, NULL);
    ParamTable_Add(gSplinePathGuideParams, &p, &p.targetId, Param_Int32(), 7, NULL);
    ParamTable_Add(gSplinePathGuideParams, &p, &p.loop, Param_Int32(), 7, NULL);
    ParamTable_Add(gSplinePathGuideParams, &p, &p.active, Param_Int32(), 7, NULL);
    ParamTable_Add(gSplinePathGuideParams, &p, &p.start, Param_Real32(), 7, NULL);
    gSplinePathGuides = QTaskList_New(100);
}
HOOK(0x51B140, SplinePathGuide_Open);

void SplinePathGuide_ParamsChanged(SplinePathGuide *g)
{
    float v;
    Task *t;

    gUnkA8375C = 0;
    if (!(g->start >= kZeroF))
        v = kZeroF;
    else if (g->start > kOneF)
        v = kOneF;
    else
        v = g->start;
    g->start = v;
    if (v > kZeroF)
        SplinePathGuide_SetStart(g);
    if (g->active)
        g->flags |= 1;
    else
        g->flags &= ~1;
    g->speed = g->speed > kZeroF ? g->speed : kZeroF;
    if (g->targetId > 0 && (t = Task_FindById(g->targetId))) {
        TaskFn fn = gTaskHandlers[gGuideStartEvent][t->type];

        if (fn)
            fn(t, NULL);
    }
}
HOOK(0x51B2C0, SplinePathGuide_ParamsChanged);

void SplinePathGuide_GetParamTable(SplinePathGuide *g, ParamTable **out)
{
    *out = gSplinePathGuideParams;
}
HOOK(0x51B390, SplinePathGuide_GetParamTable);

void SplinePathGuide_Close(void)
{
    QTaskList_Free(gSplinePathGuides);
    QTaskList_Free(gSplinePathGuideParams);
    TaskEvent_Free(gGuideMoveEvent);
    gGuideMoveEvent = 0xFF;
    TaskEvent_Free(gGuideStartEvent);
    gGuideStartEvent = 0xFF;
    TaskType_Free(gSplinePathGuideType);
    gSplinePathGuideType = TASK_NO_TYPE;
}
HOOK(0x51B3A0, SplinePathGuide_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54ED50) uint16_t gSplinePathGuideType INIT(385);
DATA(54ED50) uint8_t gGuideMoveEvent INIT(255);
DATA(54ED50) uint8_t gGuideStartEvent INIT(255);
BSS(A83754) void *gSplinePathGuideParams;
BSS(A83754) float gFrameRate;
BSS(A83754) int gUnkA8375C;
BSS(A83754) void *gSplinePathGuides;
