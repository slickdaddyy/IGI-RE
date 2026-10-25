#pragma GCC optimize("excess-precision=standard", "rounding-math")
#include <string.h>

#include "hook.h"
#include "editcam/editcam.h"
#include "magicobj/magicobjtypes.h"
#include "editcam/editcamtypes.h"

FN_EXTERN(SubLevel_Run);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1CD0, void, ParamEnum_Init, (void *e));
ORIG(0x4F1D80, void, ParamEnum_Free, (void *e));
ORIG(0x4F1DB0, void, ParamEnum_AddConstants, (void *e, const char *pattern));
ORIG(0x4F1E20, void *, Param_EnumString32, (void));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

#define SubLevel_Run (FN(0x488700, SubLevel_Run))

extern uint16_t gEditCameraType;
extern char gEditCameraFilterEnum[];
extern void *gEditCameraParams;
extern float gEditCameraLevelRange[];
extern float gEditCameraShakeRange[];
extern void *gEditCameraModel;

static const float kMinFov = 0.001f;
static const float kMaxFov = 3.0f;
static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kMinusOne = -1.0f;
static const double kOneD = 1.0;

static inline int x87_eq(long double a, long double b)
{
    return !(a < b || a > b);
}

void EditCamera_Create(EditCamera *ec);
void EditCamera_Delete(EditCamera *ec);
void EditCamera_GetParams(EditCamera *ec, void **out);
void EditCamera_Changed(EditCamera *ec, char *arg);
void *EditCamera_GetFilterEnum(void);

