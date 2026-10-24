#include "hook.h"
#include "computer/computer.h"

FN_EXTERN(Task_FalseHandler);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x43EF10, Task *, GenericPhysicsObj_New, (void *parent, Vec3d *pos, Matrix *matrix, char *name, float *vel));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4C1780, void, Debug_Log, (void *task));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x504AD0, void, EditRigidObjAttachedToBone_Create, (void *task, void *arg));
ORIG(0x504DB0, uint16_t, EditRigidObjAttachedToBone_GetType, (void));

#define HANDLER_45F920 (FN(0x45F920, Task_FalseHandler))

/* a part attached to a human that is thrown off as a physics object */
typedef struct HumanAttach {
    Task task;
    Vec3d pos;
    char unk38[0x38];
    Matrix matrix;
    char unk98[0x128 - 0x98];
    char name[16];
    char detached;
    char unk139[7];
} HumanAttach;

_Static_assert(offsetof(HumanAttach, detached) == 0x138, "HumanAttach");
_Static_assert(sizeof(HumanAttach) == 0x140, "HumanAttach");

/* AI detection query: result byte, then the two tasks involved at +8 */
typedef struct DetectQuery {
    char result;
    char unk1[7];
    Task *a;
    Task *b;
} DetectQuery;

extern uint16_t gHumanAttachType;

void HumanAttach_Create(HumanAttach *h, void *arg);
void HumanAttach_Run(HumanAttach *h);
void HumanAttach_Detach(HumanAttach *h, void *arg);
void HumanAttach_Detect(HumanAttach *h, DetectQuery *q);

void HumanAttach_Open(void)
{
    gHumanAttachType = TaskType_New("HumanAttach", sizeof(HumanAttach), EditRigidObjAttachedToBone_GetType(), 0);
    TaskType_SetHandler(gHumanAttachType, 0, HumanAttach_Create);
    TaskType_SetHandler2(gHumanAttachType, 1, HumanAttach_Run);
    TaskType_SetHandler(gHumanAttachType, Game_GetEvent9(), HumanAttach_Detach);
    TaskType_SetHandler(gHumanAttachType, Game_GetEvent8(), HANDLER_45F920);
    TaskType_SetHandler(gHumanAttachType, Game_GetEvent13(), HANDLER_45F920);
    TaskType_SetHandler(gHumanAttachType, AIDetection_GetEvent0(), HumanAttach_Detect);
}
HOOK(0x465A10, HumanAttach_Open);

void HumanAttach_Create(HumanAttach *h, void *arg)
{
    h->detached = 0;
    EditRigidObjAttachedToBone_Create(h, arg);
}
HOOK(0x465AC0, HumanAttach_Create);

void HumanAttach_Run(HumanAttach *h)
{
    if (h->detached) {
        Task_Delete(h);
        return;
    }
    Debug_Log(h);
}
HOOK(0x465AE0, HumanAttach_Run);

void HumanAttach_Detach(HumanAttach *h, void *arg)
{
    float vel[3] = {0, 0, 0};
    Task *o = GenericPhysicsObj_New(Level_GetDynamic(), &h->pos, &h->matrix, h->name, vel);
    TaskFn fn = HANDLER(Game_GetEvent9(), o->type);

    if (fn)
        fn(o, arg);
    h->detached = 1;
}
HOOK(0x465B00, HumanAttach_Detach);

void HumanAttach_Detect(HumanAttach *h, DetectQuery *q)
{
    Task *p = h->task.parent;

    if (q->a != &h->task && q->a != p && q->b != &h->task && q->b != p) {
        q->result = 1;
        return;
    }
    q->result = 0;
}
HOOK(0x465B80, HumanAttach_Detect);

void HumanAttach_Close(void)
{
    TaskType_Free(gHumanAttachType);
}
HOOK(0x465BB0, HumanAttach_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BDC48) uint16_t gHumanAttachType;
PAD(igb, 5BDC48, 5BDC4A, 0x2);
