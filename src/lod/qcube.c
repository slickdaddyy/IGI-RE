/* QCube: the per frame walk that collects visible objects, and drawing them. */
#pragma GCC optimize("excess-precision=standard")
#include <stddef.h>
#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "core/pool.h"
#include "transctx/transctx.h"

FN_EXTERN(crt_CIpow);

ORIG(0x401860, void, TaskEvent_Set, (uint16_t type, uint8_t index));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401D30, void, TaskEvent_MapSlot, (uint8_t slot, int bit));
ORIG(0x401D50, void, TaskEvent_UnmapSlot, (uint8_t slot));
ORIG(0x497520, void, Scene_SetBounds, (Scene *s, const float *min, const float *max));
ORIG(0x498070, int, TransContext_GetNearClipMethod, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0B00, void, Pool_Init_, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free_, (Pool *pool));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const double *a, const double *b));
ORIG(0x4CE090, void, Coll_SendEvent, (void *world, uint8_t slot, void *arg, const double *pos, int mask, int tree));
ORIG(0x4CE390, void, Coll_SendEventAll, (void *world, uint8_t slot, void *arg, const double *pos, int mask));
ORIG(0x4D14E0, void, QCube_Walk, (void *world));
ORIG(0x4D9800, void *, QObj_GetRenderData, (void *task));
ORIG(0x4DA2F0, void, Haze_ResetFrame, (void));
ORIG(0x4DA8B0, void, Light_ResetFrame, (void));
ORIG(0x4E5CB0, void, QCube_UpdateViewScale, (void));
ORIG(0x50F770, void, TerrainMesh_UpdateNearZ, (float *nearq));
ORIG(0x50FAE0, void, TerrainMesh_SetupLighting, (void *world, int unk));
ORIG(0x50FD00, void, TerrainMesh_SetTextures, (void *world));
ORIG(0x50FE60, void, TerrainMesh_Update, (void));
ORIG(0x511500, void, TerrainMesh_Render, (void));
ORIG(0x5116B0, void, sub_5116B0, (void *world));

/* The parts of a task the walk uses */
typedef struct QTask {
    char unk0[0x1C];
    uint16_t type;
    char unk1E[2];
    double pos[3];
    char unk38[0x10];
    void *arg;
    char unk4C[0xC];
    struct QTask *next;
} QTask;

typedef struct QWorld {
    char unk0[0xC];
    void *terrain;
    char unk10[0xD08];
    QTask *tasks;
    QTask *lists[8];
    char unk3C[0x8C];
    int ready;
    char unkDCC[0x14];
    int origin[3];
} QWorld;

/* One collected object: the handler slot to call and the task */
typedef struct QEntry {
    uint8_t slot;
    QTask *task;
} QEntry;

typedef struct QCallback {
    struct QCallback *next;
    void (*fn)(void *arg);
} QCallback;

/* Cached data of a visited cell, 2048 of them */
typedef struct QCacheEntry {
    int cell[4];
    int frame; /* -1 = free */
    int kind;  /* which pool data comes from */
    char *data;
} QCacheEntry;

/* What the enclosing event fills in: the task the camera is inside */
typedef struct QEnclosing {
    const double *pos;
    int unk4;
    int unk8;
    QTask *task;
    double box[3];
    float radius;
} QEnclosing;

_Static_assert(offsetof(QTask, next) == 0x58, "QTask");
_Static_assert(sizeof(QWorld) == 0xDEC, "QWorld");
_Static_assert(sizeof(QCacheEntry) == 0x1C, "QCacheEntry");
_Static_assert(offsetof(QEnclosing, radius) == 0x28, "QEnclosing");

typedef void (*QHandler)(QTask *task, void *arg);

