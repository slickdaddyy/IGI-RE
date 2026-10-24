#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "comprun/comprun.h"

FN_EXTERN(Car_CollFilter);

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x464250, int, Human_GetMaskType, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x467DF0, void, ComputerObject_DrawSprite, (void *arg));
ORIG(0x46B4D0, char *, ComputerRes_Get, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489630, uint8_t, AIDetection_GetEvent2, (void));
ORIG(0x4896C0, void, AIDetector_Init, (void *d, int team, int a, int mask, int b, int c));
ORIG(0x489750, void, AIDetector_Update, (void *d, void *view, void *owner, int team));
ORIG(0x489B90, void, AIDetection_RemoveRefs, (void *d));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *obj));
ORIG(0x4F0F30, void *, Param_Degrees, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define GunHolder_Handler42FC50 (FN(0x42FC50, Car_CollFilter))

/* where the detection looks from */
typedef struct DetectView {
    Vec3d pos;
    Matrix matrix;
    double angleX; /* 0x40 */
    double angleY;
    double range;
    char unk58[0x18];
    char unk70;
    char unk71[7];
} DetectView;

/* "AIStationaryGunHolder" (0x26A0 bytes), a fixed gun that spots enemies */
typedef struct GunHolder {
    RigidObj obj;
    DetectView view;      /* 0xF0 */
    char detect[0x2430];  /* 0x168, AIDetection */
    Vec3d pos;            /* 0x2598 */
    Matrix matrix;        /* 0x25B0 */
    char model[16];       /* 0x25D8 */
    float angleX;         /* 0x25E8 */
    float angleY;
    float range;
    VarString activeExpr; /* 0x25F4 */
    VarString teamExpr;   /* 0x2644 */
    int team;             /* 0x2694 */
    char active;
    char unk2699[7];
} GunHolder;

/* what the move event of the attached children gets */
typedef struct MoveInfo {
    Vec3d *pos;
    Matrix *matrix;
    void *owner;
} MoveInfo;

/* what the computer map draw event gets */
typedef struct DrawArg {
    char unk0[0xC];
    Vec3d *pos;
    Matrix *matrix;
    float radius;
    char *sprite;
} DrawArg;

_Static_assert(sizeof(DetectView) == 0x78, "DetectView");
_Static_assert(offsetof(GunHolder, pos) == 0x2598, "pos");
_Static_assert(offsetof(GunHolder, activeExpr) == 0x25F4, "activeExpr");
_Static_assert(offsetof(GunHolder, team) == 0x2694, "team");
_Static_assert(sizeof(GunHolder) == 0x26A0, "GunHolder");

extern uint16_t gGunHolderType;
extern float gGunHolderRange[];
extern void *gGunHolderParams;
extern void *gGunHolderList;

static const float kDegToRad = 0x1.1df46ap-6f;
static const float kRangeScale = 4096.0f;

static long double radius(RigidObj *o)
{
    float s = max_scale(o);

    return VirModel_GetRadius(o->model) * s;
}

void GunHolder_Create(GunHolder *t)
{
    static const Matrix ident = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
    long double r;

    RigidDynCubeObj_Init(Game_GetWorld(), t, NULL);
    t->pos.x = 0.0;
    t->pos.y = 0.0;
    t->pos.z = 0.0;
    r = radius(&t->obj);
    DynCube_Add(Game_GetWorld(), t, &t->pos, r, 1);
    t->matrix = ident;
    t->obj.matrix = t->matrix;
    t->angleX = 360.0f;
    t->angleY = 360.0f;
    t->range = 10.0f;
    t->model[0] = 0;
    t->team = 1;
    VarString_Init(&t->activeExpr);
    VarString_Init(&t->teamExpr);
    t->active = 0;
}
HOOK(0x46E210, GunHolder_Create);

void GunHolder_Run(GunHolder *t);
void GunHolder_Destroy(GunHolder *t);
void GunHolder_Changed(GunHolder *t);
void GunHolder_GetParams(void *t, void **out);
void GunHolder_GetDetection(GunHolder *t, void **out);
void GunHolder_Draw(GunHolder *t, DrawArg *a);

void GunHolder_Init(void)
{
    GunHolder p;

    gGunHolderType = TaskType_New("AIStationaryGunHolder", sizeof(GunHolder), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gGunHolderType, 0, GunHolder_Create);
    TaskType_SetHandler2(gGunHolderType, 1, GunHolder_Run);
    TaskType_SetHandler2(gGunHolderType, 2, GunHolder_Destroy);
    TaskType_SetHandler(gGunHolderType, Param_GetChangedEvent(), GunHolder_Changed);
    TaskType_SetHandler(gGunHolderType, Param_GetTableEvent(), GunHolder_GetParams);
    TaskType_SetHandler(gGunHolderType, AIDetection_GetEvent2(), GunHolder_GetDetection);
    TaskType_SetHandler(gGunHolderType, AIDetection_GetEvent0(), GunHolder_Handler42FC50);
    TaskType_SetHandler(gGunHolderType, ComputerObject_GetEvent0(), GunHolder_Draw);
    gGunHolderRange[0] = 0.0f;
    gGunHolderRange[1] = 100.0f;
    gGunHolderParams = ParamTable_New(8);
    ParamTable_Add(gGunHolderParams, &p, &p.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gGunHolderParams, &p, &p.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gGunHolderParams, &p, p.model, Param_String16(), 7, NULL);
    ParamTable_Add(gGunHolderParams, &p, &p.angleX, Param_Degrees(), 7, NULL);
    ParamTable_Add(gGunHolderParams, &p, &p.angleY, Param_Degrees(), 7, NULL);
    ParamTable_Add(gGunHolderParams, &p, &p.range, Param_Real32(), 7, gGunHolderRange);
    ParamTable_Add(gGunHolderParams, &p, &p.activeExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gGunHolderParams, &p, &p.teamExpr, Param_VarString(), 7, NULL);
    gGunHolderList = QTaskList_New(16);
}
HOOK(0x46E370, GunHolder_Init);

void GunHolder_Run(GunHolder *t)
{
    char was = t->active;

    if (VarString_HasExpr(&t->teamExpr))
        t->team = round_int(VarString_EvalReal(&t->teamExpr, t));
    if (VarString_HasExpr(&t->activeExpr))
        t->active = (char)round_int(VarString_EvalReal(&t->activeExpr, t));
    if (t->active) {
        t->view.pos = t->obj.pos;
        t->view.matrix = t->obj.matrix;
        t->view.unk70 = 0;
        AIDetector_Update(t->detect, &t->view, t, t->team);
    } else if (was) {
        AIDetection_RemoveRefs(t->detect);
        AIDetector_Init(t->detect, 1, 4, Human_GetMaskType(), 4, 8);
    }
    CALL_CHILDREN_RUN(t);
    if (t->obj.child) {
        MoveInfo info;

        ((void (*)(void *))gTaskHandlers[1][t->obj.child->type])(t->obj.child);
        info.pos = &t->obj.pos;
        info.matrix = &t->obj.matrix;
        info.owner = t;
        CALL_CHILDREN_EV(t->obj.child, MagicObj_GetMoveEvent(), &info);
    }
}
HOOK(0x46E5B0, GunHolder_Run);

void GunHolder_Destroy(GunHolder *t)
{
    VarString_Free(&t->activeExpr);
    VarString_Free(&t->teamExpr);
    if (t->obj.child)
        Task_Delete(t->obj.child);
    DynCube_Remove(t->obj.world, t);
    AIDetection_RemoveRefs(t->detect);
}
HOOK(0x46E7A0, GunHolder_Destroy);

void GunHolder_Changed(GunHolder *t)
{
    Task *old;
    long double r;

    RigidDynCubeObj_SetModel(t, VirModel_Find(t->model));
    r = radius(&t->obj);
    DynCube_Move(Game_GetWorld(), t, &t->pos, r);
    old = t->obj.child;
    t->obj.matrix = t->matrix;
    if (old)
        Task_Delete(old);
    t->obj.child = MagicObjContainer_New(NULL, t);
    RigidDynCubeObj_CreateMagicObjs(t->obj.child, t, t);
    CALL_CHILDREN_EV(t->obj.child, Param_GetChangedEvent(), NULL);
    VarString_Bind(&t->activeExpr, t);
    VarString_Bind(&t->teamExpr, t);
    t->view.angleX = t->angleX * kDegToRad;
    t->view.angleY = t->angleY * kDegToRad;
    t->view.range = t->range * kRangeScale;
    AIDetector_Init(t->detect, 1, 4, Human_GetMaskType(), 4, 8);
}
HOOK(0x46E7F0, GunHolder_Changed);

void GunHolder_GetParams(void *t, void **out)
{
    (void)t;
    *out = gGunHolderParams;
}
HOOK(0x46E9D0, GunHolder_GetParams);

void GunHolder_GetDetection(GunHolder *t, void **out)
{
    *out = t->detect;
}
HOOK(0x46E9E0, GunHolder_GetDetection);

void GunHolder_Draw(GunHolder *t, DrawArg *a)
{
    a->pos = &t->obj.pos;
    a->matrix = &t->obj.matrix;
    a->radius = radius(&t->obj);
    a->sprite = ComputerRes_Get() + 0x468;
    ComputerObject_DrawSprite(a);
    a->sprite = NULL;
}
HOOK(0x46E9F0, GunHolder_Draw);

void GunHolder_Close(void)
{
    QTaskList_Free(gGunHolderParams);
    TaskType_Free(gGunHolderType);
    QTaskList_Free(gGunHolderList);
}
HOOK(0x46EAA0, GunHolder_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BE318) uint16_t gGunHolderType;
PAD(igb, 5BE318, 5BE31A, 0x6);
PAD(igb, 5BE318, 5BE320, 0x8);
ALIAS(gGunHolderRange, pad_5BE320, 0);
BSS(5BE318) void *gGunHolderParams;
PAD(igb, 5BE318, 5BE32C, 0x4);
BSS(5BE318) void *gGunHolderList;
