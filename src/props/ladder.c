/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "props/props.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x414ED0, void, Model_GetLocalPoint, (Vec3f *out, void *obj, int i));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x416AD0, char, UseQuery_Consider, (UseQuery *q, void *obj, const Vec3d *pos));
ORIG(0x4871C0, void, StatusScreen_DrawActionIcon, (Task *human, void *sprite));
ORIG(0x491CF0, void *, Display_GetMode, (void));
ORIG(0x4B2B30, long double, Vec3_Angle, (const Vec3f *a, const Vec3f *b));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *room, void *obj, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *room, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));

/* "Ladder" (0x140 bytes), a magic object humans climb (use kind 10) */
typedef struct Ladder {
    RigidObj obj;
    char unkF0[0x40];
    void *unk130;
    char unk134[4];
    int rungs;          /* 0x138 */
    char unk13C[4];
} Ladder;

_Static_assert(sizeof(Ladder) == 0x140, "Ladder");

/* the create argument of magic objects */
typedef struct MagicCreateArg {
    const char *model;
    void *room;
    Vec3d pos;          /* 0x08 */
    char unk20[0x18];
    Matrix matrix;      /* 0x38 */
    char unk60[0x28];
    void *unk88;
} MagicCreateArg;

_Static_assert(offsetof(MagicCreateArg, unk88) == 0x88, "MagicCreateArg");

extern uint16_t gLadderType;

extern float kClimbTime;    /* 6 */
extern double kRungScale;   /* 1 / 1228.8 */
extern double kReachMin;    /* -2867.2 */
extern double kReachMax;    /* 2867.2 */
extern float kLadderAngle;  /* pi / 4 */

#define HUMAN_LADDER_TOP(h) (((char *)(h))[0xD7B])

float Fence_GetClimbTime(void)
{
    return kClimbTime;
}
HOOK(0x440270, Fence_GetClimbTime);

void Ladder_Create(Ladder *l, MagicCreateArg *arg)
{
    void **model;
    float scale, radius;
    Vec3f p2, p3;
    long double h;

    QTaskList_Add(Game_GetTaskList0(), l);
    model = VirModel_Find(arg->model);
    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    RigidDynCubeObj_Init(arg->room, l, model);
    scale = max_scale(&l->obj);
    radius = VirModel_GetRadius(l->obj.model) * scale;
    DynCube_Add(arg->room, l, &arg->pos, radius, 1);
    l->obj.matrix = arg->matrix;
    l->unk130 = arg->unk88;
    RigidDynCubeObj_CreateMagicObjs(l, l, arg->unk88);
    Model_GetLocalPoint(&p2, l, 2);
    Model_GetLocalPoint(&p3, l, 3);
    h = p2.z - p3.z;
    if (!(h >= kZero))
        h = -h;
    l->rungs = (int)round_ll(h * kRungScale);
}
HOOK(0x440280, Ladder_Create);

void Ladder_Delete(Ladder *l)
{
    QTaskList_Remove(Game_GetTaskList0(), l);
    DynCube_Remove(l->obj.room, l);
}
HOOK(0x4403C0, Ladder_Delete);

void Ladder_Use(Ladder *l, UseQuery *q);
void Ladder_GetUseKind(Ladder *l, int *out);
void Ladder_DrawIcon(Ladder *l, Task **human);

void Ladder_Open(void)
{
    gLadderType = TaskType_New("Ladder", sizeof(Ladder), MagicObj_GetType(), 0);
    TaskType_SetHandler(gLadderType, 0, Ladder_Create);
    TaskType_SetHandler2(gLadderType, 2, Ladder_Delete);
    TaskType_SetHandler(gLadderType, Param_GetChangedEvent(), TASK_NOTHING);
    TaskType_SetHandler(gLadderType, Game_GetEvent2(), Ladder_Use);
    TaskType_SetHandler(gLadderType, Game_GetEvent3(), Ladder_GetUseKind);
    TaskType_SetHandler(gLadderType, Game_GetEvent4(), TASK_NOTHING);
    TaskType_SetHandler(gLadderType, Game_GetEvent6(), Ladder_DrawIcon);
    Script_RegisterConstant("TASKTYPE_LADDER", gLadderType);
}
HOOK(0x4403E0, Ladder_Open);

/* Game event 2: offers the ladder from below when facing it, and from the
   top (human byte 0xD7B set) when standing above its upper end */
void Ladder_Use(Ladder *l, UseQuery *q)
{
    const float *m = l->obj.matrix.m;
    Task *human = q->human;
    long double dx, dy, dz, ly, y, tx;
    double lx;
    Vec3d top, t;

    dx = q->pos.x - l->obj.pos.x;
    dy = q->pos.y - l->obj.pos.y;
    dz = q->pos.z - l->obj.pos.z;
    lx = (m[6] * dz + m[3] * dy) + m[0] * dx;
    ly = (m[7] * dz + m[1] * dx) + m[4] * dy;
    y = ly;
    if (!(ly >= kZeroD))
        y = -y;
    if (!(y >= q->range) && lx > kReachMin && !(lx >= kReachMax)) {
        Vec3f u, n;
        float angle;
        long double topz;

        u.x = q->matrix.m[1];
        u.y = q->matrix.m[4];
        u.z = q->matrix.m[7];
        n.x = m[1];
        n.y = m[4];
        n.z = m[7];
        if (ly > kZeroD) {
            n.x = -n.x;
            n.y = -n.y;
            n.z = -n.z;
        }
        angle = Vec3_Angle(&u, &n);
        Model_GetPoint(&top, l, 1);
        topz = top.z + l->obj.pos.z;
        if (!(angle >= kLadderAngle) && topz > q->pos.z && q->pos.z > l->obj.pos.z) {
            q->best = l;
            q->best_pitch = 0;
            q->best_yaw = 0;
            HUMAN_LADDER_TOP(human) = 0;
        }
    }
    Model_GetPoint(&top, l, 1);
    tx = top.x + l->obj.pos.x;
    t.x = tx;
    t.y = top.y + l->obj.pos.y;
    t.z = top.z + l->obj.pos.z;
    dx = q->pos.x - tx;
    dy = q->pos.y - t.y;
    dz = q->pos.z - t.z;
    if (!((m[7] * dz + m[1] * dx) + m[4] * dy >= kZeroD) && UseQuery_Consider(q, l, &t))
        HUMAN_LADDER_TOP(human) = 1;
}
HOOK(0x4404C0, Ladder_Use);

void Ladder_GetUseKind(Ladder *l, int *out)
{
    *out = 10;
}
HOOK(0x4406C0, Ladder_GetUseKind);

void Ladder_DrawIcon(Ladder *l, Task **human)
{
    Task *h = *human;

    Display_GetMode();
    Display_GetMode();
    StatusScreen_DrawActionIcon(h, (char *)h + 0x494);
}
HOOK(0x4406D0, Ladder_DrawIcon);

void Ladder_Close(void)
{
    TaskType_Free(gLadderType);
}
HOOK(0x440700, Ladder_Close);
