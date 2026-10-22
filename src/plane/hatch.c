#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "plane/plane.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));

#define MAGICOBJ_DELETE ((void *)0x51B940)
#define PART_DEF_SIZE_4 ((void *)0x427D70)

#define gHatchType VAR(uint16_t, 0x57BD6C)
#define kHatchKeep VAR(float, 0x533760)    /* 0.97 */
#define kHatchBlend VAR(float, 0x5337D0)   /* 0.03 */

void Hatch_Create(Part *o, PartCreateArg *arg)
{
    float *def;

    PlanePart_Init(o, arg);
    def = o->def;
    o->angle = arg->owner->gear ? kZero : def[0] * kDegToRad;
    CockpitShield_SetMatrix(o, &arg->owner->v.matrix);
}
HOOK(0x435C50, Hatch_Create);

void Hatch_Run(Part *o);
void Hatch_Move(Part *o, PartMoveArg *a);
void Hatch_ParseDef(void *unused, DefParseArg *a);

void Hatch_Init(void)
{
    gHatchType = TaskType_New("Hatch", sizeof(Part), MagicObj_GetType(), 0);
    TaskType_SetHandler(gHatchType, 0, Hatch_Create);
    TaskType_SetHandler2(gHatchType, 1, Hatch_Run);
    TaskType_SetHandler2(gHatchType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler(gHatchType, MagicObj_GetMoveEvent(), Hatch_Move);
    TaskType_SetHandler(gHatchType, RigidDynCubeObj_GetEvent0(), Hatch_ParseDef);
    TaskType_SetHandler(gHatchType, RigidDynCubeObj_GetEvent1(), PART_DEF_SIZE_4);
    Script_RegisterConstant("TASKTYPE_HATCH", gHatchType);
}
HOOK(0x435D90, Hatch_Init);

/* opens while the gear is down and out, else closes */
void Hatch_Run(Part *o)
{
    Plane *pl = o->owner;
    long double full = *(float *)o->def * kDegToRad;

    if (!pl->gear && !pl->gear_state)
        o->angle = o->angle * kHatchKeep + full * kHatchBlend;
    else
        o->angle = o->angle * kHatchKeep;
    pl->hatch_state = part_state(full, o->angle);
}
HOOK(0x435E50, Hatch_Run);

void Hatch_Move(Part *o, PartMoveArg *a)
{
    PlanePart_Place(o, a, 1);
    CockpitShield_SetMatrix(o, a->matrix);
}
HOOK(0x435F60, Hatch_Move);

/* the def of CockpitShield and Hatch: one angle in degrees */
void Hatch_ParseDef(void *unused, DefParseArg *a)
{
    (void)unused;
    *(float *)a->out = Script_GetArgReal(a->args, a->index);
}
HOOK(0x436090, Hatch_ParseDef);

void Hatch_Close(void)
{
    TaskType_Free(gHatchType);
}
HOOK(0x4360B0, Hatch_Close);
