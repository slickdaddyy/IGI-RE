/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "flatwater/flatwater.h"
#include "bonemesh/bonemeshvars.h"
#include "file/filevars.h"
#include "math/mathvars.h"

/* "FlatSky": a sky dome with a gradient of five colors and a clear color,
   plus "FlatSkyLayer" cloud textures. */

ORIG(0x491E70, void, Display_SetClearColor, (uint8_t r, uint8_t g, uint8_t b));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x52CB30, void *, SkyDome_New, (void));
ORIG(0x52D030, void, SkyDome_Free, (void *dome));
ORIG(0x52D2B0, void, SkyDome_SetColors, (void *dome, float *c0, float *c2, float *c1, float *c4, float *c3));
ORIG(0x52D350, void, SkyDome_SetAngle, (void *dome, float angle));
ORIG(0x52D060, void, SkyDome_Render, (void *dome));
ORIG(0x4B2900, void, Vec2_Normalize, (float *v));
ORIG(0x4B2980, long double, Vec2_Dot, (const float *a, const float *b));

typedef struct FlatSky {
    Task task;
    char unk20[0x48];
    struct Terrain *world; /* 0x68 */
    float unk6C;        /* 5 */
    float unk70;        /* 100 */
    float unk74;        /* 0..1 */
    float clear[3];     /* 0x78 */
    void *dome;         /* 0x84 */
    float angle;        /* 0x88, degrees 0..90 */
    float colors[5][3]; /* 0x8C */
    char mirror;        /* 0xC8, colors 3 and 1 copy 4 and 2 */
    char padC9[7];
} FlatSky;

_Static_assert(sizeof(FlatSky) == 0xD0, "FlatSky");

extern uint16_t gFlatSkyLayerType;
extern uint16_t gFlatSkyType;
extern void *gFlatSkyLayerParams;
extern void *gFlatSkyParams;

extern const float kSkyRange01[];
extern const float kSkyRange11[];
extern const float kRange90[];
extern float k180th;

extern float kAlmostOne;
extern float kStripStep; /* 1/12 and a bit */
extern float kStripGrow; /* 1.001 */
extern float k16;

#define SKY_ROWS 13

void FlatSky_Render(FlatSky *s);
void FlatSky_Create(FlatSky *s);
void FlatSky_Delete(FlatSky *s);
void FlatSky_GetParams(void *task, void **out);
void FlatSky_ParamsChanged(FlatSky *s);
void FlatSkyLayer_GetParams(void *task, void **out);

