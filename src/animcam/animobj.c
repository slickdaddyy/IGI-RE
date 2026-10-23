/* AnimRigidDynCubeObj: a RigidDynCubeObj moved by AnimTask channels
   (position, speed and acceleration in world and object axes, angles). */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "animcam/animcam.h"

/* "AnimRigidDynCubeObj" (0x158 bytes), a RigidDynCubeObj (0xF0) */
typedef struct AnimObj {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x30];
    void *room;         /* 0x68 */
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x4C];
    Vec3d startPos;     /* 0xF0, restored by a reset */
    Matrix startMatrix; /* 0x108 */
    Matrix rot;         /* 0x130, turned by the angle channels */
} AnimObj;

/* the saved state: position then matrix */
typedef struct AnimObjState {
    Vec3d pos;
    Matrix matrix;      /* 0x18 */
} AnimObjState;

typedef struct AnimObjSaveMsg {
    AnimObjState *buf;
    void *unk4;
    Vec3d *pos;
} AnimObjSaveMsg;

_Static_assert(offsetof(AnimObj, startPos) == 0xF0, "startPos");
_Static_assert(sizeof(AnimObj) == 0x158, "AnimObj");

#define ANIMOBJ_STATE_SIZE 0x40

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));

#define gAnimObjType VAR(uint16_t, 0x54F498)
#define gAnimObjInputs ((const AnimInput *)0x54F4A0)

#define ANIMOBJ_INPUTS 22

static const float __attribute__((unused)) kStepScale = 0x1p-10f;
static const double __attribute__((unused)) kPi = 0x1.921fb54442d1bp+1;
static const double __attribute__((unused)) kAngleScale = 0x1p-12;

