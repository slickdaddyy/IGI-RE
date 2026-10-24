#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "sound/sound.h"

ORIG(0x4B0B00, void, Pool_Init, (void *pool, int stride, int capacity, int index_offset));
ORIG(0x4B0B70, void, Pool_Free, (void *pool));
ORIG(0x4C7770, void, DynCube_SendEventBox, (void *tree, uint8_t slot, void *arg, Vec3d *min, Vec3d *max,
                                            uint32_t flags, int unk));

/* one bit per cell of a size x size grid (size a power of two) */
typedef struct CellMask {
    struct CellMask **item;
    int shift;
    int size;
    uint8_t bits[];
} CellMask;

/* the argument of the draw event: objects draw themselves into the mask */
typedef struct CellMaskDrawArg {
    CellMask *mask;
    int size;
    double *center;
    float range;
} CellMaskDrawArg;

extern uint16_t gDynCubeObjType;
extern uint8_t gDynCubeObjMaskVisibleEvent;
extern uint16_t gQObjType;
extern int gCellMaskEnabled;
extern float gCellMaskTriX;
extern uint8_t gCellMaskDrawEvent;
extern Pool gCellMaskPool;

static const float kFloatMax = 3.4028234663852886e+38f;
static const float kOne = 1.0f;

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

uint8_t DynCubeObj_GetMaskVisibleEvent(void)
{
    return gDynCubeObjMaskVisibleEvent;
}
HOOK(0x4E5360, DynCubeObj_GetMaskVisibleEvent);

void DynCubeObj_Init(void)
{
    gDynCubeObjType = TaskType_New("DynCubeObj", 0x68, gQObjType, 0);
    gDynCubeObjMaskVisibleEvent = TaskEvent_Alloc(0x181);
}
HOOK(0x4E5370, DynCubeObj_Init);

void DynCubeObj_Shutdown(void)
{
    TaskEvent_Free(gDynCubeObjMaskVisibleEvent);
    TaskType_Free(gDynCubeObjType);
}
HOOK(0x4E53A0, DynCubeObj_Shutdown);

/* makes a mask covering pos +- range and lets every object in that box of
   tree draw its outline into it */
CellMask *CellMask_Create(int size, void *tree, int unk, Vec3d *pos, float range)
{
    int bytes = size * size / 8;
    CellMask *m = Mem_Alloc(bytes + 0xF, 4);
    Pool *p = &gCellMaskPool;
    CellMask **item;
    CellMaskDrawArg arg;
    Vec3d min, max;
    int shift;

    m->size = size;
    item = (CellMask **)(p->data + p->free[p->used] * p->stride);
    *(int *)((char *)item + p->size) = p->used;
    p->used++;
    m->item = item;
    *item = m;
    __asm__("bsf %1, %0" : "=r"(shift) : "rm"(size), "0"(size));
    m->shift = shift;
    if (bytes > 0)
        memset(m->bits, 0, bytes);
    min.x = pos->x - range;
    min.y = pos->y - range;
    min.z = -1073741824.0;
    max.x = pos->x + range;
    max.y = pos->y + range;
    max.z = 1073741824.0;
    arg.mask = m;
    arg.size = size;
    arg.center = &pos->x;
    arg.range = range;
    DynCube_SendEventBox(tree, gCellMaskDrawEvent, &arg, &min, &max, 0x20000000, unk);
    return m;
}
HOOK(0x4E53C0, CellMask_Create);

void CellMask_Free(CellMask *m)
{
    Pool *p = &gCellMaskPool;
    int slot = *(int *)((char *)m->item + p->size);

    p->used--;
    if (slot != p->used) {
        char *last = p->data + p->free[p->used] * p->stride;
        int item = p->free[slot];
        int lastSlot = *(int *)(last + p->size);

        p->free[slot] = p->free[lastSlot];
        p->free[lastSlot] = item;
        *(int *)(last + p->size) = slot;
    }
    Mem_Free(m);
}
HOOK(0x4E54E0, CellMask_Free);

void CellMask_Set(CellMask *m, int x, int y)
{
    m->bits[((y << m->shift) + (x & ~7)) >> 3] |= 1 << (x & 7);
}
HOOK(0x4E5550, CellMask_Set);

int CellMask_Test(CellMask *m, int x, int y)
{
    return m->bits[((y << m->shift) + (x & ~7)) >> 3] >> (x & 7) & 1;
}
HOOK(0x4E5580, CellMask_Test);

uint8_t CellMask_GetDrawEvent(void)
{
    return gCellMaskDrawEvent;
}
HOOK(0x4E55B0, CellMask_GetDrawEvent);

void CellMask_Init(void)
{
    gCellMaskDrawEvent = TaskEvent_Alloc(Task_GetRootType());
    Pool_Init(&gCellMaskPool, 4, 1000, 0);
}
HOOK(0x4E55C0, CellMask_Init);

void CellMask_Shutdown(void)
{
    Pool_Free(&gCellMaskPool);
    TaskEvent_Free(gCellMaskDrawEvent);
}
HOOK(0x4E55F0, CellMask_Shutdown);

