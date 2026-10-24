#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "plane/plane.h"

FN_EXTERN(EditorMagicObj_Delete);
FN_EXTERN(Hatch_ParseDef);
FN_EXTERN(RocketPod_GetArgSize);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));

#define MAGICOBJ_DELETE (FN(0x51B940, EditorMagicObj_Delete))
#define PART_DEF_SIZE_4 (FN(0x427D70, RocketPod_GetArgSize))
#define HATCH_PARSE_DEF (FN(0x436090, Hatch_ParseDef))

extern uint16_t gCockpitShieldType;

void CockpitShield_SetMatrix(PlanePart *o, Matrix *parent);

void CockpitShield_Create(PlanePart *o, MagicCreateArg *arg)
{
    float *def;

    PlanePart_Init(o, arg);
    def = o->def;
    o->angle = ((Plane *)arg->owner)->cockpit ? def[0] * kDegToRad : kZero;
    CockpitShield_SetMatrix(o, &((Plane *)arg->owner)->v.matrix);
}
HOOK(0x434E10, CockpitShield_Create);

/* matrix = parent * local * RotX(angle); three sums stay unrounded */
void CockpitShield_SetMatrix(PlanePart *o, Matrix *parent)
{
    const float *l = o->local.m, *p = parent->m;
    const float *r;
    Matrix rot, out;
    long double t00, t10, t20;
    float t01, t02, t11, t12, t21, t22;
    int n;

    Matrix_SetRotX(&rot, o->angle);
    r = rot.m;
    t00 = r[6] * l[2] + r[3] * l[1] + r[0] * l[0];
    t01 = r[7] * l[2] + r[4] * l[1] + r[1] * l[0];
    t02 = r[8] * l[2] + r[5] * l[1] + r[2] * l[0];
    t10 = r[0] * l[3] + r[6] * l[5] + r[3] * l[4];
    t11 = r[1] * l[3] + r[7] * l[5] + r[4] * l[4];
    t12 = r[2] * l[3] + r[8] * l[5] + r[5] * l[4];
    t20 = r[0] * l[6] + r[6] * l[8] + r[3] * l[7];
    t21 = r[1] * l[6] + r[7] * l[8] + r[4] * l[7];
    t22 = r[2] * l[6] + r[8] * l[8] + r[5] * l[7];
    n = o->local.count + rot.count + 1;
    out.m[0] = t20 * p[2] + t10 * p[1] + t00 * p[0];
    out.m[1] = t21 * p[2] + t11 * p[1] + t01 * p[0];
    out.m[2] = t22 * p[2] + t12 * p[1] + t02 * p[0];
    out.m[3] = t20 * p[5] + t10 * p[4] + t00 * p[3];
    out.m[4] = t21 * p[5] + t11 * p[4] + t01 * p[3];
    out.m[5] = t22 * p[5] + t12 * p[4] + t02 * p[3];
    out.m[6] = t00 * p[6] + t20 * p[8] + t10 * p[7];
    out.m[7] = t01 * p[6] + t21 * p[8] + t11 * p[7];
    out.m[8] = t02 * p[6] + t22 * p[8] + t12 * p[7];
    out.count = parent->count + n + 1;
    o->matrix = out;
}
HOOK(0x434F50, CockpitShield_SetMatrix);

void CockpitShield_Run(PlanePart *o);
void CockpitShield_Move(PlanePart *o, PartMoveArg *a);

void CockpitShield_Init(void)
{
    gCockpitShieldType = TaskType_New("CockpitShield", sizeof(PlanePart), MagicObj_GetType(), 0);
    TaskType_SetHandler(gCockpitShieldType, 0, CockpitShield_Create);
    TaskType_SetHandler2(gCockpitShieldType, 1, CockpitShield_Run);
    TaskType_SetHandler2(gCockpitShieldType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler(gCockpitShieldType, MagicObj_GetMoveEvent(), CockpitShield_Move);
    TaskType_SetHandler(gCockpitShieldType, RigidDynCubeObj_GetEvent0(), HATCH_PARSE_DEF);
    TaskType_SetHandler(gCockpitShieldType, RigidDynCubeObj_GetEvent1(), PART_DEF_SIZE_4);
    Script_RegisterConstant("TASKTYPE_COCKPITSHIELD", gCockpitShieldType);
}
HOOK(0x4351E0, CockpitShield_Init);

/* opens towards the def angle while the canopy is open, else closes */
void CockpitShield_Run(PlanePart *o)
{
    long double target = o->owner->cockpit ? *(float *)o->def * kDegToRad : kZero;

    if (fne(target, o->angle))
        o->angle = o->angle * kKeep + target * kBlend;
}
HOOK(0x4352A0, CockpitShield_Run);

void CockpitShield_Move(PlanePart *o, PartMoveArg *a)
{
    PlanePart_Place(o, a, 0);
    CockpitShield_SetMatrix(o, a->matrix);
}
HOOK(0x435300, CockpitShield_Move);

void CockpitShield_Close(void)
{
    TaskType_Free(gCockpitShieldType);
}
HOOK(0x435450, CockpitShield_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BD5C) uint16_t gCockpitShieldType;
PAD(igb, 57BD5C, 57BD5E, 0x2);
