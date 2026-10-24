#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "dxinit/dxinit.h"

typedef struct MapHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x50];
} MapHit;

typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
} DisplayMode;

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (int type, uint8_t slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401900, int, TaskType_New, (const char *name, int size, uint16_t parent, int pool, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x495200, void, Direct3D_AddRestoreCallback, (void (*fn)(void)));
ORIG(0x498000, void, TransContext_ProjectView, (float *out, const float *in));
ORIG(0x4A55FE, double, crt_floor, (double x));
ORIG(0x4A5C30, double, crt_ceil, (double x));
ORIG(0x4B0F80, int, RenderMode_GetCurrent, (void));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4B72E0, void *, SortRender_Add, (void (*fn)(void *), int size, uint32_t key));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (void *world, MapHit *results, Vec3d *points, int count, const double *dz,
                                          int unk, int unk2, unsigned int flags));

/* one falling drop or flake, positions inside a 10895.36 square tile */
typedef struct RainDrop {
    float x, y, z;
    int timer;
    float dx, dy;
    int kind; /* bit 0 / 1: tile wrapped in x / y, picks the vertex group */
} RainDrop;

/* a ground sample of the 24x24 grid around the camera (12x12 tiled twice) */
typedef struct RainCell {
    double x, y, z;
    float depth;
    unsigned flags; /* 0x20 = valid, bits 0-4 outcodes */
} RainCell;

/* heights of the drops in one vertex group, in drawing order */
typedef struct RainList {
    int count;
    float z[80];
} RainList;

typedef struct RainVertex {
    float x, y, z;
    unsigned diffuse;
    float u, v;
} RainVertex;

/* "Rain" task, 0x56D0 bytes */
typedef struct Rain {
    Task task;
    char unk20[0x48];
    void *world;
    RainList lists[4];
    RainDrop drops[80];
    int dropCount;
    RainCell cells[24 * 24];
    int cellX;
    int cellY;
    int unk5648;
    float spawn;
    int snow;
    char *texture;
    float matrix[16];
    double fallDist;
    double height;
    float fade;
    char falling;
    char hasLast;
    Vec3d last;
    unsigned color;
    IDirect3DVertexBuffer7 *vb;
} Rain;

typedef struct RainDrawItem {
    Rain *rain;
    RainCell *cell;
    double z;
} RainDrawItem;

_Static_assert(offsetof(Rain, lists) == 0x6C, "lists");
_Static_assert(offsetof(Rain, drops) == 0x57C, "drops");
_Static_assert(offsetof(Rain, cells) == 0xE40, "cells");
_Static_assert(offsetof(Rain, matrix) == 0x5658, "matrix");
_Static_assert(offsetof(Rain, last) == 0x56B0, "last");
_Static_assert(sizeof(Rain) == 0x56D0, "Rain");

extern uint16_t gRainType;
extern char gDxRainRandom[];
extern Rain *gRain;
extern int gRainEnabled;

extern const double kHalfD;
extern const float kZero;
extern const float kOne;
extern const float kHalfF;
extern const float kFadeStep;
extern const double kTile;
extern const float kTileF;
extern const double kTwoTiles;
extern const double kTileInv;
extern const float kTileInvF;
extern const double kGridOrigin;
extern const float kTwelve;
extern const float kLayer;
extern const double kLayerOrigin;
extern const double kLayerInv;
extern const double kTop;
extern const float kTopF;
extern const float kFlakeSize;
extern const float kStreak;
extern const float kSpawnStep;
extern const float kSway;
extern const float kAlphaScale;

static const float kSnowSpeed = 204.8f;
static const float kRainSpeed = 2048.0f;

void Rain_SetAlpha(Rain *r, float alpha);
void Rain_Render(Rain *r);
void Rain_UpdateGrid(Rain *r);
void Rain_Draw(Rain *r);
void Rain_FillVB(Rain *r);
void Rain_DrawCell(RainDrawItem *it);
void Rain_ClipCells(Rain *r, double z);

