/* AnimCam: a CamTask driven by AnimTask channels. The channels pick a mode
   (Flyby, Circle, Aim, Relative, None), two target objects and offsets and
   angles; the keys handler lets the editor set them from the keyboard. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "animcam/animcam.h"

/* "AnimCam" (0x220 bytes), a CamTask (0x88) */
typedef struct AnimCam {
    Task task;
    char unk20[0x68];
    void *camera;       /* 0x88 */
    Matrix axes;        /* 0x8C */
    int unkB4;
    Vec3d pos;          /* 0xB8 */
    float fov;          /* 0xD0 */
    char active;        /* 0xD4 */
    char unkD5[3];
    int slots[10];      /* 0xD8, task ids picked with Ctrl and 0-9 */
    /* the 0xA8 bytes from here are the saved state */
    void *modeFn;       /* 0x100 */
    void *lastFn;       /* 0x104 */
    int objA;           /* 0x108, task ids */
    int objB;           /* 0x10C */
    Vec3d lastA;        /* 0x110, positions kept while locked */
    Vec3d lastB;        /* 0x128 */
    char hasLastA;      /* 0x140 */
    char hasLastB;      /* 0x141 */
    char unk142[6];
    Vec3d disp;         /* 0x148 */
    Vec3d dDisp;        /* 0x160 */
    float targetFactor; /* 0x178 */
    float alpha;        /* 0x17C */
    float dAlpha;
    float beta;
    float dBeta;
    float gamma;
    float dGamma;
    float radius;       /* 0x194 */
    float dRadius;
    char lock[3];       /* 0x19C, alpha beta gamma */
    char enabled;       /* 0x19F */
    int matrixMode;     /* 0x1A0 */
    int unk1A4;
    char modeName[16];  /* 0x1A8 */
    int selA;           /* 0x1B8, task ids picked in the editor */
    int selB;           /* 0x1BC */
    Vec3d unk1C0;
    Vec3d unk1D8;
    float unk1F0;
    int unk1F4[8];
    char lockKeys[3];   /* 0x214, toggled with V B N */
    char unk217;
    int view;           /* 0x218 */
    int unk21C;
} AnimCam;

_Static_assert(offsetof(AnimCam, pos) == 0xB8, "pos");
_Static_assert(offsetof(AnimCam, modeFn) == 0x100, "modeFn");
_Static_assert(offsetof(AnimCam, disp) == 0x148, "disp");
_Static_assert(offsetof(AnimCam, lock) == 0x19C, "lock");
_Static_assert(offsetof(AnimCam, selA) == 0x1B8, "selA");
_Static_assert(offsetof(AnimCam, lockKeys) == 0x214, "lockKeys");
_Static_assert(sizeof(AnimCam) == 0x220, "AnimCam");

#define ANIMCAM_STATE_SIZE 0xA8

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const Vec3d *pos, const Matrix *axes, float fov, float aspect,
                                  float range, int frame, void *owner, int unk9, int unk10));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));
ORIG(0x4E7B80, void, DebugText_SetCursor, (int x, int y));
ORIG(0x4E7BD0, void, Debug_printf, (const char *fmt, ...));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x530260, uint16_t, CamTask_GetType, (void));

extern uint16_t gAnimCamType;
extern void *gAnimCamParams;
extern const AnimInput gAnimCamInputs[];

#define ANIMCAM_INPUTS 22

static const double kAimBack = 204800.0;
static const double kAimSide = 40960.0;
static const float kQuarterTurn = -0x1.921fb6p+0f;

