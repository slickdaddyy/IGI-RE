#pragma GCC optimize("excess-precision=standard")

#include "freecam/freecam.h"

FN_EXTERN(HitZone_GetArgSize);

/* BoneMagicObj: an animated bone model fixed to a parent object. It follows
   the parent's moves at an offset and adds the animation's root motion. */

typedef struct BoneModelInfo {
    int16_t model;
    int16_t unk2;
    int anim;
} BoneModelInfo;

typedef struct BoneMagicObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    char anim[0x10];
    float matrix[10];
    float radius;
    char unkA4[0x34];
    Vec3d offset;
    float localMatrix[10];
    Task *owner;
    BoneModelInfo *info;
    float motion[3];
    int unk12C;
} BoneMagicObj;

typedef struct BoneMagicArgs {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;
    float matrix[10];
    float localMatrix[10];
    Task *owner;
    BoneModelInfo *info;
} BoneMagicArgs;

/* argument of the move event: the parent's position and matrix */
typedef struct BoneMoveArgs {
    Vec3d *pos;
    float *matrix;
} BoneMoveArgs;

typedef struct BoneEnterArgs {
    Vec3d pos;
    float matrix[10];
} BoneEnterArgs;

typedef struct BoneScriptArgs {
    int16_t *out;
    void *args;
    int index;
} BoneScriptArgs;

typedef struct BoneIgnoreArgs {
    char result;
    char unk1[3];
    Task *other;
} BoneIgnoreArgs;

_Static_assert(offsetof(BoneMagicObj, matrix) == 0x78, "matrix");
_Static_assert(offsetof(BoneMagicObj, radius) == 0xA0, "radius");
_Static_assert(offsetof(BoneMagicObj, offset) == 0xD8, "offset");
_Static_assert(offsetof(BoneMagicObj, owner) == 0x118, "owner");
_Static_assert(sizeof(BoneMagicObj) == 0x130, "BoneMagicObj");
_Static_assert(offsetof(BoneMagicArgs, info) == 0x8C, "BoneMagicArgs");

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4D9620, void, BoneDynCubeObj_Init, (void *world, void *obj, short model, void *virModel));
ORIG(0x4D96F0, void, BoneDynCubeObj_Deinit, (void *obj));
ORIG(0x5124C0, void, BoneDynCubeObj_EnterHandler, (void *obj, void *arg));
ORIG(0x4CEC10, void *, VirModel_Find, (const char *name));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, const Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, const Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4D6110, void, Anim_PlayNow, (void *anim, int a, int id, int b));
ORIG(0x4D38C0, void, BoneObj_Update, (float *matrix, void *anim));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *unk, float *delta, void *anim));
ORIG(0x4D3210, void, BoneObj_GetEvents, (void *unk, void *anim));

extern uint16_t gBoneMagicObjType;
extern uint32_t gBoneMagicObjTypeWord;
#define kSomeHandler427F80 (FN(0x427F80, HitZone_GetArgSize))

void BoneMagicObj_Run(BoneMagicObj *o)
{
    float d[3];

    BoneObj_Update(o->matrix, o->anim);
    BoneObj_GetRootMotion(NULL, d, o->anim);
    o->motion[0] = d[0] + o->motion[0];
    o->motion[1] = d[1] + o->motion[1];
    o->motion[2] = d[2] + o->motion[2];
    BoneObj_GetEvents(NULL, o->anim);
    FC_BROADCAST((Task *)o->task.children.head, 1);
}
HOOK(0x531620, BoneMagicObj_Run);

void BoneMagicObj_Enter(BoneMagicObj *o, void *arg)
{
    BoneEnterArgs a;

    BoneDynCubeObj_EnterHandler(o, arg);
    memcpy(a.matrix, o->matrix, sizeof(a.matrix));
    memcpy(&a.pos, &o->pos, sizeof(Vec3d));
    FC_BROADCAST((Task *)o->task.children.head, RigidDynCubeObj_GetEvent2(), &a);
}
HOOK(0x5316F0, BoneMagicObj_Enter);

void BoneMagicObj_Delete(BoneMagicObj *o)
{
    BoneDynCubeObj_Deinit(o);
    DynCube_Remove(gWorld, o);
}
HOOK(0x5317B0, BoneMagicObj_Delete);

void BoneMagicObj_Create(BoneMagicObj *o, BoneMagicArgs *a);
void BoneMagicObj_Move(BoneMagicObj *o, BoneMoveArgs *a);
int BoneMagicObj_IsOther(BoneMagicObj *o, BoneIgnoreArgs *a);
void BoneMagicObj_ReadArgs(BoneMagicObj *o, BoneScriptArgs *a);

