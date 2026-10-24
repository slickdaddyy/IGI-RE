/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "camera/camera.h"
#include "material/materialtypes.h"

FN_EXTERN(HumanCamera_DeleteHandler);

/* filled by the human's camera info handler */
typedef struct HumanCameraInfo {
    Vec3d pos;
    Matrix matrix;
    float fov;
    Matrix body;
    float unk6C;
    int mode;
} HumanCameraInfo;

enum {
    CAM_FIRST_PERSON = 1,
    CAM_THIRD_PERSON,
    CAM_MOVEMENT,
    CAM_OVERRIDE,
    CAM_DEATH,
};

/* "HumanCamera" task (0x128 bytes): the camera that follows a human */
typedef struct HumanCamera {
    Task task;
    Vec3d pos;
    Matrix matrix;
    float fov;
    void *camera;
    Task **ref;
    int unk6C;
    HumanCameraInfo info;
    int prevMode;
    int unkEC;
    Vec3d look;
    float pitch;
    float yaw;
    float dist;
    float target;
    int index;
    int savedIndex;
    Task **savedRef;
    unsigned flags;
} HumanCamera;

/* an entry of the list of humans the camera can follow */
typedef struct HumanCameraEntry {
    Task **ref;
    char unk4;
    char used;
    char pad6[2];
} HumanCameraEntry;

typedef struct Profile {
    char unk0[0x538];
    int invertMouse;
} Profile;

_Static_assert(offsetof(HumanCameraInfo, mode) == 0x70, "HumanCameraInfo.mode");
_Static_assert(offsetof(HumanCamera, info) == 0x70, "HumanCamera.info");
_Static_assert(offsetof(HumanCamera, look) == 0xF0, "HumanCamera.look");
_Static_assert(sizeof(HumanCamera) == 0x128, "HumanCamera");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (int unk));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x406220, Profile *, Profile_GetActive, (void));
ORIG(0x413A70, uint16_t, HumanPlayer_GetType, (void));
ORIG(0x414FD0, int, Game_CheatKeyDown, (int key));
ORIG(0x415020, int, Game_CheatKeyPressed, (int key));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4849B0, void, HumanCamera_WatchKiller, (HumanCamera *c));
ORIG(0x489470, long double, LevelFlow_GetHour, (void));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, Vec3d *starts, Vec3d *ends, int count, uint8_t event,
                                  CollFilter *filter, int flags));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const Vec3d *pos, const Matrix *axes, float fov, float aspect,
                                  float range, int frame, void *owner, float unk9, int unk10));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));

extern uint16_t gHumanCameraType;
extern uint8_t gHumanCameraEvent;
extern HumanCameraEntry gHumanCameraList[];
extern float gCameraUnkB81700;
extern float gMouseDX;
extern float gMouseDY;

#define HUMAN_LIST_SIZE 128
#define HumanCamera_DeleteHandler (FN(0x484CE0, HumanCamera_DeleteHandler))

static const double kOne = 1.0;
static const double kQuarterPi = 0x1.921fb6p-1;
static const double kSmoothNew = 0.05;
static const double kSmoothOld = 0.95;
static const double kStepThrough = 20.48;
static const double kKeepOff = 204.8;
static const float kZero = 0.0f;
static const float kRotX = -0x1.921fb6p+0f;
static const float kZoomStep = 512.0f;
static const float kDistMin = 1024.0f;
static const float kDistMax = 409600.0f;
static const float kTurnStep = 0x1.657186p-4f;   /* 5 degrees */
static const float kPitchMax3rd = 0x1.8da7e4p+0f; /* 89 degrees */
static const float kPitchMax = 0x1.4f1a6cp+0f;    /* 75 degrees */
static const float kMouseTurn = 0x1.657186p-1f;   /* 40 degrees */
static const float kEaseStep = 0x1.555556p+8f;    /* 341.33 */
static const float kUnits = 4096.0f;
static const float kTargetMax = 10240.0f;
static const float kDeathScale = 0x1.99999ap-14f;
static const float kDeathPull = -0x1.0cccccp+0f;  /* -1.05 */