void AnimCam_Run(AnimCam *c);
void AnimCam_Create(AnimCam *c);
void AnimCam_GetParams(void *task, void **out);
void AnimCam_Control(AnimCam *c, AnimControlMsg *msg);
void AnimCam_GetInputInfo(AnimCam *c, AnimInputMsg *msg);
void AnimCam_GetStateSize(AnimCam *c, int *out);
void AnimCam_SaveState(AnimCam *c, void **msg);
void AnimCam_LoadState(AnimCam *c, void **msg);
void AnimCam_ReadKeys(AnimCam *c, float **msg);
void AnimCam_Reset(AnimCam *c);
void AnimCam_Relative(AnimCam *c);
void AnimCam_Disable(AnimCam *c);
void AnimCam_Aim(AnimCam *c);
void AnimCam_Update(AnimCam *c);

void AnimCam_Open(void)
{
    AnimCam c;
    uint16_t t = TaskType_New("AnimCam", sizeof(AnimCam), CamTask_GetType(), 0, 0);
    int i;

    gAnimCamType = t;
    TaskType_SetHandler2(t, 1, AnimCam_Run);
    TaskType_SetHandler2(gAnimCamType, 2, TASK_NOTHING);
    TaskType_SetHandler(gAnimCamType, 0, AnimCam_Create);
    TaskType_SetHandler(gAnimCamType, Param_GetTableEvent(), AnimCam_GetParams);
    TaskType_SetHandler(gAnimCamType, Param_GetChangedEvent(), TASK_NOTHING);
    TaskType_SetHandler(gAnimCamType, Vehicle_GetControlEvent(), AnimCam_Control);
    TaskType_SetHandler(gAnimCamType, Vehicle_GetInputInfoEvent(), AnimCam_GetInputInfo);
    TaskType_SetHandler(gAnimCamType, Vehicle_GetStateSizeEvent(), AnimCam_GetStateSize);
    TaskType_SetHandler(gAnimCamType, Vehicle_GetSaveEvent(), AnimCam_SaveState);
    TaskType_SetHandler(gAnimCamType, Vehicle_GetLoadEvent(), AnimCam_LoadState);
    TaskType_SetHandler(gAnimCamType, Vehicle_GetKeysEvent(), AnimCam_ReadKeys);
    gAnimCamParams = ParamTable_New(10);
    for (i = 0; i < 10; i++)
        ParamTable_Add(gAnimCamParams, &c, &c.slots[i], Param_Int32(), 7, NULL);
}
HOOK(0x5284A0, AnimCam_Open);

void AnimCam_Create(AnimCam *c)
{
    int i;

    for (i = 0; i < 10; i++)
        c->slots[i] = -1;
    c->selA = -1;
    c->selB = -1;
    c->unk1C0.x = c->unk1C0.y = c->unk1C0.z = 0.0;
    c->unk1D8.x = c->unk1D8.y = c->unk1D8.z = 0.0;
    c->camera = NULL;
    c->unk1F0 = 0.0f;
    for (i = 0; i < 8; i++)
        c->unk1F4[i] = 0;
    c->lockKeys[0] = c->lockKeys[1] = c->lockKeys[2] = 0;
    strcpy(c->modeName, "(Default)");
    AnimCam_Reset(c);
}
HOOK(0x528760, AnimCam_Create);

void AnimCam_Reset(AnimCam *c)
{
    c->disp.x = c->disp.y = c->disp.z = 0.0;
    c->dDisp.x = c->dDisp.y = c->dDisp.z = 0.0;
    c->active = 0;
    c->enabled = 1;
    c->modeFn = AnimCam_Relative;
    c->objA = -1;
    c->objB = -1;
    c->hasLastA = c->hasLastB = 0;
    c->targetFactor = 0.0f;
    c->alpha = c->dAlpha = 0.0f;
    c->beta = c->dBeta = 0.0f;
    c->gamma = c->dGamma = 0.0f;
    c->radius = c->dRadius = 0.0f;
    c->lock[0] = c->lock[1] = c->lock[2] = 0;
    c->matrixMode = 1;
}
HOOK(0x528850, AnimCam_Reset);

/* looks from the first object's position along its axes turned a quarter
   turn about x, moved by disp in the object's frame */