static void Rain_CreateVB(IDirect3DVertexBuffer7 **vb)
{
    D3DVERTEXBUFFERDESC desc;

    desc.dwSize = sizeof(desc);
    desc.dwCaps = (gD3DTnL ? 0 : D3DVBCAPS_SYSTEMMEMORY) | D3DVBCAPS_WRITEONLY;
    desc.dwFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
    desc.dwNumVertices = 0x780;
    gD3D->lpVtbl->CreateVertexBuffer(gD3D, &desc, vb, 0);
}

Rain *Rain_New(void *world, char *texture, int rain, float height, float depth)
{
    Rain *r = (Rain *)Task_New(NULL, gRainType, NULL);
    int i;

    r->dropCount = 0;
    r->spawn = 0.0f;
    DynCube_ListAdd(world, r, 0);
    r->world = world;
    r->snow = rain == 0;
    r->texture = texture;
    r->fade = 0.0f;
    r->unk5648 = 0;
    for (i = 0; i < 24 * 24; i++)
        r->cells[i].flags = 0;
    r->height = height;
    r->fallDist = height + depth;
    Rain_SetAlpha(r, 0.3125f);
    Rain_CreateVB(&r->vb);
    r->hasLast = 0;
    gRain = r;
    return r;
}
HOOK(0x498960, Rain_New);

void Rain_Free(Rain *r, void *world)
{
    gRain = NULL;
    r->vb->lpVtbl->Release(r->vb);
    DynCube_ListRemove(world, r, 0);
    Task_Delete(r);
}
HOOK(0x498A70, Rain_Free);

void Rain_Restore(void);

void Rain_Init(void)
{
    int type;

    Random_Init(gDxRainRandom);
    type = TaskType_New("Rain", sizeof(Rain), gDynCubeObjType, 0, 0);
    gRainType = type;
    TaskType_SetHandler(type, 4, Rain_Render);
    Direct3D_AddRestoreCallback(Rain_Restore);
}
HOOK(0x498AB0, Rain_Init);

void Rain_Restore(void)
{
    if (gRain) {
        IDirect3DVertexBuffer7 **vb = &gRain->vb;

        (*vb)->lpVtbl->Release(*vb);
        Rain_CreateVB(vb);
    }
}
HOOK(0x498B00, Rain_Restore);

void Rain_Render(Rain *r)
{
    int mode = Direct3D_GetRenderMode();

    if (RenderMode_GetCurrent() != mode)
        return;
    if (x87_eq(r->fade, kZero)) {
        r->hasLast = 0;
        return;
    }
    Rain_UpdateGrid(r);
    Rain_Draw(r);
}
HOOK(0x498B80, Rain_Render);

/* Drops the cells that came into range since the last frame onto the ground, at most
   as many as the camera moved cells (all of them the first time). */
