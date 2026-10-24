#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "plane/plane.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));

#define MAGICOBJ_DELETE ((void *)0x51B940)
#define TASK_RUN_CHILDREN ((void *)0x488700)
#define RUDDER_PARSE_DEF ((void *)0x436740)
#define RUDDER_DEF_SIZE ((void *)0x427F80)

#define gRudderType VAR(uint16_t, 0x57BD74)

enum {
    RUDDER_AILERON,
    RUDDER_TAILERON_LEFT,
    RUDDER_TAILERON_RIGHT,
    RUDDER_GAMMA,
};

/* fchs on the x87 value, so gcc cannot move the negation after a rounding */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

void Rudder_Create(Part *o, PartCreateArg *arg)
{
    PlanePart_Init(o, arg);
}
HOOK(0x4360C0, Rudder_Create);

void Rudder_Move(Part *o, PartMoveArg *a);

void Rudder_Init(void)
{
    gRudderType = TaskType_New("Rudder", sizeof(Part), MagicObj_GetType(), 0);
    TaskType_SetHandler(gRudderType, 0, Rudder_Create);
    TaskType_SetHandler2(gRudderType, 1, TASK_RUN_CHILDREN);
    TaskType_SetHandler2(gRudderType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler(gRudderType, MagicObj_GetMoveEvent(), Rudder_Move);
    TaskType_SetHandler(gRudderType, RigidDynCubeObj_GetEvent0(), RUDDER_PARSE_DEF);
    TaskType_SetHandler(gRudderType, RigidDynCubeObj_GetEvent1(), RUDDER_DEF_SIZE);
    Script_RegisterConstant("TASKTYPE_RUDDER", gRudderType);
    Script_RegisterConstant("RUDDER_TAILERON_LEFT", RUDDER_TAILERON_LEFT);
    Script_RegisterConstant("RUDDER_TAILERON_RIGHT", RUDDER_TAILERON_RIGHT);
    Script_RegisterConstant("RUDDER_AILERON", RUDDER_AILERON);
    Script_RegisterConstant("RUDDER_GAMMA", RUDDER_GAMMA);
}
HOOK(0x4361D0, Rudder_Init);

/* follows the owner and turns by the plane's control inputs, then
   matrix = parent * local * RotX(angle) */
void Rudder_Move(Part *o, PartMoveArg *a)
{
    VehicleConfig *cfg = a->owner->v.phys->data;
    RudderDef *def;
    const float *m = a->matrix->m, *l, *p, *r;
    long double x = m[2] * o->offset.z + m[1] * o->offset.y + m[0] * o->offset.x;
    double y = m[5] * o->offset.z + m[4] * o->offset.y + m[3] * o->offset.x;
    double z = m[8] * o->offset.z + m[7] * o->offset.y + m[6] * o->offset.x;
    float scale, radius, angle;
    long double k, v;
    Matrix rot, t, out;
    Vec3d pos;

    pos.x = x + a->pos->x;
    pos.y = y + a->pos->y;
    pos.z = z + a->pos->z;
    scale = max_scale(o->scale);
    radius = VirModel_GetRadius(o->model) * scale;
    DynCube_Move(Game_GetWorld(), o, &pos, radius);
    def = o->def;
    k = def->angle * kDegToRad;
    /* other kinds keep the scale, which shared the angle's stack slot */
    angle = scale;
    switch (def->kind) {
    case RUDDER_TAILERON_LEFT:
        v = k * a->owner->v.turn[1] - k * a->owner->v.turn[0];
        angle = v;
        if (v >= cfg->axis[0].rate)
            angle = cfg->axis[0].rate;
        break;
    case RUDDER_TAILERON_RIGHT:
        v = k * a->owner->v.turn[1] + k * a->owner->v.turn[0];
        angle = v;
        if (v >= cfg->axis[0].rate)
            angle = cfg->axis[0].rate;
        break;
    case RUDDER_AILERON:
        angle = x87_neg(k * a->owner->v.turn[0]);
        break;
    case RUDDER_GAMMA:
        angle = x87_neg(k * a->owner->v.turn[2]);
        break;
    }
    Matrix_SetRotX(&rot, angle);
    r = rot.m;
    l = o->local.m;
    t.m[0] = r[6] * l[2] + r[0] * l[0] + r[3] * l[1];
    t.m[1] = r[7] * l[2] + r[1] * l[0] + r[4] * l[1];
    t.m[2] = r[8] * l[2] + r[2] * l[0] + r[5] * l[1];
    t.m[3] = r[6] * l[5] + r[0] * l[3] + r[3] * l[4];
    t.m[4] = r[7] * l[5] + r[1] * l[3] + r[4] * l[4];
    t.m[5] = r[8] * l[5] + r[2] * l[3] + r[5] * l[4];
    t.m[6] = r[6] * l[8] + r[0] * l[6] + r[3] * l[7];
    t.m[7] = r[7] * l[8] + r[1] * l[6] + r[4] * l[7];
    t.m[8] = r[8] * l[8] + r[2] * l[6] + r[5] * l[7];
    t.unk24 = o->local.unk24 + rot.unk24 + 1;
    p = a->matrix->m;
    out.m[0] = t.m[6] * p[2] + t.m[3] * p[1] + t.m[0] * p[0];
    out.m[1] = t.m[7] * p[2] + t.m[4] * p[1] + t.m[1] * p[0];
    out.m[2] = t.m[8] * p[2] + t.m[5] * p[1] + t.m[2] * p[0];
    out.m[3] = t.m[6] * p[5] + t.m[0] * p[3] + t.m[3] * p[4];
    out.m[4] = t.m[7] * p[5] + t.m[1] * p[3] + t.m[4] * p[4];
    out.m[5] = t.m[8] * p[5] + t.m[2] * p[3] + t.m[5] * p[4];
    out.m[6] = t.m[6] * p[8] + t.m[0] * p[6] + t.m[3] * p[7];
    out.m[7] = t.m[7] * p[8] + t.m[1] * p[6] + t.m[4] * p[7];
    out.m[8] = t.m[8] * p[8] + t.m[2] * p[6] + t.m[5] * p[7];
    out.unk24 = a->matrix->unk24 + t.unk24 + 1;
    o->matrix = out;
}
HOOK(0x4362C0, Rudder_Move);