void AnimCam_Relative(AnimCam *c)
{
    Matrix a = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    Matrix b = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    Vec3d p = {0, 0, 0};
    long double x;
    double y, z;
    Task *t;
    TaskFn fn;

    if (c->objA >= 0 && (t = Task_FindById(c->objA)) && (fn = HANDLER(7, t->type))) {
        ObjInfo info;
        Matrix e, r;
        const float *m = e.m, *n = a.m;

        info.unk50 = 0;
        fn(t, &info);
        p = info.pos;
        b = info.matrix;
        a = info.matrix;
        Matrix_SetRotX(&e, kQuarterTurn);
        r.m[0] = m[6] * n[2] + m[3] * n[1] + m[0] * n[0];
        r.m[1] = m[7] * n[2] + m[4] * n[1] + m[1] * n[0];
        r.m[2] = m[8] * n[2] + m[5] * n[1] + m[2] * n[0];
        r.m[3] = m[6] * n[5] + m[3] * n[4] + m[0] * n[3];
        r.m[4] = m[7] * n[5] + m[4] * n[4] + m[1] * n[3];
        r.m[5] = m[8] * n[5] + m[5] * n[4] + m[2] * n[3];
        r.m[6] = m[6] * n[8] + m[3] * n[7] + m[0] * n[6];
        r.unk24 = e.unk24 + a.unk24 + 1;
        r.m[7] = m[7] * n[8] + m[4] * n[7] + m[1] * n[6];
        r.m[8] = m[8] * n[8] + m[5] * n[7] + m[2] * n[6];
        a = r;
    }
    c->axes = a;
    c->fov = 1.0f;
    x = b.m[1] * c->disp.y + b.m[0] * c->disp.x + b.m[2] * c->disp.z;
    y = b.m[4] * c->disp.y + b.m[3] * c->disp.x + b.m[5] * c->disp.z;
    z = b.m[7] * c->disp.y + b.m[6] * c->disp.x + b.m[8] * c->disp.z;
    p.x = x + p.x;
    p.y = y + p.y;
    p.z = z + p.z;
    c->pos = p;
    c->disp.x = c->dDisp.x + c->disp.x;
    c->disp.y = c->dDisp.y + c->disp.y;
    c->disp.z = c->dDisp.z + c->disp.z;
}
HOOK(0x528930, AnimCam_Relative);

void AnimCam_GetParams(void *task, void **out)
{
    *out = gAnimCamParams;
}
HOOK(0x528CF0, AnimCam_GetParams);

static int has_key(const float *k)
{
    uint32_t v;

    memcpy(&v, k, sizeof(v));
    return v != 0xC479C000;
}