void BoneMagicObj_Init(void)
{
    gBoneMagicObjType = TaskType_New("BoneMagicObj", sizeof(BoneMagicObj), BoneDynCubeObj_GetType(), 0, 0);
    TaskType_SetHandler(gBoneMagicObjType, 4, NULL);
    TaskType_SetHandler(gBoneMagicObjType, RigidDynCubeObj_GetEvent2(), BoneMagicObj_Enter);
    TaskType_SetHandler2(gBoneMagicObjType, 1, BoneMagicObj_Run);
    TaskType_SetHandler(gBoneMagicObjType, 0, BoneMagicObj_Create);
    TaskType_SetHandler(gBoneMagicObjType, MagicObj_GetMoveEvent(), BoneMagicObj_Move);
    TaskType_SetHandler(gBoneMagicObjType, 0xC, BoneMagicObj_IsOther);
    TaskType_SetHandler(gBoneMagicObjType, RigidDynCubeObj_GetEvent0(), BoneMagicObj_ReadArgs);
    TaskType_SetHandler(gBoneMagicObjType, RigidDynCubeObj_GetEvent1(), kSomeHandler427F80);
    TaskType_SetHandler2(gBoneMagicObjType, 2, BoneMagicObj_Delete);
    Script_RegisterConstant("TASKTYPE_BONEMAGICOBJ", gBoneMagicObjTypeWord & 0xFFFF);
}
HOOK(0x5317D0, BoneMagicObj_Init);

void BoneMagicObj_Create(BoneMagicObj *o, BoneMagicArgs *a)
{
    void *vm = VirModel_Find(a->model);

    if (!vm) {
        Error_Show("VirModel \"%s\" not available", a->model);
        for (;;)
            ;
    }
    o->info = a->info;
    BoneDynCubeObj_Init(a->world, o, o->info->model, vm);
    DynCube_Add(a->world, o, &a->pos, o->radius, 2);
    Anim_PlayNow(o->anim, 0, o->info->anim, 1);
    memcpy(&o->offset, &a->offset, sizeof(Vec3d));
    memcpy(o->localMatrix, a->localMatrix, sizeof(o->localMatrix));
    memcpy(o->matrix, a->matrix, sizeof(o->matrix));
    BoneObj_Update(o->matrix, o->anim);
    o->owner = a->owner;
    o->motion[0] = 0;
    o->motion[1] = 0;
    o->motion[2] = 0;
}
HOOK(0x5318D0, BoneMagicObj_Create);

void BoneMagicObj_Move(BoneMagicObj *o, BoneMoveArgs *a)
{
    const float *m = a->matrix, *l = o->localMatrix;
    double off[3];
    long double x, z;
    Vec3d p;
    float r[10];
    BoneMoveArgs next;

    off[0] = m[2] * o->offset.z + m[1] * o->offset.y + m[0] * o->offset.x;
    off[1] = m[5] * o->offset.z + m[4] * o->offset.y + m[3] * o->offset.x;
    off[2] = m[8] * o->offset.z + m[7] * o->offset.y + m[6] * o->offset.x;
    x = off[0] + a->pos->x;
    p.y = off[1] + a->pos->y;
    z = off[2] + a->pos->z;
    p.x = o->motion[0] + x;
    p.y = o->motion[1] + p.y;
    p.z = o->motion[2] + z;
    DynCube_Move(gWorld, o, &p, o->radius);
    m = a->matrix;
    r[0] = l[6] * m[2] + l[0] * m[0] + l[3] * m[1];
    r[1] = l[7] * m[2] + l[1] * m[0] + l[4] * m[1];
    r[2] = l[8] * m[2] + l[2] * m[0] + l[5] * m[1];
    r[3] = l[3] * m[4] + m[5] * l[6] + m[3] * l[0];
    r[4] = l[4] * m[4] + m[5] * l[7] + m[3] * l[1];
    r[5] = l[5] * m[4] + m[5] * l[8] + m[3] * l[2];
    r[6] = l[3] * m[7] + m[8] * l[6] + m[6] * l[0];
    r[7] = l[4] * m[7] + m[8] * l[7] + m[6] * l[1];
    r[8] = l[5] * m[7] + m[8] * l[8] + m[6] * l[2];
    ((int *)r)[9] = ((const int *)m)[9] + ((const int *)l)[9] + 1;
    memcpy(o->matrix, r, sizeof(r));
    next.pos = &o->pos;
    next.matrix = o->matrix;
    FC_BROADCAST((Task *)o->task.children.head, MagicObj_GetMoveEvent(), &next);
}
HOOK(0x5319B0, BoneMagicObj_Move);

/* event 0xC: result is set when other is neither this object nor its owner */
int BoneMagicObj_IsOther(BoneMagicObj *o, BoneIgnoreArgs *a)
{
    int r = (Task *)o != a->other && o->owner != a->other;

    a->result = r;
    return r;
}
HOOK(0x531CA0, BoneMagicObj_IsOther);

void BoneMagicObj_ReadArgs(BoneMagicObj *o, BoneScriptArgs *a)
{
    int16_t *out = a->out;

    *out = Script_GetArgInt(a->args, a->index++);
    *(int *)(out + 2) = Script_GetArgInt(a->args, a->index++);
}
HOOK(0x531CD0, BoneMagicObj_ReadArgs);

void BoneMagicObj_Shutdown(void)
{
    TaskType_Free(gBoneMagicObjType);
    gBoneMagicObjType = 0x181;
}
HOOK(0x531D10, BoneMagicObj_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54F848) uint32_t gBoneMagicObjTypeWord INIT(385);
ALIAS(gBoneMagicObjType, gBoneMagicObjTypeWord, 0);