/* x where row y crosses the edge (x0, y0) - (x1, y1), widening [*lo, *hi]
   (the first edge only compares against the float limits) */
static void edge_span(float y, float x0, float y0, float x1, float y1, float *lo, float *hi, int first)
{
    long double v;

    if (!((y >= y0 && y <= y1) || (y >= y1 && y <= y0)))
        return;
    v = (y - y0) * (x1 - x0) / (y1 - y0) + x0;
    if (first) {
        if (v <= kFloatMax)
            *lo = v;
        if (-kFloatMax <= v)
            *hi = v;
    } else {
        if (!(*lo < v))
            *lo = v;
        if (!(*hi > v))
            *hi = v;
    }
}

static void row_span(float y, float ax, float ay, float bx, float by, float cx, float cy, float *lo, float *hi)
{
    *lo = kFloatMax;
    *hi = -kFloatMax;
    edge_span(y, ax, ay, bx, by, lo, hi, 1);
    edge_span(y, bx, by, cx, cy, lo, hi, 0);
    edge_span(y, cx, cy, ax, ay, lo, hi, 0);
}

/* sets every cell the triangle touches, row by row */
void CellMask_FillTriangle(CellMask *m, float ax, float ay, float bx, float by, float cx, float cy)
{
    float ymin, ymax;
    int y, y0, y1;

    gCellMaskTriX = ax;
    ymin = (ay < by ? ay : by) < cy ? (ay < by ? ay : by) : cy;
    ymax = (ay > by ? ay : by) > cy ? (ay > by ? ay : by) : cy;
    y0 = round_int(ymin);
    y1 = round_int(ymax);
    for (y = y0; y <= y1; y++) {
        float fy = (float)y, fy1;
        float lo, hi, lo2, hi2, mn, mx, start, end;
        int x, xe;

        if (y < 0 || y >= m->size)
            continue;
        row_span(fy, ax, ay, bx, by, cx, cy, &lo, &hi);
        fy1 = fy + kOne;
        row_span(fy1, ax, ay, bx, by, cx, cy, &lo2, &hi2);
        mn = lo;
        mx = hi;
        if (!(ay < fy) && !(ay > fy1)) {
            if (!(lo < ax))
                mn = ax;
            if (!(hi > ax))
                mx = ax;
        }
        if (!(by < fy) && !(by > fy1)) {
            if (!(mn < bx))
                mn = bx;
            if (!(mx > bx))
                mx = bx;
        }
        if (!(cy < fy) && !(cy > fy1)) {
            if (!(mn < cx))
                mn = cx;
            if (!(mx > cx))
                mx = cx;
        }
        if (mx < mn && hi2 < lo2)
            continue;
        start = mn < lo2 ? mn : lo2;
        end = start > hi2 ? mx : hi2;
        x = round_int(start);
        xe = round_int(end);
        for (; x <= xe; x++)
            if (x >= 0 && x < m->size)
                CellMask_Set(m, x, y);
    }
}
HOOK(0x4E5610, CellMask_FillTriangle);

/* any cell set in the rectangle */
int CellMask_TestRect(CellMask *m, float x, float y, float w, float h)
{
    int x0 = round_int(x);
    int size = m->size;
    int y0 = round_int(y);
    int cw = round_int(x + w) - x0 + 1;
    int ch = round_int(y + h) - y0 + 1;
    int i, j;

    if (x0 >= size)
        return 0;
    if (x0 < 0) {
        cw += x0;
        if (cw <= 0)
            return 0;
        x0 = 0;
    }
    if (y0 >= size)
        return 0;
    if (y0 < 0) {
        ch += y0;
        if (ch <= 0)
            return 0;
        y0 = 0;
    }
    if (cw + x0 > size)
        cw = size - x0;
    if (ch + y0 > size)
        ch = size - y0;
    for (j = y0; j < ch + y0; j++)
        for (i = x0; i < cw + x0; i++)
            if (m->bits[((j << m->shift) + (i & ~7)) >> 3] >> (i & 7) & 1)
                return 1;
    return 0;
}
HOOK(0x4E5BA0, CellMask_TestRect);

void CellMask_Enable(void)
{
    gCellMaskEnabled = 1;
}
HOOK(0x4E5C90, CellMask_Enable);

void CellMask_Disable(void)
{
    gCellMaskEnabled = 0;
}
HOOK(0x4E5CA0, CellMask_Disable);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54875C) uint16_t gDynCubeObjType INIT(385);
DATA(54875C) uint8_t gDynCubeObjMaskVisibleEvent INIT(255);
PAD(igd, 54875C, 54875F, 0xD) INIT({0x00, 0x44, 0x79, 0x6E, 0x43, 0x75, 0x62, 0x65, 0x4F, 0x62, 0x6A});
DATA(54875C) int gCellMaskEnabled INIT(1);
BSS(A5E404) float gCellMaskTriX;
BSS(A5E404) uint8_t gCellMaskDrawEvent;
PAD(igb, A5E404, A5E409, 0x3);
BSS(B019E0) Pool gCellMaskPool;