void Rain_UpdateGrid(Rain *r)
{
    RainCell *c = r->cells;
    int ix = (int)(long long)crt_floor((gViewPos.x - kGridOrigin) * kTileInv + kHalfD);
    int iy = (int)(long long)crt_floor((gViewPos.y - kGridOrigin) * kTileInv + kHalfD);
    int mx = ix % 12, my = iy % 12;
    double x0, y0;
    int budget, i, j;

    if (mx < 0)
        mx += 12;
    r->cellX = mx;
    if (my < 0)
        my += 12;
    x0 = (ix - mx) * kTile;
    r->cellY = my;
    y0 = (iy - my) * kTile;
    if (!r->hasLast) {
        budget = 10000;
    } else {
        long double dx = fabsl((long double)gViewPos.x - r->last.x);
        float dy = fabsl((long double)gViewPos.y - r->last.y);

        budget = (int)(long long)crt_ceil(dx * kTileInvF * kTwelve);
        budget += (int)(long long)crt_ceil(dy * kTileInvF * kTwelve);
    }
    r->last = gViewPos;
    r->hasLast = 1;
    for (i = 0; i < 12; i++, x0 += kTile, c += 12) {
        double x = i < mx ? x0 + kTwoTiles : x0;
        double y = y0;

        for (j = 0; j < 12; j++, y += kTile, c++) {
            long double cy = j < my ? (long double)y + kTwoTiles : (long double)y;
            unsigned mask = x87_eq(c->x, x) && x87_eq(c->y, cy) ? 0x20 : 0;

            c->flags &= mask;
            if (!c->flags && budget-- > 0) {
                double dz = -r->fallDist;
                MapHit hit;

                c->x = x;
                c->y = cy;
                c->z = gViewPos.z + r->height;
                Coll_DropPointsMat(r->world, &hit, (Vec3d *)c, 1, &dz, 0xFF, 0, 0xC8000000);
                c->z = hit.pos.z;
                memcpy(&c[12], c, sizeof(Vec3d));
                memcpy(&c[24 * 12], c, sizeof(Vec3d));
                c->flags = 0x20;
                memcpy(&c[24 * 12 + 12], c, sizeof(Vec3d));
            }
            c[12].flags = c->flags;
            c[24 * 12].flags = c->flags;
            c[24 * 12 + 12].flags = c->flags;
        }
    }
    r->unk5648 = 0;
}
HOOK(0x498BD0, Rain_UpdateGrid);

void Rain_Draw(Rain *r)
{
    RainCell *c = &r->cells[(r->cellX + 1) * 24 + r->cellY + 1];
    double z = crt_floor((gViewPos.z - kLayerOrigin) * kLayerInv + kHalfD) * kLayer;
    int i, j;

    Rain_FillVB(r);
    Rain_ClipCells(r, z);
    for (i = 9; i; i--, c += 24 - 9) {
        for (j = 9; j; j--, c++) {
            if ((c[0].flags & c[1].flags & c[25].flags & c[24].flags) == 0x20) {
                float a = c[24].depth >= c[0].depth ? c[24].depth : c[0].depth;
                float b = c[25].depth >= c[1].depth ? c[25].depth : c[1].depth;
                RainDrawItem *it = SortRender_Add((void (*)(void *))Rain_DrawCell, sizeof(RainDrawItem),
                                                  f2u(a > b ? a : b));

                it->z = z;
                it->rain = r;
                it->cell = c;
            }
        }
    }
}
HOOK(0x498F10, Rain_Draw);

/* Writes every drop into the vertex group of its kind, starting at the highest one and going
   backwards: two line vertices for rain, a camera facing quad (six vertices) for snow. */
