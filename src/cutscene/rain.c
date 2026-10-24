/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>

#include "hook.h"
#include "camera/camera.h"
#include "param/paramtypes.h"

FN_EXTERN(crt_CIfmod);

/* one cube of the 7x7x7 grid of rain around the camera */
typedef struct RainCell {
    double x, y, z;
    float density;
    int visible;
} RainCell;

typedef struct RainDrop {
    Vec3f pos;
    Vec3f vel;
    Vec3f trail;
    float alpha;
} RainDrop;

/* "RainDynCubeObj", 0x4290 bytes, parent DynCubeObj */
typedef struct RainObj {
    char base[0x68];
    RainCell cells[7][7][7];
    RainDrop *dropMem;
    RainDrop *drops[20];
    int unk2B9C;
    int soundBucket;
    int soundHash;
    int unk2BA8;
    int unk2BAC;
    float unk2BB0;
    float unk2BB4;
    int unk2BB8;
    char unk2BBC[0x4C];
    Vec3d pos;
    Vec3d center;
    double sizeX;
    double sizeY;
    Vec3f viewDir;
    Vec3f move;
    Vec3f color;
    float windX;
    float windY;
    int unk2C74;
    int unk2C78;
    float cellSize;
    float trailScale;
    float spawnAccum;
    float spawnRate;
    float spread;
    float speed;
    int unk2C94;
    float size;
    float density;
    float maxDensity;
    float fadeTime;
    int dropCount;
    int cameraInside;
    int inWorld;
    char noCollide;
    char pad2CB5[3];
    VarString trigger;
    float intensity;
    int frameCount;
    int cameraCellVisible;
    int pad2D14;
    CollHit hits[49];
    int hitCount;
    int pad428C;
} RainObj;

/* what the slot 7 handler fills in */
typedef struct RainInfo {
    Vec3d pos;
    int unk18[3];
    Matrix matrix;
    float radius;
    char *name;
} RainInfo;

/* a render queue entry */
typedef struct RainPrim {
    int unk0;
    int type;
    int unk8;
    RainObj *rain;
} RainPrim;

typedef void (*RainCallback)(char *state);

_Static_assert(sizeof(RainCell) == 0x20, "RainCell");
_Static_assert(offsetof(RainObj, dropMem) == 0x2B48, "RainObj.dropMem");
_Static_assert(offsetof(RainObj, soundBucket) == 0x2BA0, "RainObj.soundBucket");
_Static_assert(offsetof(RainObj, pos) == 0x2C08, "RainObj.pos");
_Static_assert(offsetof(RainObj, viewDir) == 0x2C48, "RainObj.viewDir");
_Static_assert(offsetof(RainObj, cellSize) == 0x2C7C, "RainObj.cellSize");
_Static_assert(offsetof(RainObj, dropCount) == 0x2CA8, "RainObj.dropCount");
_Static_assert(offsetof(RainObj, trigger) == 0x2CB8, "RainObj.trigger");
_Static_assert(offsetof(RainObj, hits) == 0x2D18, "RainObj.hits");
_Static_assert(sizeof(RainObj) == 0x4290, "RainObj");
_Static_assert(offsetof(RainInfo, radius) == 0x4C, "RainInfo.radius");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x490370, unsigned int, Timer_GetMs, (void));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x497800, RainPrim *, Frame_Alloc, (int size));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B2B30, long double, Vec3_Angle, (const Vec3f *a, const Vec3f *b));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B47E0, void, Random_Seed, (void *r, unsigned int seed));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4C7760, void, DynCubeObj_ClearRenderData, (void *obj));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (void *world, void *results, Vec3d *points, int count, const double *dz,
                                          int unk, int unk2, unsigned int flags));