extern QHandler gTaskHandlers[];
#define gRenderFuncs ((void (**)(void *))0xA94E84)
extern QEntry *gQCubeList;
extern QEntry *gQCubeOut;
extern int gQCubeCount;
extern QCacheEntry gQCubeCache[];
extern Pool gQCubePool0;
extern Pool gQCubePool1;
extern Pool gQCubePool2;
extern QCallback *gQCubeCallbacks;
extern QCallback gQCubeCallbackSlots[];
extern uint8_t gQCubeEnclosingEvent;
extern uint8_t gQCubeNearClipEvent;
extern int gQCubeDetail;
extern int gQCubeMinSize;
extern QWorld *gQCubeWorld;
extern int gQCubeOrigin[];
extern float gQCubeOriginF[];
extern double gQCubeOriginD[];
extern int gQCubeDrawOutside;
extern int gQCubeInside;
extern QTask *gQCubeEnclosing;
extern int gViewSector;
extern double gQCubeEnclosingBox[];
extern float gQCubeEnclosingRadius;
extern int gQCubeNeighbourBits[];
#define gQCubeChildOrder ((uint8_t (*)[8])0xB12BE0)
#define gQCubeBitPerm ((uint8_t (*)[256])0xB12340)
extern uint8_t gQCubeOctantMask[];
extern int gCollChildTable[];
extern float gQCubeOutsideMin[];
extern float gQCubeOutsideMax[];
extern char gNearClipText[];

static const float kOne = 1.0f;
static const double kHalf = 0.5;
static const float kNearStart = 0x1.99999ap+8f; /* 409.6 */
static const float kNearFar = 20480.0f;
static const float kNearScale = 0x1.fae148p-1f; /* 0.99 */

/* the C runtime's pow on the x87 stack (_CIpow) */
static inline long double ci_pow(long double x, long double y)
{
    long double r;

    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "r"(FN(0x4A5760, crt_CIpow)) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}

/* fistp in the current rounding mode */
static inline long long x87_round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return r;
}

void QCube_AddCallback(void (*fn)(void *));
void QCube_RemoveCallback(void (*fn)(void *));

void QCube_Init(void)
{
    int r, a, b, c, i, j, v;

    gQCubeList = Mem_Alloc(16000, 4);
    for (r = 0; r < 8; r++) {
        int bit = 0;

        for (a = -1; a <= 1; a++) {
            for (b = -1; b <= 1; b++) {
                for (c = -1; c <= 1; c++) {
                    int x = r & 4 ? -c : c, y = b, code;

                    switch (r & 3) {
                    case 1:
                        y = x;
                        x = -b;
                        break;
                    case 2:
                        y = -b;
                        x = -x;
                        break;
                    case 3:
                        y = -x;
                        x = b;
                        break;
                    }
                    code = (x < 0) | (x > 0) << 1 | (y < 0) << 2 | (y > 0) << 3 | (a < 0) << 4 | (a > 0) << 5;
                    gQCubeNeighbourBits[r * 64 + code] = 1 << bit++;
                }
            }
        }
    }
    for (r = 0; r < 8; r++)
        for (j = 0; j < 8; j++)
            gQCubeChildOrder[r][j] = r < 4 ? ((j + r) & 3) + (j & 4) : ((r - j) & 3) + (~j & 4);
    for (r = 0; r < 8; r++) {
        for (v = 0; v < 256; v++) {
            int u = 0;

            for (j = 0; j < 8; j++)
                if (v & 1 << j)
                    u |= 1 << gCollChildTable[r * 8 + j];
            gQCubeBitPerm[r][u] = v;
        }
    }
    i = 0;
    for (c = 0; c < 3; c++) {
        for (b = 0; b < 3; b++) {
            for (a = 0; a < 3; a++, i++) {
                uint8_t m = 0;

                for (j = 0; j < 8; j++)
                    if ((a == 1 || a >> 1 == (j & 1)) && (b == 1 || b == (j & 2)) && (c == 1 || c * 2 == (j & 4)))
                        m |= 1 << j;
                if (!(a & 1) && !(b & 1) && !(c & 1))
                    m = 0;
                gQCubeOctantMask[i] = m;
            }
        }
    }
    TaskEvent_Set(0, 0xF);
    TaskEvent_Set(0, 0x10);
    TaskEvent_Set(0, 0x11);
    TaskEvent_Set(0, 0x12);
    TaskEvent_MapSlot(0xF, 7);
    TaskEvent_MapSlot(0x10, 8);
    gQCubeEnclosingEvent = TaskEvent_Alloc(0x181);
    gQCubeNearClipEvent = TaskEvent_Alloc(0x181);
    QCube_AddCallback((void (*)(void *))1);
    QCube_RemoveCallback((void (*)(void *))1);
}
HOOK(0x4D0EF0, QCube_Init);