void EditCamera_Init(void)
{
    EditCamera t;
    uint16_t type;

    ParamEnum_Init(gEditCameraFilterEnum);
    ParamEnum_AddConstants(gEditCameraFilterEnum, "CAMERAFILTER_TYPE_.*");
    gEditCameraModel = VirModel_Add("1717_01_1");
    type = TaskType_New("EditCamera", sizeof(EditCamera), EditorRigidObj_GetType(), 0, 0);
    gEditCameraType = type;
    TaskType_SetHandler(type, 0, EditCamera_Create);
    TaskType_SetHandler2(gEditCameraType, 2, EditCamera_Delete);
    TaskType_SetHandler(gEditCameraType, Param_GetTableEvent(), EditCamera_GetParams);
    TaskType_SetHandler(gEditCameraType, Param_GetChangedEvent(), EditCamera_Changed);
    TaskType_SetHandler2(gEditCameraType, 1, SubLevel_Run);
    TaskType_SetHandler(gEditCameraType, 4, NULL);
    gEditCameraParams = ParamTable_New(17);
    gEditCameraLevelRange[0] = 0.0f;
    gEditCameraLevelRange[1] = 1.0f;
    gEditCameraShakeRange[0] = 0.0f;
    gEditCameraShakeRange[1] = 1.0f;
    ParamTable_Add(gEditCameraParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.angles[0], Param_Angle(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.angles[1], Param_Angle(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.angles[2], Param_Angle(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.fov, Param_Real32(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.duration, Param_Real32(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.posAttach, Param_Int32(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.posFollow, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.rotAttach, Param_Int32(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.rotFollow, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.smooth, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.unk98, Param_Real32(), 7, NULL);
    {
        void *e = EditCamera_GetFilterEnum();

        ParamTable_Add(gEditCameraParams, &t, t.filterName, Param_EnumString32(), 7, e);
    }
    ParamTable_Add(gEditCameraParams, &t, &t.filterLevel, Param_Real32(), 7, gEditCameraLevelRange);
    ParamTable_Add(gEditCameraParams, &t, t.color, Param_RGB(), 7, NULL);
    ParamTable_Add(gEditCameraParams, &t, &t.shake, Param_Real32(), 7, gEditCameraShakeRange);
    Script_RegisterConstant("TASKTYPE_EDITCAMERA", gEditCameraType);
}
HOOK(0x507F10, EditCamera_Init);

void EditCamera_Create(EditCamera *ec)
{
    ec->smooth = 1;
    ec->posAttach = -1;
    ec->rotAttach = -1;
    ec->posFollow = 1;
    ec->rotFollow = 1;
    ec->pos.x = ec->pos.y = ec->pos.z = 0;
    ec->unkA8 = 0;
    ec->angles[0] = ec->angles[1] = ec->angles[2] = 0;
    ec->duration = 0;
    ec->fov = 1.0f;
    ec->unk98 = -1.0f;
    ec->refs[0] = ec->refs[1] = ec->refs[2] = NULL;
    ec->filterLevel = 0.5f;
    ec->color[0] = ec->color[1] = ec->color[2] = 0;
    ec->shake = 0;
    memcpy(ec->filterName, "CAMERAFILTER_TYPE_NONE", 23);
    ec->filter = 0;
}
HOOK(0x508290, EditCamera_Create);

void EditCamera_Delete(EditCamera *ec)
{
    int i;

    if (Task_GetId(ec) >= 0) {
        void (*fn)(void *) = (void (*)(void *))HANDLER(Level_GetDeactivateEvent(), ec->task.type);

        if (fn)
            fn(ec);
    }
    for (i = 0; i < 3; i++) {
        if (ec->refs[i]) {
            if (*ec->refs[i])
                Task_Delete(*ec->refs[i]);
            TaskRef_Remove(ec->refs[i]);
        }
    }
}
HOOK(0x508350, EditCamera_Delete);

void EditCamera_GetParams(EditCamera *ec, void **out)
{
    *out = gEditCameraParams;
}
HOOK(0x508400, EditCamera_GetParams);

/* clamps the fov, drops a look-at of itself and takes the offset and
   rotation of the position target */
void EditCamera_Changed(EditCamera *ec, char *arg)
{
    Task *rot, *pos;
    ObjInfo info;

    ec->filter = Script_EvalExpr(ec->filterName);
    if (!(ec->fov >= kMinFov))
        ec->fov = kMinFov;
    else if (!(ec->fov <= kMaxFov))
        ec->fov = kMaxFov;
    rot = Task_FindById(ec->rotAttach);
    pos = Task_FindById(ec->posAttach);
    if (rot == &ec->task)
        ec->rotAttach = -1;
    memset(&ec->attachPos, 0, sizeof(ec->attachPos));
    memset(&ec->attachRot, 0, sizeof(ec->attachRot));
    ec->attachRot.m[0] = 1.0f;
    ec->attachRot.m[4] = 1.0f;
    ec->attachRot.m[8] = 1.0f;
    if (pos) {
        if (!HANDLER(7, pos->type)) {
            ec->posAttach = -1;
        } else {
            HANDLER(Param_GetChangedEvent(), pos->type)(pos, arg);
            info.name = NULL;
            HANDLER(7, pos->type)(pos, &info);
            memcpy(&ec->attachPos, &info.pos, sizeof(ec->attachPos));
            memcpy(&ec->attachRot, &info.matrix, sizeof(ec->attachRot));
        }
    }
    if (!arg || !*arg)
        CALL_CHILDREN(ec, Param_GetChangedEvent(), NULL);
}
HOOK(0x508410, EditCamera_Changed);

void EditCamera_Shutdown(void)
{
    QTaskList_Free(gEditCameraParams);
    TaskType_Free(gEditCameraType);
    ParamEnum_Free(gEditCameraFilterEnum);
}
HOOK(0x5085E0, EditCamera_Shutdown);

uint16_t EditCamera_GetType(void)
{
    return gEditCameraType;
}
HOOK(0x508610, EditCamera_GetType);

/* turns rot to look from pos at the target task, z up */
void EditCamera_AttachRotation(Vec3d *pos, Matrix *rot, int target)
{
    Task *t = Task_FindById(target);
    ObjInfo info;
    long double dx, inv, b2l;
    double ty, tz;
    float dy, dz, a0, a1, a2, b0, b1, b2;
    /* a real multiply: fchs would flip the sign of a NaN */
    float neg = *(volatile const float *)&kMinusOne;

    if (!t || !HANDLER(7, t->type))
        return;
    info.name = NULL;
    HANDLER(7, t->type)(t, &info);
    dx = info.pos.x - pos->x;
    ty = info.pos.y - pos->y;
    tz = info.pos.z - pos->z;
    dy = ty;
    dz = tz;
    if (x87_eq(dx, kZero) && x87_eq(dy, kZero) && x87_eq(dz, kZero)) {
        dx = kOne;
    } else {
        inv = kOneD / __builtin_sqrtl(dx * dx + dy * dy + dz * dz);
        dx = dx * inv;
        dy = dy * inv;
        dz = dz * inv;
    }
    a0 = dy - kZero;
    a1 = -(dx - kZero);
    a2 = 0;
    b0 = a1 * dz - a2 * dy;
    b1 = -(a0 * dz - a2 * dx);
    b2 = a0 * dy - a1 * dx;
    if (!x87_eq(a0, kZero) || !x87_eq(a1, kZero) || !x87_eq(a2, kZero)) {
        inv = kOneD / __builtin_sqrtl(a1 * a1 + a2 * a2 + a0 * a0);
        a0 = a0 * inv;
        a1 = a1 * inv;
        a2 = a2 * inv;
    }
    if (!x87_eq(b0, kZero) || !x87_eq(b1, kZero) || !x87_eq(b2, kZero)) {
        inv = kOneD / __builtin_sqrtl(b0 * b0 + b1 * b1 + b2 * b2);
        b0 = b0 * inv;
        b1 = b1 * inv;
        b2l = inv * b2;
    } else {
        b2l = b2;
    }
    rot->m[0] = a0;
    rot->m[3] = a1;
    rot->m[6] = a2;
    rot->m[1] = b0 * neg;
    rot->m[4] = b1 * neg;
    rot->m[7] = b2l * neg;
    rot->m[2] = dx;
    rot->m[5] = dy;
    rot->m[8] = dz;
}
HOOK(0x508620, EditCamera_AttachRotation);

/* moves pos and rot from the frame of off/offRot into the frame of the
   target task */
void EditCamera_AttachPosition(Vec3d *pos, Matrix *rot, Vec3d *off, Matrix *offRot, int target)
{
    Task *t = Task_FindById(target);
    ObjInfo info;
    const float *a = offRot->m;
    const float *m = info.matrix.m;
    float *b = rot->m;
    float c[9];
    int c9;
    long double dz;
    Vec3d v;

    if (!t || !HANDLER(7, t->type))
        return;
    info.name = NULL;
    HANDLER(7, t->type)(t, &info);
    pos->x = pos->x - off->x;
    pos->y = pos->y - off->y;
    dz = pos->z - off->z;
    pos->z = dz;
    v.x = a[0] * pos->x + a[6] * dz + a[3] * pos->y;
    v.y = a[7] * dz + a[1] * pos->x + a[4] * pos->y;
    v.z = a[8] * dz + a[2] * pos->x + a[5] * pos->y;
    *pos = v;
    v.x = m[1] * pos->y + m[0] * pos->x + m[2] * pos->z;
    v.y = m[4] * pos->y + m[3] * pos->x + m[5] * pos->z;
    v.z = m[7] * pos->y + m[6] * pos->x + m[8] * pos->z;
    *pos = v;
    pos->x = info.pos.x + pos->x;
    pos->y = info.pos.y + pos->y;
    pos->z = info.pos.z + pos->z;

    c[0] = a[6] * b[6] + a[3] * b[3] + a[0] * b[0];
    c[1] = a[0] * b[1] + a[6] * b[7] + a[3] * b[4];
    c[2] = a[6] * b[8] + a[3] * b[5] + a[0] * b[2];
    c[3] = a[7] * b[6] + a[4] * b[3] + a[1] * b[0];
    c[4] = a[1] * b[1] + a[7] * b[7] + a[4] * b[4];
    c[5] = a[7] * b[8] + a[4] * b[5] + a[1] * b[2];
    c9 = rot->count + offRot->count + 1;
    c[6] = a[8] * b[6] + a[5] * b[3] + a[2] * b[0];
    c[7] = a[2] * b[1] + a[8] * b[7] + a[5] * b[4];
    c[8] = a[8] * b[8] + a[5] * b[5] + a[2] * b[2];
    memcpy(b, c, sizeof(c));
    rot->count = c9;

    c[0] = m[2] * b[6] + m[1] * b[3] + m[0] * b[0];
    c[1] = m[0] * b[1] + m[2] * b[7] + m[1] * b[4];
    c[2] = m[2] * b[8] + m[1] * b[5] + m[0] * b[2];
    c[3] = m[5] * b[6] + m[4] * b[3] + m[3] * b[0];
    c[4] = m[3] * b[1] + m[5] * b[7] + m[4] * b[4];
    c[5] = m[5] * b[8] + m[4] * b[5] + m[3] * b[2];
    c9 = rot->count + info.matrix.count + 1;
    c[6] = m[8] * b[6] + m[7] * b[3] + m[6] * b[0];
    c[7] = m[6] * b[1] + m[8] * b[7] + m[7] * b[4];
    c[8] = m[8] * b[8] + m[7] * b[5] + m[6] * b[2];
    memcpy(b, c, sizeof(c));
    rot->count = c9;
}
HOOK(0x508940, EditCamera_AttachPosition);

void *EditCamera_GetFilterEnum(void)
{
    return gEditCameraFilterEnum;
}
HOOK(0x508D60, EditCamera_GetFilterEnum);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7B260) uint16_t gEditCameraType;
PAD(igb, A7B260, A7B262, 0x6);
STORE(igb, A7B260, 1) char store_A7B268[16];
ALIAS(gEditCameraFilterEnum, store_A7B268, 0);
BSS(A7B260) void *gEditCameraParams;
PAD(igb, A7B260, A7B27C, 0x4);
STORE(igb, A7B260, 4) float store_A7B280[2];
ALIAS(gEditCameraLevelRange, store_A7B280, 0);
STORE(igb, A7B260, 4) float store_A7B288[2];
ALIAS(gEditCameraShakeRange, store_A7B288, 0);
BSS(A7B260) void *gEditCameraModel;