ORIG(0x4E1130, void, Line3D_Draw2, (Vec3d *a, Vec3d *b, float *colorA, float alphaA, float *colorB, float alphaB));
ORIG(0x4E7BD0, void, Debug_printf, (const char *fmt, ...));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1160, char, Level_IsRunning, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* MSVC _CIfmod: fmod(st1, st0), both popped */
#define CRT_CIFMOD FN(0x4A56DA, crt_CIfmod)

extern void *gCurrentWorld;
extern uint16_t gDynCubeObjType;
extern uint16_t gRainObjType;
extern int gRainVisibleCubes;
extern int gRainLineId;
extern char gRainRandom[];
extern int gRainMemory;
extern RainObj *gRainObjs[];
extern RainCallback gRainCallback;
extern void *gRainParams;
extern Matrix gViewMatrix;
extern Vec3d gViewPos;

#define RAIN_CELLS (7 * 7 * 7)
#define RAIN_DROPS 20
#define RAIN_BATCH 19

static const double kOneD = 1.0;
static const double kMinusOneD = -1.0;
static const double kZeroD = 0.0;
static const double kThreeQuartersD = 0.75;
static const double k2048 = 2048.0;
static const double k4096D = 4096.0;
static const double kMaxSize = 800.0;
static const double kDropHeight = 81920.0;
static const double kDropDepth = -409600.0;
static const float kHalf = 0.5f;
static const float kNegHalf = -0.5f;
static const float kThree = 3.0f;
static const float kSeven = 7.0f;
static const float kQuarter = 0.25f;
static const float kThreeQuarters = 0.75f;
static const float kPiF = 0x1.921fb6p+1f;
static const float kTwoPi = 0x1.921fb6p+2f;
static const float kAlphaScale = 0x1.333334p-2f;
static const float kAlphaBase = 0x1.666666p-1f;
static const float kFrames = 30.0f;
static const float kFarMove = 16384.0f;
static const float kInvUnits = 0x1p-12f;
static const float kMaxMove = 20480.0f;
static const float kUnits = 4096.0f;
static const float kInvSeven = 0x1.24924ap-3f;
static const float kColorScale = 0x1.020408p-7f;
static const float kMaxAngle = 0x1.197c98p+0f;
static const float kSpread = 0x1.41b2f8p-3f;
static const float kSpeed = 0x1.555556p+12f;
static const float kWindX = -0x1.99999ap-3f;
static const float kWindY = 0x1.99999ap-4f;

/* not inlined, so the x87 stack is empty around the call */
static __attribute__((noinline)) long double crt_fmod(long double x, long double y)
{
    long double r;

    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "r"(CRT_CIFMOD) : "st(1)", "eax", "ecx", "edx", "memory",
                     "cc");
    return r;
}

static RainCell *cell_at(RainObj *r, int i)
{
    return &r->cells[0][0][0] + i;
}

static int vec_is_zero(const Vec3f *v)
{
    return x87_eq(v->x, 0.0f) && x87_eq(v->y, 0.0f) && x87_eq(v->z, 0.0f);
}

static long double vec_inv_length(const Vec3f *v)
{
    return kOneD / x87_sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}

RainCell *RainObj_FindCell(RainObj *r, const Vec3d *p)
{
    float half = r->cellSize * kHalf;
    int i;

    for (i = 0; i < RAIN_CELLS; i++) {
        RainCell *c = cell_at(r, i);
        long double dx = c->x - p->x;
        float dy = c->y - p->y;
        float dz = c->z - p->z;

        if (__builtin_fabsl(dx) <= half && __builtin_fabsf(dy) <= half && __builtin_fabsf(dz) <= half)
            return c;
    }
    return NULL;
}
HOOK(0x4F65E0, RainObj_FindCell);

void RainObj_AddDrop(RainObj *r, const Vec3f *pos, const Vec3f *vel, int unused)
{
    RainDrop *d;

    if (r->dropCount >= RAIN_DROPS)
        return;
    d = r->drops[r->dropCount];
    d->pos = *pos;
    d->vel = *vel;
    d->trail = *vel;
    d->trail.x = r->trailScale * d->trail.x;
    d->trail.y = r->trailScale * d->trail.y;
    d->trail.z = r->trailScale * d->trail.z;
    d->alpha = Random_Float(gRainRandom) * kAlphaScale + kAlphaBase;
    r->dropCount++;
}
HOOK(0x4F66A0, RainObj_AddDrop);