void Rain_FillVB(Rain *r)
{
    float size = r->fade * kFlakeSize;
    unsigned color = r->color;
    float len = r->fade * kStreak;
    RainDrop *top = r->drops, *d;
    RainVertex *p[4];
    int i;

    if (r->dropCount > 1) {
        RainDrop *e = r->drops;

        for (i = r->dropCount - 1; i; i--, e++)
            if (e->z > top->z)
                top = e;
    }
    p[0] = NULL;
    if (r->vb->lpVtbl->Lock(r->vb, DDLOCK_WRITEONLY, (void **)&p[0], NULL))
        return;
    r->lists[0].count = 0;
    for (i = 1; i < 4; i++) {
        p[i] = p[i - 1] + 480;
        r->lists[i].count = 0;
    }
    d = top;
    for (i = 0; i < r->dropCount; i++) {
        int k = d->kind;
        RainVertex *v = p[k];
        RainList *l;

        if (r->snow == 1) {
            float vx = gView.m[1] * d->y + gView.m[0] * d->x + gView.m[2] * d->z;
            float vy = gView.m[4] * d->y + gView.m[3] * d->x + gView.m[5] * d->z;
            float vz = gView.m[7] * d->y + gView.m[6] * d->x + gView.m[8] * d->z;
            float x0 = vx - size, x1 = vx + size;
            float y0 = vy - size, y1 = vy + size;

            v[0].x = x0;
            v[0].y = y0;
            v[1].x = x1;
            v[1].y = y0;
            v[2].x = x0;
            v[2].y = y1;
            v[3].x = x1;
            v[3].y = y1;
            v[0].z = v[1].z = v[2].z = v[3].z = vz;
            v[0].diffuse = v[1].diffuse = v[2].diffuse = v[3].diffuse = 0xFFFFFFFF;
            v[0].u = 0.0f;
            v[0].v = 0.0f;
            v[1].u = 1.0f;
            v[1].v = 0.0f;
            v[2].u = 0.0f;
            v[2].v = 1.0f;
            v[3].u = 1.0f;
            v[3].v = 1.0f;
            v[4] = v[1];
            v[5] = v[2];
            v += 6;
        } else {
            v[0].diffuse = 0xFFFFFF;
            v[1].diffuse = color;
            v[0].x = v[1].x = d->x;
            v[0].y = v[1].y = d->y;
            v[0].z = len + d->z;
            v[1].z = d->z;
            v += 2;
        }
        d--;
        if (d < r->drops)
            d += r->dropCount;
        p[k] = v;
        l = &r->lists[k];
        l->z[l->count++] = d->z;
    }
    r->vb->lpVtbl->Unlock(r->vb);
    if (r->snow == 1) {
        for (i = 0; i < 3; i++) {
            r->matrix[i * 4] = gView.m[i * 3] * gNearPlaneScale;
            r->matrix[i * 4 + 1] = gView.m[i * 3 + 1] * gNearPlaneScale;
            r->matrix[i * 4 + 2] = gView.m[i * 3 + 2] * gNearPlaneScale;
            r->matrix[i * 4 + 3] = 0.0f;
        }
    } else {
        for (i = 0; i < 12; i++)
            r->matrix[i] = i % 5 ? 0.0f : gNearPlaneScale;
    }
}
HOOK(0x499090, Rain_FillVB);

/* Draws the drops of one 2x2 cell block in up to three 24576 high layers above it. */
void Rain_DrawCell(RainDrawItem *it)
{
    Rain *r = it->rain;
    RainCell *c = it->cell;
    double a = c[24].z >= c[0].z ? c[24].z : c[0].z;
    double b = c[25].z >= c[1].z ? c[25].z : c[1].z;
    float h = (a > b ? a : b) - it->z - kTopF;
    int q = ((int)(long long)crt_floor(c->y * kTileInv) & 1) << 1;
    RainList *list;
    int prim, mult, n, k;

    q |= (int)(long long)crt_floor(c->x * kTileInv) & 1;
    list = &r->lists[q];
    if (!gRainEnabled)
        return;
    gD3DListA->current = -1;
    gD3DListB->current = -1;
    gD3DListC->current = -1;
    gD3DStateList->current = -1;
    SET_RS(0x1B, 1);
    SET_RS(0x13, 5);
    SET_RS(0x14, 6);
    SET_RS(0x1C, 0);
    SET_RS(7, gD3DZEnable);
    SET_RS(0x2F, 0);
    SET_RS(0x17, 2);
    SET_RS(0xE, 0);
    SET_RS(0x16, 1);
    SET_RS(0x1D, 0);
    SET_RS(0x89, 0);
    SET_RS(0x1A, 0);
    SET_RS(0xF, 0);
    SET_RS(0x88, 1);
    if (r->snow == 1) {
        if (r->texture) {
            gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, *(IDirectDrawSurface7 **)(r->texture + 0x20));
            SET_TSS(0, 1, 4);
            k = 4;
        } else {
            SET_TSS(0, 1, 3);
            k = 3;
        }
        SET_TSS(0, 4, k);
        SET_TSS(0, 2, 2);
        SET_TSS(0, 3, 0);
        SET_TSS(0, 5, 2);
        SET_TSS(0, 6, 0);
        SET_TSS(0, 0x10, 2);
        SET_TSS(0, 0x11, 1);
        prim = D3DPT_TRIANGLELIST;
        mult = 6;
    } else {
        gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, NULL);
        SET_TSS(0, 1, 3);
        SET_TSS(0, 4, 3);
        SET_TSS(0, 2, 2);
        SET_TSS(0, 3, 0);
        SET_TSS(0, 5, 2);
        SET_TSS(0, 6, 0);
        SET_TSS(1, 1, 1);
        prim = D3DPT_LINELIST;
        mult = 2;
    }
    SET_TSS(1, 1, 1);
    r->matrix[12] = (c->x - gViewPos.x) * gNearPlaneScale;
    r->matrix[13] = (c->y - gViewPos.y) * gNearPlaneScale;
    r->matrix[15] = 1.0f;
    r->matrix[14] = (it->z - gViewPos.z + kTop) * gNearPlaneScale;
    for (k = 3; k; k--) {
        long double hl;

        r->matrix[14] = r->matrix[14] - gNearPlaneScale * kLayer;
        hl = h + kLayer;
        h = hl;
        if (!(hl >= kZero)) {
            n = list->count;
        } else if (!(h > kLayer)) {
            for (n = 0; n < list->count; n++)
                if (!(list->z[n] > h))
                    break;
        } else {
            n = 0;
        }
        if (n) {
            gD3DDevice->lpVtbl->SetTransform(gD3DDevice, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX *)r->matrix);
            gD3DDevice->lpVtbl->DrawPrimitiveVB(gD3DDevice, prim, r->vb, q * 480, n * mult, 0);
        }
    }
}
HOOK(0x499430, Rain_DrawCell);

