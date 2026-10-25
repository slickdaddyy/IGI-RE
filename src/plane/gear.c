#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "plane/plane.h"
#include "math/mathvars.h"

FN_EXTERN(EditorMagicObj_Delete);
FN_EXTERN(ParamQuat32_Size);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));

#define MAGICOBJ_DELETE (FN(0x51B940, EditorMagicObj_Delete))
#define GEAR_DEF_SIZE (FN(0x4EFF20, ParamQuat32_Size))

extern uint16_t gGearType;
extern float kQuarterPi;

void Gear_SetMatrix(PlanePart *o, Matrix *parent);

/* down at the def angle unless the gear is up */
void Gear_Create(PlanePart *o, MagicCreateArg *arg)
{
    GearDef *def;

    PlanePart_Init(o, arg);
    def = o->def;
    o->angle = ((Plane *)arg->owner)->gear ? kZero : def->angle * kDegToRad;
    Gear_SetMatrix(o, &((Plane *)arg->owner)->v.matrix);
}
HOOK(0x435460, Gear_Create);

/* matrix = parent * local * Euler(angle, 0, roll); roll follows the yaw input
   for gears with the roll flag while the gear is up */
void Gear_SetMatrix(PlanePart *o, Matrix *parent)
{
    GearDef *def = o->def;
    const float *l = o->local.m, *p = parent->m;
    const float *r;
    Matrix rot, out;
    long double t00, t10, t20;
    float t01, t02, t11, t12, t21, t22;
    float roll = 0;
    int n;

    if (def->roll && o->owner->gear)
        roll = o->owner->v.turn[2] * kQuarterPi;
    Matrix_SetEuler(&rot, o->angle, 0, roll);
    r = rot.m;
    t00 = r[0] * l[0] + r[6] * l[2] + r[3] * l[1];
    t01 = r[1] * l[0] + r[7] * l[2] + r[4] * l[1];
    t02 = r[2] * l[0] + r[8] * l[2] + r[5] * l[1];
    t10 = r[0] * l[3] + r[6] * l[5] + r[3] * l[4];
    t11 = r[1] * l[3] + r[7] * l[5] + r[4] * l[4];
    t12 = r[2] * l[3] + r[8] * l[5] + r[5] * l[4];
    t20 = r[6] * l[8] + r[3] * l[7] + r[0] * l[6];
    t21 = r[7] * l[8] + r[4] * l[7] + r[1] * l[6];
    t22 = r[8] * l[8] + r[5] * l[7] + r[2] * l[6];
    n = o->local.count + rot.count + 1;
    out.m[0] = t20 * p[2] + t10 * p[1] + t00 * p[0];
    out.m[1] = t21 * p[2] + t11 * p[1] + t01 * p[0];
    out.m[2] = t22 * p[2] + t12 * p[1] + t02 * p[0];
    out.m[3] = t00 * p[3] + t20 * p[5] + t10 * p[4];
    out.m[4] = t01 * p[3] + t21 * p[5] + t11 * p[4];
    out.m[5] = t02 * p[3] + t22 * p[5] + t12 * p[4];
    out.m[6] = t00 * p[6] + t20 * p[8] + t10 * p[7];
    out.m[7] = t01 * p[6] + t21 * p[8] + t11 * p[7];
    out.m[8] = t02 * p[6] + t22 * p[8] + t12 * p[7];
    out.count = parent->count + n + 1;
    o->matrix = out;
}
HOOK(0x4355A0, Gear_SetMatrix);

void Gear_Run(PlanePart *o);
void Gear_Move(PlanePart *o, PartMoveArg *a);
void Gear_ParseDef(void *unused, DefParseArg *a);

void Gear_Init(void)
{
    gGearType = TaskType_New("Gear", sizeof(PlanePart), MagicObj_GetType(), 0);
    TaskType_SetHandler(gGearType, 0, Gear_Create);
    TaskType_SetHandler2(gGearType, 1, Gear_Run);
    TaskType_SetHandler2(gGearType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler(gGearType, MagicObj_GetMoveEvent(), Gear_Move);
    TaskType_SetHandler(gGearType, RigidDynCubeObj_GetEvent0(), Gear_ParseDef);
    TaskType_SetHandler(gGearType, RigidDynCubeObj_GetEvent1(), GEAR_DEF_SIZE);
    Script_RegisterConstant("TASKTYPE_GEAR", gGearType);
}
HOOK(0x435870, Gear_Init);

/* retracts once the gear is up and the hatch has opened */
void Gear_Run(PlanePart *o)
{
    Plane *pl = o->owner;
    long double full = ((GearDef *)o->def)->angle * kDegToRad;
    long double target = pl->gear && pl->hatch_state == 1 ? (long double)kZero : full;
    long double d;

    if (fne(target, o->angle))
        o->angle = o->angle * kKeep + target * kBlend;
    d = o->angle - target;
    if (d < kZero)
        d = -d;
    if (d < kHundredth)
        o->angle = target;
    pl->gear_state = part_state(full, o->angle);
}
HOOK(0x435930, Gear_Run);

void Gear_Move(PlanePart *o, PartMoveArg *a)
{
    PlanePart_Place(o, a, 0);
    Gear_SetMatrix(o, a->matrix);
}
HOOK(0x435A80, Gear_Move);

void Gear_ParseDef(void *unused, DefParseArg *a)
{
    GearDef *d = a->out;
    int i = a->index;

    (void)unused;
    d->angle = Script_GetArgReal(a->args, i);
    d->roll = Script_GetArgInt(a->args, i + 1);
    d->unk8 = Script_GetArgReal(a->args, i + 2);
    d->unkC = Script_GetArgReal(a->args, i + 3);
    d->unk10 = Script_GetArgReal(a->args, i + 4);
}
HOOK(0x435BD0, Gear_ParseDef);

void Gear_Close(void)
{
    TaskType_Free(gGearType);
}
HOOK(0x435C30, Gear_Close);

uint16_t Gear_GetType(void)
{
    return gGearType;
}
HOOK(0x435C40, Gear_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5336D8) float kBlend INIT(0.01999998f);
DATA(5337C0) float kQuarterPi INIT(0.7853982f);
ALIAS(kLadderAngle, kQuarterPi, 0);
BSS(57BD64) uint16_t gGearType;
PAD(igb, 57BD64, 57BD66, 0x2);