/* 8 points around a human: four at the feet, four 2662.4 up */
static const double kBodyPoints[8][3] = {
    {-0x1.666666p+9, 0x1.666666p+9, 0},
    {0x1.666666p+9, 0x1.666666p+9, 0},
    {-0x1.666666p+9, -0x1.666666p+9, 0},
    {0x1.666666p+9, -0x1.666666p+9, 0},
    {-0x1.666666p+9, 0x1.666666p+9, 0x1.4cccccp+11},
    {0x1.666666p+9, 0x1.666666p+9, 0x1.4cccccp+11},
    {-0x1.666666p+9, -0x1.666666p+9, 0x1.4cccccp+11},
    {0x1.666666p+9, -0x1.666666p+9, 0x1.4cccccp+11},
};

static Vec3d *task_pos(Task *t)
{
    return (Vec3d *)((char *)t + 0x20);
}

/* camera = pos and a turned by -90 degrees around x */
static void set_view(HumanCamera *c, const Vec3d *pos, const Matrix *a, float fov)
{
    const float *m = a->m;
    Matrix r, out;
    int i, j;

    Matrix_SetRotX(&r, kRotX);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            out.m[i * 3 + j] = m[i * 3 + 2] * r.m[6 + j] + m[i * 3 + 1] * r.m[3 + j] + m[i * 3] * r.m[j];
    out.count = a->count + r.count + 1;
    c->pos = *pos;
    c->fov = fov;
    c->matrix = out;
}

int HumanCamera_HasKiller(HumanCamera *c)
{
    Task *h;
    Task **killer;

    if (!c->ref)
        return 0;
    h = *c->ref;
    if (TaskType_IsA(h->type, HumanPlayer_GetType()) && (killer = *(Task ***)((char *)h + 0xDE8)) && *killer)
        return 1;
    return 0;
}
HOOK(0x482580, HumanCamera_HasKiller);

void HumanCamera_CreateHandler(HumanCamera *c);
void HumanCamera_RunHandler(HumanCamera *c);

void HumanCamera_Open(void)
{
    int i;

    gHumanCameraType = TaskType_New("HumanCamera", sizeof(HumanCamera), Task_GetRootType(), 0, 0);
    gHumanCameraEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gHumanCameraType, 0, HumanCamera_CreateHandler);
    TaskType_SetHandler2(gHumanCameraType, 1, HumanCamera_RunHandler);
    TaskType_SetHandler2(gHumanCameraType, 2, HumanCamera_DeleteHandler);
    for (i = 0; i < HUMAN_LIST_SIZE; i++) {
        gHumanCameraList[i].ref = NULL;
        gHumanCameraList[i].used = 0;
    }
}
HOOK(0x4825C0, HumanCamera_Open);

void HumanCamera_CreateHandler(HumanCamera *c)
{
    static const Matrix identity = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    static const Vec3d zero = {0, 0, 0};

    c->pos = zero;
    c->look = zero;
    c->matrix = identity;
    c->ref = NULL;
    c->prevMode = 0;
    c->pitch = 0.0f;
    c->yaw = 0.0f;
    c->dist = 0.0f;
    c->target = 0.0f;
    c->index = 0;
    c->savedIndex = 0;
    c->savedRef = NULL;
    c->flags = 0;
    c->fov = x87_tan(kQuarterPi);
}
HOOK(0x482650, HumanCamera_CreateHandler);

void HumanCamera_FindHuman(HumanCamera *c);
void HumanCamera_ControlListInput(HumanCamera *c);
void HumanCamera_FirstPersonEnter(HumanCamera *c);
void HumanCamera_FirstPersonUpdate(HumanCamera *c);
void HumanCamera_ThirdPersonEnter(HumanCamera *c);
void HumanCamera_ThirdPersonControlInput(HumanCamera *c);
void HumanCamera_ThirdPersonUpdate(HumanCamera *c);
void HumanCamera_MovementEnter(HumanCamera *c);
void HumanCamera_MovementControlInput(HumanCamera *c);
void HumanCamera_MovementUpdate(HumanCamera *c);
void HumanCamera_DeathCameraEnter(HumanCamera *c);
void HumanCamera_DeathCameraControlInput(HumanCamera *c);
void HumanCamera_DeathCameraUpdate(HumanCamera *c);