static float max_scale(const AnimObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

ORIG(0x52A230, void, AnimObj_Control, (AnimObj *o, AnimControlMsg *msg));
void AnimObj_GetInputInfo(AnimObj *o, AnimInputMsg *msg);
void AnimObj_GetStateSize(AnimObj *o, int *out);
void AnimObj_SaveState(AnimObj *o, AnimObjSaveMsg *msg);
void AnimObj_LoadState(AnimObj *o, AnimObjState **msg);

void AnimObj_Open(void)
{
    uint16_t t = TaskType_New("AnimRigidDynCubeObj", sizeof(AnimObj), RigidDynCubeObj_GetType(), 0, 0);

    gAnimObjType = t;
    TaskType_SetHandler2(t, 1, TASK_NOTHING);
    TaskType_SetHandler(gAnimObjType, 0, TASK_NOTHING);
    TaskType_SetHandler2(gAnimObjType, 2, TASK_NOTHING);
    TaskType_SetHandler(gAnimObjType, Vehicle_GetControlEvent(), AnimObj_Control);
    TaskType_SetHandler(gAnimObjType, Vehicle_GetInputInfoEvent(), AnimObj_GetInputInfo);
    TaskType_SetHandler(gAnimObjType, Vehicle_GetStateSizeEvent(), AnimObj_GetStateSize);
    TaskType_SetHandler(gAnimObjType, Vehicle_GetSaveEvent(), AnimObj_SaveState);
    TaskType_SetHandler(gAnimObjType, Vehicle_GetLoadEvent(), AnimObj_LoadState);
}
HOOK(0x52AAF0, AnimObj_Open);

/* not tested yet: written from the disassembly, kept out of the build */
#if 0
/* channels 0-2 position, 3-8 speed and acceleration in world axes, 9-14 in
   object axes, 15-20 turn speed and acceleration, 21 the id of a task to
   move with (none when <= 0). The keys are integrated in place. */
void AnimObj_Control(AnimObj *o, AnimControlMsg *msg)
{
    float *k = msg->keys;
    const float *m, *e, *r;
    float v0, v1, v2, ax, ay, az, scale;
    long double s;
    Matrix rot, x;
    Vec3d p;
    Task *parent;
    int id;

    if (msg->mode == 0) {
        o->matrix = o->startMatrix;
        o->rot = o->startMatrix;
        scale = max_scale(o);
        DynCube_Move(o->room, o, &o->startPos, VirModel_GetRadius(o->model) * scale);
    }
    k = msg->keys;
    s = (long double)k[3] * kStepScale;
    v1 = (long double)k[4] * kStepScale;
    v2 = (long double)k[5] * kStepScale;
    k[3] = (long double)k[6] + k[3];
    k[4] = (long double)k[7] + k[4];
    k[5] = (long double)k[8] + k[5];
    m = o->matrix.m;
    s = s + (long double)m[0] * k[9] * kStepScale;
    v1 = (long double)m[3] * k[9] * kStepScale + v1;
    v2 = (long double)m[6] * k[9] * kStepScale + v2;
    s = s + (long double)m[1] * k[10] * kStepScale;
    v1 = (long double)m[4] * k[10] * kStepScale + v1;
    v2 = (long double)m[7] * k[10] * kStepScale + v2;
    v0 = (long double)m[2] * k[11] * kStepScale + s;
    v1 = (long double)m[5] * k[11] * kStepScale + v1;
    v2 = (long double)m[8] * k[11] * kStepScale + v2;
    k[0] = (long double)v0 + k[0];
    k[1] = (long double)v1 + k[1];
    k[2] = (long double)v2 + k[2];
    k[9] = (long double)k[12] + k[9];
    k[10] = (long double)k[13] + k[10];
    k[11] = (long double)k[14] + k[11];
    k[15] = (long double)k[18] + k[15];
    k[16] = (long double)k[19] + k[16];
    k[17] = (long double)k[20] + k[17];
    az = (long double)k[17] * kStepScale * kPi * kAngleScale;
    ay = (long double)k[16] * kStepScale * kPi * kAngleScale;
    ax = (long double)k[15] * kStepScale * kPi * kAngleScale;
    Matrix_SetEuler(&rot, ax, ay, az);
    e = rot.m;
    id = (int)round_ll(msg->keys[21]);
    if (id <= 0)
        id = -1;
    parent = Task_FindById(id);
    if (parent) {
        const Vec3d *ppos = (const Vec3d *)((char *)parent + 0x20);
        const float *pm;
        long double qx, qy, qz;
        double dx, dy, dz;
        float k0, k1, k2;

        /* other tasks keep their matrix 8 bytes further on */
        if (TaskType_IsA(parent->type, RigidDynCubeObj_GetType()))
            pm = (const float *)((char *)parent + 0x70);
        else
            pm = (const float *)((char *)parent + 0x78);
        r = o->rot.m;
        x.m[0] = e[6] * r[2] + e[0] * r[0] + e[3] * r[1];
        x.m[1] = e[7] * r[2] + e[1] * r[0] + e[4] * r[1];
        x.m[2] = e[8] * r[2] + e[2] * r[0] + e[5] * r[1];
        x.m[3] = e[6] * r[5] + e[0] * r[3] + e[3] * r[4];
        x.m[4] = e[7] * r[5] + e[1] * r[3] + e[4] * r[4];
        x.m[5] = e[8] * r[5] + e[2] * r[3] + e[5] * r[4];
        x.m[6] = e[0] * r[6] + e[3] * r[7] + e[6] * r[8];
        x.m[7] = e[4] * r[7] + e[7] * r[8] + e[1] * r[6];
        x.m[8] = e[5] * r[7] + e[8] * r[8] + e[2] * r[6];
        x.unk24 = o->rot.unk24 + rot.unk24 + 1;
        o->rot = x;
        x.m[0] = r[6] * pm[2] + r[0] * pm[0] + r[3] * pm[1];
        x.m[1] = r[7] * pm[2] + r[1] * pm[0] + r[4] * pm[1];
        x.m[2] = r[8] * pm[2] + r[2] * pm[0] + r[5] * pm[1];
        x.m[3] = r[6] * pm[5] + r[0] * pm[3] + r[3] * pm[4];
        x.m[4] = r[1] * pm[3] + r[7] * pm[5] + r[4] * pm[4];
        x.m[5] = r[8] * pm[5] + r[2] * pm[3] + r[5] * pm[4];
        x.m[6] = r[3] * pm[7] + r[6] * pm[8] + r[0] * pm[6];
        x.m[7] = r[1] * pm[6] + r[4] * pm[7] + r[7] * pm[8];
        x.m[8] = r[5] * pm[7] + r[8] * pm[8] + r[2] * pm[6];
        x.unk24 = ((const int *)pm)[9] + o->rot.unk24 + 1;
        o->matrix = x;
        /* the speed is added a second time */
        k = msg->keys;
        k[0] = (long double)v0 + k[0];
        k[1] = (long double)v1 + k[1];
        k[2] = (long double)v2 + k[2];
        k0 = k[0];
        k1 = k[1];
        k2 = k[2];
        qx = pm[2] * k2 + pm[1] * k1 + pm[0] * k0;
        qy = pm[3] * k0 + pm[5] * k2 + pm[4] * k1;
        qz = pm[6] * k0 + pm[8] * k2 + pm[7] * k1;
        dx = qx;
        dy = qy;
        dz = qz;
        p.x = dx + ppos->x;
        p.y = dy + ppos->y;
        p.z = dz + ppos->z;
    } else {
        m = o->matrix.m;
        x.m[0] = e[6] * m[2] + e[0] * m[0] + e[3] * m[1];
        x.m[1] = e[7] * m[2] + e[1] * m[0] + e[4] * m[1];
        x.m[2] = e[8] * m[2] + e[2] * m[0] + e[5] * m[1];
        x.m[3] = e[3] * m[4] + e[6] * m[5] + e[0] * m[3];
        x.m[4] = e[4] * m[4] + e[7] * m[5] + e[1] * m[3];
        x.m[5] = e[5] * m[4] + e[8] * m[5] + e[2] * m[3];
        x.m[6] = e[6] * m[8] + e[0] * m[6] + e[3] * m[7];
        x.m[7] = e[7] * m[8] + e[1] * m[6] + e[4] * m[7];
        x.m[8] = e[8] * m[8] + e[2] * m[6] + e[5] * m[7];
        x.unk24 = o->matrix.unk24 + rot.unk24 + 1;
        o->matrix = x;
        p.x = (long double)v0 + o->pos.x;
        p.y = (long double)v1 + o->pos.y;
        p.z = (long double)v2 + o->pos.z;
    }
    scale = max_scale(o);
    DynCube_Move(o->room, o, &p, VirModel_GetRadius(o->model) * scale);
}
HOOK(0x52A230, AnimObj_Control);
#endif

void AnimObj_GetInputInfo(AnimObj *o, AnimInputMsg *msg)
{
    if (msg->index >= 0 && msg->index < ANIMOBJ_INPUTS) {
        msg->out = &gAnimObjInputs[msg->index];
        return;
    }
    msg->out = NULL;
}
HOOK(0x52ABD0, AnimObj_GetInputInfo);

void AnimObj_GetStateSize(AnimObj *o, int *out)
{
    *out = ANIMOBJ_STATE_SIZE;
}
HOOK(0x52AC00, AnimObj_GetStateSize);

void AnimObj_SaveState(AnimObj *o, AnimObjSaveMsg *msg)
{
    AnimObjState *s = msg->buf;

    *msg->pos = o->pos;
    s->matrix = o->matrix;
    s->pos = o->pos;
}
HOOK(0x52AC10, AnimObj_SaveState);

void AnimObj_LoadState(AnimObj *o, AnimObjState **msg)
{
    AnimObjState *s = *msg;
    float m;

    o->matrix = s->matrix;
    m = max_scale(o);
    DynCube_Move(o->room, o, &s->pos, VirModel_GetRadius(o->model) * m);
}
HOOK(0x52AC50, AnimObj_LoadState);

void AnimObj_Close(void)
{
    TaskType_Free(gAnimObjType);
}
HOOK(0x52AD00, AnimObj_Close);
