/* GunFlame, GunClip and GunMovingPart: the model parts that ride on a gun. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "guncasing/guncasing.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416D30, void *, Game_GetRandom2, (void));
ORIG(0x47E6F0, uint8_t, sub_47E6F0, (void));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B3640, void, Matrix_RotateY, (Matrix *out, const Matrix *m, float angle));
ORIG(0x4B47C0, int, Random_Int, (void *rng, int n));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C4950, void, sub_4C4950, (void *a, void *b, int c));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D9850, uint8_t, MagicObj_GetHideEvent, (void));
ORIG(0x4D9860, void *, MagicObj_GetList, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E75A0, int, Render_GetFrame, (void));
ORIG(0x4E81F0, char *, Render_GetView, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));

#define gGunFlameType VAR(uint16_t, 0x54074C)
#define gGunClipType VAR(uint16_t, 0x54074E)
#define gGunMovingPartType VAR(uint16_t, 0x540750)

#define Task_NullHandler ((void *)0x4D3710)
#define Task_TrueHandler ((void *)0x4EFA10)
#define Human_ClearResult ((void *)0x45E3D0)
#define GunClip_RenderHandler ((void *)0x472880)

void GunFlame_CreateHandler(GunPart *p, GunPartArgs *a);
void GunFlame_DeleteHandler(GunPart *p);
void GunPart_MoveHandler(GunPart *p, MoveMsg *msg);
void GunFlame_RenderHandler(GunPart *p, char *frame);
void GunPart_HideHandler(GunPart *p, int *hide);
void GunPart_CreateHandler(GunPart *p, GunPartArgs *a);
void GunMovingPart_RenderHandler(GunPart *p);

void GunParts_Open(void)
{
    gGunFlameType = TaskType_New("GunFlame", 0x140, MagicObj_GetType(), 0, 0);
    TaskType_SetHandler(gGunFlameType, 0, GunFlame_CreateHandler);
    TaskType_SetHandler2(gGunFlameType, 1, Task_NullHandler);
    TaskType_SetHandler2(gGunFlameType, 2, GunFlame_DeleteHandler);
    TaskType_SetHandler(gGunFlameType, Param_GetChangedEvent(), Task_NullHandler);
    TaskType_SetHandler(gGunFlameType, RigidDynCubeObj_GetEvent0(), Task_NullHandler);
    TaskType_SetHandler(gGunFlameType, RigidDynCubeObj_GetEvent1(), Task_TrueHandler);
    TaskType_SetHandler(gGunFlameType, RigidDynCubeObj_GetEvent2(), GunFlame_RenderHandler);
    TaskType_SetHandler(gGunFlameType, MagicObj_GetMoveEvent(), GunPart_MoveHandler);
    TaskType_SetHandler(gGunFlameType, MagicObj_GetHideEvent(), GunPart_HideHandler);
    TaskType_SetHandler(gGunFlameType, World_GetEvent1(), NULL);
    TaskType_SetHandler(gGunFlameType, World_GetEvent0(), NULL);
    TaskType_SetHandler(gGunFlameType, sub_47E6F0(), NULL);
    TaskType_SetHandler(gGunFlameType, Game_GetEvent7(), Human_ClearResult);

    gGunClipType = TaskType_New("GunClip", 0x140, MagicObj_GetType(), 0, 0);
    TaskType_SetHandler(gGunClipType, 0, GunPart_CreateHandler);
    TaskType_SetHandler2(gGunClipType, 1, Task_NullHandler);
    TaskType_SetHandler2(gGunClipType, 2, Task_NullHandler);
    TaskType_SetHandler(gGunClipType, Param_GetChangedEvent(), Task_NullHandler);
    TaskType_SetHandler(gGunClipType, RigidDynCubeObj_GetEvent0(), Task_NullHandler);
    TaskType_SetHandler(gGunClipType, RigidDynCubeObj_GetEvent1(), Task_TrueHandler);
    TaskType_SetHandler(gGunClipType, RigidDynCubeObj_GetEvent2(), GunClip_RenderHandler);
    TaskType_SetHandler(gGunClipType, World_GetEvent1(), NULL);
    TaskType_SetHandler(gGunClipType, World_GetEvent0(), NULL);
    TaskType_SetHandler(gGunClipType, sub_47E6F0(), NULL);
    TaskType_SetHandler(gGunClipType, Game_GetEvent7(), Human_ClearResult);

    gGunMovingPartType = TaskType_New("GunMovingPart", 0x140, MagicObj_GetType(), 0, 0);
    TaskType_SetHandler(gGunMovingPartType, 0, GunPart_CreateHandler);
    TaskType_SetHandler2(gGunMovingPartType, 1, Task_NullHandler);
    TaskType_SetHandler2(gGunMovingPartType, 2, Task_NullHandler);
    TaskType_SetHandler(gGunMovingPartType, Param_GetChangedEvent(), Task_NullHandler);
    TaskType_SetHandler(gGunMovingPartType, RigidDynCubeObj_GetEvent0(), Task_NullHandler);
    TaskType_SetHandler(gGunMovingPartType, RigidDynCubeObj_GetEvent1(), Task_TrueHandler);
    TaskType_SetHandler(gGunMovingPartType, RigidDynCubeObj_GetEvent2(), GunMovingPart_RenderHandler);
    TaskType_SetHandler(gGunMovingPartType, World_GetEvent1(), NULL);
    TaskType_SetHandler(gGunMovingPartType, World_GetEvent0(), NULL);
    TaskType_SetHandler(gGunMovingPartType, sub_47E6F0(), NULL);
    TaskType_SetHandler(gGunMovingPartType, Game_GetEvent7(), Human_ClearResult);

    Script_RegisterConstant("TASKTYPE_GUNFLAME", gGunFlameType);
    Script_RegisterConstant("TASKTYPE_GUNCLIP", gGunClipType);
    Script_RegisterConstant("TASKTYPE_GUNMOVINGPART", gGunMovingPartType);
}
HOOK(0x477220, GunParts_Open);

static void **gunpart_model(GunPartArgs *a)
{
    void **model = VirModel_Find(a->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", a->model);
        for (;;)
            ;
    }
    return model;
}

void GunFlame_CreateHandler(GunPart *p, GunPartArgs *a)
{
    void **model;

    QTaskList_Add(MagicObj_GetList(), p);
    model = gunpart_model(a);
    p->offset = a->offset;
    p->local = a->local;
    RigidDynCubeObj_Init(a->world, p, model);
    p->o.matrix = a->matrix;
    p->unk138 = a->unk8C;
    p->unk130 = a->unk88;
    p->flashStart = -1;
}
HOOK(0x477600, GunFlame_CreateHandler);

void GunFlame_DeleteHandler(GunPart *p)
{
    QTaskList_Remove(MagicObj_GetList(), p);
}
HOOK(0x4776A0, GunFlame_DeleteHandler);

/* puts the part at its offset and rotation relative to the gun */
void GunPart_MoveHandler(GunPart *p, MoveMsg *msg)
{
    const float *m = msg->matrix->m;
    const float *l = p->local.m;
    double x, y, z;
    Matrix r;
    int i, j;

    x = (m[2] * p->offset.z + m[1] * p->offset.y) + m[0] * p->offset.x;
    y = (m[5] * p->offset.z + m[4] * p->offset.y) + m[3] * p->offset.x;
    z = (m[8] * p->offset.z + m[7] * p->offset.y) + m[6] * p->offset.x;
    p->o.pos.x = x + msg->pos->x;
    p->o.pos.y = y + msg->pos->y;
    p->o.pos.z = z + msg->pos->z;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            r.m[i * 3 + j] = (m[i * 3 + 1] * l[3 + j] + m[i * 3 + 2] * l[6 + j]) + m[i * 3] * l[j];
    r.unk24 = msg->matrix->unk24 + p->local.unk24 + 1;
    p->o.matrix = r;
}
HOOK(0x4776C0, GunPart_MoveHandler);