/* a drop falling along dir, turned by up to spread / 2 and sped up by 0.75 .. 1 */
void RainObj_SpawnDrop(RainObj *r, const Vec3f *pos, const Vec3f *dir, float spread, float speed, int unk)
{
    Vec3f a, p, q, n, v, w;
    Matrix m;
    long double inv, s, wz;
    float hi, lo, r1, az;

    if (r->dropCount >= RAIN_DROPS)
        return;
    a = *dir;
    if (!vec_is_zero(&a)) {
        inv = vec_inv_length(&a);
        a.x = a.x * inv;
        a.y = a.y * inv;
        a.z = a.z * inv;
    }
    hi = 1.0f;
    lo = 0.0f;
    if (__builtin_fabsf(a.x) > kThreeQuartersD) {
        hi = 0.0f;
        lo = 1.0f;
    }
    p.x = lo * a.z;
    p.y = -(hi * a.z);
    p.z = hi * a.y - lo * a.x;
    q = p;
    if (!vec_is_zero(&p)) {
        inv = vec_inv_length(&p);
        q.x = p.x * inv;
        q.y = q.y * inv;
        q.z = q.z * inv;
    }
    n.x = q.y * a.z - q.z * a.y;
    n.y = -(q.x * a.z - q.z * a.x);
    n.z = q.x * a.y - q.y * a.x;
    r1 = Random_Float(gRainRandom) * spread;
    az = kPiF - Random_Float(gRainRandom) * kTwoPi;
    Matrix_SetEuler(&m, spread * kHalf - r1, 0.0f, az);
    v.x = m.m[2] * n.x + m.m[5] * q.x + m.m[8] * a.x;
    v.y = m.m[2] * n.y + m.m[5] * q.y + m.m[8] * a.y;
    v.z = m.m[2] * n.z + m.m[5] * q.z + m.m[8] * a.z;
    s = (Random_Float(gRainRandom) * kQuarter + kThreeQuarters) * speed;
    w.x = v.x;
    w.y = v.y;
    wz = v.z;
    if (!vec_is_zero(&v)) {
        inv = vec_inv_length(&v);
        w.x = v.x * inv;
        w.y = v.y * inv;
        wz = inv * v.z;
    }
    w.x = w.x * s;
    w.y = w.y * s;
    w.z = wz * s;
    RainObj_AddDrop(r, pos, &w, unk);
}
HOOK(0x4F6750, RainObj_SpawnDrop);

/* centres the grid on the last camera position */
void RainObj_PlaceCells(RainObj *r)
{
    long double hx = r->cellSize * kThree;
    float hy = r->cellSize * kThree;
    float hz = r->cellSize * kThree;
    Vec3d p = r->pos;
    long double bx = p.x - hx;
    long double by = p.y - hy;
    long double bz = p.z - hz;
    int i, j, k;

    for (i = 0; i < 7; i++) {
        for (j = 0; j < 7; j++) {
            for (k = 0; k < 7; k++) {
                RainCell *c = &r->cells[i][j][k];

                c->x = i * r->cellSize + bx;
                c->y = j * r->cellSize + by;
                c->visible = 1;
                c->density = 0.0f;
                c->z = k * r->cellSize + bz;
            }
        }
    }
}
HOOK(0x4F6B00, RainObj_PlaceCells);

