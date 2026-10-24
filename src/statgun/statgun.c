#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

/* "StationaryGun" (0x220 bytes): a mounted M2HB a human can take over */
typedef struct StatGun {
    RigidObj obj;
    Vec3d basePos;          /* 0xF0, script parameter */
    Matrix baseMatrix;      /* 0x108, script parameter */
    Vec3d relPos;           /* 0x130, base in the frame of the carrier */
    Matrix relMatrix;       /* 0x148 */
    Vec3d muzzlePos;        /* 0x170 */
    Matrix muzzleMatrix;    /* 0x188 */
    char model[0x10];       /* 0x1B0 */
    char weapon[0x20];      /* 0x1C0, weapon id expression */
    float maxPitch;         /* 0x1E0, degrees */
    float minPitch;
    float yawRange;
    int ammo;
    float recoilPitch;      /* 0x1F0 */
    float recoilYaw;
    float pitch;            /* 0x1F8, radians */
    float yaw;
    float outPitch;         /* 0x200, pitch and yaw with recoil */
    float outYaw;
    Human *user;            /* 0x208 */
    Task *slots[3];         /* 0x20C */
    int lastTick;           /* 0x218 */
    char used;              /* 0x21C */
    char pad21D[3];
} StatGun;

/* sent with the RigidDynCubeObj move event */
typedef struct MoveMsg {
    Vec3d *pos;
    Matrix *matrix;
} MoveMsg;

/* the search used to pick what a human can use (see 0x416AD0) */
typedef struct UseQuery {
    Task *human;
    int unk4;
    Vec3d pos;
} UseQuery;

/* Game event 4: a human takes the gun */
typedef struct AttachMsg {
    Human *h;
    char unk4[0x40];
    int unk44;
} AttachMsg;

/* AIDetection event 0 */
typedef struct DetectMsg {
    char visible;
    char unk1[7];
    void *a;
    void *b;
} DetectMsg;

_Static_assert(offsetof(StatGun, model) == 0x1B0, "StatGun");
_Static_assert(offsetof(StatGun, user) == 0x208, "StatGun");
_Static_assert(sizeof(StatGun) == 0x220, "StatGun");

extern uint16_t gStatGunType;
extern void *gStatGunParams;
extern int gStatGunAmmoRange[];

extern float kZero;
extern double kZeroD;
extern float kPi;
extern float kDegToRad;
extern float kTurnStep;
extern float kRecoilFade;
extern double kTwoPiD;
extern float kEyeUp;
extern float kEyeBack;

ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x416890, uint8_t, Game_GetEvent14, (void));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int max));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F0F30, void *, Param_Degrees, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F2050, void, WeaponSlots_Init, (Task **slots));
ORIG(0x4F2290, void, Weapon_DeleteAll, (void *h));
ORIG(0x4F2140, void, WeaponSlots_SelectNext, (void *h, Task **slots, int i));
ORIG(0x4F2310, void, Weapon_Move, (Task *w, Vec3d *pos, Matrix *m));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4658D0, void, HumanView_SetCamera, (HumanView *v, Vec3d *pos, Matrix *m));
ORIG(0x465910, void, HumanView_ClearCamOverride, (HumanView *v));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x413BD0, void *, WeaponType_Find, (int id));
ORIG(0x477C50, Task *, Gun_New, (void *h, int a, int id, int ammo, char b));
ORIG(0x416AD0, void, UseQuery_Consider, (UseQuery *q, void *obj, Vec3d *pos));
ORIG(0x4871C0, void, StatusScreen_DrawActionIcon, (void *h, void *p));

void StationaryGun_RunHandler(StatGun *o);
void StationaryGun_DeleteHandler(StatGun *o);
void StationaryGun_Changed(StatGun *o);
void StationaryGun_UseQuery(StatGun *o, UseQuery *q);
void StationaryGun_GetUseKind(void *task, int *out);
void StationaryGun_Attach(StatGun *o, AttachMsg *m);
void StationaryGun_Event6(void *task, Human **msg);
void StationaryGun_Event14(StatGun *o, int *out);
void StationaryGun_GetParams(void *task, void **out);
void StationaryGun_Move(StatGun *o, MoveMsg *m);
void StationaryGun_Detect(StatGun *o, DetectMsg *m);
void StationaryGun_Fire(StatGun *o, char fire);
void StationaryGun_Aim(StatGun *o);
void StationaryGun_Clamp(StatGun *o);
void StationaryGun_UpdateCarrier(StatGun *o);

