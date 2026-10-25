/* WaterMesh: the wave table, per vertex height and normal, the light on the
   water and the fade with distance. The quad tree itself is in src/freecam. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "terredit/terredit.h"
#include "terredit/terredittypes.h"
#include "transctx/transctxtypes.h"

/* the WaterMesh primitive in the render queue */
typedef struct WaterDrawNode {
    char unk0[0xC];
    WaterMesh *mesh;
} WaterDrawNode;

ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4B29A0, void, Vec2d_Normalize, (double *v));
ORIG(0x4B29D0, long double, Vec2d_Length, (const double *v));
ORIG(0x4B29F0, long double, Vec2d_Dot, (const double *a, const double *b));
ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (void *list, void *count));
ORIG(0x52FFA0, void, WaterMesh_Subdivide, (WaterMesh *w, WaterNode *node));
ORIG(0x52FE60, void, WaterMesh_CheckVisible, (WaterMesh *w, WaterNode *node));
ORIG(0x52FBA0, void, WaterMesh_SetEdgeFlags, (WaterMesh *w, int level, int x, int y));
ORIG(0x52F630, void, WaterMesh_DrawSplitTri, (WaterMesh *w, WaterNode *node, WaterTri *t, int base, int count,
                                              int recalc));
ORIG(0x52F3D0, void, WaterMesh_CalcEnvUV, (WaterMesh *w, float *uv, const Vec3d *pos, const float *normal));
ORIG(0x5116D0, void, TerrainMesh_DrawTriangle, (WaterTri *t));

extern int gWaterPrimitive;
extern Pool gLightPool;
extern const char gWaterName[];
extern uint32_t gWaterSplits[];
extern int gWaterUnusedNodeCount;
extern int gWaterUnusedInt;
extern TransContext gTransContext;

extern float kZero;
extern float kOne;
extern double kOneD;
extern float kHalf;
extern float kOneHalf;
extern float kTenth;
extern float kTwoPiF;
extern double kWaterTwoPi;
extern float kPeriodScale;
extern double kGridMin;
extern double kGridScale;
extern double kZeroD;
extern double kMinusTwoD;
extern double kFinestSize;
extern double kHalfD;

/* x87 "not equal" (C3 clear): false for NaN */
static inline int fne(long double a)
{
    return a < kZero || a > kZero;
}

void WaterMesh_Draw(WaterDrawNode *node);
void WaterMesh_DrawCells(WaterMesh *w);
void WaterMesh_DrawNode(WaterMesh *w, WaterNode *node, int level, int x, int y);

/* fills the 5 x 5 wave table from the time; returns time / period2 */
uint32_t WaterMesh_UpdateWaves(WaterMesh *w)
{
    uint32_t period = w->period2, p2, t = 0;
    long double fp2, fperiod;
    int i, j, k;

    p2 = (uint32_t)x87_fistp((long double)(uint64_t)period * kPeriodScale);
    fp2 = (int)p2;
    fperiod = (int)period;
    for (i = 1; i <= 5; i++) {
        k = i;
        for (j = 0; j < 5; j++) {
            long double half = (long double)k * kHalf;
            long double q1, q2;

            t = w->time;
            q1 = (long double)(uint64_t)((t + 0xBD) % p2) / fp2;
            q2 = (long double)(uint64_t)(t % period) / fperiod;
            w->waves[(i - 1) * 5 + j][0] = x87_sin(q2 * kWaterTwoPi + half) * w->cell;
            w->waves[(i - 1) * 5 + j][1] = x87_sin(q1 * kWaterTwoPi + half) * w->cell;
            k += i;
        }
    }
    return t / period;
}
HOOK(0x52D9F0, WaterMesh_UpdateWaves);

