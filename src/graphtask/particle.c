/* "Particle2DDynCubeObjTask": a group of screen facing sprite particles
   (smoke, sparks, blood) that lives in the dyncube tree. */
#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "igi.h"
#include "math/x87.h"
#include "graphtask/graphtask.h"

typedef struct SpriteAnim {
    char unk0[4];
    int16_t count;
    char unk6[10];
    uint8_t frames[1];  /* 0x10, frame number + 1 per animation step */
} SpriteAnim;

typedef struct Sprite {
    char unk0[0xC];
    SpriteAnim *anim;
} Sprite;

typedef struct Particle2D {
    Vec3d pos;
    Vec3f vel;          /* 0x18 */
    int16_t collideTimer; /* 0x24, frames to the next collision check */
    int16_t delay;      /* 0x26, frames before it starts to fade */
    int16_t frame;      /* 0x28 */
    int16_t frameTimer; /* 0x2A */
    float spin;         /* 0x2C */
    float level;        /* 0x30, fades to 0 */
    float fade;         /* 0x34 */
    float angle;        /* 0x38 */
    float scale;        /* 0x3C */
    Vec3d lastPos;      /* 0x40, at the last collision check */
} Particle2D;

/* passed to the hit callback */
typedef struct Particle2DHit {
    CollHit *hit;
    int unk4;
    Vec3d from;
    Vec3d to;
} Particle2DHit;

typedef struct Particle2DTask {
    Task task;
    char unk20[0x48];
    Particle2D *buf;    /* 0x68 */
    Particle2D **list;  /* 0x6C */
    CollHit *hits;      /* 0x70 */
    Vec3d *starts;      /* 0x74 */
    Vec3d *ends;        /* 0x78 */
    int *idx;           /* 0x7C */
    Sprite *sprite;     /* 0x80 */
    int unk84;
    Vec3d min;          /* 0x88 */
    Vec3d max;          /* 0xA0 */
    void *world;        /* 0xB8 */
    float size[2];      /* 0xBC */
    Vec3f gravity;      /* 0xC4 */
    Vec3f color;        /* 0xD0 */
    float light;        /* 0xDC */
    float maxDepth;     /* 0xE0 */
    float grow;         /* 0xE4 */
    uint32_t flags;     /* 0xE8 */
    void (*callback)(Particle2DHit *hit); /* 0xEC */
    int count;          /* 0xF0 */
    int frameDelay;     /* 0xF4 */
    int capacity;       /* 0xF8 */
    char animated;      /* 0xFC */
    char autoDelete;    /* 0xFD */
    char unkFE[2];
} Particle2DTask;

/* a traced collision segment, for debugging */
typedef struct Particle2DTrail {
    Vec3d from;
    Vec3d to;
} Particle2DTrail;

_Static_assert(sizeof(Particle2D) == 0x58, "Particle2D");
_Static_assert(sizeof(Particle2DHit) == 0x38, "Particle2DHit");
_Static_assert(offsetof(Particle2DTask, min) == 0x88, "Particle2DTask.min");
_Static_assert(sizeof(Particle2DTask) == 0x100, "Particle2DTask");

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x497F20, void, TransContext_Project, (float *out, float *depth, const Vec3d *pos));
ORIG(0x498040, void, TransContext_ScaleSize, (float *out, const float *size, float depth));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B47E0, void, Random_Seed, (void *r, unsigned seed));
ORIG(0x4B53B0, void, QVertex_DrawQuadEx, (Sprite *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float r, float g, float b, float z, float a, int frame,
                                          unsigned flags, int unk));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4C7760, void, DynCubeObj_ClearRenderData, (void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, Vec3d *starts, Vec3d *ends, int count, int event,
                                  void *msg, int flags));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));

