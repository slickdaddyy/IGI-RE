#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

#define HANDLER_449950 ((void *)0x449950)

ORIG(0x4D0D80, int, Mesh3D_GetEvent5, (void));

/* script arguments of an ExplodeMagicObj (0x3C bytes) */
typedef struct ExplodeParams {
    char model[16];
    float health;
    float unk14;
    float unk18;
    float unk1C;
    float unk20;
    int unk24;
    int unk28;
    char sound[16];
} ExplodeParams;

/* the argument of the create handler */
typedef struct ExplodeCreateArg {
    const char *model;
    int world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    void *owner;
    ExplodeParams *params;
} ExplodeCreateArg;

/* the argument of the script argument handler */
typedef struct ScriptArg {
    ExplodeParams *out;
    void *args;
    int index;
} ScriptArg;

/* a magic object that swaps its model and explodes once it took enough damage */
typedef struct ExplodeMagicObj {
    Obj obj;
    Vec3d offset;
    Matrix local;
    void *owner;
    int unk134;
    ExplodeParams *params;
    float damage;
    float health;
    float unk144;
    char exploded;
    char pad149[7];
} ExplodeMagicObj;

/* the argument of the Game event 9 handler */
typedef struct HitArg {
    int unk0;
    float damage;
} HitArg;

_Static_assert(sizeof(ExplodeParams) == 0x3C, "ExplodeParams");
_Static_assert(offsetof(ExplodeCreateArg, params) == 0x8C, "ExplodeCreateArg");
_Static_assert(offsetof(ExplodeMagicObj, params) == 0x138, "ExplodeMagicObj");
_Static_assert(sizeof(ExplodeMagicObj) == 0x150, "ExplodeMagicObj");

#define gExplodeMagicObjType VAR(uint16_t, 0x57BF7C)

static const float kUnits = 4096.0f;

void ExplodeMagicObj_Create(ExplodeMagicObj *o, ExplodeCreateArg *arg)
{
    void **model;

    QTaskList_Add(Game_GetTaskList3(), o);
    model = VirModel_Find(arg->model);
    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    o->offset = arg->offset;
    o->local = arg->local;
    RigidDynCubeObj_Init(arg->world, o, model);
    DynCube_Add(arg->world, o, &arg->pos, obj_radius(&o->obj), 2);
    o->obj.matrix = arg->matrix;
    o->damage = 0.0f;
    o->params = arg->params;
    o->exploded = 0;
    o->health = arg->params->health;
    o->unk144 = arg->params->unk1C;
    o->owner = arg->owner;
    RigidDynCubeObj_CreateMagicObjs(o, o, o->owner);
}
HOOK(0x43BE80, ExplodeMagicObj_Create);

void ExplodeMagicObj_Run(ExplodeMagicObj *o);
void ExplodeMagicObj_ParseArgs(void *task, ScriptArg *a);
void ExplodeMagicObj_GetArgSize(void *task, int *out);
void ExplodeMagicObj_Hit(ExplodeMagicObj *o, HitArg *arg);
void ExplodeMagicObj_Blast(ExplodeMagicObj *o, void *blast);

void ExplodeMagicObj_Init(void)
{
    gExplodeMagicObjType = TaskType_New("ExplodeMagicObj", sizeof(ExplodeMagicObj), MagicObj_GetType(), 0);
    TaskType_SetHandler(gExplodeMagicObjType, 0, ExplodeMagicObj_Create);
    TaskType_SetHandler2(gExplodeMagicObjType, 1, ExplodeMagicObj_Run);
    TaskType_SetHandler2(gExplodeMagicObjType, 2, HANDLER_449950);
    TaskType_SetHandler(gExplodeMagicObjType, RigidDynCubeObj_GetEvent0(), ExplodeMagicObj_ParseArgs);
    TaskType_SetHandler(gExplodeMagicObjType, RigidDynCubeObj_GetEvent1(), ExplodeMagicObj_GetArgSize);
    TaskType_SetHandler(gExplodeMagicObjType, Game_GetEvent9(), ExplodeMagicObj_Hit);
    TaskType_SetHandler(gExplodeMagicObjType, Game_GetEvent11(), ExplodeMagicObj_Blast);
    Script_RegisterConstant("TASKTYPE_EXPLODEMAGICOBJ", gExplodeMagicObjType);
}
HOOK(0x43BFD0, ExplodeMagicObj_Init);

void ExplodeMagicObj_Run(ExplodeMagicObj *o)
{
    ExplodeParams *p;
    void **model;

    if (o->exploded || !(o->damage >= o->health))
        return;
    model = VirModel_Find(o->params->model);
    if (!model) {
        Error_Show("VirModel \"%s\" not available", o->params->model);
        for (;;)
            ;
    }
    RigidDynCubeObj_SetModel(o, model);
    o->obj.unkDC = 0;
    CALL_CHILDREN(&o->obj.task, Mesh3D_GetEvent5());
    p = o->params;
    Explosion_Spawn(&o->obj.pos, 1, 0, o->unk144, p->unk14 * kUnits, p->unk18 * kUnits, 6144.0f, p->unk24, p->unk28, 0,
                    "1009_01_1", p->sound, 0x15, 2, 102400.0f, 0);
    o->exploded = 1;
}
HOOK(0x43C0B0, ExplodeMagicObj_Run);

void ExplodeMagicObj_ParseArgs(void *task, ScriptArg *a)
{
    ExplodeParams *p = a->out;
    int i = a->index;

    Script_GetArgString(a->args, i, p->model, 16);
    Script_GetArgString(a->args, i + 1, p->sound, 16);
    p->health = Script_GetArgReal(a->args, i + 2);
    p->unk28 = Script_GetArgInt(a->args, i + 3);
    p->unk24 = Script_GetArgInt(a->args, i + 4);
    p->unk20 = Script_GetArgReal(a->args, i + 5);
    p->unk18 = Script_GetArgReal(a->args, i + 6);
    p->unk1C = Script_GetArgReal(a->args, i + 7);
    p->unk14 = Script_GetArgReal(a->args, i + 8);
}
HOOK(0x43C1F0, ExplodeMagicObj_ParseArgs);

void ExplodeMagicObj_GetArgSize(void *task, int *out)
{
    *out = sizeof(ExplodeParams);
}
HOOK(0x43C2A0, ExplodeMagicObj_GetArgSize);

void ExplodeMagicObj_Hit(ExplodeMagicObj *o, HitArg *arg)
{
    o->damage = arg->damage + o->damage;
}
HOOK(0x43C2B0, ExplodeMagicObj_Hit);

void ExplodeMagicObj_Blast(ExplodeMagicObj *o, void *blast)
{
    if (o->exploded)
        return;
    o->damage = Game_BlastFalloff(blast, o, &o->obj.pos, obj_radius(&o->obj), 1) + o->damage;
}
HOOK(0x43C2D0, ExplodeMagicObj_Blast);

void ExplodeMagicObj_Close(void)
{
    TaskType_Free(gExplodeMagicObjType);
}
HOOK(0x43C380, ExplodeMagicObj_Close);
