/* SimpointObj: simple flying debris. A SimpointObjContainer moves its
   SimpointObj children with damping, gravity and a decaying spin. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "editobj/editobj.h"

FN_EXTERN(Task_FalseHandler);
FN_EXTERN(Task_Nothing);

/* "SimpointObjContainer" (0x64) */
typedef struct SimpointObjContainer {
    Task task;
    int frames;         /* life of each object */
    char model[16];
    char unk34[16];
    float damping;      /* velocity factor per frame */
    float unk48;
    float unk4C;
    float gravity;
    Vec3f spin;         /* euler angles per frame, decays */
    char autoDelete;    /* delete the container when it is empty */
    char unk61[3];
} SimpointObjContainer;

/* "SimpointObj" (0x100), a RigidDynCubeObj */
typedef struct SimpointObj {
    RigidDynCubeObj obj;
    Vec3f vel;
    int life;
} SimpointObj;

_Static_assert(sizeof(SimpointObjContainer) == 0x64, "SimpointObjContainer");
_Static_assert(sizeof(SimpointObj) == 0x100, "SimpointObj");

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define TASK_FALSEHANDLER (FN(0x45F920, Task_FalseHandler))

extern uint16_t gSimpointObjContainerType;
extern uint16_t gSimpointObjType;
extern char gSimpointObjRandom[];

static const float kSpinDecay = 0x1.fae148p-1f;

void SimpointObjContainer_Run(SimpointObjContainer *c);
void SimpointObj_Create(SimpointObj *o);
void SimpointObj_Delete(SimpointObj *o);
void SimpointObj_Run(SimpointObj *o);

void SimpointObj_Open(void)
{
    uint16_t t = TaskType_New("SimpointObjContainer", sizeof(SimpointObjContainer), Task_GetRootType(), 0x20, 0);

    gSimpointObjContainerType = t;
    TaskType_SetHandler(t, 0, TASK_NOTHING);
    TaskType_SetHandler2(gSimpointObjContainerType, 2, TASK_NOTHING);
    TaskType_SetHandler2(gSimpointObjContainerType, 1, SimpointObjContainer_Run);
    t = TaskType_New("SimpointObj", sizeof(SimpointObj), RigidDynCubeObj_GetType(), 0, 0);
    gSimpointObjType = t;
    TaskType_SetHandler(t, 0, SimpointObj_Create);
    TaskType_SetHandler2(gSimpointObjType, 2, SimpointObj_Delete);
    TaskType_SetHandler2(gSimpointObjType, 1, SimpointObj_Run);
    TaskType_SetHandler(gSimpointObjType, 12, TASK_FALSEHANDLER);
    Random_Init(gSimpointObjRandom);
}
HOOK(0x5034E0, SimpointObj_Open);

void SimpointObjContainer_Run(SimpointObjContainer *c)
{
    SimpointObj *o = (SimpointObj *)c->task.head;
    float k, fall, ax, ay, az;
    Matrix e;

    if (!o->obj.task.next || !o) {
        if (c->autoDelete)
            Task_Delete(c);
        return;
    }
    k = c->damping;
    fall = c->gravity * c->unk4C * k;
    c->spin.x = ax = c->spin.x * kSpinDecay;
    c->spin.y = ay = c->spin.y * kSpinDecay;
    c->spin.z = az = c->spin.z * kSpinDecay;
    Matrix_SetEuler(&e, ax, ay, az);
    do {
        const float *w = o->obj.matrix.m;
        long double vx = k * o->vel.x;
        float vy, vz;
        Vec3d p;
        Matrix r;

        o->vel.x = vx;
        vy = k * o->vel.y;
        o->vel.y = vy;
        vz = k * o->vel.z - fall;
        o->vel.z = vz;
        p.x = vx + o->obj.pos.x;
        p.y = vy + o->obj.pos.y;
        p.z = vz + o->obj.pos.z;
        o->obj.pos = p;
        r.m[0] = e.m[0] * w[0] + e.m[2] * w[6] + e.m[1] * w[3];
        r.m[1] = e.m[2] * w[7] + e.m[1] * w[4] + e.m[0] * w[1];
        r.m[2] = e.m[0] * w[2] + e.m[2] * w[8] + e.m[1] * w[5];
        r.m[3] = e.m[3] * w[0] + e.m[5] * w[6] + e.m[4] * w[3];
        r.m[4] = e.m[5] * w[7] + e.m[4] * w[4] + e.m[3] * w[1];
        r.m[5] = e.m[3] * w[2] + e.m[5] * w[8] + e.m[4] * w[5];
        r.m[6] = e.m[6] * w[0] + e.m[8] * w[6] + e.m[7] * w[3];
        r.m[7] = e.m[8] * w[7] + e.m[7] * w[4] + e.m[6] * w[1];
        r.m[8] = e.m[6] * w[2] + e.m[8] * w[8] + e.m[7] * w[5];
        r.count = o->obj.matrix.count + e.count + 1;
        /* the original renormalises a local copy past 1000 and throws it away */
        o->obj.matrix = r;
        o = (SimpointObj *)o->obj.task.next;
    } while (o && o->obj.task.next);
    CALL_CHILDREN(&c->task, 1);
}
HOOK(0x5035B0, SimpointObjContainer_Run);

