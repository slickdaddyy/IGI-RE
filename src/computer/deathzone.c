#pragma GCC optimize("excess-precision=standard")

#include <math.h>

#include "hook.h"
#include "computer/computer.h"

FN_EXTERN(Task_FalseHandler);
FN_EXTERN(VehiclePart_Delete);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, int, Game_GetWorld, (void));
ORIG(0x42BC20, uint16_t, Train_GetType, (void));
ORIG(0x460C60, TaskArray *, Human_GetList, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (int world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (int world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (int world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (void *obj));

#define VEHICLEPART_DELETE (FN(0x426860, VehiclePart_Delete))
#define HANDLER_45F920 (FN(0x45F920, Task_FalseHandler))

/* a MagicObj that rides on a vehicle and hurts the humans it touches */
typedef struct DeathZone {
    Obj o;
    char unkA8[0xF0 - sizeof(Obj)];
    Vec3d offset;
    Matrix rel;
    Task *vehicle;
    int unk134;
} DeathZone;

_Static_assert(offsetof(DeathZone, rel) == 0x108, "DeathZone");
_Static_assert(sizeof(DeathZone) == 0x138, "DeathZone");

typedef struct DeathZoneArg {
    char *model;
    int world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix rel;
    Task *vehicle;
} DeathZoneArg;

_Static_assert(offsetof(DeathZoneArg, vehicle) == 0x88, "DeathZoneArg");

/* sent to every HumanAI while the zone moves */
typedef struct DeathZoneMsg {
    struct DeathZone *zone;
    float *speed;
} DeathZoneMsg;

typedef struct MoveArg {
    Vec3d *pos;
    float *matrix;
} MoveArg;

extern uint16_t gDeathZoneType;
extern uint8_t gDeathZoneEvent;

static const float kZero = 0.0f;

void DeathZone_Create(DeathZone *z, DeathZoneArg *a);
void DeathZone_Run(DeathZone *z);
void DeathZone_Move(DeathZone *z, MoveArg *a);
uint8_t DeathZone_GetEvent(void);

void DeathZone_Open(void)
{
    gDeathZoneType = TaskType_New("DeathZone", sizeof(DeathZone), MagicObj_GetType(), 0);
    TaskType_SetHandler(gDeathZoneType, 0, DeathZone_Create);
    TaskType_SetHandler2(gDeathZoneType, 1, DeathZone_Run);
    TaskType_SetHandler2(gDeathZoneType, 2, VEHICLEPART_DELETE);
    TaskType_SetHandler(gDeathZoneType, MagicObj_GetMoveEvent(), DeathZone_Move);
    TaskType_SetHandler(gDeathZoneType, Game_GetEvent9(), NULL);
    TaskType_SetHandler(gDeathZoneType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gDeathZoneType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gDeathZoneType, 9, NULL);
    TaskType_SetHandler(gDeathZoneType, AIDetection_GetEvent0(), HANDLER_45F920);
    Script_RegisterConstant("TASKTYPE_DEATHZONE", gDeathZoneType);
    gDeathZoneEvent = TaskEvent_Alloc(0);
}
HOOK(0x465BC0, DeathZone_Open);

void DeathZone_Create(DeathZone *z, DeathZoneArg *a)
{
    void **model = VirModel_Find(a->model);
    float scale;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", a->model);
        for (;;)
            ;
    }
    z->offset = a->offset;
    z->rel = a->rel;
    RigidDynCubeObj_Init(a->world, z, model);
    scale = max_scale(&z->o);
    DynCube_Add(a->world, z, &a->pos, VirModel_GetRadius(z->o.model) * scale, 2);
    z->o.matrix = a->matrix;
    z->vehicle = a->vehicle;
}
HOOK(0x465CD0, DeathZone_Create);

static void send_all(DeathZone *z, float *speed)
{
    DeathZoneMsg m;
    TaskFn fn;
    int i;

    m.zone = z;
    m.speed = speed;
    for (i = 0; i < Human_GetList()->count; i++) {
        uint16_t type = Human_GetList()->items[i]->type;

        fn = HANDLER(DeathZone_GetEvent(), type);
        fn(Human_GetList()->items[i], &m);
    }
}

void DeathZone_Run(DeathZone *z)
{
    Task *v = z->vehicle;
    Task *p;

    if (!v || !TaskType_IsA(v->type, Vehicle_GetType()))
        return;
    if (v->type == Train_GetType()) {
        p = v->parent;
        if (p->type == Train_GetType())
            v = p;
        if (!islessgreater(*(float *)((char *)v + 0x108), kZero))
            return;
    } else if (PhysicsObj_IsResting(v)) {
        return;
    }
    send_all(z, (float *)((char *)v + 0x108));
}
HOOK(0x465DE0, DeathZone_Run);

/* follows the vehicle: offset and rotation are relative to it */
void DeathZone_Move(DeathZone *z, MoveArg *a)
{
    float *m;
    double r[3], d[3];
    Vec3d p;
    float scale, radius;
    Matrix out;
    const float *b = z->rel.m;

    if (!TaskType_IsA(z->vehicle->type, Vehicle_GetType()))
        return;
    m = a->matrix;
    d[0] = z->offset.x;
    d[1] = z->offset.y;
    d[2] = z->offset.z;
    r[0] = m[2] * d[2] + m[1] * d[1] + m[0] * d[0];
    r[1] = m[5] * d[2] + m[4] * d[1] + m[3] * d[0];
    r[2] = m[8] * d[2] + m[7] * d[1] + m[6] * d[0];
    p.x = r[0] + a->pos->x;
    p.y = r[1] + a->pos->y;
    p.z = r[2] + a->pos->z;
    scale = max_scale(&z->o);
    radius = VirModel_GetRadius(z->o.model) * scale;
    DynCube_Move(Game_GetWorld(), z, &p, radius);
    m = a->matrix;
    out.m[0] = m[0] * b[0] + b[3] * m[1] + m[2] * b[6];
    out.m[1] = m[0] * b[1] + b[4] * m[1] + m[2] * b[7];
    out.m[2] = m[0] * b[2] + b[5] * m[1] + m[2] * b[8];
    out.m[3] = m[5] * b[6] + m[3] * b[0] + m[4] * b[3];
    out.m[4] = m[5] * b[7] + m[3] * b[1] + m[4] * b[4];
    out.m[5] = m[5] * b[8] + m[3] * b[2] + m[4] * b[5];
    out.m[6] = m[8] * b[6] + m[6] * b[0] + m[7] * b[3];
    out.m[7] = m[8] * b[7] + m[6] * b[1] + m[7] * b[4];
    out.m[8] = m[8] * b[8] + m[6] * b[2] + m[7] * b[5];
    out.unk24 = ((int *)m)[9] + z->rel.unk24 + 1;
    z->o.matrix = out;
}
HOOK(0x465F50, DeathZone_Move);

void DeathZone_Close(void)
{
    TaskType_Free(gDeathZoneType);
    TaskEvent_Free(gDeathZoneEvent);
}
HOOK(0x466210, DeathZone_Close);

uint8_t DeathZone_GetEvent(void)
{
    return gDeathZoneEvent;
}
HOOK(0x466230, DeathZone_GetEvent);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53F4FC) uint16_t gDeathZoneType INIT(385);
DATA(53F4FC) uint8_t gDeathZoneEvent INIT(255);
PAD(igd, 53F4FC, 53F4FF, 0x1);