/* lays out the top grid, the wave table and the lighting after a change */
void WaterMesh_Init(WaterMesh *w)
{
    float light[3] = { 0, 0, 0 };
    long double size = (long double)(uint64_t)w->size, d, r;
    double v[2];
    uint32_t i, j;
    int k, n;

    if (size > kGridMin)
        w->n = x87_fistp(size * kGridScale);
    else
        w->n = 1;
    w->root.parent = NULL;
    w->root.x = w->originX;
    w->root.y = w->originY;
    w->root.active = 0;
    w->root.level = -1;
    w->root.flags = 0;
    w->root.size = w->size * 2 / w->n;
    for (i = 0; i < w->n; i++) {
        for (j = 0; j < w->n; j++) {
            WaterNode *c = &w->top[i][j];

            c->parent = NULL;
            c->x = (long double)(uint64_t)(w->size / w->n) * (uint64_t)i + w->originX;
            c->y = (long double)(uint64_t)(w->size / w->n) * (uint64_t)j + w->originY;
            c->size = w->size / w->n;
            c->active = 0;
            c->level = 0;
            c->flags = 0;
        }
    }
    w->cell = w->wavelength * kTenth;
    w->time = 0;
    w->unk4AB8 = 0;
    WaterMesh_UpdateWaves(w);
    w->side[1] = -w->dirX;
    w->side[0] = w->dirY;
    Vec2d_Normalize(w->side);
    v[0] = 1.0;
    v[1] = 0.0;
    d = Vec2d_Dot(w->side, v);
    v[0] = d * w->side[0];
    v[1] = d * w->side[1];
    w->projX = Vec2d_Length(v);
    v[0] = 0.0;
    v[1] = 1.0;
    d = Vec2d_Dot(w->side, v);
    v[0] = d * w->side[0];
    v[1] = d * w->side[1];
    w->projY = Vec2d_Length(v);
    w->slope = w->amp / w->wavelength;
    r = light[0];
    n = gLightPool.used;
    if (n > 0) {
        long double g = light[1], b = light[2];

        for (k = 0; k < n; k++) {
            const float *l = (const float *)(gLightPool.data + gLightPool.stride * gLightPool.free[k]);

            r = r + l[3];
            g = g + l[4];
            b = b + l[5];
        }
        light[2] = b;
        light[1] = g;
    }
    w->debugColors = 0;
    w->unk4AC4 = 0;
    w->envMap = 0;
    w->lit[0] = r + w->ambient[0];
    w->lit[1] = (long double)light[1] + w->ambient[1];
    w->lit[2] = (long double)light[2] + w->ambient[2];
}
HOOK(0x52D6C0, WaterMesh_Init);

/* the water surface at pos: height, depth below it, normal and colours */
void WaterMesh_GetVertex(WaterMesh *w, WaterVertex *out, const Vec3d *pos)
{
    int ix = x87_fistp(x87_abs(pos->x) / w->cell) % 5;
    int iy = x87_fistp(x87_abs(pos->y) / w->cell) % 5;
    const double *wave = w->waves[ix * 5 + iy];
    long double th, s, h, k, nz, inv;
    float as;

    th = (((long double)pos->y + wave[1]) * w->projY + ((long double)pos->x + wave[0]) * w->projX - w->phase) /
         w->wavelength * kTwoPiF;
    s = x87_sin(th);
    h = (s + kOne) * w->amp + w->level;
    memcpy(&out->pos, pos, 16);
    out->pos.z = h;
    out->depth = pos->z - h;
    as = x87_abs(s);
    k = (long double)kOne - as;
    if (x87_cos(th) > kZero) {
        out->normal[0] = x87_neg(w->dirX * k * w->slope);
        out->normal[1] = x87_neg(w->dirY * k * w->slope);
    } else {
        out->normal[0] = w->dirX * k * w->slope;
        out->normal[1] = w->dirY * k * w->slope;
    }
    nz = ((long double)as + kOne) * kHalf;
    out->normal[2] = nz;
    if (fne(out->normal[0]) || fne(out->normal[1]) || fne(nz)) {
        inv = kOneD / x87_sqrt(nz * nz + (long double)out->normal[0] * out->normal[0] +
                               (long double)out->normal[1] * out->normal[1]);
        out->normal[0] = out->normal[0] * inv;
        out->normal[1] = out->normal[1] * inv;
        out->normal[2] = nz * inv;
    }
    out->color = *(uint32_t *)((char *)gCurrentWorld + 0xD64);
    out->color2 = *(uint32_t *)((char *)gCurrentWorld + 0xD3C);
    out->lit = 1;
}
HOOK(0x52DAE0, WaterMesh_GetVertex);

/* 1 near the camera, falling to 0 between 1 and 1.5 times size away */
long double WaterMesh_GetFade(WaterMesh *w, const Vec3d *p, int size)
{
    long double dx = x87_abs((long double)w->cam.x - p->x), m, f, r;
    float dy = x87_abs((long double)w->cam.y - p->y);
    float dz = x87_abs((long double)w->cam.z - w->level);

    m = dy > dz ? dy : dz;
    if (dx > m)
        m = dx;
    f = size;
    r = (f * kOneHalf - m) / (f * kHalf);
    if (r < kZero)
        return kZero;
    if (!(r <= kOne))
        return kOne;
    return r;
}
HOOK(0x52DCB0, WaterMesh_GetFade);