static void cache_clear(void)
{
    int i;

    for (i = 0; i < 0x800; i++) {
        gQCubeCache[i].frame = -1;
        gQCubeCache[i].data = NULL;
    }
}

void QCube_Reset(void)
{
    cache_clear();
    Pool_Init_(&gQCubePool0, 0x640, 700, 0x20);
    Pool_Init_(&gQCubePool1, 0xC80, 0x32, 0x20);
    Pool_Init_(&gQCubePool2, 8000, 0, 0x20);
}
HOOK(0x4D11C0, QCube_Reset);

void QCube_Clear(void)
{
    cache_clear();
    Pool_Free_(&gQCubePool0);
    Pool_Free_(&gQCubePool1);
    Pool_Free_(&gQCubePool2);
}
HOOK(0x4D1220, QCube_Clear);

void QCube_Shutdown(void)
{
    TaskEvent_UnmapSlot(0xF);
    TaskEvent_UnmapSlot(0x10);
    TaskEvent_Free(0xF);
    TaskEvent_Free(0x10);
    TaskEvent_Free(0x11);
    TaskEvent_Free(0x12);
    TaskEvent_Free(gQCubeEnclosingEvent);
    TaskEvent_Free(gQCubeNearClipEvent);
    Mem_Free(gQCubeList);
    gQCubeList = NULL;
}
HOOK(0x4D1260, QCube_Shutdown);

void QCube_RunCallbacks(QCallback *c, void *arg);

static void draw_entry(const QEntry *e)
{
    QTask *t = e->task;

    gTaskHandlers[e->slot * 0x180 + t->type](t, t->arg);
}

/*
 * Calls the handler of every collected object. While the camera is inside
 * a task (a room), objects outside its sphere are drawn with the outside
 * scene bounds, or not at all when gQCubeDrawOutside is clear.
 */
void QCube_DrawObjects(QWorld *w)
{
    QEntry *e = gQCubeList;
    int inside = 1, i;
    struct {
        QEntry *list;
        int count;
    } arg;

    sub_5116B0(w);
    Coll_SendEventAll(w, 0x11, 0, gTransContext.pos, 0x20000000);
    if (!gQCubeEnclosing) {
        for (i = 0; i < gQCubeCount; i++, e++)
            draw_entry(e);
    } else {
        for (i = 0; i < gQCubeCount; i++, e++) {
            float r = gQCubeEnclosingRadius;

            if (e->task->type == VAR(uint16_t, 0xC30064) ||
                !((long double)r * r >= Vec3d_DistanceSq(gQCubeEnclosingBox, e->task->pos))) {
                if (gQCubeDrawOutside) {
                    if (inside)
                        Scene_SetBounds(&gScene, gQCubeOutsideMin, gQCubeOutsideMax);
                    draw_entry(e);
                    inside = 0;
                }
            } else {
                if (!inside) {
                    float lo[2], hi[2];

                    lo[0] = gScene.x;
                    lo[1] = gScene.y;
                    hi[0] = gScene.w + gScene.x;
                    hi[1] = gScene.h + gScene.y;
                    Scene_SetBounds(&gScene, lo, hi);
                }
                draw_entry(e);
                inside = 1;
            }
        }
    }
    Coll_SendEventAll(w, 0x12, 0, gTransContext.pos, 0x20000000);
    arg.list = gQCubeList;
    arg.count = gQCubeCount;
    QCube_RunCallbacks(gQCubeCallbacks, &arg);
}
HOOK(0x4D12C0, QCube_DrawObjects);

void QCube_RunCallbacks(QCallback *c, void *arg)
{
    for (; c; c = c->next)
        c->fn(arg);
}
HOOK(0x4D14A0, QCube_RunCallbacks);

uint8_t QCube_GetEnclosingEvent(void)
{
    return gQCubeEnclosingEvent;
}
HOOK(0x4D14C0, QCube_GetEnclosingEvent);