static float StationaryGun_Radius(StatGun *o)
{
    return VirModel_GetRadius(o->obj.model) * max_scale(&o->obj);
}

void StationaryGun_CreateHandler(StatGun *o)
{
    float radius;

    QTaskList_Add(Game_GetTaskList0(), o);
    RigidDynCubeObj_Init(Game_GetWorld(), o, NULL);
    memset(&o->basePos, 0, sizeof(o->basePos));
    radius = StationaryGun_Radius(o);
    DynCube_Add(Game_GetWorld(), o, &o->basePos, radius, 1);
    memset(&o->baseMatrix, 0, sizeof(o->baseMatrix));
    o->baseMatrix.m[0] = 1.0f;
    o->baseMatrix.m[4] = 1.0f;
    o->baseMatrix.m[8] = 1.0f;
    o->obj.matrix = o->baseMatrix;
    o->outPitch = 0.0f;
    o->outYaw = 0.0f;
    o->pitch = 0.0f;
    o->yaw = 0.0f;
    o->recoilPitch = 0.0f;
    o->recoilYaw = 0.0f;
    o->user = NULL;
    o->lastTick = -1;
    o->used = 0;
    o->model[0] = 0;
    crt_sprintf(o->weapon, "WEAPON_ID_M2HB");
    o->ammo = -1;
    o->maxPitch = 25.0f;
    o->minPitch = -25.0f;
    o->yawRange = 90.0f;
}
HOOK(0x473D30, StationaryGun_CreateHandler);

