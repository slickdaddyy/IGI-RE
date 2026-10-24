/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "flatwater/flatwater.h"

/* "FlatWater": a flat water plane at a fixed height, drawn with the
   "FlatWaterLayer" textures under it. */

ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B24C0, void, Texture_LoadFile, (void *tex, void *info, const char *name));
ORIG(0x4B5B60, int, Resource_Exists, (const char *name));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x401C10, Task *, Task_FindLastChild, (void *task, uint16_t type));
ORIG(0x499FF0, int, Float_Log2, (float f));
ORIG(0x4B2C70, int, Vec3_RayPlane, (float *out, const float *p, const float *dir, const float *normal));

typedef struct FlatWater {
    Task task;
    char unk20[0x48];
    char *world;    /* 0x68 */
    int unk6C;
    double height;  /* 0x70 */
    float unk78;    /* 30000 */
    float unk7C;    /* 0..1 */
    float color[3]; /* 0x80 */
    int unk8C;
} FlatWater;

_Static_assert(sizeof(FlatWater) == 0x90, "FlatWater");

/* what the world keeps a pointer to while a FlatWater exists */
typedef struct WaterState {
    double height;
    float color[3]; /* average of the layer colors */
} WaterState;

extern WaterState gWaterState;
extern uint16_t gFlatWaterLayerType;
extern void *gFlatWaterParams;
extern uint16_t gFlatWaterType;
extern void *gFlatWaterLayerParams;

extern const float kRange01[];
extern const float kRange11[];

extern double gCamZ;
extern float gTerrainFogScale;
extern float k4096;
extern float kNear; /* 409.6 */
extern float kHalf;
extern float kFlatTenth;
extern float kFltMax;
extern float kFltMaxNeg;
extern float kFogBias; /* 130 */
extern float k2000;
extern double k256th;
extern double k2Pow30;

#define WATER_ROWS 10
#define WATER_MAX 1000

typedef struct Point2 {
    float x, y;
} Point2;

/* the edge of the water polygon on screen, from the first row */
typedef struct WaterEdge {
    float ax, ay, bx, by, dx;
    int side;
} WaterEdge;

void FlatWater_Render(FlatWater *w);
void FlatWater_Create(FlatWater *w);
void FlatWater_Delete(FlatWater *w);
void FlatWater_GetParams(void *task, void **out);
void FlatWater_ParamsChanged(FlatWater *w);
void FlatWaterLayer_GetParams(void *task, void **out);

void FlatWater_Open(void)
{
    FlatWater w;
    FlatLayer l;

    gFlatWaterType = TaskType_New("FlatWater", sizeof(FlatWater), Task_GetRootType(), 0);
    gFlatWaterLayerType = TaskType_New("FlatWaterLayer", sizeof(FlatLayer), Task_GetRootType(), 0);
    TaskType_SetHandler(gFlatWaterType, 0, FlatWater_Create);
    TaskType_SetHandler2(gFlatWaterType, 2, FlatWater_Delete);
    TaskType_SetHandler(gFlatWaterType, 4, FlatWater_Render);
    TaskType_SetHandler2(gFlatWaterType, 1, SUBLEVEL_RUN);
    TaskType_SetHandler(gFlatWaterType, Param_GetTableEvent(), FlatWater_GetParams);
    TaskType_SetHandler(gFlatWaterType, Param_GetChangedEvent(), FlatWater_ParamsChanged);
    TaskType_SetHandler(gFlatWaterLayerType, 0, FlatLayer_Create);
    TaskType_SetHandler2(gFlatWaterLayerType, 2, FlatLayer_Delete);
    TaskType_SetHandler(gFlatWaterLayerType, Param_GetTableEvent(), FlatWaterLayer_GetParams);
    TaskType_SetHandler(gFlatWaterLayerType, Param_GetChangedEvent(), FlatLayer_ParamsChanged);
    TaskType_SetHandler2(gFlatWaterLayerType, 1, FlatLayer_Run);
    gFlatWaterParams = ParamTable_New(4);
    ParamTable_Add(gFlatWaterParams, &w, &w.unk7C, Param_Real32(), 7, kRange01);
    ParamTable_Add(gFlatWaterParams, &w, &w.height, Param_Real64(), 7, NULL);
    ParamTable_Add(gFlatWaterParams, &w, &w.unk78, Param_Real32(), 7, NULL);
    ParamTable_Add(gFlatWaterParams, &w, &w.color, Param_RGB(), 7, NULL);
    gFlatWaterLayerParams = ParamTable_New(6);
    ParamTable_Add(gFlatWaterLayerParams, &l, &l.name, Param_String256(), 7, NULL);
    ParamTable_Add(gFlatWaterLayerParams, &l, &l.scale, Param_Real32(), 7, NULL);
    ParamTable_Add(gFlatWaterLayerParams, &l, &l.speedU, Param_Real32(), 7, kRange11);
    ParamTable_Add(gFlatWaterLayerParams, &l, &l.speedV, Param_Real32(), 7, kRange11);
    ParamTable_Add(gFlatWaterLayerParams, &l, &l.alpha, Param_Real32(), 7, kRange01);
    ParamTable_Add(gFlatWaterLayerParams, &l, &l.color, Param_RGB(), 7, NULL);
}
HOOK(0x51C2B0, FlatWater_Open);

