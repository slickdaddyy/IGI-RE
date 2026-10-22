#include "hook.h"
#include "igi.h"
#include "math/x87.h"
#include "menu/menu.h"

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));

/* the moving wire frame shapes behind the menus */
typedef struct BackgroundFX {
    Task task;
    float time;
} BackgroundFX;

/* 2x2 rotation and scale, then a move */
typedef struct BgTransform {
    float m[4];
    float tx, ty;
    float scale;
} BgTransform;

typedef struct BgLines {
    int count;
    float seg[][4];
} BgLines;

typedef struct BgCircle {
    float x, y, r;
} BgCircle;

/* (cols + 1) lines across and (rows + 1) lines down a w by h box centred on x, y */
typedef struct BgGrid {
    int cols, rows;
    float x, y, w, h;
} BgGrid;

typedef struct BgShape {
    BgLines *lines;
    int circleCount;
    BgCircle *circles;
    BgGrid *grid;
} BgShape;

#define gBackgroundFXType VAR(uint16_t, 0x539AF0)
#define gBgShapes ((BgShape *)0x539BE8)
#define gGridStep VAR(const float, 0x53357C)
#define gCircleDetail VAR(const float, 0x533580)
#define gTwoPi VAR(const float, 0x5334A8)
#define gBgSpeed ((const float *)0x533584)