void SimpointObj_Create(SimpointObj *o)
{
    RigidDynCubeObj_Init(gCurrentWorld, o, NULL);
    DynCube_ListAdd(gCurrentWorld, o, 0);
}
HOOK(0x503B60, SimpointObj_Create);

void SimpointObj_Delete(SimpointObj *o)
{
    DynCube_ListRemove(gCurrentWorld, o, 0);
}
HOOK(0x503B90, SimpointObj_Delete);

void SimpointObj_Run(SimpointObj *o)
{
    if (o->life <= 0) {
        Task_Delete(o);
        return;
    }
    o->life--;
}
HOOK(0x503BB0, SimpointObj_Run);

void SimpointObj_Close(void)
{
    TaskType_Free(gSimpointObjContainerType);
    TaskType_Free(gSimpointObjType);
}
HOOK(0x503BD0, SimpointObj_Close);

uint16_t SimpointObj_GetType(void)
{
    return gSimpointObjType;
}
HOOK(0x503BF0, SimpointObj_GetType);

SimpointObjContainer *SimpointObjContainer_New(int frames, const char *model, const char *name2, float damping,
                                               float unk48, float unk4C, float gravity, Vec3f *spin, char autoDelete)
{
    SimpointObjContainer *c = (SimpointObjContainer *)Task_New(Level_GetDynamic(), gSimpointObjContainerType, NULL);

    c->frames = frames;
    strcpy(c->model, model);
    if (name2)
        strcpy(c->unk34, name2);
    else
        c->unk34[0] = 0;
    c->unk48 = unk48;
    c->damping = damping;
    c->gravity = gravity;
    c->unk4C = unk4C;
    c->spin = *spin;
    c->autoDelete = autoDelete;
    return c;
}
HOOK(0x503C00, SimpointObjContainer_New);

SimpointObj *SimpointObjContainer_Add(SimpointObjContainer *c, uint16_t type, Vec3d *pos, Matrix *m, Vec3f *vel)
{
    SimpointObj *o = (SimpointObj *)Task_New(c, type, NULL);
    void **model = VirModel_Find(c->model);

    if (!model) {
        Error_Show("VirModel is null");
        for (;;)
            ;
    }
    RigidDynCubeObj_SetModel(o, model);
    o->obj.pos = *pos;
    o->obj.matrix = *m;
    o->vel = *vel;
    o->life = c->frames;
    return o;
}
HOOK(0x503CC0, SimpointObjContainer_Add);

void SimpointObjContainer_SetAutoDelete(SimpointObjContainer *c, char on)
{
    c->autoDelete = on;
}
HOOK(0x503D50, SimpointObjContainer_SetAutoDelete);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7A628) uint16_t gSimpointObjContainerType;
BSS(A7A628) uint16_t gSimpointObjType;
PAD(igb, A7A628, A7A62C, 0x4);
PAD(igb, A7A628, A7A630, 0x10);
ALIAS(gSimpointObjRandom, pad_A7A630, 0);