/* screen y of the first row's line at x, or +-FLT_MAX for a vertical line */
static long double edge_y(const WaterEdge *e, long double x)
{
    long double p = (e->by - e->ay) * (x - e->ax);

    if (fne(e->dx, kZero))
        return p / e->dx + e->ay;
    return p > kZero ? kFltMax : kFltMaxNeg;
}

/* a clipped point is kept on the first row, or on later rows when it lies
   on the left or right screen edge */
static int keep_point(float x, int row)
{
    if (row != 0 && fne(x, gViewX) && fne(gViewW + gViewX, x))
        return 0;
    if (!(x >= gViewX))
        return 0;
    return gViewW + gViewX >= x;
}

/* "a < b" as tested with C0: true when unordered */
static int flt(float a, float b)
{
    return !(a >= b);
}

/* Draws each layer as one polygon: the far edge of the water plane is
   projected for ten rows bending towards the horizon, clipped to the
   screen, the screen corners on the water side are added, and the points
   are sorted top to bottom. The last layer is drawn once more in the water
   color with a fog alpha from the depth. */
void FlatWater_Render(FlatWater *w)
{
    Point2 pts[WATER_MAX], uv[WATER_MAX], sa, sb;
    int flags[WATER_MAX], order[WATER_MAX];
    float depth[WATER_MAX], fog[WATER_MAX];
    QVertex verts[WATER_MAX];
    QVertexStrip strip;
    /* the original leaves the edge unset when the far edge is behind the
       view; any side other than 0 or 1 then adds all screen corners */
    WaterEdge edge = { .side = -1 };
    float a, h, nega, cam[10], right[3], fa[3], n0[3], n1[2], va[3], vb[3], ox, oy, dxf, dyf, d14, d18;
    long double dh, inv, e, n1z, va2, dx, k01, t, delta, xw, yh;
    FlatLayer *l;
    int i, j, k, n, c;

    a = w->unk78 * k4096;
    dh = w->height - gCamZ;
    h = dh;
    if (!(dh >= kZero))
        dh = -dh;
    if (!(dh >= kNear))
        return;
    memcpy(cam, gViewM, sizeof(cam));
    if (!fne(cam[6], kZero) && !fne(cam[7], kZero))
        cam[6] = 1.0f;
    cam[8] = 0;
    right[2] = 0;
    inv = kOne64 / __builtin_sqrtl((long double)cam[6] * cam[6] + (long double)cam[7] * cam[7]);
    cam[6] = cam[6] * inv;
    cam[7] = cam[7] * inv;
    right[0] = kZero - cam[7];
    right[1] = -(kZero - cam[6]);
    l = (FlatLayer *)Task_FindChild(w, gFlatWaterLayerType);
    if (!l)
        return;
    nega = -a;
    do {
        ox = gViewCX + gViewX;
        oy = gViewCY + gViewY;
        n = 0;
        e = cam[6] * a;
        fa[0] = e;
        n0[0] = right[0] * a + e;
        e = cam[7] * a;
        fa[1] = e;
        n0[1] = right[1] * a + e;
        e = cam[8] * a;
        fa[2] = e;
        n0[2] = right[2] * a + e + h;

        for (k = 0; k < WATER_ROWS; k++) {
            int side;

            n1[0] = nega * right[0] + fa[0];
            n1[1] = nega * right[1] + fa[1];
            n1z = nega * right[2] + fa[2] + h;
            va[0] = n1z * gViewM[2] + n1[1] * gViewM[1] + n1[0] * gViewM[0];
            va[1] = n1z * gViewM[5] + n1[1] * gViewM[4] + n1[0] * gViewM[3];
            va2 = n1z * gViewM[8] + n1[1] * gViewM[7] + n1[0] * gViewM[6];
            va[2] = va2;
            vb[0] = n0[2] * gViewM[2] + n0[1] * gViewM[1] + n0[0] * gViewM[0];
            vb[1] = n0[2] * gViewM[5] + n0[1] * gViewM[4] + n0[0] * gViewM[3];
            vb[2] = n0[2] * gViewM[8] + n0[1] * gViewM[7] + n0[0] * gViewM[6];
            if (!(va2 > kZero) || !(vb[2] > kZero))
                continue;
            TransContext_ProjectView(&sa.x, va);
            TransContext_ProjectView(&sb.x, vb);
            side = !(sa.x >= sb.x) ^ !(h >= kZero);
            edge.side = side;
            dx = sb.x - sa.x;
            dxf = dx;
            k01 = (long double)k * kFlatTenth;
            t = oy - dx * kHalf;
            t = t - (sa.y + sb.y) * kHalf;
            delta = t * (k01 * k01);
            sa.y = delta + sa.y;
            sb.y = delta + sb.y;
            if (!(va[2] >= kZero) || !(vb[2] >= kZero))
                continue;

            /* from a towards b, onto the screen rectangle */
            dyf = sb.y - sa.y;
            if (sa.x > sb.x) {
                pts[n].y = (gViewX - sa.x) * dyf / dxf + sa.y;
                pts[n].x = gViewX;
            } else {
                xw = gViewW + gViewX;
                pts[n].y = (xw - sa.x) * dyf / dxf + sa.y;
                pts[n].x = xw;
            }
            if (!(pts[n].y >= gViewY) || !(gViewH + gViewY >= pts[n].y)) {
                if (sa.y > sb.y) {
                    pts[n].x = (gViewY - sa.y) * dxf / dyf + sa.x;
                    pts[n].y = gViewY;
                } else {
                    yh = gViewH + gViewY;
                    pts[n].x = (yh - sa.y) * dxf / dyf + sa.x;
                    pts[n].y = yh;
                }
            }
            if (keep_point(pts[n].x, k)) {
                flags[n] = k == 0;
                n++;
            }

            /* from b towards a */
            d18 = sa.y - sb.y;
            d14 = sa.x - sb.x;
            if (sb.x > sa.x) {
                pts[n].y = (gViewX - sb.x) * d18 / d14 + sb.y;
                pts[n].x = gViewX;
            } else {
                xw = gViewW + gViewX;
                pts[n].y = (xw - sb.x) * d18 / d14 + sb.y;
                pts[n].x = xw;
            }
            if (!(pts[n].y >= gViewY) || !(gViewH + gViewY >= pts[n].y)) {
                if (sb.y > sa.y) {
                    pts[n].x = (gViewY - sb.y) * d14 / d18 + sb.x;
                    pts[n].y = gViewY;
                } else {
                    yh = gViewH + gViewY;
                    pts[n].x = (yh - sb.y) * d14 / d18 + sb.x;
                    pts[n].y = yh;
                }
            }
            if (keep_point(pts[n].x, k)) {
                flags[n] = k == 0;
                n++;
            }
            if (k == 0) {
                edge.ax = sa.x;
                edge.ay = sa.y;
                edge.bx = sb.x;
                edge.by = sb.y;
            }
        }

        /* screen corners on the water side of the first row */
        edge.dx = edge.bx - edge.ax;
        for (i = 0; i < 4; i++) {
            long double cx = (long double)(i & 1) * gViewW + gViewX;
            long double cy = (long double)(i >> 1 & 1) * gViewH + gViewY;
            float cyf = cy;

            if ((!(cy >= edge_y(&edge, cx))) != edge.side) {
                pts[n].x = cx;
                pts[n].y = cyf;
                flags[n] = 0;
                n++;
            }
        }

        /* points where the polygon outline meets the screen sides */
        for (j = 0; j < n; j++) {
            long double ex, ly;
            int dup = 0, t1, lo, hi;

            for (i = 0; i < n; i++) {
                if (i != j && !fne(pts[j].y, pts[i].y))
                    dup = 1;
            }
            ex = fne(pts[j].x, gViewX) ? (long double)gViewX : (long double)gViewW + gViewX;
            ly = edge_y(&edge, ex);
            if (dup)
                continue;
            if ((ly >= pts[j].y) != edge.side || (!(ly > pts[j].y)) == edge.side) {
                pts[n].x = ex;
                pts[n].y = pts[j].y;
                flags[n] = 0;
                n++;
                j = 0;
                continue;
            }
            t1 = fne(pts[j].x, gViewX);
            lo = -1;
            hi = -1;
            for (i = 0; i < n; i++) {
                if (i == j)
                    continue;
                if (flt(pts[i].y, pts[j].y) &&
                    (lo == -1 || (pts[i].y >= pts[lo].y &&
                                  (fne(pts[i].y, pts[lo].y) || (pts[i].x > pts[lo].x) != t1))) &&
                    flags[i])
                    lo = i;
                if (pts[i].y > pts[j].y &&
                    (hi == -1 || (!(pts[i].y > pts[hi].y) &&
                                  (fne(pts[i].y, pts[hi].y) || (pts[i].x > pts[hi].x) != t1))) &&
                    flags[i])
                    hi = i;
            }
            if (lo == -1 || hi == -1 || !fne(pts[lo].x, pts[hi].x))
                continue;
            pts[n].x = (pts[hi].x - pts[lo].x) * (pts[j].y - pts[lo].y) / (pts[hi].y - pts[lo].y) + pts[lo].x;
            pts[n].y = pts[j].y;
            if (!(pts[n].y >= gViewY) || pts[n].y > gViewH + gViewY)
                continue;
            flags[n] = 0;
            n++;
            j = 0;
        }

        /* order by y, then x */
        {
            float px = -0x1.fffffep+127f, py = -0x1.fffffep+127f;

            for (c = 0; c < n; c++) {
                float bx = 0x1.fffffep+127f, by = 0x1.fffffep+127f;
                int best = -1;

                for (i = 0; i < n; i++) {
                    if ((flt(py, pts[i].y) || (flt(px, pts[i].x) && !fne(py, pts[i].y))) &&
                        (flt(pts[i].y, by) || (flt(pts[i].x, bx) && !fne(pts[i].y, by)))) {
                        bx = pts[i].x;
                        by = pts[i].y;
                        best = i;
                    }
                }
                order[c] = best;
                px = bx;
                py = by;
            }
        }

        /* texture coordinates and depth from where each point meets the plane */
        for (i = 0; i < n; i++) {
            long double su = (pts[i].x - ox) / gViewScaleX;
            long double sv = (pts[i].y - oy) / gViewScaleY;
            float dir[3], p[3], nrm[3], hit[3];
            long double t1, z, f;
            double t2;

            dir[0] = sv * gViewM[3] + su * gViewM[0] + gViewM[6];
            dir[1] = sv * gViewM[4] + su * gViewM[1] + gViewM[7];
            dir[2] = sv * gViewM[5] + su * gViewM[2] + gViewM[8];
            if (fne(dir[0], kZero) || fne(dir[1], kZero) || fne(dir[2], kZero)) {
                inv = kOne64 / __builtin_sqrtl((long double)dir[0] * dir[0] + (long double)dir[1] * dir[1] +
                                               (long double)dir[2] * dir[2]);
                dir[0] = dir[0] * inv;
                dir[1] = dir[1] * inv;
                dir[2] = dir[2] * inv;
            }
            p[0] = 0;
            p[1] = 0;
            p[2] = -h;
            nrm[0] = 0;
            nrm[1] = 0;
            nrm[2] = -h;
            if (fne(nrm[2], kZero)) {
                inv = kOne64 / __builtin_sqrtl((long double)nrm[2] * nrm[2]);
                nrm[0] = kZero;
                nrm[1] = kZero;
                nrm[2] = nrm[2] * inv;
            }
            Vec3_RayPlane(hit, p, dir, nrm);
            hit[0] = hit[0] - p[0];
            hit[1] = hit[1] - p[1];
            hit[2] = hit[2] - p[2];
            t1 = (gCamPos[0] + k2Pow30) * l->scale;
            t2 = (gCamPos[1] + k2Pow30) * l->scale;
            t1 = t1 - (long double)x87_fistp(t1);
            t2 = t2 - (long double)x87_fistp(t2);
            uv[i].x = hit[0] * l->scale + t1 + l->offsetU;
            uv[i].y = hit[1] * l->scale + t2 + l->offsetV;
            z = hit[2] * gViewM[8] + hit[1] * gViewM[7] + hit[0] * gViewM[6];
            if (!(z > k4096))
                z = k4096;
            depth[i] = z;
            f = ((long double)Float_Log2(depth[i]) - kFogBias) * gTerrainFogScale * k2000 * k256th;
            if (!(f >= kZero))
                f = kZero;
            else if (f > kOne)
                f = kOne;
            fog[i] = f;
        }

        if (n > 2) {
            strip.untextured = 0;
            strip.flags = 0x51420000;
            strip.unk8 = -1;
            strip.count = n;
            strip.verts = verts;
            gQVertexTexture = l->loaded ? l->texture : NULL;
            for (c = 0; c < n; c++) {
                QVertex *v = &verts[c];

                i = order[c];
                memcpy(&v->a, &l->alpha, 4 * sizeof(float));
                memcpy(&v->u, &uv[i], sizeof(Point2));
                memcpy(&v->x, &pts[i], sizeof(Point2));
                memcpy(&v->z, &depth[i], sizeof(float));
            }
            gRenderFuncs[gQVertexStrip](&strip);
        }
        if (l == (FlatLayer *)Task_FindLastChild(w, gFlatWaterLayerType) && n > 2) {
            strip.verts = verts;
            strip.flags = 0x51400000;
            strip.untextured = 0;
            strip.unk8 = -1;
            strip.count = n;
            gQVertexTexture = NULL;
            for (c = 0; c < n; c++) {
                QVertex *v = &verts[c];

                i = order[c];
                memcpy(&v->a, &fog[i], sizeof(float));
                memcpy(&v->r, w->color, 3 * sizeof(float));
                v->u = 0;
                v->v = 0;
                memcpy(&v->x, &pts[i], sizeof(Point2));
                memcpy(&v->z, &depth[i], sizeof(float));
            }
            gRenderFuncs[gQVertexStrip](&strip);
        }
        l = (FlatLayer *)Task_FindNext(l, gFlatWaterLayerType);
    } while (l);
}
HOOK(0x51C5B0, FlatWater_Render);