static int round_int(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static void set_line_vertex(QVertex *v)
{
    v->z = 1.0f;
    v->u = 0;
    v->v = 0;
    v->a = 0.075f;
    v->r = 0;
    v->g = 1.0f;
    v->b = 0;
}

static void draw_strip(QVertex *v, int count)
{
    QVertexStrip s;

    s.untextured = 1;
    s.flags = 0x80000;
    s.unk8 = 0;
    s.count = count;
    s.verts = v;
    gRenderFuncs[gQVertexStrip](&s);
}

void BackgroundFX_SetTransform(BgTransform *t, float angle, float scale, float tx, float ty)
{
    long double c = x87_cos(angle) * scale;
    long double s = x87_sin(angle);

    t->tx = tx;
    t->ty = ty;
    t->scale = scale;
    t->m[0] = c;
    t->m[1] = s * scale;
    t->m[2] = -s * scale;
    t->m[3] = c;
}
HOOK(0x4192E0, BackgroundFX_SetTransform);

void BackgroundFX_DrawGrid(BgGrid *g, BgTransform *t)
{
    QVertex v[2];
    float wx, wy, hx, hy, x0, y0, x, y, dx, dy;
    long double cx;
    int i;

    set_line_vertex(&v[0]);
    set_line_vertex(&v[1]);
    cx = (long double)g->x * t->m[0] + (long double)g->y * t->m[2] + t->tx;
    y0 = rf((long double)g->y * t->m[3] + (long double)g->x * t->m[1] + t->ty);
    wx = rf((long double)g->w * t->m[0]);
    wy = rf((long double)g->w * t->m[1]);
    hx = rf((long double)g->h * t->m[2]);
    hy = rf((long double)g->h * t->m[3]);
    x0 = rf(cx - ((long double)hx + wx) * 0.5f);
    y0 = rf(y0 - ((long double)hy + wy) * 0.5f);
    dx = rf((long double)hx * gGridStep);
    dy = rf((long double)hy * gGridStep);
    x = x0;
    y = y0;
    for (i = 0; i <= g->cols; i++) {
        v[0].x = x;
        v[0].y = y;
        v[1].x = rf((long double)x + wx);
        v[1].y = rf((long double)y + wy);
        x = rf((long double)dx + x);
        y = rf((long double)dy + y);
        draw_strip(v, 2);
    }
    dx = rf((long double)wx * gGridStep);
    dy = rf((long double)wy * gGridStep);
    x = x0;
    y = y0;
    for (i = 0; i <= g->rows; i++) {
        v[0].x = x;
        v[0].y = y;
        v[1].x = rf((long double)x + hx);
        v[1].y = rf((long double)y + hy);
        x = rf((long double)dx + x);
        y = rf((long double)dy + y);
        draw_strip(v, 2);
    }
}
HOOK(0x419330, BackgroundFX_DrawGrid);

void BackgroundFX_DrawLines(BgLines *l, BgTransform *t)
{
    QVertex v[2];
    int i;

    set_line_vertex(&v[0]);
    set_line_vertex(&v[1]);
    for (i = 0; i < l->count; i++) {
        float *s = l->seg[i];

        v[0].x = rf((long double)s[1] * t->m[2] + (long double)s[0] * t->m[0] + t->tx);
        v[0].y = rf((long double)s[1] * t->m[3] + (long double)s[0] * t->m[1] + t->ty);
        v[1].x = rf((long double)s[3] * t->m[2] + (long double)s[2] * t->m[0] + t->tx);
        v[1].y = rf((long double)s[2] * t->m[1] + (long double)s[3] * t->m[3] + t->ty);
        draw_strip(v, 2);
    }
}
HOOK(0x419540, BackgroundFX_DrawLines);

void BackgroundFX_DrawCircle(BgCircle *c, BgTransform *t)
{
    QVertex v[41];
    long double angle = 0.0f, step, rx;
    float cx, cy, r;
    int n, i;

    cx = rf((long double)c->y * t->m[2] + (long double)c->x * t->m[0] + t->tx);
    cy = rf((long double)c->y * t->m[3] + (long double)t->m[1] * c->x + t->ty);
    rx = (long double)c->r * t->scale;
    r = rf(rx);
    n = round_int(rx * gCircleDetail);
    if (n >= 40)
        n = 40;
    if (n > 0) {
        step = gTwoPi / (long double)n;
        for (i = 0; i < n; i++) {
            v[i].z = 1.0f;
            v[i].u = 0;
            v[i].v = 0;
            v[i].a = 0.075f;
            v[i].r = 0;
            v[i].g = 1.0f;
            v[i].b = 0;
            v[i].x = rf(x87_cos(angle) * r + cx);
            v[i].y = rf(x87_sin(angle) * r + cy);
            angle += step;
        }
    }
    v[n > 0 ? n : 0] = v[0];
    draw_strip(v, n + 1);
}
HOOK(0x419630, BackgroundFX_DrawCircle);

void BackgroundFX_Create(BackgroundFX *fx);
void BackgroundFX_Run(BackgroundFX *fx);
void BackgroundFX_Draw(BackgroundFX *fx);

void BackgroundFX_Init(void)
{
    gBackgroundFXType = TaskType_New("BackgroundFX", sizeof(BackgroundFX), Task_GetRootType(), 0);
    TaskType_SetHandler2(gBackgroundFXType, 1, BackgroundFX_Run);
    TaskType_SetHandler2(gBackgroundFXType, Screen_GetDrawEvent(), BackgroundFX_Draw);
    TaskType_SetHandler2(gBackgroundFXType, 2, Task_Nothing);
    TaskType_SetHandler(gBackgroundFXType, 0, BackgroundFX_Create);
    TaskType_SetHandler(gBackgroundFXType, Param_GetChangedEvent(), Task_Nothing);
}
HOOK(0x419760, BackgroundFX_Init);

void BackgroundFX_Create(BackgroundFX *fx)
{
    fx->time = 0;
}
HOOK(0x4197F0, BackgroundFX_Create);

void BackgroundFX_Run(BackgroundFX *fx)
{
    fx->time = rf((long double)fx->time + 1.0f);
}
HOOK(0x419800, BackgroundFX_Run);

void BackgroundFX_DrawLayers(BackgroundFX *fx, float time);

void BackgroundFX_Draw(BackgroundFX *fx)
{
    float time = fx->time;

    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    BackgroundFX_DrawLayers(fx, time);
    BackgroundFX_DrawLayers(fx, rf((long double)time + 1.0f));
}
HOOK(0x419820, BackgroundFX_Draw);

void BackgroundFX_DrawShape(BgShape *s, BgTransform *t);

void BackgroundFX_DrawLayers(BackgroundFX *fx, float time)
{
    const float *k = gBgSpeed; /* 0x533584 .. 0x5335A4 */
    BgTransform t;
    float scale;

    scale = rf(x87_cos((long double)time * k[8]) * 0.25f + 1.25f);
    BackgroundFX_SetTransform(&t, rf((long double)time * k[7]), scale, 200.0f, 300.0f);
    BackgroundFX_DrawShape(&gBgShapes[0], &t);
    scale = rf(x87_cos((long double)time * k[6]) * k[5] + k[4]);
    BackgroundFX_SetTransform(&t, rf((long double)time * k[3]), scale, 400.0f, 250.0f);
    BackgroundFX_DrawShape(&gBgShapes[1], &t);
    scale = rf(x87_sin((long double)time * k[2]) + 2.0f);
    BackgroundFX_SetTransform(&t, rf((long double)time * k[0]), scale, 150.0f, 250.0f);
    BackgroundFX_DrawShape(&gBgShapes[2], &t);
}
HOOK(0x419860, BackgroundFX_DrawLayers);

void BackgroundFX_DrawShape(BgShape *s, BgTransform *t)
{
    int i;

    if (s->lines)
        BackgroundFX_DrawLines(s->lines, t);
    for (i = 0; i < s->circleCount; i++)
        BackgroundFX_DrawCircle(&s->circles[i], t);
    if (s->grid)
        BackgroundFX_DrawGrid(s->grid, t);
}
HOOK(0x419950, BackgroundFX_DrawShape);

void BackgroundFX_Shutdown(void)
{
    TaskType_Free(gBackgroundFXType);
    gBackgroundFXType = 0x181;
}
HOOK(0x4199B0, BackgroundFX_Shutdown);

Task *BackgroundFX_New(Task *parent)
{
    return Task_New(parent, gBackgroundFXType, NULL);
}
HOOK(0x4199D0, BackgroundFX_New);