extern uint16_t gDynCubeObjType;
extern uint16_t gParticle2DType;
extern uint8_t gParticle2DEvent;
extern Particle2DTrail gParticle2DTrails[];
#define gParticle2DRandom ((void *)0xA7A4C0)
extern int gParticle2DMem;
extern int gParticle2DMemMax;
extern int gParticle2DTrailIdx;
extern int16_t gParticle2DTimer;
extern char *gLights;
extern int *gLightIndex;
extern int gLightCount;
extern int gLightStride;
extern float gAmbient[];

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(kHalf, 0x3F000000);
F32(kNear, 0x45000000);            /* 2048 */
F32(kMeter, 0x45800000);           /* 4096 */
F32(kRadiusPad, 0x46000000);       /* 8192 */
F32(kInv127, 0x3C010204);
F32(k255, 0x437F0000);
F32(kPi, 0x40490FDB);
F32(kTwoPi, 0x40C90FDB);
F64(kOneD, 0x3FF0000000000000ull);
F64(kHalfD, 0x3FE0000000000000ull);
F64(kBoxSlack, 0x40C8000000000000ull); /* 12288 */
F64(kAxisLimit, 0x3FE8000000000000ull); /* 0.75 */
F64(kDblMax, 0x7FEFFFFFFFFFFFFFull);

void Particle2D_Create(Particle2DTask *t);
void Particle2D_Run(Particle2DTask *t);
void Particle2D_Delete(Particle2DTask *t);
void Particle2D_Draw(Particle2DTask *t);
void Particle2D_AddToWorld(Particle2DTask *t, void **msg);

void Particle2D_Open(void)
{
    gParticle2DType = TaskType_New("Particle2DDynCubeObjTask", sizeof(Particle2DTask), gDynCubeObjType, 25, 0);
    gParticle2DEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gParticle2DType, 0, Particle2D_Create);
    TaskType_SetHandler2(gParticle2DType, 1, Particle2D_Run);
    TaskType_SetHandler2(gParticle2DType, 2, Particle2D_Delete);
    TaskType_SetHandler(gParticle2DType, 4, Particle2D_Draw);
    TaskType_SetHandler(gParticle2DType, World_GetEvent0(), Particle2D_AddToWorld);
    Random_Init(gParticle2DRandom);
    Random_Seed(gParticle2DRandom, 0x12332153);
    gParticle2DMem = 0;
    gParticle2DMemMax = 0;
}
HOOK(0x4FCF50, Particle2D_Open);

void Particle2D_GetLight(Particle2DTask *t, float *out);

void Particle2D_Draw(Particle2DTask *t)
{
    float light[3] = {0, 0, 0};
    float r, g, b, alpha = 0; /* the original leaves alpha unset when there are no flags */
    int i = 0;

    if (t->light > kZero.f) {
        long double cx, cz, m;
        float cy;

        Particle2D_GetLight(t, light);
        cx = light[0] + (long double)t->color.x;
        cy = light[1] + t->color.y;
        cz = light[2] + (long double)t->color.z;
        m = cy > cz ? cy : cz;
        m = cx > m ? cx : m;
        m = kOne.f > m ? kOne.f : m;
        m = m * kInv127.f;
        r = cx / m;
        g = cy / m;
        b = cz / m;
    } else {
        r = t->color.x;
        g = t->color.y;
        b = t->color.z;
    }
    if (!t->count)
        return;
    do {
        Particle2D *p = t->list[i];
        float scr[2], depth, size[2], half[2];
        float q[8];
        int frame;

        TransContext_Project(scr, &depth, &p->pos);
        if (!(depth > kNear.f) || depth >= t->maxDepth)
            continue;
        size[0] = t->size[0];
        size[1] = t->size[1];
        if (NONZERO(t->grow)) {
            size[0] = size[0] * p->scale;
            size[1] = size[1] * p->scale;
        }
        TransContext_ScaleSize(half, size, depth);
        if (!(half[0] >= kHalf.f))
            half[0] = kHalf.f;
        if (!(half[1] >= kHalf.f))
            half[1] = kHalf.f;
        if (NONZERO(p->spin)) {
            long double s, hc, sh, ch;
            float c, t1, t2, t3, t4, d1;

            c = x87_cos(p->angle);
            s = x87_sin(p->angle);
            t1 = -half[1] * s;
            t2 = c * half[0];
            q[0] = t1 - t2;
            hc = -half[1] * c;
            t3 = -s * half[0];
            q[1] = hc - t3;
            q[2] = t2 + t1;
            q[3] = t3 + hc;
            sh = s * half[1];
            q[4] = sh - t2;
            ch = c * half[1];
            t4 = ch;
            q[5] = ch - t3;
            d1 = t4 + t3;
            q[0] = scr[0] + q[0];
            q[1] = scr[1] + q[1];
            q[2] = scr[0] + q[2];
            q[3] = scr[1] + q[3];
            q[4] = scr[0] + q[4];
            q[5] = scr[1] + q[5];
            q[6] = scr[0] + (sh + t2);
            q[7] = scr[1] + d1;
        } else {
            q[0] = scr[0] - half[0];
            q[1] = scr[1] - half[1];
            q[2] = scr[0] + half[0];
            q[3] = q[1];
            q[4] = q[0];
            q[5] = scr[1] + half[1];
            q[6] = q[2];
            q[7] = q[5];
        }
        if (t->flags) {
            if (t->flags & 0x80000) {
                alpha = p->level;
            } else {
                alpha = kOne.f;
                r = t->color.x * p->level;
                g = t->color.y * p->level;
                b = t->color.z * p->level;
            }
        }
        frame = t->animated ? t->sprite->anim->frames[p->frame] - 1 : 0;
        QVertex_DrawQuadEx(t->sprite, q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7], r, g, b, depth, alpha, frame,
                           t->flags | 0x1000000, 0);
    } while (++i < t->count);
}
HOOK(0x4FD020, Particle2D_Draw);