static float Rain_View(int row, long double px, long double py, long double pz)
{
    return gView.m[row + 2] * pz + gView.m[row + 1] * py + gView.m[row] * px;
}

/* Outcodes of the cells around the camera: bit 5 set, bits 0-4 the outcodes shared by the
   ground point and the point 73728 above it (0: left, 1: top, 2: right, 3: bottom, 4: behind). */
void Rain_ClipCells(Rain *r, double z)
{
    RainCell *c = &r->cells[(r->cellX + 1) * 24 + r->cellY + 1];
    float w = Display_GetMode()->width;
    float h = Display_GetMode()->height;
    int i, j;

    for (i = 10; i; i--, c += 24 - 10) {
        for (j = 10; j; j--, c++) {
            unsigned flags = 0x1F;

            if (c->flags & 0x20) {
                float v[3], u[3], s[2];
                long double px = c->x - gViewPos.x, py = c->y - gViewPos.y, pz = z - gViewPos.z;
                long double depth;
                double z2;

                v[0] = Rain_View(0, px, py, pz);
                v[1] = Rain_View(3, px, py, pz);
                depth = gView.m[8] * pz + gView.m[7] * py + gView.m[6] * px;
                v[2] = depth;
                if (depth > kZero) {
                    TransContext_ProjectView(s, v);
                    flags = (s[0] > kZero ? ~1u : ~0u) & 0x3F;
                    if (s[1] > kZero)
                        flags &= ~2u;
                    if (!(s[0] >= w))
                        flags &= ~4u;
                    flags &= !(s[1] >= h) ? ~0x18u : ~0x10u;
                } else {
                    flags = (v[0] > kZero ? ~1u : ~4u) & 0x3F;
                    flags &= v[1] > kZero ? ~2u : ~8u;
                }
                z2 = z + kTop;
                px = c->x - gViewPos.x;
                py = c->y - gViewPos.y;
                pz = z2 - gViewPos.z;
                u[0] = Rain_View(0, px, py, pz);
                u[1] = Rain_View(3, px, py, pz);
                depth = gView.m[8] * pz + gView.m[7] * py + gView.m[6] * px;
                u[2] = depth;
                if (depth > kZero) {
                    TransContext_ProjectView(s, u);
                    if (s[0] > kZero)
                        flags &= ~1u;
                    if (s[1] > kZero)
                        flags &= ~2u;
                    if (!(s[0] >= w))
                        flags &= ~4u;
                    flags &= !(s[1] >= h) ? ~0x18u : ~0x10u;
                } else {
                    flags &= u[0] > kZero ? ~1u : ~4u;
                    flags &= u[1] > kZero ? ~2u : ~8u;
                }
                c->depth = (u[2] + v[2]) * kHalfF;
            }
            c->flags = flags;
        }
    }
}
HOOK(0x499910, Rain_ClipCells);