void HumanCamera_RunHandler(HumanCamera *c)
{
    int firstPerson = 0;
    int changed;
    Task *h;
    float unk9;
    int frame;

    if (!c->ref || !(h = *c->ref)) {
        HumanCamera_FindHuman(c);
        return;
    }
    gTaskHandlers[gHumanCameraEvent][h->type](h, &c->info);
    changed = c->prevMode != c->info.mode;
    c->prevMode = c->info.mode;
    switch (c->info.mode) {
    case CAM_FIRST_PERSON:
        if (changed)
            HumanCamera_FirstPersonEnter(c);
        Task_Nothing(c);
        HumanCamera_FirstPersonUpdate(c);
        firstPerson = 1;
        break;
    case CAM_THIRD_PERSON:
        if (changed)
            HumanCamera_ThirdPersonEnter(c);
        HumanCamera_ThirdPersonControlInput(c);
        HumanCamera_ThirdPersonUpdate(c);
        break;
    case CAM_MOVEMENT:
        if (changed)
            HumanCamera_MovementEnter(c);
        HumanCamera_MovementControlInput(c);
        HumanCamera_MovementUpdate(c);
        break;
    case CAM_OVERRIDE:
        if (changed)
            Task_Nothing(c);
        Task_Nothing(c);
        HumanCamera_FirstPersonUpdate(c);
        break;
    case CAM_DEATH:
        if (changed)
            HumanCamera_DeathCameraEnter(c);
        HumanCamera_DeathCameraControlInput(c);
        HumanCamera_DeathCameraUpdate(c);
        c->flags &= ~1u;
        break;
    }
    HumanCamera_ControlListInput(c);
    unk9 = LevelFlow_GetHour();
    frame = Screen_GetFrame();
    Camera_Set(c->camera, &c->pos, &c->matrix, c->fov, 1.0f, 300.0f, frame, c, unk9, firstPerson);
    gCameraUnkB81700 = c->info.unk6C;
}
HOOK(0x482700, HumanCamera_RunHandler);

static void list_error(void)
{
    Error_Show("ERROR IN HUMANCAMERA HUMANLIST");
    for (;;)
        ;
}

/* follows the first human of the list */
void HumanCamera_FindHuman(HumanCamera *c)
{
    int i;

    for (i = 0; i < HUMAN_LIST_SIZE; i++) {
        if (!gHumanCameraList[i].used)
            continue;
        if (!*gHumanCameraList[i].ref)
            list_error();
        c->ref = gHumanCameraList[i].ref;
        c->index = i;
        return;
    }
}
HOOK(0x482880, HumanCamera_FindHuman);

void HumanCamera_NextHuman(HumanCamera *c);
void HumanCamera_PrevHuman(HumanCamera *c);

void HumanCamera_ControlListInput(HumanCamera *c)
{
    if (Game_CheatKeyPressed(0x10)) {
        HumanCamera_NextHuman(c);
        return;
    }
    if (Game_CheatKeyPressed(0x1E)) {
        c->ref = c->savedRef;
        c->index = c->savedIndex;
        return;
    }
    if (Game_CheatKeyPressed(0x12))
        HumanCamera_PrevHuman(c);
}
HOOK(0x4828D0, HumanCamera_ControlListInput);

void HumanCamera_NextHuman(HumanCamera *c)
{
    int start = c->index;
    int idx = start + 1;
    int i, e;

    c->index = idx;
    for (i = 0; i < HUMAN_LIST_SIZE; i++, idx++) {
        e = idx % HUMAN_LIST_SIZE;
        if (!gHumanCameraList[e].used)
            continue;
        if (!*gHumanCameraList[e].ref)
            list_error();
        c->ref = gHumanCameraList[e].ref;
        c->index = e;
        return;
    }
    c->index = start;
}
HOOK(0x482930, HumanCamera_NextHuman);

void HumanCamera_PrevHuman(HumanCamera *c)
{
    int idx = c->index - 1;
    int i, e;

    c->index = idx;
    for (i = 0; i < HUMAN_LIST_SIZE; i++) {
        e = idx - i;
        if (e < 0)
            e += HUMAN_LIST_SIZE;
        if (!gHumanCameraList[e].used)
            continue;
        if (!*gHumanCameraList[e].ref)
            list_error();
        c->index = e;
        c->ref = gHumanCameraList[e].ref;
        return;
    }
    c->index = idx + 1;
}
HOOK(0x4829A0, HumanCamera_PrevHuman);

void HumanCamera_FirstPersonEnter(HumanCamera *c)
{
    c->dist = 0.0f;
}
HOOK(0x482A30, HumanCamera_FirstPersonEnter);