/* ambient plus the active lights, scaled by the light factor, 0..255 */
void Particle2D_GetLight(Particle2DTask *t, float *out)
{
    long double sx = kZero.f, sy = kZero.f, sz = kZero.f, x;
    float fy, fz, y, z;
    int i;

    for (i = 0; i < gLightCount; i++) {
        float *l = (float *)(gLights + gLightIndex[i] * gLightStride);

        sx = sx + l[3];
        sy = sy + l[4];
        sz = sz + l[5];
    }
    fz = sz;
    fy = sy;
    x = sx + gAmbient[0];
    out[0] = x;
    y = gAmbient[1] + fy;
    out[1] = y;
    z = gAmbient[2] + fz;
    out[2] = z;
    out[0] = x * t->light * k255.f;
    out[1] = y * t->light * k255.f;
    out[2] = z * t->light * k255.f;
}
HOOK(0x4FD4E0, Particle2D_GetLight);

void Particle2D_Create(Particle2DTask *t)
{
    t->min.x = kDblMax.d;
    t->min.y = kDblMax.d;
    t->min.z = kDblMax.d;
    t->max.x = -kDblMax.d;
    t->max.y = -kDblMax.d;
    t->max.z = -kDblMax.d;
    t->list = NULL;
    t->count = 0;
    t->autoDelete = 1;
    t->animated = 0;
    t->hits = NULL;
    t->starts = NULL;
    t->ends = NULL;
    t->idx = NULL;
}
HOOK(0x4FD5E0, Particle2D_Create);

void Particle2D_Remove(Particle2DTask *t, int i);
long double Particle2D_GetRadius(Particle2DTask *t);
void Particle2D_GetCenter(Vec3d *out, Particle2DTask *t);

/* moves, animates and fades the particles, keeps the bounding box in the
   dyncube tree and every 15 frames traces each particle for hits */