/* the flame shows every other frame while the gun fires, turned randomly */
void GunFlame_RenderHandler(GunPart *p, char *frame)
{
    Gun *gun = ((GunColl *)p->o.task.parent)->gun;
    int now = Render_GetFrame();
    int angles[3];
    MoveMsg msg;
    TaskFn fn;
    int i;

    if (gun->lastShot < now - 1) {
        p->flashStart = -1;
        return;
    }
    if (p->flashStart == -1)
        p->flashStart = now;
    if ((now - p->flashStart) & 1)
        return;
    i = Random_Int(Game_GetRandom2(), 3);
    angles[0] = 0;
    angles[1] = 4;
    angles[2] = 7;
    Matrix_RotateY(&p->o.matrix, &p->o.matrix, angles[i] * kDegToRad);
    msg.pos = (Vec3d *)frame;
    msg.matrix = (Matrix *)(frame + 0x18);
    msg.obj = &p->o.task;
    fn = TASK_HANDLER(MagicObj_GetMoveEvent(), p->o.task.type);
    if (fn)
        fn(p, &msg);
    TASK_HANDLER(4, RigidDynCubeObj_GetType())(p, 0);
}
HOOK(0x4778E0, GunFlame_RenderHandler);

/* asks the gun's owner whether the part shows */
void GunPart_HideHandler(GunPart *p, int *hide)
{
    Gun *gun = ((GunColl *)p->o.task.parent)->gun;
    Task *owner = gun->o.task.parent;
    int v;

    if (!TASK_HANDLER(Gun_GetEvent1(), owner->type))
        return;
    v = *hide;
    TASK_HANDLER(Gun_GetEvent1(), owner->type)(owner, &v);
    p->o.hidden = v ? 0 : 1;
}
HOOK(0x4779E0, GunPart_HideHandler);

void GunPart_CreateHandler(GunPart *p, GunPartArgs *a)
{
    void **model = gunpart_model(a);

    p->offset = a->offset;
    p->local = a->local;
    RigidDynCubeObj_Init(a->world, p, model);
    p->o.hidden = 0;
    p->o.matrix = a->matrix;
    p->unk138 = a->unk8C;
    p->unk130 = a->unk88;
    sub_4C4950(p, p, p->unk130);
}
HOOK(0x477A70, GunPart_CreateHandler);

/* takes position and rotation from the owner's view model */
void GunMovingPart_RenderHandler(GunPart *p)
{
    Gun *gun = ((GunColl *)p->o.task.parent)->gun;

    if (!*(int *)(Render_GetView() + 0x5C))
        return;
    {
        Task *owner = gun->o.task.parent;

        if (TASK_HANDLER(Gun_GetEvent4(), owner->type)) {
            struct {
                Vec3d pos;
                Matrix matrix;
            } bone;

            TASK_HANDLER(Gun_GetEvent4(), owner->type)(owner, &bone);
            p->o.pos = bone.pos;
            p->o.matrix = bone.matrix;
        }
    }
    TASK_HANDLER(4, RigidDynCubeObj_GetType())(p, 0);
}
HOOK(0x477B10, GunMovingPart_RenderHandler);

void GunParts_Close(void)
{
    TaskType_Free(gGunMovingPartType);
    TaskType_Free(gGunClipType);
    TaskType_Free(gGunFlameType);
}
HOOK(0x477BC0, GunParts_Close);