/* looks through the human's eyes */
void HumanCamera_FirstPersonUpdate(HumanCamera *c)
{
    Vec3d pos = c->info.pos;
    Matrix m = c->info.matrix;
    float fov = c->info.fov;

    c->look = c->info.pos;
    set_view(c, &pos, &m, fov);
}
HOOK(0x482A40, HumanCamera_FirstPersonUpdate);

void HumanCamera_ThirdPersonEnter(HumanCamera *c)
{
    c->dist = 1024.0f;
    c->pitch = 0.0f;
    c->yaw = 0.0f;
}
HOOK(0x482C10, HumanCamera_ThirdPersonEnter);

void HumanCamera_ThirdPersonControlInput(HumanCamera *c)
{
    if (Game_CheatKeyDown(0x49))
        c->dist = c->dist - kZoomStep;
    if (Game_CheatKeyDown(0x51))
        c->dist = c->dist + kZoomStep;
    if (!(c->dist >= kDistMin))
        c->dist = kDistMin;
    else if (c->dist > kDistMax)
        c->dist = kDistMax;
    if (Game_CheatKeyDown(0x48))
        c->pitch = c->pitch - kTurnStep;
    if (Game_CheatKeyDown(0x50))
        c->pitch = c->pitch + kTurnStep;
    if (!(c->pitch >= -kPitchMax3rd))
        c->pitch = -kPitchMax3rd;
    if (c->pitch > kPitchMax3rd)
        c->pitch = kPitchMax3rd;
    if (Game_CheatKeyDown(0x4B))
        c->yaw = c->yaw - kTurnStep;
    if (Game_CheatKeyDown(0x4D))
        c->yaw = c->yaw + kTurnStep;
}
HOOK(0x482C30, HumanCamera_ThirdPersonControlInput);

void HumanCamera_LookAt(Matrix *out, const Vec3d *from, const Vec3d *to);

/* orbits the human at dist; the look point follows the eye height slowly */
void HumanCamera_ThirdPersonUpdate(HumanCamera *c)
{
    Vec3d pos;
    Matrix a;
    float fov;

    if (x87_eq(c->dist, 0)) {
        pos = c->info.pos;
        a = c->info.matrix;
        fov = c->info.fov;
        c->look = c->info.pos;
    } else {
        const float *b = c->info.body.m;
        Matrix e;
        Vec3d p, l;
        long double nd, v2, wz;
        double d0, d1, wx, wy;

        Matrix_SetEuler(&e, c->pitch, 0, c->yaw);
        nd = -c->dist;
        d0 = e.m[1] * nd;
        d1 = e.m[4] * nd;
        v2 = nd * e.m[7];
        wx = b[2] * v2 + b[1] * d1 + b[0] * d0;
        wy = b[5] * v2 + b[4] * d1 + b[3] * d0;
        wz = v2 * b[8] + b[7] * d1 + b[6] * d0;
        p = c->info.pos;
        l = c->look;
        l.z = p.z * kSmoothNew + l.z * kSmoothOld;
        pos.x = p.x + wx;
        l.x = p.x;
        pos.y = p.y + wy;
        l.y = p.y;
        c->look = l;
        pos.z = l.z + wz;
        /* the serial of a is left unset, like the original */
        HumanCamera_LookAt(&a, &pos, &l);
        fov = x87_tan(kQuarterPi);
    }
    set_view(c, &pos, &a, fov);
}
HOOK(0x482D70, HumanCamera_ThirdPersonUpdate);

/* rows: right, forward (from to to) and up; z is up, without roll */
void HumanCamera_LookAt(Matrix *out, const Vec3d *from, const Vec3d *to)
{
    Vec3d d;
    long double z, r2, inv;
    float fx, fy, rx, ry, rz, r0, r1, ux, uy, uz, u0, u1, u2;

    d.x = to->x - from->x;
    d.y = to->y - from->y;
    d.z = to->z - from->z;
    if (x87_eq(d.x, 0) && x87_eq(d.y, 0)) {
        d.x = 0.0;
        d.y = 1.0;
        z = 0.0;
    } else {
        Vec3d_Normalize(&d);
        z = d.z;
    }
    fx = d.x;
    fy = d.y;
    rx = fy - kZero;
    ry = -(fx - kZero);
    rz = 0.0f;
    r0 = rx;
    r1 = ry;
    r2 = rz;
    if (!x87_eq(rx, 0) || !x87_eq(ry, 0) || !x87_eq(rz, 0)) {
        inv = kOne / x87_sqrt(rx * rx + ry * ry + rz * rz);
        r0 = rx * inv;
        r1 = ry * inv;
        r2 = inv * rz;
    }
    ux = r1 * z - r2 * fy;
    uy = -(r0 * z - r2 * fx);
    uz = r0 * fy - r1 * fx;
    u0 = ux;
    u1 = uy;
    u2 = uz;
    if (!x87_eq(ux, 0) || !x87_eq(uy, 0) || !x87_eq(uz, 0)) {
        inv = kOne / x87_sqrt(ux * ux + uy * uy + uz * uz);
        u0 = ux * inv;
        u1 = uy * inv;
        u2 = uz * inv;
    }
    out->m[0] = r0;
    out->m[1] = fx;
    out->m[2] = u0;
    out->m[3] = r1;
    out->m[4] = fy;
    out->m[5] = u1;
    out->m[6] = r2;
    out->m[7] = z;
    out->m[8] = u2;
}
HOOK(0x483100, HumanCamera_LookAt);