void AnimCam_Control(AnimCam *c, AnimControlMsg *msg)
{
    const float *k;

    c->active = 0;
    if (c->enabled)
        c->active = 1;
    switch ((unsigned int)round_ll(msg->keys[0])) {
    case 0:
        c->modeFn = TASK_NOTHING;
        strcpy(c->modeName, "Flyby");
        break;
    case 1:
        c->modeFn = TASK_NOTHING;
        strcpy(c->modeName, "Circle");
        break;
    case 2:
        c->modeFn = AnimCam_Aim;
        strcpy(c->modeName, "Aim");
        break;
    case 3:
        c->modeFn = AnimCam_Relative;
        strcpy(c->modeName, "Relative");
        break;
    case 4:
        c->modeFn = AnimCam_Disable;
        strcpy(c->modeName, "(None)");
        break;
    }
    k = msg->keys;
    if (has_key(&k[1]))
        c->objA = (int)round_ll(k[1]);
    if (has_key(&k[2]))
        c->objB = (int)round_ll(k[2]);
    if (has_key(&k[3]))
        c->disp.x = k[3];
    if (has_key(&k[4]))
        c->disp.y = k[4];
    if (has_key(&k[5]))
        c->disp.z = k[5];
    if (has_key(&k[6]))
        c->dDisp.x = k[6];
    if (has_key(&k[7]))
        c->dDisp.y = k[7];
    if (has_key(&k[8]))
        c->dDisp.z = k[8];
    if (has_key(&k[9]))
        fcopy(&c->targetFactor, &k[9]);
    if (has_key(&k[10]))
        fcopy(&c->alpha, &k[10]);
    if (has_key(&k[11]))
        fcopy(&c->dAlpha, &k[11]);
    if (has_key(&k[12]))
        fcopy(&c->beta, &k[12]);
    if (has_key(&k[13]))
        fcopy(&c->dBeta, &k[13]);
    if (has_key(&k[14]))
        fcopy(&c->gamma, &k[14]);
    if (has_key(&k[15]))
        fcopy(&c->dGamma, &k[15]);
    if (has_key(&k[16]))
        fcopy(&c->radius, &k[16]);
    if (has_key(&k[17]))
        fcopy(&c->dRadius, &k[17]);
    if (has_key(&k[18]))
        c->lock[0] = (char)round_ll(k[18]);
    if (has_key(&k[19]))
        c->lock[1] = (char)round_ll(k[19]);
    if (has_key(&k[20]))
        c->lock[2] = (char)round_ll(k[20]);
    if (has_key(&k[21]))
        c->matrixMode = (int)round_ll(k[21]);
    if (msg->mode == 0)
        AnimCam_Reset(c);
    if (msg->mode > 0)
        AnimCam_Update(c);
}
HOOK(0x528D00, AnimCam_Control);

void AnimCam_Disable(AnimCam *c)
{
    c->active = 0;
    c->enabled = 0;
}
HOOK(0x528FF0, AnimCam_Disable);

static int nonzero(long double v)
{
    return v < 0 || v > 0;
}

static long double inv_len(long double a, long double b, long double c)
{
    return 1.0L / __builtin_sqrtl(a * a + b * b + c * c);
}

/* gets the object's position if it has a GetInfo handler */
static void object_pos(int id, Vec3d *pos)
{
    Task *t;
    TaskFn fn;

    if ((t = Task_FindById(id)) && (fn = HANDLER(7, t->type))) {
        ObjInfo info;

        info.unk50 = 0;
        fn(t, &info);
        *pos = info.pos;
    }
}

/* looks from object A (or disp) at object B; the original builds the frame
   with cross products and normalises each vector, but drops the components
   it never reads */