void WaterMesh_Open(void)
{
    gWaterPrimitive = Primitive_Register(gWaterName);
    Primitive_SetHandler(gWaterPrimitive, WaterMesh_Draw);
}
HOOK(0x52DD70, WaterMesh_Open);

/* ambient plus every light, weighted by the up part of the normal */
void WaterMesh_Light(WaterMesh *w, float *color, const float *normal)
{
    float v[3], len, g = 0, b = 0;
    long double sr = kZero, sg = kZero, sb = kZero;
    int i, n;

    memcpy(v, normal, sizeof(v));
    v[2] = v[2] * kTenth;
    if (fne(v[0]) || fne(v[1]) || fne(v[2])) {
        len = x87_sqrt((long double)v[2] * v[2] + (long double)v[0] * v[0] + (long double)v[1] * v[1]);
        v[2] = kOneD / len * v[2];
    }
    n = gLightPool.used;
    if (n > 0) {
        for (i = 0; i < n; i++) {
            const float *l = (const float *)(gLightPool.data + gLightPool.stride * gLightPool.free[i]);

            if (v[2] > kZero) {
                sr = (long double)v[2] * l[3] + sr;
                sg = (long double)v[2] * l[4] + sg;
                sb = (long double)v[2] * l[5] + sb;
            }
        }
        b = sb;
        g = sg;
    }
    color[0] = sr + w->ambient[0];
    color[1] = (long double)g + w->ambient[1];
    color[2] = (long double)b + w->ambient[2];
}
HOOK(0x52E180, WaterMesh_Light);

/* the camera's top cell and the three next to it on the camera's side are
   subdivided; every other top cell is switched off */
void WaterMesh_Draw(WaterDrawNode *node)
{
    WaterMesh *w = node->mesh;
    float up[3] = { 0.0f, 0.0f, 1.0f };
    long double h, size, n, cs;
    uint32_t ix, iy, i, j, q;
    int sx, sy, nx, ny;
    WaterNode *c;

    memset(gWaterSplits, 0, 20 * sizeof(uint32_t));
    gWaterUnusedNodeCount = 0;
    gWaterUnusedInt = 0;
    WaterMesh_Light(w, w->lit, up);
    w->cam = gTransContext.pos;
    h = (long double)w->cam.z - w->level;
    memcpy(w->camDir, &gTransContext.matrix.m[6], sizeof(w->camDir));
    if (!(h > kZeroD))
        h = kZeroD;
    w->camHeight = h;
    q = w->time % w->period1;
    w->phase = (long double)(uint64_t)q * w->wavelength / (int)w->period1;
    WaterMesh_UpdateWaves(w);
    Debug_Log();
    Haze_GetVisible(&w->hazeList, &w->hazeCount);
    size = (long double)(uint64_t)w->size;
    n = (long double)(uint64_t)w->n;
    w->count = 0;
    ix = x87_fistp(((long double)w->cam.x - w->originX) / size * n);
    iy = x87_fistp(((long double)w->cam.y - w->originY) / size * n);
    if (w->n - 1 < ix)
        ix = w->n - 1;
    if (w->n - 1 < iy)
        iy = w->n - 1;
    c = &w->top[ix][iy];
    cs = (long double)(uint64_t)c->size;
    sx = -1 - 2 * x87_fistp(((long double)w->cam.x - c->x) / cs * kMinusTwoD);
    sy = -1 - 2 * x87_fistp(((long double)w->cam.y - c->y) / cs * kMinusTwoD);
    nx = (int)ix + (sx >= 0 ? 0 : sx);
    ny = (int)iy + (sy >= 0 ? 0 : sy);
    c = &w->top[nx <= 0 ? 0 : nx][ny <= 0 ? 0 : ny];
    w->root.x = c->x;
    w->root.y = c->y;
    for (i = 0; i < w->n; i++) {
        for (j = 0; j < w->n; j++) {
            c = &w->top[i][j];
            c->active = 1;
            if (i == ix && j == iy) {
                c->flags |= sx > 0 ? 0x10 : 8;
                c->flags |= sy > 0 ? 2 : 4;
            } else if (i == ix + sx && j == iy) {
                c->flags |= sx > 0 ? 8 : 0x10;
                c->flags |= sy > 0 ? 2 : 4;
            } else if (i == ix && j == iy + sy) {
                c->flags |= sx > 0 ? 0x10 : 8;
                c->flags |= sy > 0 ? 4 : 2;
            } else if (i == ix + sx && j == iy + sy) {
                c->flags |= sx > 0 ? 8 : 0x10;
                c->flags |= sy > 0 ? 4 : 2;
            } else {
                c->active = 0;
                continue;
            }
            c->parent = &w->root;
            WaterMesh_Subdivide(w, c);
        }
    }
    WaterMesh_DrawCells(w);
}
HOOK(0x52DD90, WaterMesh_Draw);