void Particle2D_Run(Particle2DTask *t)
{
    Vec3d max = {-kDblMax.d, -kDblMax.d, -kDblMax.d};
    Vec3d min = {kDblMax.d, kDblMax.d, kDblMax.d};
    int i = 0, last = 0, n, j;

    if (!t->count) {
        if (t->autoDelete)
            Task_Delete(t);
        return;
    }
    if (t->animated)
        last = t->sprite->anim->count - 1;
    do {
        Particle2D *p = t->list[i];

        p->vel.x = p->vel.x + t->gravity.x;
        p->vel.y = t->gravity.y + p->vel.y;
        p->vel.z = t->gravity.z + p->vel.z;
        p->pos.x = p->vel.x + p->pos.x;
        p->pos.y = p->vel.y + p->pos.y;
        p->pos.z = p->vel.z + p->pos.z;
        p->angle = p->spin + p->angle;
        p->scale = t->grow + p->scale;
        if (last && ++p->frameTimer >= t->frameDelay) {
            p->frameTimer = 0;
            if (p->frame < last)
                p->frame++;
        }
        if (p->delay) {
            p->delay--;
            i++;
        } else {
            long double level = (long double)p->level - p->fade;

            p->level = level;
            if (level > kZero.f && p->scale > kZero.f)
                i++;
            else
                Particle2D_Remove(t, i);
        }
    } while (i < t->count);

    if (t->count) {
        Vec3d dmin, dmax;

        for (i = 0; i < t->count; i++) {
            Particle2D *p = t->list[i];

            if (!(max.x >= p->pos.x))
                max.x = p->pos.x;
            if (!(max.y >= p->pos.y))
                max.y = p->pos.y;
            if (!(max.z >= p->pos.z))
                max.z = p->pos.z;
            if (min.x > p->pos.x)
                min.x = p->pos.x;
            if (min.y > p->pos.y)
                min.y = p->pos.y;
            if (min.z > p->pos.z)
                min.z = p->pos.z;
        }
        dmin.x = min.x - t->min.x;
        dmin.y = min.y - t->min.y;
        dmin.z = min.z - t->min.z;
        dmax.x = max.x - t->max.x;
        dmax.y = max.y - t->max.y;
        dmax.z = max.z - t->max.z;
        /* the last test was probably meant against -12288; as it is the
           box is updated every frame */
        if (!(dmin.x >= 0) || !(dmin.y >= 0) || !(dmin.z >= 0) || dmax.x > 0 || dmax.y > 0 || dmax.z > 0
            || !(dmin.x <= kBoxSlack.d) || !(dmin.y <= kBoxSlack.d) || !(dmin.z <= kBoxSlack.d)
            || !(dmax.x >= kBoxSlack.d) || !(dmax.y >= kBoxSlack.d) || !(dmax.z >= kBoxSlack.d)) {
            Vec3d c;
            float radius;

            t->min = min;
            t->max = max;
            radius = Particle2D_GetRadius(t);
            Particle2D_GetCenter(&c, t);
            DynCube_Move(t->world, t, &c, radius);
        }
    }

    if (t->callback) {
        struct {
            int unk0;
            int unk4;
        } msg;

        n = 0;
        for (i = 0; i < t->count; i++) {
            Particle2D *p = t->list[i];

            if (p->collideTimer)
                p->collideTimer--;
            if (!p->collideTimer) {
                t->idx[n] = i;
                t->starts[n] = p->lastPos;
                t->ends[n] = p->pos;
                n++;
                p->collideTimer = 15;
                p->lastPos = p->pos;
            }
        }
        msg.unk0 = 0; /* never set by the original */
        msg.unk4 = 0;
        Coll_Lines(t->world, t->hits, t->starts, t->ends, n, gParticle2DEvent, &msg, 0x20000000);
        for (j = 0; j < n; j++) {
            Particle2DHit hit;

            if (!t->hits[j].found)
                continue;
            gParticle2DTrails[gParticle2DTrailIdx].from = t->starts[j];
            gParticle2DTrails[gParticle2DTrailIdx].to = t->ends[j];
            if (++gParticle2DTrailIdx == 256)
                gParticle2DTrailIdx = 0;
            hit.hit = &t->hits[j];
            hit.unk4 = 0; /* never set by the original */
            hit.from = t->starts[j];
            hit.to = t->ends[j];
            t->callback(&hit);
            Particle2D_Remove(t, t->idx[j]);
        }
    }
}
HOOK(0x4FD660, Particle2D_Run);

/* half the bounding box diagonal plus 2 meters, at least 1 */
long double Particle2D_GetRadius(Particle2DTask *t)
{
    Vec3d d;
    long double r;

    d.x = t->max.x - t->min.x;
    d.y = t->max.y - t->min.y;
    d.z = t->max.z - t->min.z;
    if (!NONZERO(d.x) && !NONZERO(d.y) && !NONZERO(d.z))
        return kOne.f;
    r = Vec3d_Length(&d) * kHalfD.d + kRadiusPad.f;
    if (!(r >= kOne.f))
        r = kOne.f;
    return r;
}
HOOK(0x4FDC20, Particle2D_GetRadius);

void Particle2D_GetCenter(Vec3d *out, Particle2DTask *t)
{
    out->x = t->max.x + t->min.x;
    out->y = t->max.y + t->min.y;
    out->z = t->max.z + t->min.z;
    out->x = out->x * kHalfD.d;
    out->y = out->y * kHalfD.d;
    out->z = out->z * kHalfD.d;
}
HOOK(0x4FDCD0, Particle2D_GetCenter);