void HumanCamera_MovementEnter(HumanCamera *c)
{
    c->dist = 1024.0f;
    c->target = 10240.0f;
    c->pitch = 0.0f;
    c->yaw = 0.0f;
}
HOOK(0x483360, HumanCamera_MovementEnter);

/* moves dist towards target and lets the keys change target */
static void ease_dist(HumanCamera *c)
{
    long double s;

    if (!(c->dist >= c->target)) {
        s = c->dist + kEaseStep;
        c->dist = s;
        if (s > c->target)
            c->dist = c->target;
    } else if (c->dist > c->target) {
        s = c->dist - kEaseStep;
        c->dist = s;
        if (!(s >= c->target))
            c->dist = c->target;
    }
    if (Game_CheatKeyDown(0x49))
        c->target = c->target - kZoomStep;
    if (Game_CheatKeyDown(0x51))
        c->target = c->target + kZoomStep;
}

void HumanCamera_MovementControlInput(HumanCamera *c)
{
    Profile *p = Profile_GetActive();
    long double s;
    float dy;

    ease_dist(c);
    dy = gMouseDY;
    if (p->invertMouse)
        dy = -dy;
    s = dy * kMouseTurn + c->pitch;
    c->pitch = s;
    if (!(s >= -kPitchMax))
        c->pitch = -kPitchMax;
    else if (s > kPitchMax)
        c->pitch = kPitchMax;
    c->yaw = gMouseDX * -kMouseTurn + c->yaw;
}
HOOK(0x483390, HumanCamera_MovementControlInput);

char HumanCamera_IsBlocked(HumanCamera *c, Task *h, Vec3d *to, Vec3d *from);

/* behind the human at target distance; pulled in front of walls */
static void follow(HumanCamera *c)
{
    Task *h = *c->ref;
    const float *b = c->info.body.m;
    Matrix e, m;
    Vec3d p, w, start, end, d;
    long double nd, v2, wz;
    double d0, d1, wx, wy;
    float fov;
    CollHit hit;
    CollFilter filter;

    Matrix_SetEuler(&e, c->pitch, 0, c->yaw);
    nd = -c->target;
    p = c->info.pos;
    d0 = e.m[1] * nd;
    d1 = e.m[4] * nd;
    v2 = nd * e.m[7];
    wx = b[2] * v2 + b[1] * d1 + b[0] * d0;
    wy = b[5] * v2 + b[4] * d1 + b[3] * d0;
    wz = v2 * b[8] + b[7] * d1 + b[6] * d0;
    w.x = p.x + wx;
    w.y = p.y + wy;
    w.z = p.z + wz;
    /* the serial of m is left unset, like the original */
    HumanCamera_LookAt(&m, &w, &p);
    fov = x87_tan(kQuarterPi);
    start = p;
    filter.ignore = h;
    end = w;
    for (;;) {
        uint8_t ev = Game_GetEvent8();

        Coll_Lines(Game_GetWorld(), &hit, &start, &end, 1, ev, &filter, -1);
        if (!hit.found)
            break;
        if (hit.unk54 && hit.material && hit.material->unk00 > 0) {
            d.x = hit.pos.x - start.x;
            d.y = hit.pos.y - start.y;
            d.z = hit.pos.z - start.z;
            Vec3d_Normalize(&d);
            d.x = d.x * kStepThrough;
            d.y = d.y * kStepThrough;
            d.z = d.z * kStepThrough;
            start.x = d.x + hit.pos.x;
            start.y = d.y + hit.pos.y;
            start.z = d.z + hit.pos.z;
            continue;
        }
        if (HumanCamera_IsBlocked(c, h, &w, &p)) {
            double len;
            long double s;

            d.x = hit.pos.x - p.x;
            d.y = hit.pos.y - p.y;
            d.z = hit.pos.z - p.z;
            len = Vec3d_Length(&d);
            Vec3d_Normalize(&d);
            c->dist = len;
            s = len - kKeepOff;
            if (!(s >= 0.0))
                s = 0.0;
            d.x = s * d.x;
            d.y = s * d.y;
            d.z = s * d.z;
            w.x = d.x + p.x;
            w.y = d.y + p.y;
            w.z = d.z + p.z;
        }
        break;
    }
    set_view(c, &w, &m, fov);
}