static void swap_nodes(WaterNode *a, WaterNode *b)
{
    WaterNode t = *a;

    *a = *b;
    *b = t;
}

/* draws the top cells that were not split, then every unsplit node of the
   finer grids, each with a fade from its distance to the camera */
void WaterMesh_DrawCells(WaterMesh *w)
{
    uint32_t i, j, g;
    int a, b;

    for (i = 0; i < w->n; i++) {
        for (j = 0; j < w->n; j++) {
            WaterNode *c = &w->top[i][j];

            c->edges = 0;
            if (!c->active) {
                WaterMesh_CheckVisible(w, c);
                WaterMesh_DrawNode(w, c, 0, 0, 0);
            }
        }
    }
    /* a grid filled from the far corner is turned so that [0][0] is the
       corner with the smallest x and y */
    for (g = 0; g < (uint32_t)w->count; g++) {
        WaterNode (*q)[4] = w->grid[g];
        double minx = q[0][0].x, miny = q[0][0].y, twice;
        int r, k;

        twice = (long double)(uint64_t)q[0][0].size + (long double)(uint64_t)q[0][0].size;
        for (r = 0; r < 4; r++) {
            for (k = 1; k < 4; k++) {
                if (minx > q[r][k].x)
                    minx = q[r][k].x;
                if (!(q[r][k].y >= miny))
                    miny = q[r][k].y;
            }
        }
        if (!(minx >= q[0][2].x) && (long double)twice + miny > q[0][2].y) {
            swap_nodes(&q[0][2], &q[2][0]);
            swap_nodes(&q[0][3], &q[2][1]);
            swap_nodes(&q[1][2], &q[3][0]);
            swap_nodes(&q[1][3], &q[3][1]);
        }
    }
    for (g = 0; g < (uint32_t)w->count; g++) {
        for (a = 0; a < 4; a++) {
            for (b = 0; b < 4; b++) {
                WaterNode *c = &w->grid[g][a][b];
                long double half = (long double)(uint64_t)(c->size >> 1);
                Vec3d p;

                p.x = half + c->x;
                p.y = half + c->y;
                p.z = w->level;
                c->fade = WaterMesh_GetFade(w, &p, c->size);
            }
        }
    }
    for (g = 0; g < (uint32_t)w->count; g++) {
        if ((long double)(uint64_t)w->grid[g][0][0].size == kFinestSize)
            w->unk4AB8 = w->grid[g][0][0].level;
        for (a = 0; a < 4; a++) {
            for (b = 0; b < 4; b++) {
                WaterNode *c = &w->grid[g][a][b];

                if (!c->active) {
                    WaterMesh_SetEdgeFlags(w, g, a, b);
                    WaterMesh_DrawNode(w, c, g, a, b);
                } else {
                    c->edges = 0;
                }
            }
        }
    }
}
HOOK(0x52E2D0, WaterMesh_DrawCells);

/* x87 "equal" (C3 set): also true for NaN */
static inline int feq(long double a, long double b)
{
    return !(a < b || a > b);
}

/* blends a vertex between its two neighbours by the node's fade */
static void smooth(Vec3d *p, float *n, double za, double zb, long double f, long double nf)
{
    float nz;

    p->z = ((long double)za + zb) * nf * kHalfD + f * p->z;
    nz = f * n[2] + nf;
    n[2] = nz;
    if (fne(n[0]) || fne(n[1]) || fne(nz)) {
        long double inv = kOneD / x87_sqrt((long double)n[0] * n[0] + (long double)n[1] * n[1] +
                                           (long double)nz * nz);

        n[0] = n[0] * inv;
        n[1] = n[1] * inv;
        n[2] = n[2] * inv;
    }
}