uint8_t QCube_GetNearClipEvent(void)
{
    return gQCubeNearClipEvent;
}
HOOK(0x4D14D0, QCube_GetNearClipEvent);

/* Starts a frame: empties the list and picks the smallest cell size to visit. */
void QCube_Begin(QWorld *w)
{
    int i, d;

    gQCubeOut = gQCubeList;
    VAR(int, 0xB97DF0) = -1;
    VAR(int, 0xA538DC) = 0;
    VAR(int, 0xA538E0) = 0;
    gQCubeCount = 0;
    VAR(int, 0xB12B40) = 0;
    gQCubeWorld = w;
    for (i = 0; i < 3; i++)
        gQCubeOrigin[i] = w->origin[i];
    for (i = 0; i < 3; i++)
        gQCubeOriginF[i] = w->origin[i];
    for (i = 0; i < 3; i++)
        gQCubeOriginD[i] = w->origin[i];
    VAR(uint8_t, 0xA538D8) = 0;
    if (gTransContext.fov > kOne)
        d = gQCubeDetail;
    else
        d = x87_round(gQCubeDetail / ci_pow(gTransContext.fov, kHalf));
    gQCubeMinSize = d < 0x46 ? 0x46 : d;
    QCube_Walk(w);
}
HOOK(0x4D2540, QCube_Begin);

void QCube_UpdateNearPlane(QWorld *w, const char *flags);
void QCube_RunTaskList(QTask *t);

void QCube_Render(QWorld *w, const char *flags, int full)
{
    int i;

    VAR(int, 0xB12BCC) = VAR(int, 0xB16C68);
    VAR(int, 0xB16C68) = 0;
    if (!w->ready) {
        TerrainMesh_SetupLighting(w, 0);
        w->ready = 1;
    }
    QCube_UpdateViewScale();
    Light_ResetFrame();
    Haze_ResetFrame();
    if (VAR(int, 0xBCAB38)) {
        if (full)
            QCube_Begin(w);
        if (w->terrain && full) {
            TerrainMesh_SetTextures(w);
            TerrainMesh_Update();
        }
        QCube_UpdateNearPlane(w, flags);
        if (flags[0])
            QCube_RunTaskList(w->tasks);
        if (full) {
            QEnclosing q;

            q.unk8 = 0;
            q.task = NULL;
            q.pos = gTransContext.pos;
            q.unk4 = 0;
            Coll_SendEvent(w, QCube_GetEnclosingEvent(), &q, gTransContext.pos, -1, 5);
            gViewSector = q.unk8;
            for (i = 0; i < 3; i++)
                gQCubeEnclosingBox[i] = q.box[i];
            gQCubeEnclosing = q.task;
            gQCubeEnclosingRadius = q.radius;
            if (q.task) {
                gQCubeDrawOutside = 0;
                gTaskHandlers[4 * 0x180 + q.task->type](q.task, 0);
                gQCubeInside = 1;
            }
        }
        if (!w->terrain || full) {
            if (w->terrain) {
                TerrainMesh_SetTextures(w);
                TerrainMesh_Render();
            }
            if (full)
                QCube_DrawObjects(w);
        }
        for (i = 0; i < 8; i++)
            if (flags[i + 1])
                QCube_RunTaskList(w->lists[i]);
    }
    gViewSector = 0;
    gQCubeEnclosing = NULL;
    gQCubeInside = 0;
    gQCubeDrawOutside = 1;
}
HOOK(0x4D2650, QCube_Render);

/* Runs handler slots 4 and 5 of every task on a list */
void QCube_RunTaskList(QTask *t)
{
    for (; t; t = t->next) {
        QHandler h = gTaskHandlers[4 * 0x180 + t->type];

        if (h)
            h(t, QObj_GetRenderData(t));
        h = gTaskHandlers[5 * 0x180 + t->type];
        if (h)
            h(t, QObj_GetRenderData(t));
    }
}
HOOK(0x4D27F0, QCube_RunTaskList);

static void near_clip_list(QTask *t, float *nearq)
{
    for (; t; t = t->next) {
        QHandler h = gTaskHandlers[QCube_GetNearClipEvent() * 0x180 + t->type];

        if (h)
            h(t, nearq);
    }
}