void AnimCam_Aim(AnimCam *c)
{
    Vec3d a = {0, 0, 0}, b = {0, 0, 0}, t = {0, 0, 0};
    char fixed = 0;
    float ay, az, by, bz, dx, dz, r0, r1, r2, rx, rz, c0, c1, c2, cx, cz;
    float vx, vy, w0, w1, w2, wx, wy, u0, u1, u2, ux, uy;
    long double dy, k, vz, wz, uz;
    const float *m;
    Matrix rot, x;

    if (c->objA < 0) {
        a = c->disp;
        fixed = 1;
    } else {
        object_pos(c->objA, &a);
    }
    if (c->hasLastA && c->lock[0]) {
        a = c->lastA;
    } else {
        c->lastA = a;
        c->hasLastA = 1;
    }
    if (c->objB >= 0)
        object_pos(c->objB, &b);
    if (c->hasLastB && c->lock[1]) {
        b = c->lastB;
    } else {
        c->lastB = b;
        c->hasLastB = 1;
    }
    by = b.y;
    bz = b.z;
    ay = a.y;
    az = a.z;
    dx = (long double)b.x - (long double)a.x;
    dy = (long double)by - ay;
    dz = (long double)bz - az;
    if (nonzero(dx) || nonzero(dy) || nonzero(dz)) {
        k = inv_len(dx, dy, dz);
        dx = dx * k;
        dy = dy * k;
        dz = dz * k;
    } else {
        dx = 1.0f;
    }
    r0 = dy;
    r1 = -dx;
    r2 = 0.0f;
    c0 = (long double)r1 * dz - (long double)r2 * dy;
    c1 = -((long double)r0 * dz - (long double)r2 * dx);
    c2 = (long double)r0 * dy - (long double)r1 * dx;
    rx = r0;
    rz = r2;
    if (nonzero(r0) || nonzero(r1) || nonzero(r2)) {
        k = inv_len(r1, r2, r0);
        rx = r0 * k;
        rz = r2 * k;
    }
    cx = c0;
    cz = c2;
    if (nonzero(c0) || nonzero(c1) || nonzero(c2)) {
        k = inv_len(c0, c1, c2);
        cx = c0 * k;
        cz = c2 * k;
    }
    Matrix_SetRotX(&rot, kQuarterTurn);
    vx = rx;
    vy = dx;
    vz = cx;
    if (nonzero(rx) || nonzero(dx) || nonzero(cx)) {
        k = inv_len(rx, dx, cx);
        vx = rx * k;
        vy = dx * k;
        vz = k * cx;
    }
    w0 = (long double)cx * dz - (long double)cz * dx;
    w1 = -((long double)rz * cx - (long double)rx * cz);
    w2 = (long double)rz * dx - (long double)rx * dz;
    u0 = (long double)w2 * vy - (long double)w1 * vz;
    u1 = -((long double)w2 * vx - (long double)w0 * vz);
    u2 = (long double)w1 * vx - (long double)w0 * vy;
    wx = w0;
    wy = w1;
    wz = w2;
    if (nonzero(w0) || nonzero(w1) || nonzero(w2)) {
        k = inv_len(w2, w1, w0);
        wx = w0 * k;
        wy = w1 * k;
        wz = k * w2;
    }
    ux = u0;
    uy = u1;
    uz = u2;
    if (nonzero(u0) || nonzero(u1) || nonzero(u2)) {
        k = inv_len(u0, u1, u2);
        ux = u0 * k;
        uy = u1 * k;
        uz = k * u2;
    }
    m = rot.m;
    x.m[0] = m[6] * vz + m[3] * vy + m[0] * vx;
    x.m[1] = m[7] * vz + m[4] * vy + m[1] * vx;
    x.m[2] = m[8] * vz + m[5] * vy + m[2] * vx;
    x.m[3] = m[0] * wx + m[3] * wy + m[6] * wz;
    x.m[4] = m[1] * wx + m[4] * wy + m[7] * wz;
    x.m[5] = m[2] * wx + m[5] * wy + m[8] * wz;
    x.m[6] = m[6] * uz + m[3] * uy + m[0] * ux;
    x.m[7] = m[7] * uz + m[4] * uy + m[1] * ux;
    x.m[8] = m[8] * uz + m[5] * uy + m[2] * ux;
    x.unk24 = rot.unk24 + 1;
    if (!fixed) {
        t.x = x.m[1] * c->disp.y + x.m[2] * c->disp.z + x.m[0] * c->disp.x;
        t.y = x.m[4] * c->disp.y + x.m[5] * c->disp.z + x.m[3] * c->disp.x;
        t.z = x.m[7] * c->disp.y + x.m[8] * c->disp.z + x.m[6] * c->disp.x;
    }
    c->axes = x;
    c->fov = 1.0f;
    t.x = a.x + t.x;
    t.y = a.y + t.y;
    t.z = a.z + t.z;
    c->pos = t;
    c->disp.x = c->dDisp.x + c->disp.x;
    c->disp.y = c->dDisp.y + c->disp.y;
    c->disp.z = c->dDisp.z + c->disp.z;
}
HOOK(0x529010, AnimCam_Aim);