void RainObj_CreateHandler(RainObj *r)
{
    int i;

    r->sizeX = 500.0;
    r->sizeY = 500.0;
    r->dropCount = 0;
    r->pos.x = 0;
    r->pos.y = 0;
    r->pos.z = 0;
    r->center.x = 0;
    r->center.y = 0;
    r->center.z = 0;
    r->spawnAccum = 0.0f;
    r->inWorld = 0;
    r->cameraInside = 0;
    r->unk2C74 = 0;
    r->unk2C78 = 0;
    r->move.x = 0.0f;
    r->move.y = 0.0f;
    r->move.z = 0.0f;
    r->size = 15.0f;
    r->density = 0.25f;
    r->maxDensity = 0.5f;
    r->windX = kWindX;
    r->windY = kWindY;
    r->fadeTime = 1.0f;
    r->noCollide = 1;
    r->unk2BA8 = 0;
    r->unk2BAC = 16;
    r->unk2BB0 = 1.0f;
    r->unk2BB4 = 1.0f;
    r->unk2BB8 = 0;
    r->soundBucket = Sound_HashBucket("gen_32");
    r->soundHash = Sound_HashName("gen_32");
    r->frameCount = 0;
    VarString_Init(&r->trigger);
    r->hitCount = 0;
    for (i = 0; i < 8; i++) {
        if (!gRainObjs[i]) {
            gRainObjs[i] = r;
            return;
        }
    }
}
HOOK(0x4F6C00, RainObj_CreateHandler);

int RainObj_IsCameraOutside(RainObj *r);
void RainObj_SpawnDrop(RainObj *r, const Vec3f *pos, const Vec3f *dir, float spread, float speed, int unk);

/* fades the rain in or out, spawns drops and refills the cells */
void RainObj_Simulate(RainObj *r)
{
    float fade = r->fadeTime * kFrames;
    float growth;
    long double g, inv, acc;
    Vec3f dir, p;
    int i;

    if (!(fade > 1.0f))
        fade = 1.0f;
    g = (kFarMove - x87_sqrt(r->move.x * r->move.x + r->move.y * r->move.y + r->move.z * r->move.z)) * kInvUnits;
    if (!(g > 0.0f))
        g = 0.0f;
    growth = g + g + 1.0f;
    r->cameraInside = RainObj_IsCameraOutside(r) == 0;
    if (!x87_eq(VarString_EvalReal(&r->trigger, r), kZeroD) && r->cameraInside) {
        if (!(r->intensity >= fade))
            r->intensity = r->intensity + 1.0f;
    } else {
        if (!(r->intensity > 0.0f))
            return;
        r->intensity = r->intensity - 1.0f;
    }
    inv = kOneD / x87_sqrt(r->windX * r->windX + r->windY * r->windY + kOneD);
    dir.x = r->windX * inv;
    dir.y = r->windY * inv;
    dir.z = inv * kMinusOneD;
    acc = r->intensity / fade * r->density * r->spawnRate + r->spawnAccum;
    r->spawnAccum = acc;
    while (acc >= 1.0f) {
        float x = kHalf - Random_Float(gRainRandom);

        p.y = (kHalf - Random_Float(gRainRandom)) * r->cellSize;
        p.x = x * r->cellSize;
        p.z = r->cellSize * kHalf;
        RainObj_SpawnDrop(r, &p, &dir, r->spread, r->speed, r->unk2C94);
        acc = r->spawnAccum - 1.0f;
        r->spawnAccum = acc;
    }
    for (i = 0; i < RAIN_CELLS; i++) {
        RainCell *c = cell_at(r, i);

        if (!(c->density >= r->maxDensity))
            c->density = r->maxDensity / growth + c->density;
    }
}
HOOK(0x4F6D70, RainObj_Simulate);

/* 0 while the camera is inside the rain area */
int RainObj_IsCameraOutside(RainObj *r)
{
    double hx;
    long double hy;

    if (!(r->sizeX >= kZeroD))
        return 0;
    if (x87_eq(r->pos.z + r->pos.x + r->pos.y, kZeroD))
        return 1;
    hx = r->sizeX * k2048;
    if (r->center.x - hx > r->pos.x)
        return 1;
    hy = r->sizeY * k2048;
    if (r->center.y - hy > r->pos.y)
        return 1;
    if (!(hx + r->center.x >= r->pos.x))
        return 1;
    if (!(hy + r->center.y >= r->pos.y))
        return 1;
    return 0;
}
HOOK(0x4F6FE0, RainObj_IsCameraOutside);