/*
 * Asks every visible object how close it gets to the camera and moves the
 * near clipping plane out to 99% of that.
 */
void QCube_UpdateNearPlane(QWorld *w, const char *flags)
{
    QEntry *e = gQCubeList;
    struct {
        float dist;
        int zbuffer;
    } arg;
    float nearq[3];
    int i;

    arg.dist = kNearStart;
    arg.zbuffer = 0;
    if (gRenderFuncs[TransContext_GetNearClipMethod()])
        gRenderFuncs[TransContext_GetNearClipMethod()](&arg);
    if (!arg.zbuffer || !VAR(int, 0x543A64))
        return;
    nearq[0] = 0;
    nearq[1] = 0;
    nearq[2] = kNearFar;
    for (i = 0; i < gQCubeCount; i++, e++) {
        if (e->slot == 4) {
            QTask *t = e->task;
            QHandler h = gTaskHandlers[QCube_GetNearClipEvent() * 0x180 + t->type];

            if (h)
                h(t, nearq);
        }
    }
    if (flags[0])
        near_clip_list(w->tasks, nearq);
    for (i = 0; i < 8; i++)
        if (flags[i + 1])
            near_clip_list(w->lists[i], nearq);
    TerrainMesh_UpdateNearZ(nearq);
    nearq[2] = nearq[2] * kNearScale;
    arg.dist = nearq[2];
    arg.zbuffer = 0;
    if (gRenderFuncs[TransContext_GetNearClipMethod()])
        gRenderFuncs[TransContext_GetNearClipMethod()](&arg);
    crt_sprintf(gNearClipText, "Closest Z: %.2f", (double)nearq[2]);
}
HOOK(0x4D2840, QCube_UpdateNearPlane);

/* Pool_Free of one element: the last used slot takes its place */
static void pool_release(Pool *p, char *elem)
{
    int slot = *(int *)(elem + p->size);
    int last = --p->used;

    if (slot != last) {
        char *le = p->data + p->free[last] * p->stride;
        int tmp = p->free[slot];
        int other = *(int *)(le + p->size);

        p->free[slot] = p->free[other];
        p->free[other] = tmp;
        *(int *)(le + p->size) = slot;
    }
}

void QCube_FlushCache(void)
{
    int i;

    for (i = 0; i < 0x800; i++) {
        QCacheEntry *c = &gQCubeCache[i];

        if (c->data) {
            if (c->kind == 0) {
                pool_release(&gQCubePool0, c->data);
            } else if (c->kind == 1) {
                pool_release(&gQCubePool1, c->data);
            } else if (c->kind == 2) {
                pool_release(&gQCubePool2, c->data);
            } else {
                Error_Show("Illegal alloc info");
                for (;;)
                    ;
            }
            c->data = NULL;
        }
        c->frame = -1;
    }
}
HOOK(0x4D2A20, QCube_FlushCache);

/* Ten slots for functions called with the object list after each frame */
void QCube_AddCallback(void (*fn)(void *))
{
    int i;

    for (i = 0; i < 10; i++) {
        QCallback *c = &gQCubeCallbackSlots[i];

        if (!c->fn) {
            c->next = gQCubeCallbacks;
            gQCubeCallbacks = c;
            c->fn = fn;
            return;
        }
    }
    Error_Show("No more QCube callbacks available.");
    for (;;)
        ;
}
HOOK(0x4D2B80, QCube_AddCallback);

void QCube_RemoveCallback(void (*fn)(void *))
{
    QCallback *prev = NULL, *c;

    for (c = gQCubeCallbacks; c; prev = c, c = c->next) {
        if (c->fn == fn) {
            if (prev)
                prev->next = c->next;
            else
                gQCubeCallbacks = c->next;
            c->fn = NULL;
            return;
        }
    }
    Error_Show("Callback not found");
    for (;;)
        ;
}
HOOK(0x4D2BD0, QCube_RemoveCallback);

void QCube_SetDetail(int detail)
{
    gQCubeDetail = detail;
}
HOOK(0x4D2C20, QCube_SetDetail);