/* swaps particle i with the last one and drops it */
void Particle2D_Remove(Particle2DTask *t, int i)
{
    if (!t->count)
        return;
    if (t->count - 1 != i) {
        Particle2D *p = t->list[i];

        t->list[i] = t->list[t->count - 1];
        t->list[t->count - 1] = p;
    }
    t->count--;
}
HOOK(0x4FDD30, Particle2D_Remove);

void Particle2D_Delete(Particle2DTask *t)
{
    Mem_Free(t->buf);
    t->buf = NULL;
    Mem_Free(t->list);
    t->list = NULL;
    Mem_Free(t->hits);
    t->hits = NULL;
    Mem_Free(t->idx);
    t->idx = NULL;
    Mem_Free(t->starts);
    t->starts = NULL;
    Mem_Free(t->ends);
    t->ends = NULL;
    DynCube_Remove(t->world, t);
    gParticle2DMem += -0x100 - t->capacity * (int)sizeof(Particle2D);
}
HOOK(0x4FDD70, Particle2D_Delete);

/* World event 0: adds the group to msg[1]'s dyncube tree, then passes the
   event on to the children */
void Particle2D_AddToWorld(Particle2DTask *t, void **msg)
{
    float radius = Particle2D_GetRadius(t);
    Vec3d c;
    Task *ch;

    Particle2D_GetCenter(&c, t);
    DynCube_Add(msg[1], t, &c, radius, 2);
    ch = (Task *)t->task.children.head;
    if (!ch->node.next || !ch)
        return;
    do {
        TaskFn fn;
        Task *next = (Task *)ch->node.next;

        gTaskStack[gTaskStackTop++] = next && next->node.next ? next : NULL;
        fn = HANDLER(World_GetEvent0(), ch->type);
        if (fn)
            fn(ch, msg);
        ch = gTaskStack[--gTaskStackTop];
    } while (ch);
}
HOOK(0x4FDE00, Particle2D_AddToWorld);

void Particle2D_Close(void)
{
    TaskEvent_Free(gParticle2DEvent);
    TaskType_Free(gParticle2DType);
    gParticle2DType = 0x181;
}
HOOK(0x4FDEC0, Particle2D_Close);

uint8_t Particle2D_GetCollideEvent(void)
{
    return gParticle2DEvent;
}
HOOK(0x4FDEF0, Particle2D_GetCollideEvent);

/* a new particle group; flags are the QVertex blend flags (0x80000: the
   level is the alpha, else it scales the color) */
Particle2DTask *Particle2D_New(void *parent, void *world, Sprite *sprite, const Vec3f *gravity, const Vec3f *color,
                               const float *size, float maxDepth, uint32_t flags, int frameDelay, float grow,
                               float light, int capacity, void (*callback)(Particle2DHit *hit))
{
    Vec3d zero = {0, 0, 0};
    Particle2DTask *t = (Particle2DTask *)Task_New(parent, gParticle2DType, NULL);
    int i;

    DynCubeObj_ClearRenderData(t);
    DynCube_Add(world, t, &zero, kMeter.f, 2);
    t->capacity = capacity;
    t->world = world;
    t->gravity = *gravity;
    t->color = *color;
    t->size[0] = size[0];
    t->size[1] = size[1];
    t->maxDepth = maxDepth;
    t->frameDelay = frameDelay;
    t->flags = flags | 0x40000;
    t->grow = grow;
    t->animated = 0;
    t->sprite = sprite;
    if (sprite->anim)
        t->animated = 1;
    t->light = light;
    t->callback = callback;
    Mem_PushMode(1);
    t->buf = Mem_Alloc(t->capacity * sizeof(Particle2D), 4);
    t->list = Mem_Alloc(t->capacity * sizeof(Particle2D *), 4);
    t->idx = Mem_Alloc(t->capacity * sizeof(int), 4);
    t->hits = Mem_Alloc(t->capacity * sizeof(CollHit), 4);
    t->starts = Mem_Alloc(t->capacity * sizeof(Vec3d), 4);
    t->ends = Mem_Alloc(t->capacity * sizeof(Vec3d), 4);
    Mem_PopMode();
    for (i = 0; i < t->capacity; i++)
        t->list[i] = &t->buf[i];
    gParticle2DMem += t->capacity * (int)sizeof(Particle2D) + 0x100;
    if (gParticle2DMemMax <= gParticle2DMem)
        gParticle2DMemMax = gParticle2DMem;
    return t;
}
HOOK(0x4FDF00, Particle2D_New);

