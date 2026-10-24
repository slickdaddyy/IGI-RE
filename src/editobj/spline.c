/* SplineObj: a chain of SplineObjWaypoint tasks with spline segments between them. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "editobj/editobj.h"

FN_EXTERN(ConditionalContainer_ForwardEvent4);
FN_EXTERN(SplineObjWaypoint_Changed);
FN_EXTERN(SplineObjWaypoint_Create);
FN_EXTERN(SplineObjWaypoint_Delete);
FN_EXTERN(Task_Nothing);

/* "SplineObj" (0x68) */
typedef struct SplineObj {
    Task task;
    int unk20;
    int maxSegments;
    float length;
    Vec3f pos;          /* moves all waypoints when changed */
    Vec3f oldPos;
    float angle;        /* turns all waypoints around the first one */
    float oldAngle;
    char unk4C;
    char collide;
    char unk4E;
    char unk4F;
    Vec3f color1;
    Vec3f color2;
} SplineObj;

typedef struct SplineObjSegment {
    char unk0[0xF8];
    float length;
    float start;
    float end;
} SplineObjSegment;

/* "SplineObjWaypoint" (0x1A8), a RigidDynCubeObj */
typedef struct SplineObjWaypoint {
    Obj obj;
    SplineObjSegment *seg;
    int unkF4;
    Vec3d point;
    Matrix rot;
    Vec3d attachPos;    /* point in the frame of the attached object */
    Matrix attachRot;
    char modelName[16];
    char segModel[16];
    int segments;
    int unk19C;
    char unk1A0;
    char straight;
    char unk1A2;
    char unk1A3;
    char reversed;
    char autoOrient;
    char unk1A6[2];
} SplineObjWaypoint;