void FlatWater_Create(FlatWater *w)
{
    WaterState *s;

    w->world = gWorld;
    w->height = 5.0;
    w->unk7C = 0x1.aaaaap-1f;
    w->unk78 = 30000.0f;
    w->color[0] = 0;
    w->color[1] = 0;
    w->color[2] = 0;
    DynCube_ListAdd(w->world, w, -1);
    *(WaterState **)(gWorld + 0xDFC) = &gWaterState;
    s = *(WaterState **)(gWorld + 0xDFC);
    memcpy(&s->height, &w->height, sizeof(double));
}
HOOK(0x51D840, FlatWater_Create);

void FlatWater_Delete(FlatWater *w)
{
    *(WaterState **)(gWorld + 0xDFC) = NULL;
    DynCube_ListRemove(w->world, w, -1);
}
HOOK(0x51D8B0, FlatWater_Delete);

void FlatWater_GetParams(void *task, void **out)
{
    *out = gFlatWaterParams;
}
HOOK(0x51D8E0, FlatWater_GetParams);

/* NaN and below 0 give 0, above 1 gives 1 */
static float clamp01(float v)
{
    if (!(v >= kZero))
        return kZero;
    if (v <= kOne)
        return v;
    return kOne;
}

void FlatWater_ParamsChanged(FlatWater *w)
{
    FlatLayer *l;
    int n = 0;

    memcpy(&gWaterState.height, &w->height, sizeof(double));
    gWaterState.color[0] = 0;
    gWaterState.color[1] = 0;
    gWaterState.color[2] = 0;
    l = (FlatLayer *)Task_FindChild(w, gFlatWaterLayerType);
    if (l) {
        do {
            n++;
            gWaterState.color[0] = gWaterState.color[0] + l->color[0];
            gWaterState.color[1] = gWaterState.color[1] + l->color[1];
            gWaterState.color[2] = gWaterState.color[2] + l->color[2];
            l = (FlatLayer *)Task_FindNext(l, gFlatWaterLayerType);
        } while (l);
        if (n > 0) {
            long double k = kOne / (long double)n;

            gWaterState.color[0] = k * gWaterState.color[0];
            gWaterState.color[1] = k * gWaterState.color[1];
            gWaterState.color[2] = k * gWaterState.color[2];
        }
    }
    w->color[0] = clamp01(w->color[0]);
    w->color[1] = clamp01(w->color[1]);
    w->color[2] = clamp01(w->color[2]);
}
HOOK(0x51D8F0, FlatWater_ParamsChanged);