void RainObj_RemoveDrop(RainObj *r, int i);

/* a drop that leaves the bottom of its cube is removed; the one swapped into
   its place is skipped this frame */
void RainObj_MoveDrops(RainObj *r)
{
    int i;

    for (i = 0; i < r->dropCount; i++) {
        RainDrop *d = r->drops[i];
        long double z;

        d->pos.x = d->vel.x + d->pos.x;
        d->pos.y = d->vel.y + d->pos.y;
        z = d->vel.z + d->pos.z;
        d->pos.z = z;
        if (!(z >= r->cellSize * kNegHalf))
            RainObj_RemoveDrop(r, i);
    }
}
HOOK(0x4F70B0, RainObj_MoveDrops);

void RainObj_RemoveDrop(RainObj *r, int i)
{
    int n = r->dropCount;

    if (!n)
        return;
    if (n - 1 != i) {
        RainDrop *d = r->drops[i];

        r->drops[i] = r->drops[n - 1];
        r->drops[r->dropCount - 1] = d;
    }
    r->dropCount--;
}
HOOK(0x4F7120, RainObj_RemoveDrop);

/* state[0]: raining here, state[1]: the camera's cube is open to the sky */
void RainObj_RunHandler(RainObj *r)
{
    char state[2];

    RainObj_Simulate(r);
    RainObj_MoveDrops(r);
    r->frameCount++;
    if (!gRainCallback)
        return;
    state[0] = r->cameraInside && !x87_eq(VarString_EvalReal(&r->trigger, r), kZeroD);
    state[1] = (char)r->cameraCellVisible;
    gRainCallback(state);
}
HOOK(0x4F7170, RainObj_RunHandler);