void HumanCamera_MovementUpdate(HumanCamera *c)
{
    follow(c);
}
HOOK(0x4834D0, HumanCamera_MovementUpdate);

/* true when the lines from all 8 points around the human to `to` are blocked */
char HumanCamera_IsBlocked(HumanCamera *c, Task *h, Vec3d *to, Vec3d *from)
{
    Vec3d *hp = task_pos(h);
    Vec3d pts[8], d;
    CollHit hit;
    CollFilter filter;
    int i;

    filter.ignore = h;
    for (i = 0; i < 8; i++) {
        pts[i].x = kBodyPoints[i][0] + hp->x;
        pts[i].y = kBodyPoints[i][1] + hp->y;
        pts[i].z = kBodyPoints[i][2] + hp->z;
    }
    for (i = 0; i < 8;) {
        Vec3d *pt = &pts[i];
        uint8_t ev = Game_GetEvent8();

        Coll_Lines(Game_GetWorld(), &hit, pt, to, 1, ev, &filter, -1);
        if (!hit.found)
            return 0;
        if (hit.unk54 && hit.material && hit.material->unk00 > 0) {
            d.x = hit.pos.x - pt->x;
            d.y = hit.pos.y - pt->y;
            d.z = hit.pos.z - pt->z;
            Vec3d_Normalize(&d);
            d.x = d.x * kStepThrough;
            d.y = d.y * kStepThrough;
            d.z = d.z * kStepThrough;
            pt->x = d.x + hit.pos.x;
            pt->y = d.y + hit.pos.y;
            pt->z = d.z + hit.pos.z;
            continue;
        }
        i++;
    }
    return 1;
}
HOOK(0x483A00, HumanCamera_IsBlocked);

void HumanCamera_DeathCameraEnter(HumanCamera *c)
{
    c->dist = 1024.0f;
    c->target = 10240.0f;
    c->pitch = 0.0f;
    c->yaw = 0.0f;
    c->flags = 1;
}
HOOK(0x483D40, HumanCamera_DeathCameraEnter);

void HumanCamera_DeathCameraCollide(HumanCamera *c, float dpitch, float dyaw);

void HumanCamera_DeathCameraControlInput(HumanCamera *c)
{
    if (HumanCamera_HasKiller(c) && (c->flags & 1))
        return;
    ease_dist(c);
    HumanCamera_DeathCameraCollide(c, gMouseDY * kMouseTurn, gMouseDX * kMouseTurn);
    if (!(c->pitch >= -kPitchMax)) {
        c->pitch = -kPitchMax;
        return;
    }
    if (c->pitch > kPitchMax)
        c->pitch = kPitchMax;
}
HOOK(0x483D80, HumanCamera_DeathCameraControlInput);

/* turns the camera and pulls it in where the lines from the center to its
   sides, top and bottom hit something facing it */