/* scrolls the texture, keeping both offsets in 0..1 */
void FlatLayer_Run(FlatLayer *l)
{
    long double x, v;
    float u;

    x = l->speedU + l->offsetU;
    u = x;
    l->offsetU = x;
    v = l->speedV + l->offsetV;
    l->offsetV = v;
    if (u > kOne) {
        do {
            x = l->offsetU - kOne;
            l->offsetU = x;
        } while (x > kOne);
    }
    if (v > kOne) {
        do {
            x = l->offsetV - kOne;
            l->offsetV = x;
        } while (x > kOne);
    }
    if (!(l->offsetU >= kZero)) {
        do {
            x = l->offsetU + kOne;
            l->offsetU = x;
        } while (!(x >= kZero));
    }
    if (!(l->offsetV >= kZero)) {
        do {
            x = l->offsetV + kOne;
            l->offsetV = x;
        } while (!(x >= kZero));
    }
}
HOOK(0x51DA90, FlatLayer_Run);

void FlatLayer_Create(FlatLayer *l)
{
    l->loaded = 0;
    l->speedU = 0;
    l->speedV = 0;
    l->offsetU = 0;
    l->offsetV = 0;
    l->scale = 0x1.a36e2ep-14f;
    l->alpha = 1.0f;
    l->color[0] = 1.0f;
    l->color[1] = 1.0f;
    l->color[2] = 1.0f;
    crt_sprintf(l->lastName, gScriptEmptyString);
    crt_sprintf(l->name, gScriptEmptyString);
}
HOOK(0x51DB80, FlatLayer_Create);