void RainObj_GetInfoHandler(RainObj *r, RainInfo *info)
{
    long double m = !(r->sizeY >= r->sizeX) ? r->sizeX : r->sizeY;
    Vec3d p;

    if (m >= kMaxSize)
        m = kMaxSize;
    p = r->center;
    info->unk18[0] = 0;
    info->unk18[1] = 0;
    info->unk18[2] = 0;
    p.z = m * k4096D + p.z;
    info->pos = p;
    info->matrix = (Matrix){{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    info->radius = r->sizeX * k4096D;
    if (info->name)
        crt_sprintf(info->name, "RainDynCubeObj");
}
HOOK(0x4F71F0, RainObj_GetInfoHandler);

void RainObj_DeleteHandler(RainObj *r)
{
    int i;

    VarString_Free(&r->trigger);
    Mem_Free(r->dropMem);
    r->dropMem = NULL;
    gRainMemory -= 0x45B0;
    for (i = 0; i < 8; i++) {
        if (gRainObjs[i] == r) {
            gRainObjs[i] = NULL;
            break;
        }
    }
    DynCube_ListRemove(gCurrentWorld, r, 0);
}
HOOK(0x4F72E0, RainObj_DeleteHandler);

void RainObj_RenderHandler(RainObj *r);
void RainObj_GetParamTableHandler(void *task, void **out);
void RainObj_ParamChangedHandler(RainObj *r);
void RainObj_Draw(RainPrim *prim);

void RainObj_Open(void)
{
    RainObj r;
    int i;

    gRainObjType = TaskType_New("RainDynCubeObj", sizeof(RainObj), gDynCubeObjType, 0, 0);
    TaskType_SetHandler(gRainObjType, 0, RainObj_CreateHandler);
    TaskType_SetHandler2(gRainObjType, 1, RainObj_RunHandler);
    TaskType_SetHandler2(gRainObjType, 2, RainObj_DeleteHandler);
    TaskType_SetHandler(gRainObjType, 4, RainObj_RenderHandler);
    TaskType_SetHandler(gRainObjType, Param_GetChangedEvent(), RainObj_ParamChangedHandler);
    TaskType_SetHandler(gRainObjType, Param_GetTableEvent(), RainObj_GetParamTableHandler);
    TaskType_SetHandler(gRainObjType, 7, RainObj_GetInfoHandler);
    gRainParams = ParamTable_New(15);
    ParamTable_Add(gRainParams, &r, &r.center, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.sizeX, Param_Real64(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.sizeY, Param_Real64(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.size, Param_Real32(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.density, Param_Real32(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.maxDensity, Param_Real32(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.windX, Param_Real32(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.windY, Param_Real32(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.fadeTime, Param_Real32(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.noCollide, Param_Bool8(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.trigger, Param_VarString(), 7, NULL);
    gRainLineId = Primitive_Register("Line");
    Primitive_SetHandler(gRainLineId, RainObj_Draw);
    Random_Init(gRainRandom);
    Random_Seed(gRainRandom, Timer_GetMs());
    for (i = 0; i < 8; i++)
        gRainObjs[i] = NULL;
    gRainMemory = 0;
}
HOOK(0x4F7360, RainObj_Open);

void RainObj_GetParamTableHandler(void *task, void **out)
{
    *out = gRainParams;
}
HOOK(0x4F7620, RainObj_GetParamTableHandler);

/* each drop is drawn once in every visible cube */
void RainObj_Draw(RainPrim *prim)
{
    RainObj *r = prim->rain;
    float color[3];
    int i, n;

    color[0] = r->color.x * kColorScale;
    color[1] = r->color.y * kColorScale;
    color[2] = r->color.z * kColorScale;
    for (i = 0; i < RAIN_CELLS; i++) {
        RainCell *c = cell_at(r, i);

        if (!c->visible)
            continue;
        for (n = 0; n < r->dropCount; n++) {
            RainDrop *d = r->drops[n];
            Vec3d a, b;
            long double z;

            a.x = d->pos.x + c->x;
            a.y = d->pos.y + c->y;
            a.z = d->pos.z + c->z;
            b.x = d->pos.x + c->x;
            b.y = d->pos.y + c->y;
            b.z = d->pos.z + c->z;
            b.x = d->trail.x + b.x;
            b.y = d->trail.y + b.y;
            z = d->trail.z + b.z;
            b.x = b.x - r->move.x;
            b.y = b.y - r->move.y;
            b.z = z - r->move.z;
            Line3D_Draw2(&a, &b, color, 0.0f, color, d->alpha * c->density);
        }
    }
}
HOOK(0x4F7630, RainObj_Draw);

void RainObj_UpdateCells(RainObj *r);
void RainObj_QueueDraw(RainObj *r);

static float clamp_move(float m)
{
    if (!(m >= -kMaxMove))
        return -kMaxMove;
    if (m > kMaxMove)
        return kMaxMove;
    return m;
}

void RainObj_RenderHandler(RainObj *r)
{
    Matrix view;

    Level_IsRunning();
    if (!x87_eq(r->pos.x, kZeroD)) {
        r->move.x = gViewPos.x - r->pos.x;
        r->move.y = gViewPos.y - r->pos.y;
        r->move.z = gViewPos.z - r->pos.z;
        r->move.x = clamp_move(r->move.x) * kHalf;
        r->move.y = clamp_move(r->move.y) * kHalf;
        r->move.z = clamp_move(r->move.z) * kHalf;
    }
    r->pos = gViewPos;
    view = gViewMatrix;
    r->viewDir.x = view.m[6];
    r->viewDir.y = view.m[7];
    r->viewDir.z = view.m[8];
    RainObj_UpdateCells(r);
    if (r->cameraInside)
        RainObj_QueueDraw(r);
}
HOOK(0x4F77C0, RainObj_RenderHandler);

void RainObj_QueueDraw(RainObj *r)
{
    RainPrim *p = Frame_Alloc(sizeof(RainPrim));

    p->type = gRainLineId;
    p->rain = r;
    RenderQueue_Add(0, p);
}
HOOK(0x4F7960, RainObj_QueueDraw);

void RainObj_WrapCell(RainObj *r, RainCell *c, float size);
void RainObj_CheckCell(RainObj *r, RainCell *c);
int RainObj_IsCameraCellVisible(RainObj *r);

/* moves the cells along with the camera, culls them, and hides the cubes
   below the ground or a roof */
void RainObj_UpdateCells(RainObj *r)
{
    Vec3d pts[49];
    double dz[49];
    int colI[49], colJ[49];
    float wrap = r->cellSize * kSeven;
    int i, j, k, n, left;

    gRainVisibleCubes = 0;
    for (i = 0; i < RAIN_CELLS; i++) {
        RainObj_WrapCell(r, cell_at(r, i), wrap);
        RainObj_CheckCell(r, cell_at(r, i));
    }
    if (r->noCollide)
        goto done;
    n = 0;
    for (i = 0; i < 7; i++) {
        for (j = 0; j < 7; j++) {
            for (k = 0; k < 7 && !r->cells[i][j][k].visible; k++)
                ;
            if (k == 7)
                continue;
            pts[n].x = r->cells[i][j][6].x;
            pts[n].y = r->cells[i][j][6].y;
            pts[n].z = r->cells[i][j][6].z;
            pts[n].z = pts[n].z + kDropHeight;
            colI[n] = i;
            colJ[n] = j;
            dz[n] = kDropDepth;
            n++;
        }
    }
    for (left = n, k = 0; left > 0; left -= RAIN_BATCH, k += RAIN_BATCH)
        Coll_DropPointsMat(gCurrentWorld, &r->hits[k], &pts[k], left > RAIN_BATCH ? RAIN_BATCH : left, &dz[k], 0xFF,
                           0, 0xFFFFFFFF);
    for (k = 0; k < n; k++) {
        if (!r->hits[k].found)
            continue;
        for (j = 0; j < 7; j++) {
            RainCell *c = &r->cells[colI[k]][colJ[k]][j];

            if (!(c->z - r->cellSize * kHalf >= r->hits[k].pos.z))
                c->visible = 0;
        }
    }
    r->cameraCellVisible = RainObj_IsCameraCellVisible(r);
    r->hitCount = n;
done:
    Debug_printf("Visible Cubes: %d\n", gRainVisibleCubes);
}
HOOK(0x4F7990, RainObj_UpdateCells);

int RainObj_IsCameraCellVisible(RainObj *r)
{
    long double half = r->cellSize * kHalf;
    int v = 0;
    int i;

    for (i = 0; i < RAIN_CELLS; i++) {
        RainCell *c = cell_at(r, i);
        long double dx = c->x - r->pos.x;
        float dy = c->y - r->pos.y;
        float dz = c->z - r->pos.z;

        if (__builtin_fabsl(dx) <= half && __builtin_fabsf(dy) <= half && __builtin_fabsf(dz) <= half)
            v |= c->visible;
    }
    return v;
}
HOOK(0x4F7C20, RainObj_IsCameraCellVisible);

/* keeps the cell within half a grid of the camera */
void RainObj_WrapCell(RainObj *r, RainCell *c, float size)
{
    double dx = c->x - r->pos.x;
    double dy = c->y - r->pos.y;
    double dz = c->z - r->pos.z;
    double nx, ny;
    long double nz, half;
    float fz;

    nx = c->x - (dx - crt_fmod(dx, size));
    c->x = nx;
    ny = c->y - (dy - crt_fmod(dy, size));
    c->y = ny;
    fz = crt_fmod(dz, size);
    nz = c->z - (dz - fz);
    c->z = nz;
    half = size * kHalf;
    if (!(half >= dx))
        c->x = nx - size;
    if (!(half >= dy))
        c->y = ny - size;
    if (dz > half)
        c->z = nz - size;
    half = size * kNegHalf;
    if (half > dx)
        c->x = size + c->x;
    if (half > dy)
        c->y = size + c->y;
    if (!(dz >= half))
        c->z = size + c->z;
}
HOOK(0x4F7CC0, RainObj_WrapCell);

/* cells further than one cube that are outside the view cone are hidden */
void RainObj_CheckCell(RainObj *r, RainCell *c)
{
    Vec3f d;
    long double z, inv;
    float len;

    d.x = c->x - r->pos.x;
    d.y = c->y - r->pos.y;
    z = c->z - r->pos.z;
    d.z = z;
    len = x87_sqrt(d.x * d.x + d.y * d.y + z * z);
    if (!(len < r->cellSize)) {
        if (!vec_is_zero(&d)) {
            inv = kOneD / len;
            d.x = d.x * inv;
            d.y = d.y * inv;
            d.z = d.z * inv;
        }
        if (__builtin_fabsl(Vec3_Angle(&d, &r->viewDir)) > kMaxAngle) {
            c->visible = 0;
            return;
        }
    }
    gRainVisibleCubes++;
    c->visible = 1;
}
HOOK(0x4F7DF0, RainObj_CheckCell);

void RainObj_AllocDrops(RainObj *r);

void RainObj_ParamChangedHandler(RainObj *r)
{
    int inWorld = r->inWorld;
    float size = r->size * kUnits * kInvSeven;

    r->pos.x = 0;
    r->pos.y = 0;
    r->pos.z = 0;
    r->cellSize = size;
    r->color.x = 104.0f;
    r->color.y = 112.0f;
    r->color.z = 127.0f;
    r->spread = kSpread;
    r->speed = kSpeed;
    r->trailScale = 1.5f;
    r->spawnRate = 2.0f;
    if (!inWorld) {
        RainObj_AllocDrops(r);
        if (!r->inWorld)
            goto add;
    }
    DynCube_ListRemove(gCurrentWorld, r, 0);
    r->inWorld = 0;
add:
    DynCubeObj_ClearRenderData(r);
    DynCube_ListAdd(gCurrentWorld, r, 0);
    r->inWorld = 1;
    gRainMemory += sizeof(RainObj);
    VarString_Bind(&r->trigger, r);
    RainObj_PlaceCells(r);
    r->intensity = 0.0f;
}
HOOK(0x4F7F00, RainObj_ParamChangedHandler);

void RainObj_AllocDrops(RainObj *r)
{
    char mode = Mem_GetMode();
    int i;

    if (!mode)
        Mem_SetDefaultMode();
    r->dropMem = Mem_Alloc(RAIN_DROPS * sizeof(RainDrop), 4);
    if (!mode)
        Mem_ClearMode();
    for (i = 0; i < RAIN_DROPS; i++)
        r->drops[i] = r->dropMem + i;
    gRainMemory += RAIN_DROPS * sizeof(RainDrop);
}
HOOK(0x4F8040, RainObj_AllocDrops);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E2CC) uint16_t gRainObjType INIT(385);
PAD(igd, 54E2CC, 54E2CE, 0x2);
BSS(A76C88) int gRainVisibleCubes;
BSS(A76C88) int gRainLineId;
PAD(igb, A76C88, A76C90, 0xC);
ALIAS(gRainRandom, pad_A76C90, 0);
BSS(A76C88) int gRainMemory;
PAD(igb, A76C88, A76CA0, 0x4);
PAD(igb, A76C88, A76CA4, 0x20);
ALIAS(gRainObjs, pad_A76CA4, 0);
BSS(A76C88) RainCallback gRainCallback;
PAD(igb, A76C88, A76CC8, 0x4);
BSS(A76C88) void *gRainParams;