void FlatSky_Open(void)
{
    FlatSky s;
    FlatLayer l;

    gFlatSkyType = TaskType_New("FlatSky", sizeof(FlatSky), Task_GetRootType(), 0);
    gFlatSkyLayerType = TaskType_New("FlatSkyLayer", sizeof(FlatLayer), Task_GetRootType(), 0);
    TaskType_SetHandler(gFlatSkyType, 0, FlatSky_Create);
    TaskType_SetHandler2(gFlatSkyType, 2, FlatSky_Delete);
    TaskType_SetHandler(gFlatSkyType, 4, FlatSky_Render);
    TaskType_SetHandler2(gFlatSkyType, 1, SUBLEVEL_RUN);
    TaskType_SetHandler(gFlatSkyType, Param_GetTableEvent(), FlatSky_GetParams);
    TaskType_SetHandler(gFlatSkyType, Param_GetChangedEvent(), FlatSky_ParamsChanged);
    TaskType_SetHandler(gFlatSkyLayerType, 0, FlatLayer_Create);
    TaskType_SetHandler2(gFlatSkyLayerType, 2, FlatLayer_Delete);
    TaskType_SetHandler(gFlatSkyLayerType, Param_GetTableEvent(), FlatSkyLayer_GetParams);
    TaskType_SetHandler(gFlatSkyLayerType, Param_GetChangedEvent(), FlatLayer_ParamsChanged);
    TaskType_SetHandler2(gFlatSkyLayerType, 1, FlatLayer_Run);
    gFlatSkyParams = ParamTable_New(11);
    ParamTable_Add(gFlatSkyParams, &s, &s.unk74, Param_Real32(), 7, kSkyRange01);
    ParamTable_Add(gFlatSkyParams, &s, &s.unk6C, Param_Real32(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.unk70, Param_Real32(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.clear, Param_RGB(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.mirror, Param_Bool8(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.angle, Param_Degrees(), 7, kRange90);
    ParamTable_Add(gFlatSkyParams, &s, &s.colors[0], Param_RGB(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.colors[2], Param_RGB(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.colors[1], Param_RGB(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.colors[4], Param_RGB(), 7, NULL);
    ParamTable_Add(gFlatSkyParams, &s, &s.colors[3], Param_RGB(), 7, NULL);
    gFlatSkyLayerParams = ParamTable_New(6);
    ParamTable_Add(gFlatSkyLayerParams, &l, &l.name, Param_String256(), 7, NULL);
    ParamTable_Add(gFlatSkyLayerParams, &l, &l.scale, Param_Real32(), 7, NULL);
    ParamTable_Add(gFlatSkyLayerParams, &l, &l.speedU, Param_Real32(), 7, kSkyRange11);
    ParamTable_Add(gFlatSkyLayerParams, &l, &l.speedV, Param_Real32(), 7, kSkyRange11);
    ParamTable_Add(gFlatSkyLayerParams, &l, &l.alpha, Param_Real32(), 7, kSkyRange01);
    ParamTable_Add(gFlatSkyLayerParams, &l, &l.color, Param_RGB(), 7, NULL);
}
HOOK(0x51DDF0, FlatSky_Open);

/* Draws each layer as a strip of 13 quads covering the part of the screen
   above the horizon. The horizon line is found by projecting two far points
   straight ahead and behind on the sky plane; screen points are turned back
   into sky plane positions for the texture coordinates. Layers fade out
   towards the horizon. */
void FlatSky_Render(FlatSky *s)
{
    float a, b, c, d, cam[10], p, r0, r1, r2, r3, r4, r5, r6, r7, nega, t0, t1, t2;
    float v0[3], v1[3], s0[2], s1[2], q[4][2], dir[2], perp[2], mid[2], dist[2];
    float maxPerp, minAlong, maxAlong, stepX, stepY, e2, e3, e4, ox, oy;
    long double ce, inv, n7, qq, r8, u, t, ea, eb, ec, ed;
    QVertex verts[SKY_ROWS * 2];
    QVertexStrip strip;
    FlatLayer *l;
    int i;

    a = s->unk70 * k4096;
    b = s->unk6C * k4096;
    ce = a - (a - kNear) * s->unk74;
    c = ce;
    d = kOne / (ce - a);
    SkyDome_Render(s->dome);
    for (i = 0; i < 10; i++)
        ((uint32_t *)cam)[i] = ((uint32_t *)gTransContext.matrix.m)[i];
    if (!fne(cam[6], kZero) && !fne(cam[7], kZero))
        cam[6] = 1.0f;

    /* frame turned to face along the view's horizontal direction */
    inv = kOne64 / __builtin_sqrtl((long double)cam[6] * cam[6] + (long double)cam[7] * cam[7]);
    cam[6] = cam[6] * inv;
    n7 = inv * cam[7];
    p = kZero - n7;
    qq = -(kZero - cam[6]);
    r0 = gTransContext.matrix.m[2] * kZero + gTransContext.matrix.m[1] * qq + gTransContext.matrix.m[0] * p;
    *(uint32_t *)&r1 = ((uint32_t *)gTransContext.matrix.m)[2];
    r2 = gTransContext.matrix.m[1] * n7 + gTransContext.matrix.m[0] * cam[6] + gTransContext.matrix.m[2] * kZero;
    r3 = gTransContext.matrix.m[5] * kZero + gTransContext.matrix.m[4] * qq + gTransContext.matrix.m[3] * p;
    *(uint32_t *)&r4 = ((uint32_t *)gTransContext.matrix.m)[5];
    r5 = gTransContext.matrix.m[4] * n7 + gTransContext.matrix.m[3] * cam[6] + gTransContext.matrix.m[5] * kZero;
    r6 = gTransContext.matrix.m[8] * kZero + gTransContext.matrix.m[7] * qq + gTransContext.matrix.m[6] * p;
    *(uint32_t *)&r7 = ((uint32_t *)gTransContext.matrix.m)[8];
    r8 = gTransContext.matrix.m[7] * n7 + gTransContext.matrix.m[6] * cam[6] + gTransContext.matrix.m[8] * kZero;

    nega = -a;
    t0 = r2 * a + r1 * b;
    v0[0] = r0 * a + t0;
    t1 = r5 * a + r4 * b;
    v0[1] = r3 * a + t1;
    t2 = r7 * b;
    u = r8 * a;
    v0[2] = r6 * a + u + t2;
    v1[0] = nega * r0 + t0;
    v1[1] = nega * r3 + t1;
    v1[2] = nega * r6 + u + t2;
    if (!(v0[2] >= kNear) || !(v1[2] >= kNear))
        return;
    TransContext_ProjectView(s0, v0);
    TransContext_ProjectView(s1, v1);

    q[0][0] = gScene.x;
    q[0][1] = gScene.y;
    q[1][0] = gScene.w + gScene.x;
    q[1][1] = gScene.y;
    q[2][0] = gScene.x;
    q[2][1] = gScene.h + gScene.y;
    q[3][0] = q[1][0];
    q[3][1] = q[2][1];
    dir[0] = s1[0] - s0[0];
    dir[1] = s1[1] - s0[1];
    Vec2_Normalize(dir);
    perp[1] = -dir[0];
    perp[0] = dir[1];
    maxPerp = -1.0f;
    minAlong = 100000.0f;
    maxAlong = -100000.0f;
    mid[0] = (s1[0] - s0[0]) * kHalf + s0[0];
    mid[1] = (s1[1] - s0[1]) * kHalf + s0[1];
    for (i = 0; i < 4; i++) {
        dist[0] = q[i][0] - mid[0];
        dist[1] = q[i][1] - mid[1];
        t = Vec2_Dot(dist, perp);
        if (!(maxPerp > t))
            maxPerp = t;
        t = Vec2_Dot(dist, dir);
        if (minAlong >= t)
            minAlong = t;
        if (!(maxAlong > t))
            maxAlong = t;
    }
    if (!(maxPerp >= kZero))
        return;

    stepX = maxPerp * perp[0] * kStripStep;
    stepY = maxPerp * perp[1] * kStripStep;
    ea = maxAlong * dir[0] * kStripGrow;
    e2 = maxAlong * dir[1] * kStripGrow;
    e3 = minAlong * dir[0] * kStripGrow;
    e4 = minAlong * dir[1] * kStripGrow;
    ea = ea + mid[0];
    eb = mid[1] + e2;
    ec = mid[0] + e3;
    ed = mid[1] + e4;
    for (i = 0; i < SKY_ROWS; i++) {
        verts[2 * i].x = ec;
        verts[2 * i].y = ed;
        verts[2 * i + 1].x = ea;
        verts[2 * i + 1].y = eb;
        ec = ec + stepX;
        ed = ed + stepY;
        ea = ea + stepX;
        eb = eb + stepY;
    }
    ox = gScene.halfW + gScene.x;
    oy = gScene.halfH + gScene.y;

    for (l = (FlatLayer *)Task_FindChild(s, gFlatSkyLayerType); l;
         l = (FlatLayer *)Task_FindNext(l, gFlatSkyLayerType)) {
        long double m6 = gTransContext.matrix.m[6], m7 = gTransContext.matrix.m[7], m8 = gTransContext.matrix.m[8];

        for (i = 0; i < SKY_ROWS * 2; i++) {
            QVertex *v = &verts[i];
            long double su = (v->x - ox) / gTransContext.scaleX;
            long double sv = (v->y - oy) / gTransContext.scaleY;
            float w0, w1, w2, x, y, f;
            long double r;

            w0 = sv * gTransContext.matrix.m[3] + su * gTransContext.matrix.m[0] + m6;
            w1 = sv * gTransContext.matrix.m[4] + su * gTransContext.matrix.m[1] + m7;
            w2 = sv * gTransContext.matrix.m[5] + su * gTransContext.matrix.m[2] + m8;
            r = b / w2;
            x = r * w0;
            y = r * w1;
            v->u = x * l->scale + l->offsetU;
            v->v = y * l->scale + l->offsetV;
            v->z = r * w2 * gTransContext.matrix.m[8] + y * gTransContext.matrix.m[7] + x * gTransContext.matrix.m[6];
            ((uint32_t *)&v->r)[0] = ((uint32_t *)l->color)[0];
            ((uint32_t *)&v->r)[1] = ((uint32_t *)l->color)[1];
            ((uint32_t *)&v->r)[2] = ((uint32_t *)l->color)[2];
            if (!(v->z >= c)) {
                v->a = 1.0f;
            } else {
                f = (c - kOne / v->z * c * a) * d;
                v->a = f;
                if (!(f >= kZero))
                    v->a = 0;
                else if (f > kAlmostOne)
                    v->a = 0x1.ff7ceep-1f;
            }
            v->a = v->a * l->alpha;
            v->z = v->z * k16;
        }
        strip.lines = 0;
        strip.flags = 0x41480000;
        strip.queue = -1;
        strip.count = SKY_ROWS * 2;
        strip.verts = verts;
        gQVertexTexture = l->loaded ? &l->texture : NULL;
        gRenderModeCurrent.methods[gQVertexStrip](&strip);
    }
}
HOOK(0x51E1F0, FlatSky_Render);

void FlatSky_Create(FlatSky *s)
{
    static const float blue[3] = { 0x1.851eb8p-3f, 0x1.333334p-2f, 0x1.b851ecp-2f };
    int i, j;

    s->world = gCurrentWorld;
    s->unk74 = 0.5f;
    s->unk70 = 100.0f;
    s->unk6C = 5.0f;
    s->clear[0] = 0;
    s->clear[1] = 0;
    s->clear[2] = 0;
    DynCube_ListAdd(s->world, s, -1);
    s->dome = SkyDome_New();
    s->angle = 15.0f;
    s->mirror = 0;
    for (i = 0; i < 5; i++)
        for (j = 0; j < 3; j++)
            s->colors[i][j] = blue[j];
}
HOOK(0x51EA10, FlatSky_Create);

void FlatSky_Delete(FlatSky *s)
{
    SkyDome_Free(s->dome);
    DynCube_ListRemove(s->world, s, -1);
}
HOOK(0x51EB00, FlatSky_Delete);

void FlatSky_GetParams(void *task, void **out)
{
    *out = gFlatSkyParams;
}
HOOK(0x51EB30, FlatSky_GetParams);

void FlatSky_ParamsChanged(FlatSky *s)
{
    uint8_t b = x87_fistp(s->clear[2] * k255);
    uint8_t g = x87_fistp(s->clear[1] * k255);
    uint8_t r = x87_fistp(s->clear[0] * k255);
    float angle;
    int j;

    Display_SetClearColor(r, g, b);
    if (s->mirror) {
        for (j = 0; j < 3; j++)
            s->colors[3][j] = s->colors[4][j];
        for (j = 0; j < 3; j++)
            s->colors[1][j] = s->colors[2][j];
    }
    angle = s->angle * kPi * k180th;
    SkyDome_SetAngle(s->dome, angle);
    SkyDome_SetColors(s->dome, s->colors[0], s->colors[2], s->colors[1], s->colors[4], s->colors[3]);
}
HOOK(0x51EB40, FlatSky_ParamsChanged);

void FlatLayer_Delete(FlatLayer *l)
{
    if (l->loaded) {
        Texture_Free(&l->texture);
        l->loaded = 0;
    }
}
HOOK(0x51EC30, FlatLayer_Delete);

void FlatSkyLayer_GetParams(void *task, void **out)
{
    *out = gFlatSkyLayerParams;
}
HOOK(0x51EC60, FlatSkyLayer_GetParams);

void FlatSky_Close(void)
{
    QTaskList_Free(gFlatSkyLayerParams);
    QTaskList_Free(gFlatSkyParams);
    TaskType_Free(gFlatSkyLayerType);
    TaskType_Free(gFlatSkyType);
}
HOOK(0x51EC70, FlatSky_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(534B18) float kAlmostOne INIT(0.999f);
DATA(534C10) float k16 INIT(16.0f);
DATA(534C80) float kStripGrow INIT(1.001f);
DATA(534C80) float kStripStep INIT(0.08341667f);
DATA(534C80) float k180th INIT(0.0055555557f);
STORE(igd, 54EEB0, 4) const float store_54EEB0[2] INIT({0.0f, 1.0f});
ALIAS(kSkyRange01, store_54EEB0, 0);
STORE(igd, 54EEB0, 4) const float store_54EEB8[2] INIT({-1.0f, 1.0f});
ALIAS(kSkyRange11, store_54EEB8, 0);
STORE(igd, 54EEB0, 4) const float store_54EEC0[8] INIT({0.0f, 90.0f, 7.143938e+31f, 65383756.0f, 4.5452035e+30f, 0.0f, 7.143938e+31f, 1.1150596e-38f});
ALIAS(kRange90, store_54EEC0, 0);
BSS(BA1FDC) uint16_t gFlatSkyLayerType;
BSS(BA1FDC) uint16_t gFlatSkyType;
BSS(BA1FDC) void *gFlatSkyLayerParams;
BSS(BA1FDC) void *gFlatSkyParams;