void StationaryGun_Open(void)
{
    StatGun g;

    gStatGunType = TaskType_New("StationaryGun", sizeof(StatGun), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gStatGunType, 0, StationaryGun_CreateHandler);
    TaskType_SetHandler2(gStatGunType, 1, StationaryGun_RunHandler);
    TaskType_SetHandler2(gStatGunType, 2, StationaryGun_DeleteHandler);
    TaskType_SetHandler(gStatGunType, Param_GetChangedEvent(), StationaryGun_Changed);
    TaskType_SetHandler(gStatGunType, Game_GetEvent2(), StationaryGun_UseQuery);
    TaskType_SetHandler(gStatGunType, Game_GetEvent3(), StationaryGun_GetUseKind);
    TaskType_SetHandler(gStatGunType, Game_GetEvent4(), StationaryGun_Attach);
    TaskType_SetHandler(gStatGunType, Game_GetEvent6(), StationaryGun_Event6);
    TaskType_SetHandler(gStatGunType, Game_GetEvent14(), StationaryGun_Event14);
    TaskType_SetHandler(gStatGunType, Param_GetTableEvent(), StationaryGun_GetParams);
    TaskType_SetHandler(gStatGunType, RigidDynCubeObj_GetEvent4(), StationaryGun_Move);
    TaskType_SetHandler(gStatGunType, 6, TASK_NOTHING);
    TaskType_SetHandler(gStatGunType, AIDetection_GetEvent0(), StationaryGun_Detect);
    gStatGunAmmoRange[0] = 0;
    gStatGunAmmoRange[1] = 100000;
    gStatGunParams = ParamTable_New(8);
    ParamTable_Add(gStatGunParams, &g, &g.basePos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, &g.baseMatrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, g.model, Param_String16(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, g.weapon, Param_String32(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, &g.maxPitch, Param_Degrees(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, &g.minPitch, Param_Degrees(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, &g.yawRange, Param_Degrees(), 7, NULL);
    ParamTable_Add(gStatGunParams, &g, &g.ammo, Param_Int32(), 7, gStatGunAmmoRange);
}
HOOK(0x473ED0, StationaryGun_Open);

/* once per tick: turns with the user's input (or back to rest), fires, aims */
void StationaryGun_RunHandler(StatGun *o)
{
    Human *u;
    long double lim, p;
    char fire;

    if (o->lastTick == Flow_GetFrames())
        return;
    o->lastTick = Flow_GetFrames();
    u = o->user;
    if (u) {
        o->pitch = u->input[1] * kPi + o->pitch;
        o->yaw = -(u->input[0] * kPi) + o->yaw;
        StationaryGun_Clamp(o);
        fire = u->input[6] != kZero;
        o->outPitch = o->pitch + o->recoilPitch;
        o->outYaw = o->yaw + o->recoilYaw;
        o->recoilPitch = o->recoilPitch * kRecoilFade;
        o->recoilYaw = o->recoilYaw * kRecoilFade;
        StationaryGun_Fire(o, fire);
    } else {
        lim = o->minPitch * kDegToRad;
        if (lim > o->pitch) {
            p = o->pitch + kTurnStep;
            o->pitch = p;
            if (p > lim)
                o->pitch = lim;
        } else if (lim < o->pitch) {
            p = o->pitch - kTurnStep;
            o->pitch = p;
            if (p < lim)
                o->pitch = lim;
        }
        StationaryGun_Fire(o, 0);
    }
    StationaryGun_Aim(o);
    BROADCAST(&o->obj.task, 1);
}
HOOK(0x474190, StationaryGun_RunHandler);

/* passes the fire button to the mounted weapon and takes its recoil */
void StationaryGun_Fire(StatGun *o, char fire)
{
    Task *w = WeaponSlots_Get(o->slots, 0);
    WeaponUseMsg msg;
    static const float kOne = 1.0f;

    if (!w)
        return;
    msg.fire = fire;
    msg.shot = 0;
    msg.eye = NULL;
    msg.view = NULL;
    msg.vel = NULL;
    msg.unk10 = 0;
    msg.recoilPitch = 0.0f;
    msg.recoilYaw = 0.0f;
    if (o->user)
        MOV32(msg.unk28, o->user->unk4D4);
    else
        MOV32(msg.unk28, kOne);
    msg.h = o->user;
    HANDLER(Human_GetEvent0(), w->type)(w, &msg);
    MOV32(o->recoilPitch, msg.recoilPitch);
    MOV32(o->recoilYaw, msg.recoilYaw);
}
HOOK(0x474360, StationaryGun_Fire);

/* turns the gun to its pitch and yaw, places the weapon at the muzzle and
   puts the user's camera behind it */
void StationaryGun_Aim(StatGun *o)
{
    static const int kLast[9] = {1, 0, 0, 0, 0, 0, 0, 1, 1};
    Task *w = WeaponSlots_Get(o->slots, 0);
    float *n = o->baseMatrix.m;
    float *m = o->obj.matrix.m;
    void **model;
    Matrix t, e;
    Vec3d p;
    Human *u;
    long double dx;
    float dy, dz;
    int i, idx = 1;

    if (!w)
        return;
    Matrix_SetRotZ(&o->obj.matrix, o->outYaw);
    for (i = 0; i < 9; i++)
        t.m[i] = mat_elem(n, i / 3, m, i % 3, kLast[i]);
    t.count = o->baseMatrix.count + o->obj.matrix.count + 1;
    o->obj.matrix = t;
    model = o->obj.model;
    if (model && *(short *)(*(char **)(*(char **)((char *)*model + 0xC) + 0xC) + 0x7C) < 1)
        idx = 0;
    Model_GetPoint(&p, o, idx);
    p.x = p.x + o->obj.pos.x;
    p.y = p.y + o->obj.pos.y;
    p.z = p.z + o->obj.pos.z;
    Matrix_SetEuler(&e, o->outPitch, 0.0f, 0x1.921fb6p+1f);
    for (i = 0; i < 9; i++)
        t.m[i] = mat_elem(m, i / 3, e.m, i % 3, 1);
    t.count = o->obj.matrix.count + e.count + 1;
    e = t;
    Weapon_Move(w, &p, &e);
    o->muzzlePos = p;
    o->muzzleMatrix = e;
    u = o->user;
    if (!u)
        return;
    dx = e.m[2] * kEyeUp - e.m[1] * kEyeBack;
    dy = e.m[5] * kEyeUp - e.m[4] * kEyeBack;
    dz = e.m[8] * kEyeUp - e.m[7] * kEyeBack;
    p.x = dx + p.x;
    p.y = dy + p.y;
    p.z = dz + p.z;
    HumanView_SetCamera(u->view, &p, &e);
}
HOOK(0x474420, StationaryGun_Aim);

/* keeps pitch and yaw inside the limits (yaw only when it is under 360) */
void StationaryGun_Clamp(StatGun *o)
{
    long double lim, y;
    float yf;

    lim = o->minPitch * kDegToRad;
    if (o->pitch < lim) {
        o->pitch = lim;
    } else {
        lim = o->maxPitch * kDegToRad;
        if (o->pitch > lim)
            o->pitch = lim;
    }
    y = o->yawRange * kDegToRad;
    yf = y;
    if (!(y < kTwoPiD))
        return;
    lim = -o->yawRange * kDegToRad;
    if (o->yaw < lim) {
        o->yaw = lim;
        return;
    }
    if (o->yaw > yf)
        MOV32(o->yaw, yf);
}
HOOK(0x4748B0, StationaryGun_Clamp);

void StationaryGun_DeleteHandler(StatGun *o)
{
    QTaskList_Remove(Game_GetTaskList0(), o);
    DynCube_Remove(o->obj.room, o);
}
HOOK(0x474960, StationaryGun_DeleteHandler);

/* the script parameters changed: reload the model, reset and remount the weapon */
void StationaryGun_Changed(StatGun *o)
{
    float radius;
    int id;

    RigidDynCubeObj_SetModel(o, VirModel_Find(o->model));
    radius = StationaryGun_Radius(o);
    DynCube_Move(Game_GetWorld(), o, &o->basePos, radius);
    o->obj.matrix = o->baseMatrix;
    o->outPitch = 0.0f;
    o->outYaw = 0.0f;
    o->pitch = 0.0f;
    o->yaw = 0.0f;
    o->recoilPitch = 0.0f;
    o->recoilYaw = 0.0f;
    o->user = NULL;
    o->lastTick = -1;
    o->used = 0;
    WeaponSlots_Init(o->slots);
    Weapon_DeleteAll(o);
    Mem_PushMode(1);
    id = Script_EvalExpr(o->weapon);
    Mem_PopMode();
    if (WeaponType_Find(id)) {
        Gun_New(o, 0, id, o->ammo, 0);
        WeaponSlots_SelectNext(o, o->slots, 0);
        StationaryGun_Aim(o);
    }
    StationaryGun_UpdateCarrier(o);
}
HOOK(0x474980, StationaryGun_Changed);

/* stores the base position and matrix in the frame of the object carrying the gun */
void StationaryGun_UpdateCarrier(StatGun *o)
{
    static const int kLast[9] = {0, 0, 0, 1, 1, 1, 1, 0, 0};
    Task *c = Task_FindAncestor(o, RigidDynCubeObj_GetType());
    RigidObj *a = (RigidObj *)c;
    float *n = o->baseMatrix.m;
    float *am;
    long double dz;
    Vec3d r;
    Matrix t;
    int i;

    if (!c) {
        memset(&o->relPos, 0, sizeof(o->relPos));
        memset(&o->relMatrix, 0, sizeof(o->relMatrix));
        o->relMatrix.m[0] = 1.0f;
        o->relMatrix.m[4] = 1.0f;
        o->relMatrix.m[8] = 1.0f;
        return;
    }
    am = a->matrix.m;
    o->relPos.x = o->basePos.x - a->pos.x;
    o->relPos.y = o->basePos.y - a->pos.y;
    o->relPos.z = dz = o->basePos.z - a->pos.z;
    r.x = am[6] * dz + am[0] * o->relPos.x + am[3] * o->relPos.y;
    r.y = am[1] * o->relPos.x + am[7] * dz + am[4] * o->relPos.y;
    r.z = am[2] * o->relPos.x + am[8] * dz + am[5] * o->relPos.y;
    o->relPos = r;
    for (i = 0; i < 3; i++) {
        MOV32(o->relMatrix.m[i * 3], am[i]);
        MOV32(o->relMatrix.m[i * 3 + 1], am[i + 3]);
        MOV32(o->relMatrix.m[i * 3 + 2], am[i + 6]);
    }
    o->relMatrix.count = a->matrix.count;
    for (i = 0; i < 9; i++)
        t.m[i] = mat_elem(n, i / 3, o->relMatrix.m, i % 3, kLast[i]);
    t.count = o->relMatrix.count + o->baseMatrix.count + 1;
    o->relMatrix = t;
}
HOOK(0x474AE0, StationaryGun_UpdateCarrier);

/* offers the gun to a human looking for something to use, if it faces it */
void StationaryGun_UseQuery(StatGun *o, UseQuery *q)
{
    long double px, dx, dy, dz;
    Vec3d p;

    if (o->used)
        return;
    Model_GetPoint(&p, o, 0);
    px = p.x + o->obj.pos.x;
    p.x = px;
    p.y = p.y + o->obj.pos.y;
    p.z = p.z + o->obj.pos.z;
    dx = q->pos.x - px;
    dy = q->pos.y - p.y;
    dz = q->pos.z - p.z;
    if (o->obj.matrix.m[7] * dz + o->obj.matrix.m[4] * dy + o->obj.matrix.m[1] * dx < kZeroD)
        return;
    UseQuery_Consider(q, o, &p);
}
HOOK(0x474E30, StationaryGun_UseQuery);

void StationaryGun_GetUseKind(void *task, int *out)
{
    *out = 6;
}
HOOK(0x474ED0, StationaryGun_GetUseKind);

void StationaryGun_Attach(StatGun *o, AttachMsg *m)
{
    if (!m->unk44)
        o->user = m->h;
    o->used = 1;
}
HOOK(0x474EE0, StationaryGun_Attach);

void StationaryGun_Event6(void *task, Human **msg)
{
    Human *h = *msg;

    StatusScreen_DrawActionIcon(h, h->unk434);
}
HOOK(0x474F10, StationaryGun_Event6);

void StationaryGun_Event14(StatGun *o, int *out)
{
    out[2] = 0;
    out[1] = 0;
    out[0] = 0;
    HANDLER(1, o->obj.task.type)(o);
}
HOOK(0x474F30, StationaryGun_Event14);

void StationaryGun_GetParams(void *task, void **out)
{
    *out = gStatGunParams;
}
HOOK(0x474F50, StationaryGun_GetParams);

/* the carrier moved: move the base along and tell the children */
void StationaryGun_Move(StatGun *o, MoveMsg *m)
{
    float *f = m->matrix->m;
    MoveMsg out;
    Vec3d p;
    Matrix t;
    float radius;
    int i;

    p.x = f[2] * o->relPos.z + f[1] * o->relPos.y + f[0] * o->relPos.x;
    p.y = f[5] * o->relPos.z + f[4] * o->relPos.y + f[3] * o->relPos.x;
    p.z = f[8] * o->relPos.z + f[7] * o->relPos.y + f[6] * o->relPos.x;
    o->basePos = p;
    o->basePos.x = m->pos->x + o->basePos.x;
    o->basePos.y = m->pos->y + o->basePos.y;
    o->basePos.z = m->pos->z + o->basePos.z;
    for (i = 0; i < 9; i++)
        t.m[i] = mat_elem(f, i / 3, o->relMatrix.m, i % 3, 1);
    t.count = m->matrix->count + o->relMatrix.count + 1;
    o->baseMatrix = t;
    radius = StationaryGun_Radius(o);
    DynCube_Move(o->obj.room, o, &o->basePos, radius);
    o->obj.matrix = o->baseMatrix;
    StationaryGun_Aim(o);
    out.pos = &o->basePos;
    out.matrix = &o->baseMatrix;
    BROADCAST(&o->obj.task, RigidDynCubeObj_GetEvent4(), &out);
}
HOOK(0x474F60, StationaryGun_Move);

/* the gun never blocks the view between its user and someone else */
void StationaryGun_Detect(StatGun *o, DetectMsg *m)
{
    if (m->a != o && m->a != o->user && m->b != o && m->b != o->user)
        m->visible = 1;
    else
        m->visible = 0;
}
HOOK(0x4752B0, StationaryGun_Detect);

void StationaryGun_Close(void)
{
    QTaskList_Free(gStatGunParams);
    TaskType_Free(gStatGunType);
}
HOOK(0x4752E0, StationaryGun_Close);

uint16_t StationaryGun_GetType(void)
{
    return gStatGunType;
}
HOOK(0x475300, StationaryGun_GetType);

void StationaryGun_GetMuzzle(StatGun *o, Vec3d *pos, Matrix *m)
{
    *m = o->muzzleMatrix;
    *pos = o->muzzlePos;
}
HOOK(0x475310, StationaryGun_GetMuzzle);

void StationaryGun_Release(StatGun *o)
{
    HumanView_ClearCamOverride(o->user->view);
    o->user = NULL;
    o->used = 0;
}
HOOK(0x475340, StationaryGun_Release);

char StationaryGun_IsUsed(StatGun *o)
{
    return o->used;
}
HOOK(0x475370, StationaryGun_IsUsed);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5335A8) float kRecoilFade INIT(0.9f);
ALIAS(kNineTenths, kRecoilFade, 0);
DATA(533728) double kTwoPiD INIT(6.283185307179589);
DATA(53387C) float kTurnStep INIT(0.08726647f);
ALIAS(kTrackDead, kTurnStep, 0);
DATA(534134) float kEyeUp INIT(819.2f);
PAD(igb, 5BE380, 5BE380, 0x8);
ALIAS(gStatGunAmmoRange, pad_5BE380, 0);
BSS(5BE380) uint16_t gStatGunType;
PAD(igb, 5BE380, 5BE38A, 0x2);
BSS(5BE380) void *gStatGunParams;