/* Loads the texture when the name changed. A missing file turns the name
   into "File not found". */
void FlatLayer_ParamsChanged(FlatLayer *l)
{
    char path[512];

    l->color[0] = clamp01(l->color[0]);
    l->color[1] = clamp01(l->color[1]);
    l->color[2] = clamp01(l->color[2]);
    crt_sprintf(path, "LOCAL:%s%s", gScriptEmptyString, l->name);
    if (l->name[0]) {
        if (crt_strstr(l->lastName, l->name) && strlen(l->name) == strlen(l->lastName)) {
            crt_sprintf(l->lastName, l->name);
            return;
        }
        if (!crt_strstr(l->name, "File not found") && Resource_Exists(path)) {
            if (l->loaded) {
                Texture_Free(l->texture);
                l->loaded = 0;
            }
            Texture_LoadFile(l->texture, NULL, path);
            l->loaded = 1;
        } else {
            crt_sprintf(l->name, "File not found");
        }
    }
    crt_sprintf(l->lastName, l->name);
}
HOOK(0x51DBF0, FlatLayer_ParamsChanged);

void FlatWaterLayer_GetParams(void *task, void **out)
{
    *out = gFlatWaterLayerParams;
}
HOOK(0x51DDA0, FlatWaterLayer_GetParams);