_Static_assert(sizeof(SplineObj) == 0x68, "SplineObj");
_Static_assert(offsetof(SplineObjWaypoint, attachRot) == 0x150, "attachRot");
_Static_assert(sizeof(SplineObjWaypoint) == 0x1A8, "SplineObjWaypoint");

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401BE0, Task *, Task_FindChild, (void *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (void *task, uint16_t type));
ORIG(0x401CC0, Task *, Task_FindAncestor, (void *task, uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F11F0, uint8_t, Level_GetEventDA4, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define CONDITIONALCONTAINER_FORWARDEVENT4 (FN(0x4879E0, ConditionalContainer_ForwardEvent4))
#define SPLINEOBJWAYPOINT_CREATE (FN(0x5008F0, SplineObjWaypoint_Create))
#define SPLINEOBJWAYPOINT_DELETE (FN(0x500DE0, SplineObjWaypoint_Delete))
#define SPLINEOBJWAYPOINT_CHANGED (FN(0x500EF0, SplineObjWaypoint_Changed))

#define gSplineObjDebugText ((char *)0xA7A50C)
#define gSplineObjWaypointParams VAR(void *, 0xA7A508)
#define gSplineObjSegmentRange VAR(RangeI, 0xA7A610)
#define gSplineObjType VAR(uint16_t, 0xA7A618)
#define gSplineObjParams VAR(void *, 0xA7A61C)
#define gSplineObjWaypointType VAR(uint16_t, 0xA7A620)

static const float kZero = 0.0f;
static const float kFar = 4096.0f;
static const float kMeter = 1.0f / 4096.0f;
static const float kPi = 0x1.921fb6p+1f;

/* Stores the waypoint's place relative to the RigidDynCubeObj it hangs under */
void SplineObjWaypoint_UpdateAttach(SplineObjWaypoint *w)
{
    Obj *a = (Obj *)Task_FindAncestor(w->obj.task.parent, RigidDynCubeObj_GetType());
    const float *m, *t, *r;
    long double dz;
    Vec3d p;
    Matrix n;

    if (!a) {
        w->attachPos.x = 0.0;
        w->attachPos.y = 0.0;
        w->attachPos.z = 0.0;
        w->attachRot = (Matrix){{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
        return;
    }
    m = a->matrix.m;
    w->attachPos.x = w->point.x - a->pos.x;
    w->attachPos.y = w->point.y - a->pos.y;
    dz = w->point.z - a->pos.z;
    w->attachPos.z = dz;
    p.x = m[6] * dz + m[0] * w->attachPos.x + m[3] * w->attachPos.y;
    p.y = m[1] * w->attachPos.x + m[4] * w->attachPos.y + m[7] * dz;
    p.z = m[2] * w->attachPos.x + m[5] * w->attachPos.y + m[8] * dz;
    w->attachPos = p;
    w->attachRot = (Matrix){{m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]}, a->matrix.unk24};
    t = w->attachRot.m;
    r = w->rot.m;
    n.m[0] = t[6] * r[2] + r[1] * t[3] + r[0] * t[0];
    n.m[1] = r[1] * t[4] + t[1] * r[0] + t[7] * r[2];
    n.unk24 = w->attachRot.unk24 + w->rot.unk24 + 1;
    n.m[2] = t[2] * r[0] + t[8] * r[2] + r[1] * t[5];
    n.m[3] = t[6] * r[5] + t[0] * r[3] + t[3] * r[4];
    n.m[4] = t[7] * r[5] + t[4] * r[4] + t[1] * r[3];
    n.m[5] = t[8] * r[5] + t[5] * r[4] + t[2] * r[3];
    n.m[6] = r[7] * t[3] + t[6] * r[8] + t[0] * r[6];
    n.m[7] = r[8] * t[7] + r[7] * t[4] + t[1] * r[6];
    n.m[8] = t[2] * r[6] + r[8] * t[8] + r[7] * t[5];
    w->attachRot = n;
}
HOOK(0x501D70, SplineObjWaypoint_UpdateAttach);

/* Sum of the segment lengths of the SplineObj with this id */
float SplineObj_GetLength(int id)
{
    Task *o = Task_FindById(id);
    SplineObjWaypoint *w;
    float sum = 0.0f;

    if (!o)
        return kZero;
    for (w = (SplineObjWaypoint *)Task_FindChild(o, gSplineObjWaypointType); w;
         w = (SplineObjWaypoint *)Task_FindNext(w, gSplineObjWaypointType))
        if (w->seg)
            sum = sum + w->seg->length;
    return sum;
}
HOOK(0x5020C0, SplineObj_GetLength);

int SplineObj_GetWaypointCount(int id)
{
    Task *o = Task_FindById(id);
    Task *w;
    int n = 0;

    if (!o)
        return 0;
    for (w = Task_FindChild(o, gSplineObjWaypointType); w; w = Task_FindNext(w, gSplineObjWaypointType))
        n++;
    return n;
}
HOOK(0x502130, SplineObj_GetWaypointCount);

/* Length of the spline up to waypoint index */
float SplineObj_GetLengthTo(int id, int index)
{
    Task *o = Task_FindById(id);
    SplineObjWaypoint *w;
    float sum = 0.0f;
    int i = 0;

    if (!o)
        return kZero;
    for (w = (SplineObjWaypoint *)Task_FindChild(o, gSplineObjWaypointType); w && i != index + 1;
         w = (SplineObjWaypoint *)Task_FindNext(w, gSplineObjWaypointType), i++)
        if (w->seg)
            sum = sum + w->seg->length;
    return sum;
}
HOOK(0x502180, SplineObj_GetLengthTo);

void SplineObj_GetWaypointPos(Vec3d *out, int id, int index)
{
    SplineObjWaypoint *w = (SplineObjWaypoint *)Task_FindChild(Task_FindById(id), gSplineObjWaypointType);

    for (; index; index--)
        w = (SplineObjWaypoint *)Task_FindNext(w, gSplineObjWaypointType);
    *out = w->point;
}
HOOK(0x502200, SplineObj_GetWaypointPos);

void SplineObj_Create(SplineObj *o);
void SplineObj_Changed(SplineObj *o, char *arg);
void SplineObj_GetParams(void *task, void **out);
void SplineObj_GetInfo(SplineObj *o, ObjInfo *out);
void SplineObjWaypoint_GetParams(void *task, void **out);
void SplineObjWaypoint_GetInfo(SplineObjWaypoint *w, ObjInfo *out);
void SplineObjWaypoint_GetDebugText(SplineObjWaypoint *w, char **out);
void SplineObjWaypoint_CollLines(SplineObjWaypoint *w, void *arg);
void SplineObjWaypoint_CollLine(SplineObjWaypoint *w, void *arg);
void SplineObjWaypoint_Slot10(SplineObjWaypoint *w, void *arg);
void SplineObjWaypoint_Slot11(SplineObjWaypoint *w, void *arg);

void SplineObj_Open(void)
{
    SplineObj o;
    SplineObjWaypoint w;

    gSplineObjType = TaskType_New("SplineObj", sizeof(SplineObj), Task_GetRootType(), 0, 0);
    gSplineObjWaypointType = TaskType_New("SplineObjWaypoint", sizeof(SplineObjWaypoint), RigidDynCubeObj_GetType(),
                                          0, 0);
    TaskType_SetHandler(gSplineObjType, 0, SplineObj_Create);
    TaskType_SetHandler2(gSplineObjType, 2, TASK_NOTHING);
    TaskType_SetHandler(gSplineObjType, Param_GetChangedEvent(), SplineObj_Changed);
    TaskType_SetHandler(gSplineObjType, Param_GetTableEvent(), SplineObj_GetParams);
    TaskType_SetHandler(gSplineObjType, 7, SplineObj_GetInfo);
    TaskType_SetHandler(gSplineObjType, RigidDynCubeObj_GetEvent4(), CONDITIONALCONTAINER_FORWARDEVENT4);
    TaskType_SetHandler(gSplineObjWaypointType, 0, SPLINEOBJWAYPOINT_CREATE);
    TaskType_SetHandler2(gSplineObjWaypointType, 2, SPLINEOBJWAYPOINT_DELETE);
    TaskType_SetHandler(gSplineObjWaypointType, Param_GetChangedEvent(), SPLINEOBJWAYPOINT_CHANGED);
    TaskType_SetHandler(gSplineObjWaypointType, Param_GetTableEvent(), SplineObjWaypoint_GetParams);
    TaskType_SetHandler(gSplineObjWaypointType, 7, SplineObjWaypoint_GetInfo);
    TaskType_SetHandler(gSplineObjWaypointType, Level_GetEventDA4(), SplineObjWaypoint_GetDebugText);
    TaskType_SetHandler(gSplineObjWaypointType, RigidDynCubeObj_GetEvent4(), TASK_NOTHING);
    TaskType_SetHandler(gSplineObjWaypointType, 9, SplineObjWaypoint_CollLines);
    TaskType_SetHandler(gSplineObjWaypointType, 8, SplineObjWaypoint_CollLine);
    TaskType_SetHandler(gSplineObjWaypointType, 10, SplineObjWaypoint_Slot10);
    TaskType_SetHandler(gSplineObjWaypointType, 11, SplineObjWaypoint_Slot11);
    gSplineObjSegmentRange.min = 2;
    gSplineObjSegmentRange.max = 20;
    gSplineObjParams = ParamTable_New(10);
    ParamTable_Add(gSplineObjParams, &o, &o.unk4E, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.collide, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.unk4C, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.unk4F, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.maxSegments, Param_Int32(), 7, &gSplineObjSegmentRange);
    ParamTable_Add(gSplineObjParams, &o, &o.unk20, Param_Int32(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.pos, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.angle, Param_Angle(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.color1, Param_RGB(), 7, NULL);
    ParamTable_Add(gSplineObjParams, &o, &o.color2, Param_RGB(), 7, NULL);
    gSplineObjWaypointParams = ParamTable_New(8);
    ParamTable_Add(gSplineObjWaypointParams, &w, &w.rot, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gSplineObjWaypointParams, &w, &w.point, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSplineObjWaypointParams, &w, w.modelName, Param_String16(), 7, NULL);
    ParamTable_Add(gSplineObjWaypointParams, &w, w.segModel, Param_String16(), 7, NULL);
    ParamTable_Add(gSplineObjWaypointParams, &w, &w.segments, Param_Int32(), 7, &gSplineObjSegmentRange);
    ParamTable_Add(gSplineObjWaypointParams, &w, &w.straight, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSplineObjWaypointParams, &w, &w.reversed, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSplineObjWaypointParams, &w, &w.autoOrient, Param_Bool8(), 7, NULL);
}
HOOK(0x502250, SplineObj_Open);

void SplineObj_Create(SplineObj *o)
{
    o->angle = kPi;
    o->oldAngle = kPi;
    o->unk20 = 0;
    o->unk4E = 0;
    o->collide = 0;
    o->unk4C = 0;
    o->maxSegments = 20;
    o->length = 0.0f;
    o->pos = (Vec3f){0.0f, 0.0f, 0.0f};
    o->oldPos = (Vec3f){0.0f, 0.0f, 0.0f};
    o->color1 = (Vec3f){1.0f, 1.0f, 1.0f};
    o->color2 = (Vec3f){0.0f, 0.0f, 0.0f};
}
HOOK(0x5026E0, SplineObj_Create);

/* Moves and turns the waypoints by the change of pos / angle, then passes
   the event on to the children */
void SplineObj_Changed(SplineObj *o, char *arg)
{
    SplineObjWaypoint *first, *w;
    float angle;

    if (arg && *arg) {
        o->oldPos = o->pos;
        o->oldAngle = o->angle;
    }
    angle = o->angle;
    if (__builtin_islessgreater(angle, o->oldAngle)) {
        first = (SplineObjWaypoint *)Task_FindChild(o, gSplineObjWaypointType);
        if (first) {
            Matrix m;
            Vec3d *c;

            Matrix_SetRotZ(&m, o->angle - o->oldAngle);
            c = &first->point;
            w = first;
            do {
                const float *r = w->rot.m;
                long double dx = w->point.x - c->x;
                long double dy = w->point.y - c->y;
                long double dz = w->point.z - c->z;
                float x = m.m[2] * dz + m.m[1] * dy + m.m[0] * dx;
                float y = m.m[5] * dz + m.m[4] * dy + m.m[3] * dx;
                long double z = m.m[8] * dz + m.m[7] * dy + m.m[6] * dx;
                Matrix n;

                w->point.x = x + c->x;
                w->point.y = y + c->y;
                w->point.z = z + c->z;
                n.m[0] = m.m[0] * r[0] + m.m[6] * r[2] + m.m[3] * r[1];
                n.m[1] = m.m[1] * r[0] + m.m[7] * r[2] + m.m[4] * r[1];
                n.m[2] = m.m[2] * r[0] + m.m[8] * r[2] + m.m[5] * r[1];
                n.m[3] = m.m[0] * r[3] + m.m[6] * r[5] + m.m[3] * r[4];
                n.m[4] = m.m[1] * r[3] + m.m[7] * r[5] + m.m[4] * r[4];
                n.m[5] = m.m[2] * r[3] + m.m[8] * r[5] + m.m[5] * r[4];
                n.m[6] = m.m[6] * r[8] + m.m[3] * r[7] + m.m[0] * r[6];
                n.m[7] = m.m[7] * r[8] + m.m[4] * r[7] + m.m[1] * r[6];
                n.m[8] = m.m[8] * r[8] + m.m[5] * r[7] + m.m[2] * r[6];
                n.unk24 = w->rot.unk24 + m.unk24 + 1;
                /* the original renormalises a local copy past 1000 and throws it away */
                w->rot = n;
                w = (SplineObjWaypoint *)Task_FindNext(w, gSplineObjWaypointType);
            } while (w);
        }
        angle = o->angle;
        o->oldAngle = angle;
    }
    if (__builtin_islessgreater(o->pos.x, o->oldPos.x) || __builtin_islessgreater(o->pos.y, o->oldPos.y) ||
        __builtin_islessgreater(o->pos.z, o->oldPos.z)) {
        Matrix m;
        float dx = o->pos.x - o->oldPos.x;
        float dy = o->pos.y - o->oldPos.y;
        float dz = o->pos.z - o->oldPos.z;
        float x, y, z;

        Matrix_SetRotZ(&m, angle);
        x = m.m[2] * dz + m.m[1] * dy + m.m[0] * dx;
        y = m.m[5] * dz + m.m[4] * dy + m.m[3] * dx;
        z = m.m[8] * dz + m.m[7] * dy + m.m[6] * dx;
        for (w = (SplineObjWaypoint *)Task_FindChild(o, gSplineObjWaypointType); w;
             w = (SplineObjWaypoint *)Task_FindNext(w, gSplineObjWaypointType)) {
            w->point.x = x + w->point.x;
            w->point.y = y + w->point.y;
            w->point.z = z + w->point.z;
        }
        o->oldPos = o->pos;
    }
    if (o->unk20 < 0)
        o->unk20 = 0;
    if (arg && *arg)
        return;
    CALL_CHILDREN(&o->task, Param_GetChangedEvent(), arg);
}
HOOK(0x502740, SplineObj_Changed);

void SplineObj_GetParams(void *task, void **out)
{
    *out = gSplineObjParams;
}
HOOK(0x502E80, SplineObj_GetParams);

void SplineObj_GetInfo(SplineObj *o, ObjInfo *out)
{
    SplineObjWaypoint *w = (SplineObjWaypoint *)Task_FindChild(o, gSplineObjWaypointType);

    out->pos = (Vec3d){0, 0, 0};
    out->radius = kFar;
    if (w) {
        float s;

        out->pos = w->point;
        s = max_scale(&w->obj);
        out->radius = VirModel_GetRadius(w->obj.model) * s;
    }
    out->unk18[0] = 0;
    out->unk18[1] = 0;
    out->unk18[2] = 0;
    out->matrix = (Matrix){{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
    out->name = NULL;
}
HOOK(0x502E90, SplineObj_GetInfo);

void SplineObjWaypoint_GetParams(void *task, void **out)
{
    *out = gSplineObjWaypointParams;
}
HOOK(0x502F90, SplineObjWaypoint_GetParams);

void SplineObjWaypoint_GetInfo(SplineObjWaypoint *w, ObjInfo *out)
{
    float s;

    out->pos = w->point;
    out->unk18[0] = 0;
    out->unk18[1] = 0;
    out->unk18[2] = 0;
    out->matrix = (Matrix){{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
    s = max_scale(&w->obj);
    out->name = NULL;
    out->radius = VirModel_GetRadius(w->obj.model) * s;
}
HOOK(0x502FA0, SplineObjWaypoint_GetInfo);

void SplineObjWaypoint_GetDebugText(SplineObjWaypoint *w, char **out)
{
    if (!w->seg)
        return;
    crt_sprintf(gSplineObjDebugText, "Segment length: %f meter\n", (double)(w->seg->length * kMeter));
    crt_sprintf(gSplineObjDebugText, "%sModel length: %f meter\n", gSplineObjDebugText,
                (double)((w->seg->end - w->seg->start) * kMeter));
    *out = gSplineObjDebugText;
}
HOOK(0x503070, SplineObjWaypoint_GetDebugText);

/* The collision slots only answer when the SplineObj has collide set */
void SplineObjWaypoint_CollLines(SplineObjWaypoint *w, void *arg)
{
    if (((SplineObj *)w->obj.task.parent)->collide)
        gTaskHandlers[9][RigidDynCubeObj_GetType()](w, arg);
}
HOOK(0x5030F0, SplineObjWaypoint_CollLines);

void SplineObjWaypoint_CollLine(SplineObjWaypoint *w, void *arg)
{
    if (((SplineObj *)w->obj.task.parent)->collide)
        gTaskHandlers[8][RigidDynCubeObj_GetType()](w, arg);
}
HOOK(0x503120, SplineObjWaypoint_CollLine);

void SplineObjWaypoint_Slot10(SplineObjWaypoint *w, void *arg)
{
    if (((SplineObj *)w->obj.task.parent)->collide)
        gTaskHandlers[10][RigidDynCubeObj_GetType()](w, arg);
}
HOOK(0x503150, SplineObjWaypoint_Slot10);

void SplineObjWaypoint_Slot11(SplineObjWaypoint *w, void *arg)
{
    if (((SplineObj *)w->obj.task.parent)->collide)
        gTaskHandlers[11][RigidDynCubeObj_GetType()](w, arg);
}
HOOK(0x503180, SplineObjWaypoint_Slot11);

void SplineObj_Close(void)
{
    TaskType_Free(gSplineObjType);
    TaskType_Free(gSplineObjWaypointType);
    QTaskList_Free(gSplineObjParams);
    QTaskList_Free(gSplineObjWaypointParams);
}
HOOK(0x5031B0, SplineObj_Close);