static void draw_tri(WaterMesh *w, WaterNode *node, WaterTri *t, uint32_t flags, uint32_t extra, int fine)
{
    uint32_t edges = node->edges;

    if (edges & flags) {
        int parts = fine && (edges & extra & 0x1E0) ? 4 : 2;

        t->texture = w->texture[0];
        t->envMap = 0;
        WaterMesh_DrawSplitTri(w, node, t, 0, parts, fine);
        if (w->envMap && fine) {
            t->texture = w->texture[1];
            t->envMap = 1;
            WaterMesh_DrawSplitTri(w, node, t, 0, parts, fine);
        }
    } else {
        t->envMap = 0;
        t->texture = w->texture[0];
        TerrainMesh_DrawTriangle(t);
        if (w->envMap && fine) {
            t->envMap = 1;
            t->texture = w->texture[1];
            TerrainMesh_DrawTriangle(t);
        }
    }
}

/* draws one unsplit node as an n x n patch (n is 6 at the finest levels,
   else 1); vertices on the edge of a finer grid stay flat so they meet it */
__attribute__((noipa)) void WaterMesh_DrawNode(WaterMesh *w, WaterNode *node, int level, int x, int y)
{
    Vec3d pos[13 * 13], p;
    float normal[13 * 13][3], color[13 * 13][3], uv[13 * 13][2];
    WaterVertex v;
    WaterTri t;
    int fine = 0, n, i, j, r, k, idx;
    double step;

    (void)level;
    (void)x;
    (void)y;
    if (!node->visible)
        return;
    gWaterUnusedNodeCount++;
    if ((uint32_t)node->level > (uint32_t)w->unk4AB8 || w->unk4AB8 == 0) {
        fine = 1;
        n = 6;
    } else {
        n = 1;
    }
    t.hazeList = w->hazeList;
    t.hazeCount = w->hazeCount;
    step = (long double)(uint64_t)node->size / n;
    p.x = node->x;
    p.y = node->y;
    p.z = w->level;
    for (i = 0; i <= n; i++) {
        for (j = 0; j <= n; j++) {
            idx = i * 13 + j;
            if (!fine) {
                pos[idx] = p;
            } else {
                uint32_t lvl = w->unk4AB8;
                WaterNode (*g)[4] = w->grid[lvl];
                int ok = 1;

                if ((uint32_t)node->level == lvl + 1 &&
                    ((feq(node->x, g[0][0].x) && i == 0) || (feq(node->x, g[3][0].x) && i == n) ||
                     (feq(node->y, g[0][0].y) && j == 0) || (feq(node->y, g[0][3].y) && j == n)))
                    ok = 0;
                if ((uint32_t)node->level == lvl + 2 &&
                    ((feq(node->x, g[0][0].x) && i == 0) || (node->x > g[3][0].x && i == n) ||
                     (feq(node->y, g[0][0].y) && j == 0) || (node->y > g[0][3].y && j == n)))
                    ok = 0;
                if ((uint32_t)w->count <= lvl + 2 || ok) {
                    WaterMesh_GetVertex(w, &v, &p);
                    pos[idx] = v.pos;
                    memcpy(normal[idx], v.normal, sizeof(normal[idx]));
                } else {
                    pos[idx].x = p.x;
                    pos[idx].y = p.y;
                    pos[idx].z = w->level;
                    normal[idx][0] = 0.0f;
                    normal[idx][1] = 0.0f;
                    normal[idx][2] = 1.0f;
                }
            }
            p.y = p.y + step;
        }
        p.y = node->y;
        p.x = p.x + step;
    }
    if (fine) {
        long double f = node->fade, nf = (long double)kOne - f;

        /* odd rows, even columns from above and below */
        for (r = 0; r < (n + 1) >> 1; r++)
            if (n > 2)
                for (k = 0; k < (n - 1) >> 1; k++) {
                    idx = (1 + 2 * r) * 13 + 2 + 2 * k;
                    smooth(&pos[idx], normal[idx], pos[idx - 13].z, pos[idx + 13].z, f, nf);
                }
        /* even rows, odd columns from the sides */
        if (n > 2)
            for (r = 0; r < (n - 1) >> 1; r++)
                if (n >= 1)
                    for (k = 0; k < (n + 1) >> 1; k++) {
                        idx = (2 + 2 * r) * 13 + 1 + 2 * k;
                        smooth(&pos[idx], normal[idx], pos[idx - 1].z, pos[idx + 1].z, f, nf);
                    }
        /* odd rows, odd columns along the diagonal */
        if (n >= 1)
            for (r = 0; r < (n + 1) >> 1; r++)
                for (k = 0; k < (n + 1) >> 1; k++) {
                    idx = (1 + 2 * r) * 13 + 1 + 2 * k;
                    smooth(&pos[idx], normal[idx], pos[idx - 12].z, pos[idx + 12].z, f, nf);
                }
    }
    for (i = 0; i <= n; i++) {
        for (j = 0; j <= n; j++) {
            idx = i * 13 + j;
            if (fine) {
                WaterMesh_Light(w, color[idx], normal[idx]);
                WaterMesh_CalcEnvUV(w, uv[idx], &pos[idx], normal[idx]);
            } else {
                uv[idx][0] = 0.0f;
                uv[idx][1] = 0.0f;
                memcpy(color[idx], w->lit, sizeof(color[idx]));
            }
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            uint32_t flags = 0, extra = 0;

            idx = i * 13 + j;
            if (i == 0) {
                flags = 0x10;
                extra = j >= n / 2 ? 0x100 : 0x20;
            }
            if (j == 0) {
                flags |= 4;
                extra |= i < n / 2 ? 0x80 : 0x100;
            }
            if (i == n - 1) {
                flags |= 8;
                extra |= j < n / 2 ? 0x40 : 0x80;
            }
            if (j == n - 1) {
                flags |= 2;
                extra |= i < n / 2 ? 0x40 : 0x20;
            }
            t.pos[0] = pos[idx];
            t.pos[1] = pos[idx + 13];
            t.pos[2] = pos[idx + 1];
            memcpy(t.color[0], color[idx], sizeof(t.color[0]));
            memcpy(t.color[1], color[idx + 13], sizeof(t.color[1]));
            memcpy(t.color[2], color[idx + 1], sizeof(t.color[2]));
            if (w->envMap && fine) {
                memcpy(t.uv[0], uv[idx], sizeof(t.uv[0]));
                memcpy(t.uv[1], uv[idx + 13], sizeof(t.uv[1]));
                memcpy(t.uv[2], uv[idx + 1], sizeof(t.uv[2]));
            }
            draw_tri(w, node, &t, flags & 0x14, extra, fine);
            t.pos[0] = pos[idx + 1];
            t.pos[1] = pos[idx + 14];
            t.pos[2] = pos[idx + 13];
            if (w->debugColors) {
                for (k = 0; k < 3; k++) {
                    t.color[k][0] = 1.0f;
                    t.color[k][1] = 1.0f;
                    t.color[k][2] = 0.0f;
                }
            } else {
                memcpy(t.color[0], color[idx + 1], sizeof(t.color[0]));
                memcpy(t.color[1], color[idx + 14], sizeof(t.color[1]));
                memcpy(t.color[2], color[idx + 13], sizeof(t.color[2]));
            }
            if (w->envMap && fine) {
                memcpy(t.uv[0], uv[idx + 1], sizeof(t.uv[0]));
                memcpy(t.uv[1], uv[idx + 14], sizeof(t.uv[1]));
                memcpy(t.uv[2], uv[idx + 13], sizeof(t.uv[2]));
            }
            draw_tri(w, node, &t, flags & 0xA, extra, fine);
        }
    }
}
HOOK(0x52E660, WaterMesh_DrawNode);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5334A8) float kTwoPiF INIT(6.2831855f);
ALIAS(gTwoPi, kTwoPiF, 0);
ALIAS(kTwoPi, kTwoPiF, 0);
DATA(534CB8) double kGridScale INIT(3.725290298461914e-09);
DATA(534CB8) double kWaterTwoPi INIT(6.283185307179586);
DATA(534CB8) float kPeriodScale INIT(0.703f);
PAD(igd, 534CB8, 534CCC, 0x4);
DATA(534CB8) double kMinusTwoD INIT(-2.0);
DATA(534CB8) double kFinestSize INIT(8388608.0);
STORE(igd, 54F75C, 1) const char store_54F75C[12] INIT({87, 97, 116, 101, 114, 77, 101, 115, 104});
ALIAS(gWaterName, store_54F75C, 0);
BSS(A84578) int gWaterPrimitive;
BSS(A84578) int gWaterUnusedNodeCount;
BSS(A84578) int gWaterUnusedInt;
BSS(B041E0) Pool gLightPool;