void HumanCamera_DeathCameraCollide(HumanCamera *c, float dpitch, float dyaw)
{
    static const int lineFlag[4] = {0, 0, 1, 1};
    const float *b = c->info.body.m;
    int flags[2] = {1, 1};
    long double a = c->dist * kDeathScale * kUnits;
    float na = -a;
    double off[5][3];
    Vec3d p = c->info.pos;
    Vec3d l1[5], l2[5], starts[4], ends[4];
    CollHit hits[4];
    CollFilter filter;
    Matrix e;
    uint8_t ev;
    int i;

    off[0][0] = 0;
    off[0][1] = 0;
    off[0][2] = 0;
    off[1][0] = -a;
    off[1][1] = -c->dist;
    off[1][2] = 0;
    off[2][0] = a;
    off[2][1] = -c->dist;
    off[2][2] = 0;
    off[3][0] = 0;
    off[3][1] = -c->dist;
    off[3][2] = a;
    off[4][0] = 0;
    off[4][1] = -c->dist;
    off[4][2] = na;
    filter.ignore = *c->ref;
    Matrix_SetEuler(&e, c->pitch, 0, c->yaw);
    for (i = 0; i < 5; i++) {
        const double *o = off[i];
        long double t0 = e.m[2] * o[2] + e.m[0] * o[0] + e.m[1] * o[1];
        long double t1 = e.m[5] * o[2] + e.m[3] * o[0] + e.m[4] * o[1];
        long double t2 = e.m[8] * o[2] + e.m[6] * o[0] + e.m[7] * o[1];
        double wx = b[1] * t1 + b[2] * t2 + b[0] * t0;
        double wy = b[4] * t1 + b[5] * t2 + b[3] * t0;
        long double wz = b[7] * t1 + b[8] * t2 + b[6] * t0;

        l1[i].x = p.x + wx;
        l1[i].y = p.y + wy;
        l1[i].z = p.z + wz;
    }
    Matrix_SetEuler(&e, dpitch + c->pitch, 0, dyaw + c->yaw);
    for (i = 0; i < 5; i++) {
        const double *o = off[i];
        long double t0 = e.m[1] * o[1] + e.m[2] * o[2] + e.m[0] * o[0];
        long double t1 = e.m[4] * o[1] + e.m[5] * o[2] + e.m[3] * o[0];
        long double t2 = e.m[7] * o[1] + e.m[8] * o[2] + e.m[6] * o[0];
        double wx = b[1] * t1 + b[2] * t2 + b[0] * t0;
        double wy = b[4] * t1 + b[5] * t2 + b[3] * t0;
        long double wz = b[7] * t1 + b[8] * t2 + b[6] * t0;

        l2[i].x = wx + p.x;
        l2[i].y = wy + p.y;
        l2[i].z = wz + p.z;
    }
    for (i = 0; i < 4; i++) {
        ends[i] = l2[i + 1];
        starts[i] = l1[0];
    }
    ev = Game_GetEvent8();
    Coll_Lines(Game_GetWorld(), hits, starts, ends, 4, ev, &filter, -1);
    for (i = 0; i < 4; i++) {
        CollHit *h = &hits[i];
        long double dot, dx, dy, dz, q2, pull;
        float q0, q1, v;

        if (!h->found)
            continue;
        dot = (ends[i].y - starts[i].y) * h->normal.y + (ends[i].x - starts[i].x) * h->normal.x +
              (ends[i].z - starts[i].z) * h->normal.z;
        if (dot >= 0)
            continue;
        dx = h->pos.x - p.x;
        dy = h->pos.y - p.y;
        dz = h->pos.z - p.z;
        q0 = dz * b[6] + dy * b[3] + dx * b[0];
        q1 = dz * b[7] + dy * b[4] + dx * b[1];
        q2 = dz * b[8] + dy * b[5] + dx * b[2];
        pull = (q2 * e.m[7] + q1 * e.m[4] + q0 * e.m[1]) * kDeathPull;
        v = pull > kUnits ? (float)pull : kUnits;
        c->dist = v;
        c->target = v;
        flags[lineFlag[i]] = 0;
    }
    c->yaw = dyaw + c->yaw;
    c->pitch = dpitch + c->pitch;
    if (flags[0] && flags[1])
        c->target = c->target + kEaseStep;
    if (c->target >= kTargetMax)
        c->target = kTargetMax;
}
HOOK(0x483EC0, HumanCamera_DeathCameraCollide);

void HumanCamera_DeathCameraUpdate(HumanCamera *c)
{
    if (HumanCamera_HasKiller(c) && (c->flags & 1)) {
        HumanCamera_WatchKiller(c);
        return;
    }
    follow(c);
}
HOOK(0x484450, HumanCamera_DeathCameraUpdate);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(540990) uint16_t gHumanCameraType INIT(385);
PAD(igd, 540990, 540992, 0x2) INIT({0xFF});
BSS(B81700) float gCameraUnkB81700;
ALIAS(gLayerFov, gCameraUnkB81700, 0);
BSS(BC20F4) float gMouseDX;
BSS(BC20F4) float gMouseDY;