/* frames: how many frames the level takes to fade from 1 to 0 (0 for one) */
void Particle2D_Add(Particle2DTask *t, const Vec3d *pos, const float *vel, float level, float spin, float angle,
                    float scale, int delay, int frames)
{
    Particle2D *p;

    if (t->count >= t->capacity)
        return;
    p = t->list[t->count];
    p->pos = *pos;
    p->lastPos = *pos;
    p->vel.x = vel[0];
    p->vel.y = vel[1];
    p->vel.z = vel[2];
    p->collideTimer = gParticle2DTimer;
    gParticle2DTimer = (int16_t)(gParticle2DTimer + 1) % 16;
    p->delay = delay;
    p->level = level;
    p->angle = angle;
    p->scale = scale;
    p->spin = spin;
    p->frame = 0;
    p->frameTimer = 0;
    p->fade = frames ? kOne.f / (long double)frames : kOne.f;
    t->count++;
}
HOOK(0x4FE120, Particle2D_Add);

/* one particle from pos, its direction a random tilt of up to spread from dir */
void Particle2D_Emit(Particle2DTask *t, const Vec3d *pos, const float *dir, float spread, float speedMin,
                     float speedMax, float level, float spin, float angle, float scale, int delay, int frames)
{
    float u[3], v[3], w[3], c[3], d[3], e[2], vel[3], tilt, ax, az;
    long double a, b, speed, ez;
    Matrix m;
    int i;

    u[0] = dir[0];
    u[1] = dir[1];
    u[2] = dir[2];
    if (NONZERO(u[0]) || NONZERO(u[1]) || NONZERO(u[2])) {
        long double inv = kOneD.d / x87_sqrt(((long double)u[0] * u[0] + (long double)u[1] * u[1])
                                             + (long double)u[2] * u[2]);

        for (i = 0; i < 3; i++)
            u[i] = u[i] * inv;
    }
    if (__builtin_fabsl(u[0]) > kAxisLimit.d) {
        a = kOne.f;
        b = kZero.f;
    } else {
        a = kZero.f;
        b = kOne.f;
    }
    v[0] = a * u[2];
    v[1] = -(b * u[2]);
    v[2] = b * u[1] - a * u[0];
    w[0] = v[0];
    w[1] = v[1];
    w[2] = v[2];
    if (NONZERO(v[0]) || NONZERO(v[1]) || NONZERO(v[2])) {
        long double inv = kOneD.d / x87_sqrt(((long double)v[0] * v[0] + (long double)v[1] * v[1])
                                             + (long double)v[2] * v[2]);

        for (i = 0; i < 3; i++)
            w[i] = v[i] * inv;
    }
    c[0] = (long double)w[1] * u[2] - (long double)w[2] * u[1];
    c[1] = -((long double)w[0] * u[2] - (long double)w[2] * u[0]);
    c[2] = (long double)w[0] * u[1] - (long double)w[1] * u[0];
    tilt = Random_Float(gParticle2DRandom) * spread;
    az = kPi.f - Random_Float(gParticle2DRandom) * kTwoPi.f;
    ax = spread * kHalf.f - tilt;
    Matrix_SetEuler(&m, ax, 0, az);
    for (i = 0; i < 3; i++)
        d[i] = ((long double)m.m[2] * c[i] + (long double)m.m[5] * w[i]) + (long double)m.m[8] * u[i];
    speed = ((long double)speedMax - speedMin) * Random_Float(gParticle2DRandom) + speedMin;
    if (NONZERO(d[0]) || NONZERO(d[1]) || NONZERO(d[2])) {
        long double inv = kOneD.d / x87_sqrt(((long double)d[0] * d[0] + (long double)d[1] * d[1])
                                             + (long double)d[2] * d[2]);

        e[0] = d[0] * inv;
        e[1] = d[1] * inv;
        ez = inv * d[2];
    } else {
        e[0] = d[0];
        e[1] = d[1];
        ez = d[2];
    }
    vel[0] = e[0] * speed;
    vel[1] = e[1] * speed;
    vel[2] = ez * speed;
    Particle2D_Add(t, pos, vel, level, spin, angle, scale, delay, frames);
}
HOOK(0x4FE210, Particle2D_Emit);
