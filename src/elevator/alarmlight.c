#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

ORIG(0x4DCCB0, int, Sector_AddObject, (int world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (int world, void *obj, Vec3d *pos));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *obj));

/* a light that turns on and off with a script condition and breaks when hit */
typedef struct AlarmLight {
    Obj obj;
    Vec3d place;
    Matrix placeMatrix;
    char offModel[16];
    char onModel[16];
    char deadModel[16];
    char onSound[32];
    char deadSound[32];
    char isOn;
    char isDestroyed;
    char registered;
    char pad1A3;
    VarString cond;
    Task *sound;
} AlarmLight;

_Static_assert(offsetof(AlarmLight, offModel) == 0x130, "AlarmLight");
_Static_assert(sizeof(AlarmLight) == 0x1F8, "AlarmLight");

#define gAlarmLightParams VAR(void *, 0x57BE9C)
#define gAlarmLightReflectorType VAR(uint16_t, 0x57BEA0)
#define gAlarmLightType VAR(uint16_t, 0x57BEA2)

static const float kZero = 0.0f;

void AlarmLight_Create(AlarmLight *al)
{
    int i;

    RigidDynCubeObj_Init(gCurrentWorld, al, NULL);
    al->place.x = 0;
    al->place.y = 0;
    al->place.z = 0;
    DynCube_Add(gCurrentWorld, al, &al->place, obj_radius(&al->obj), 1);
    Sector_AddObject(al->obj.world, al, DynCubeObj_GetPosition(al));
    for (i = 0; i < 9; i++)
        al->placeMatrix.m[i] = i % 4 == 0 ? 1.0f : 0.0f;
    al->placeMatrix.unk24 = 0;
    al->obj.matrix = al->placeMatrix;
    al->offModel[0] = 0;
    al->onModel[0] = 0;
    al->deadModel[0] = 0;
    al->onSound[0] = 0;
    al->deadSound[0] = 0;
    al->isOn = 0;
    al->isDestroyed = 0;
    al->registered = 0;
    VarString_Init(&al->cond);
    al->sound = NULL;
    QTaskList_Add(Game_GetTaskList3(), al);
}
HOOK(0x43A0E0, AlarmLight_Create);

void AlarmLight_Run(AlarmLight *al)
{
    char wasOn = al->isOn;

    if (wasOn && !al->isDestroyed) {
        if (!al->sound)
            al->sound = Sound_Play(al, al->onSound);
    } else if (al->sound) {
        Task_Delete(al->sound);
        al->sound = NULL;
    }
    if (al->isDestroyed)
        return;
    if (VarString_HasExpr(&al->cond))
        al->isOn = (char)round_ll(VarString_EvalReal(&al->cond, al));
    if (wasOn) {
        if (!al->isOn)
            RigidDynCubeObj_SetModel(al, VirModel_Find(al->offModel));
    } else if (al->isOn) {
        RigidDynCubeObj_SetModel(al, VirModel_Find(al->onModel));
    }
    CALL_CHILDREN(&al->obj.task, 1);
    if (al->obj.unkE8)
        ((void (*)(void *))HANDLER(1, al->obj.unkE8->type))(al->obj.unkE8);
}
HOOK(0x43A270, AlarmLight_Run);

void AlarmLight_Delete(AlarmLight *al)
{
    QTaskList_Remove(Game_GetTaskList3(), al);
    VarString_Free(&al->cond);
    if (Task_GetId(al) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), al->obj.task.type);

        if (fn)
            ((void (*)(void *))fn)(al);
    }
    if (al->obj.unkE8)
        Task_Delete(al->obj.unkE8);
    Sector_RemoveObject(al);
    DynCube_Remove(al->obj.world, al);
}
HOOK(0x43A3C0, AlarmLight_Delete);

void AlarmLight_GetParams(void *task, void **out)
{
    *out = gAlarmLightParams;
}
HOOK(0x43A440, AlarmLight_GetParams);

void AlarmLight_Changed(AlarmLight *al, const char *arg)
{
    Task *t;

    RigidDynCubeObj_SetModel(al, VirModel_Find(al->onModel));
    DynCube_Move(al->obj.world, al, &al->place, obj_radius(&al->obj));
    Sector_MoveObject(al->obj.world, al, DynCubeObj_GetPosition(al));
    t = al->obj.unkE8;
    al->obj.matrix = al->placeMatrix;
    if (t)
        Task_Delete(t);
    t = MagicObjContainer_New(NULL, al);
    al->obj.unkE8 = t;
    sub_4C4950(t, al, al);
    if (!arg || !*arg)
        CALL_CHILDREN(al->obj.unkE8, Param_GetChangedEvent(), NULL);
    VarString_Bind(&al->cond, al);
}
HOOK(0x43A450, AlarmLight_Changed);

void AlarmLight_Destroy(AlarmLight *al);

void AlarmLight_Hit(AlarmLight *al)
{
    AlarmLight_Destroy(al);
}
HOOK(0x43A5E0, AlarmLight_Hit);

void AlarmLight_Destroy(AlarmLight *al)
{
    if (al->isDestroyed)
        return;
    Explosion_Spawn(&al->obj.pos, 1, 0, 0.05f, 4096.0f, 0.0f, 2048.0f, 5, 2, 0, "1009_01_1", al->deadSound, 0x15, 2,
                    20480.0f, 0);
    al->isDestroyed = 1;
    RigidDynCubeObj_SetModel(al, VirModel_Find(al->deadModel));
}
HOOK(0x43A5F0, AlarmLight_Destroy);

void AlarmLight_Blast(AlarmLight *al, void *blast)
{
    long double f = Game_BlastFalloff(blast, al, &al->obj.pos, obj_radius(&al->obj), 1);

    if (f < kZero || f > kZero)
        AlarmLight_Destroy(al);
}
HOOK(0x43A660, AlarmLight_Blast);

void AlarmLight_Activate(AlarmLight *al)
{
    char name[64];
    char buf[128];

    if (al->registered)
        return;
    al->registered = 1;
    Task_GetName(name, al);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &al->isOn);
    crt_sprintf(buf, "%s.isDestroyed", name);
    Script_RegisterBool8(buf, &al->isDestroyed);
}
HOOK(0x43A710, AlarmLight_Activate);

void AlarmLight_Deactivate(AlarmLight *al)
{
    char name[64];
    char buf[128];

    if (!al->registered)
        return;
    Task_GetName(name, al);
    al->registered = 0;
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isDestroyed", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x43A790, AlarmLight_Deactivate);

void AlarmLight_Close(void)
{
    QTaskList_Free(gAlarmLightParams);
    TaskType_Free(gAlarmLightReflectorType);
    TaskType_Free(gAlarmLightType);
}
HOOK(0x43A810, AlarmLight_Close);