void Rain_Close(void)
{
    TaskType_Free(gRainType);
}
HOOK(0x499CE0, Rain_Close);

/* Fades the effect in or out, moves the drops down (snow also sways) and spawns new
   ones at the top of the layer. */
void Rain_Update(Rain *r)
{
    float speed;
    long double f, acc;
    RainDrop *d = r->drops;
    int i;

    speed = r->snow ? kSnowSpeed : kRainSpeed;
    r->unk5648 += 3;
    if (r->falling) {
        f = r->fade + kFadeStep;
        if (!(f < kOne))
            f = kOne;
    } else {
        f = r->fade - kFadeStep;
        if (!(f > kZero))
            f = kZero;
    }
    r->fade = f;
    if (x87_eq(r->fade, kZero))
        return;
    for (i = 0; i < r->dropCount; i++, d++) {
        float z = d->z - speed;

        d->z = z;
        if (z < kZero)
            d->z = z + kLayer;
        if (r->snow) {
            long double x;

            if (d->timer-- < 0) {
                d->timer += Random_Int(gDxRainRandom, 30) + 30;
                d->dx = (Random_Float(gDxRainRandom) - kHalfF) * kSway;
                d->dy = (Random_Float(gDxRainRandom) - kHalfF) * kSway;
            }
            x = d->dx + d->x;
            d->x = x;
            d->y = d->dy + d->y;
            if (!(x >= kZero)) {
                d->x = d->x + kTileF;
                d->kind ^= 1;
            } else if (d->x > kTileF) {
                d->x = d->x - kTileF;
                d->kind ^= 1;
            }
            if (!(d->y >= kZero)) {
                d->y = d->y + kTileF;
                d->kind ^= 2;
            } else if (d->y > kTileF) {
                d->y = d->y - kTileF;
                d->kind ^= 2;
            }
        }
    }
    acc = speed + r->spawn;
    r->spawn = acc;
    while (acc > kSpawnStep) {
        float x;

        if (r->dropCount == 80)
            return;
        d = &r->drops[r->dropCount];
        x = Random_Float(gDxRainRandom) * kTileF;
        d->y = Random_Float(gDxRainRandom) * kTileF;
        d->x = x;
        d->timer = 0;
        d->z = kLayer - speed;
        d->kind = Random_Int(gDxRainRandom, 4);
        r->dropCount++;
        acc = r->spawn - kSpawnStep;
        r->spawn = acc;
    }
}
HOOK(0x499CF0, Rain_Update);

void Rain_SetFalling(Rain *r, char on)
{
    r->falling = on;
}
HOOK(0x499F90, Rain_SetFalling);

void Rain_SetAlpha(Rain *r, float alpha)
{
    r->color = 0xFFFFFF - ((unsigned)x87_fistp(alpha * kAlphaScale) << 24);
}
HOOK(0x499FA0, Rain_SetAlpha);

void Rain_Enable(int on)
{
    gRainEnabled = on;
}
HOOK(0x499FD0, Rain_Enable);

int Rain_IsEnabled(void)
{
    return gRainEnabled;
}
HOOK(0x499FE0, Rain_IsEnabled);
