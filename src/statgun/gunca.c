#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

FN_EXTERN(GunMovingPart_RenderHandler);
FN_EXTERN(Human_ClearResult);

/* "GunCABarrel" and "GunCAClip" (0x138 bytes each), MagicObj parts */
typedef struct GunCAPart {
    RigidObj obj;
    Vec3d unkF0;
    Matrix unk108;
    void *unk130;
    char unk134[4];
} GunCAPart;

/* what the create handler gets */
typedef struct GunCAArgs {
    const char *model;
    void *world;
    char unk8[0x18];
    Vec3d unk20;
    Matrix matrix;      /* 0x38 */
    Matrix unk60;
    void *unk88;
} GunCAArgs;

/* a view model point of a human (event 0x477C30) */
typedef struct ViewPoint {
    Vec3d pos;
    Matrix matrix;
} ViewPoint;

/* the message of the Human event 1 handler */
typedef struct GunCAMsg {
    char unk0[8];
    int what;
} GunCAMsg;

_Static_assert(sizeof(GunCAPart) == 0x138, "GunCAPart");
_Static_assert(offsetof(GunCAArgs, unk88) == 0x88, "GunCAArgs");

extern uint16_t gGunCAClipType;
extern uint16_t gGunCABarrelType;
extern float gGunCAMagic;

#define HANDLER_45E3D0 (FN(0x45E3D0, Human_ClearResult))
#define HANDLER_477B10 (FN(0x477B10, GunMovingPart_RenderHandler))

ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4E81F0, char *, Viewport_GetCurrentCamera, (void));
ORIG(0x477C30, uint8_t, Gun_GetEvent3, (void));
ORIG(0x477D10, void, GunCasing_New, (void *a, Vec3d *pos, Vec3f *vel, Matrix *m, const char *name));

void GunCAPart_Create(GunCAPart *o, GunCAArgs *a);
void GunCABarrel_Follow(GunCAPart *o);
void GunCABarrel_Event(GunCAPart *o, GunCAMsg *msg);

void GunCA_Open(void)
{
    gGunCABarrelType = TaskType_New("GunCABarrel", sizeof(GunCAPart), MagicObj_GetType(), 0);
    TaskType_SetHandler(gGunCABarrelType, 0, GunCAPart_Create);
    TaskType_SetHandler2(gGunCABarrelType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gGunCABarrelType, 2, TASK_NOTHING);
    TaskType_SetHandler(gGunCABarrelType, RigidDynCubeObj_GetEvent2(), GunCABarrel_Follow);
    TaskType_SetHandler(gGunCABarrelType, Human_GetEvent1(), GunCABarrel_Event);
    TaskType_SetHandler(gGunCABarrelType, Game_GetEvent7(), HANDLER_45E3D0);
    gGunCAClipType = TaskType_New("GunCAClip", sizeof(GunCAPart), MagicObj_GetType(), 0);
    TaskType_SetHandler(gGunCAClipType, 0, GunCAPart_Create);
    TaskType_SetHandler2(gGunCAClipType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gGunCAClipType, 2, TASK_NOTHING);
    TaskType_SetHandler(gGunCAClipType, RigidDynCubeObj_GetEvent2(), HANDLER_477B10);
    TaskType_SetHandler(gGunCAClipType, Human_GetEvent1(), TASK_NOTHING);
    TaskType_SetHandler(gGunCAClipType, Game_GetEvent7(), HANDLER_45E3D0);
    Script_RegisterConstant("TASKTYPE_CACLIP", gGunCAClipType);
    Script_RegisterConstant("TASKTYPE_CABARREL", gGunCABarrelType);
}
HOOK(0x472670, GunCA_Open);

void GunCAPart_Create(GunCAPart *o, GunCAArgs *a)
{
    void **model = VirModel_Find(a->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", a->model);
        for (;;)
            ;
    }
    o->unkF0 = a->unk20;
    o->unk108 = a->unk60;
    RigidDynCubeObj_Init(a->world, o, model);
    o->obj.unkEC = 0;
    o->obj.matrix = a->matrix;
    o->unk130 = a->unk88;
    RigidDynCubeObj_CreateMagicObjs(o, o, a->unk88);
}
HOOK(0x4727F0, GunCAPart_Create);

/* sticks to a view model point of the human holding the gun */
void GunCABarrel_Follow(GunCAPart *o)
{
    Task *owner = *(Task **)((char *)o->obj.task.parent + 0x20);
    Task *h;
    TaskFn fn;
    ViewPoint p;

    if (!*(int *)(Viewport_GetCurrentCamera() + 0x5C))
        return;
    h = owner->parent;
    if (HANDLER(Gun_GetEvent3(), h->type)) {
        fn = HANDLER(Gun_GetEvent3(), h->type);
        fn(h, &p);
        o->obj.pos = p.pos;
        o->obj.matrix = p.matrix;
    }
    HANDLER(4, RigidDynCubeObj_GetType())(o, 0);
}
HOOK(0x472880, GunCABarrel_Follow);

/* 5: blows the barrel apart into six pieces, 6: swaps the model */
void GunCABarrel_Event(GunCAPart *o, GunCAMsg *msg)
{
    Vec3d pos;
    Vec3f vel;
    void **model;
    int i;

    if (msg->what == 5) {
        for (i = 0; i < 6; i++) {
            Model_GetPoint(&pos, o, i);
            pos.x += o->obj.pos.x;
            pos.y += o->obj.pos.y;
            pos.z += o->obj.pos.z;
            vel.x = -o->obj.matrix.m[1] * gGunCAMagic;
            vel.y = -o->obj.matrix.m[4] * gGunCAMagic;
            vel.z = -o->obj.matrix.m[7] * gGunCAMagic;
            GunCasing_New(Level_GetDynamic(), &pos, &vel, &o->obj.matrix, "113_01_1");
        }
        RigidDynCubeObj_SetModel(o, VirModel_Find("101_02_1"));
    }
    if (msg->what == 6) {
        model = VirModel_Find("101_05_1");
        if (!model) {
            Error_Show("VirModel is null");
            for (;;)
                ;
        }
        RigidDynCubeObj_SetModel(o, model);
    }
}
HOOK(0x472930, GunCABarrel_Event);

void GunCA_Close(void)
{
    TaskType_Free(gGunCABarrelType);
    TaskType_Free(gGunCAClipType);
}
HOOK(0x472A50, GunCA_Close);