void AnimCam_Update(AnimCam *c)
{
    ((void (*)(AnimCam *))c->modeFn)(c);
    c->lastFn = c->modeFn;
    Camera_Set(c->camera, &c->pos, &c->axes, c->fov, 1.0f, 0.0f, Screen_GetFrame(), NULL, 0, 0);
}
HOOK(0x529A10, AnimCam_Update);

void AnimCam_GetInputInfo(AnimCam *c, AnimInputMsg *msg)
{
    if (msg->index >= 0 && msg->index < ANIMCAM_INPUTS) {
        msg->out = &gAnimCamInputs[msg->index];
        return;
    }
    msg->out = NULL;
}
HOOK(0x529A70, AnimCam_GetInputInfo);

void AnimCam_ReadSelectKeys(void *unused, AnimCam *c);
void AnimCam_ReadModeKeys(AnimCam *c, float **msg);
void AnimCam_DrawInfo(AnimCam *c);

void AnimCam_ReadKeys(AnimCam *c, float **msg)
{
    int i;

    for (i = 0; i < ANIMCAM_INPUTS; i++)
        (*msg)[i] = ANIM_NO_KEY;
    AnimCam_ReadSelectKeys(c, c);
    AnimCam_ReadModeKeys(c, msg);
    AnimCam_DrawInfo(c);
}
HOOK(0x529AA0, AnimCam_ReadKeys);

/* Ctrl+0..9 picks a slot as object A, with Shift as object B */
void AnimCam_ReadSelectKeys(void *unused, AnimCam *c)
{
    c->active = 1;
    if (KEY_DOWN(KEY_LCONTROL) || KEY_DOWN(KEY_RCONTROL)) {
        int *sel = KEY_DOWN(KEY_LSHIFT) || KEY_DOWN(KEY_RSHIFT) ? &c->selB : &c->selA;
        int i;

        if (KEY_DOWN(KEY_0))
            *sel = c->slots[0];
        for (i = 1; i < 10; i++) {
            if (KEY_DOWN(KEY_1 + i - 1))
                *sel = c->slots[i];
        }
    }
    if (KEY_DOWN(KEY_9))
        c->unk1F0 = 0.0f;
    if (KEY_PRESSED(KEY_V))
        c->lockKeys[0] ^= 1;
    if (KEY_PRESSED(KEY_B))
        c->lockKeys[1] ^= 1;
    if (KEY_PRESSED(KEY_N))
        c->lockKeys[2] ^= 1;
}
HOOK(0x529AE0, AnimCam_ReadSelectKeys);

static void set_mode_keys(AnimCam *c, float **msg, float mode)
{
    (*msg)[0] = mode;
    (*msg)[1] = c->selA;
    (*msg)[2] = c->selB;
    (*msg)[3] = c->unk1C0.x;
    (*msg)[4] = c->unk1C0.y;
    (*msg)[5] = c->unk1C0.z;
    (*msg)[6] = c->unk1D8.x;
    (*msg)[7] = c->unk1D8.y;
    (*msg)[8] = c->unk1D8.z;
    (*msg)[18] = c->lockKeys[0];
    (*msg)[19] = c->lockKeys[1];
    (*msg)[20] = c->lockKeys[2];
}

/* F aims at object B from behind it, C A R pick Circle, Aim and Relative
   with the current values, E holds None */