void FlatWater_Close(void)
{
    QTaskList_Free(gFlatWaterLayerParams);
    QTaskList_Free(gFlatWaterParams);
    TaskType_Free(gFlatWaterLayerType);
    TaskType_Free(gFlatWaterType);
}
HOOK(0x51DDB0, FlatWater_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5333E0) float kOne INIT(1.0f);
ALIAS(kBOne, kOne, 0);
DATA(5333EC) float kZero INIT(0.0f);
ALIAS(gZero, kZero, 0);
ALIAS(kBZero, kZero, 0);
DATA(534B38) float kFltMaxNeg INIT(-3.4028235e+38f);
DATA(534B38) float kFltMax INIT(3.4028235e+38f);
PAD(igd, 534B38, 534B40, 0x1C) INIT({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xF3, 0x3F, 0x00, 0x00, 0x20, 0x41});
DATA(534B38) float kFlatTenth INIT(0.1f);
DATA(534C50) double k256th INIT(0.00390625);
PAD(igd, 534C50, 534C58, 0x18) INIT({0x3E, 0xCF, 0xF3, 0x38, 0x00, 0x00, 0xC0, 0xC0, 0xE4, 0x38, 0x8E, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xAC, 0x40});
DATA(534C50) float k2000 INIT(2000.0f);
PAD(igd, 534C50, 534C74, 0x4);
DATA(534C50) double k2Pow30 INIT(1073741824.0);
PAD_CONST(igd, 54EE68, 54EE68, 0x8) INIT({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F});
ALIAS(kRange01, pad_54EE68, 0);
PAD_CONST(igd, 54EE68, 54EE70, 0x40) INIT({0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x3F, 0x46, 0x6C, 0x61, 0x74, 0x57, 0x61, 0x74, 0x65, 0x72, 0x4C, 0x61, 0x79, 0x65, 0x72, 0x00, 0x00, 0x46, 0x6C, 0x61, 0x74, 0x57, 0x61, 0x74, 0x65, 0x72, 0x00, 0x00, 0x00, 0x46, 0x69, 0x6C, 0x65, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x66, 0x6F, 0x75, 0x6E, 0x64, 0x00, 0x00, 0x4C, 0x4F, 0x43, 0x41, 0x4C, 0x3A, 0x25, 0x73, 0x25, 0x73});
ALIAS(kRange11, pad_54EE70, 0);
BSS(BA2000) WaterState gWaterState;
BSS(BA2000) uint16_t gFlatWaterLayerType;
PAD(igb, BA2000, BA201A, 0x2);
BSS(BA2000) void *gFlatWaterParams;
BSS(BA2000) uint16_t gFlatWaterType;
PAD(igb, BA2000, BA2022, 0x2);
BSS(BA2000) void *gFlatWaterLayerParams;