void AnimCam_ReadModeKeys(AnimCam *c, float **msg)
{
    if (KEY_PRESSED(KEY_F)) {
        /* left unset in the original when there is no object */
        double x = 0, y = 0;
        long double z = 0;
        Task *t;
        TaskFn fn;

        if (c->selB >= 0 && (t = Task_FindById(c->selB)) && (fn = HANDLER(7, t->type))) {
            ObjInfo info;
            const float *m = info.matrix.m;
            double a1, a2, b1, b2;
            long double sx;

            info.unk50 = 0;
            fn(t, &info);
            a1 = m[4] * kAimBack;
            a2 = m[7] * kAimBack;
            sx = (long double)m[1] * kAimBack;
            b1 = m[3] * kAimSide;
            b2 = m[6] * kAimSide;
            sx = sx + (long double)m[0] * kAimSide;
            a1 = b1 + a1;
            a2 = b2 + a2;
            x = info.pos.x + sx;
            y = info.pos.y + a1;
            z = (long double)info.pos.z + a2;
        }
        (*msg)[0] = 2.0f;
        (*msg)[1] = -1.0f;
        (*msg)[2] = c->selB;
        (*msg)[3] = x;
        (*msg)[4] = y;
        (*msg)[5] = z;
        (*msg)[6] = c->unk1D8.x;
        (*msg)[7] = c->unk1D8.y;
        (*msg)[8] = c->unk1D8.z;
        (*msg)[18] = c->lockKeys[0];
        (*msg)[19] = c->lockKeys[1];
        (*msg)[20] = c->lockKeys[2];
    }
    if (KEY_PRESSED(KEY_C))
        set_mode_keys(c, msg, 1.0f);
    if (KEY_PRESSED(KEY_A))
        set_mode_keys(c, msg, 2.0f);
    if (KEY_PRESSED(KEY_R))
        set_mode_keys(c, msg, 3.0f);
    if (KEY_DOWN(KEY_E))
        (*msg)[0] = 4.0f;
}
HOOK(0x529C20, AnimCam_ReadModeKeys);

void AnimCam_DrawInfo(AnimCam *c)
{
    char locks[4] = "abg";

    DebugText_SetCursor(0x14, 5);
    Debug_printf("%s", c->modeName);
    DebugText_SetCursor(0x14, 6);
    Debug_printf("A:%d", c->selA);
    DebugText_SetCursor(0x14, 7);
    Debug_printf("B:%d", c->selB);
    DebugText_SetCursor(0x14, 8);
    Debug_printf("U:%.2f", c->unk1F0);
    DebugText_SetCursor(0x14, 9);
    Debug_printf("U:%.2f", c->unk1F0);
    /* upper case when locked */
    locks[0] += c->lockKeys[0] ? 0xE0 : 0;
    locks[1] += c->lockKeys[1] ? 0xE0 : 0;
    locks[2] += c->lockKeys[2] ? 0xE0 : 0;
    DebugText_SetCursor(0x14, 10);
    Debug_printf("L:%s", locks);
    DebugText_SetCursor(0x14, 11);
    switch (c->view) {
    case 0:
        Debug_printf("M:World");
        break;
    case 1:
        Debug_printf("M:Object");
        break;
    case 2:
        Debug_printf("M:Speed");
        break;
    case 3:
        Debug_printf("M:Angles");
        break;
    }
}
HOOK(0x52A010, AnimCam_DrawInfo);

/* Space stops the camera until the next reset */
void AnimCam_Run(AnimCam *c)
{
    if (KEY_DOWN(KEY_SPACE))
        c->enabled = 0;
    c->active = 0;
    if (c->enabled)
        c->active = 1;
    AnimCam_Update(c);
}
HOOK(0x52A180, AnimCam_Run);

void AnimCam_GetStateSize(AnimCam *c, int *out)
{
    *out = ANIMCAM_STATE_SIZE;
}
HOOK(0x52A1C0, AnimCam_GetStateSize);

void AnimCam_SaveState(AnimCam *c, void **msg)
{
    memcpy(msg[0], &c->modeFn, ANIMCAM_STATE_SIZE);
}
HOOK(0x52A1D0, AnimCam_SaveState);

void AnimCam_LoadState(AnimCam *c, void **msg)
{
    memcpy(&c->modeFn, msg[0], ANIMCAM_STATE_SIZE);
}
HOOK(0x52A1F0, AnimCam_LoadState);

void AnimCam_Close(void)
{
    QTaskList_Free(gAnimCamParams);
    TaskType_Free(gAnimCamType);
}
HOOK(0x52A210, AnimCam_Close);
