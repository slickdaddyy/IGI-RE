#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "task/tasktypes.h"

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x490370, int, Timer_GetMs, (void));
ORIG(0x4977B0, void, Scene_QueueClipWindow, (const void *prim, int queue));
ORIG(0x497680, void, Scene_SetRect, (void *scene, const float *rect));
ORIG(0x497800, void *, Frame_Alloc, (int size));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4A2600, void, BandFilter_FillStripes, (void *filter));
ORIG(0x4A26C0, void, BandFilter_FillSolid, (void *filter));
ORIG(0x4A2720, void, BandFilter_AddNoise, (void *filter));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x4B2450, void, Texture_Download, (void *tex, void *palette, int pitch, int width, int height, int format,
                                        void *data));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B4910, void, Draw_RectZ, (float x, float y, float w, float h, float r, float g, float b, unsigned int flags,
                                  int unk9, float z));
ORIG(0x4B53B0, void, QVertex_DrawQuadEx, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float r, float g, float b, float z, float a, int frame,
                                          unsigned int flags, int unk20));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));
ORIG(0x4B6EC0, void *, Text_New, (void *font, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (void *text));
ORIG(0x4B6F50, void, Text_DrawAlt, (void *text));
ORIG(0x4B72E0, void *, SortRender_Add, (void (*fn)(void *), int size, unsigned int key));

/* static noise lines over the map computer screen */
typedef struct LineFilter {
    Task task;
    int unk20, unk24, unk28;
    float z;
    float level;
    int sorted; /* draw through SortRender, else at once */
} LineFilter;

/* black bars above and below a clip window */
typedef struct WideScreenFilter {
    Task task;
    float color[3];
    float unk2C;
    float w, h, x, y;
    float barH;
} WideScreenFilter;

typedef struct FilterTexture {
    int device;
    int unk4[7];
    void *surface;
} FilterTexture;

/* security camera feed: rolling bands, noise and "CAMERA n" */
typedef struct BandFilter {
    Task task;
    float z;
    float level; /* noise, falls back to 0 */
    float maxLevel; /* 0 = no signal, a flat rectangle */
    FilterTexture tex;
    int counter; /* 0..period, bands shift half a step past 45 */
    int period;
    int timer;
    int interval;
    int camera;
    float color[3];
    void *font;
} BandFilter;

/* camcorder view: scan lines, corner marks, battery and timecode */
typedef struct HandiCamFilter {
    Task task;
    float color[3];
    float z;
    void *font;
    char battery[0x18]; /* a Sprite */
    float blink;
    int lastMs;
} HandiCamFilter;

_Static_assert(sizeof(LineFilter) == 0x38, "LineFilter");
_Static_assert(sizeof(WideScreenFilter) == 0x44, "WideScreenFilter");
_Static_assert(sizeof(BandFilter) == 0x74, "BandFilter");
_Static_assert(sizeof(HandiCamFilter) == 0x54, "HandiCamFilter");

/* a render queue node or SortRender item */
typedef struct FilterPrim {
    void *next;
    int id;
    int unk8;
    void *task;
} FilterPrim;

typedef struct ClipPrim {
    void *next;
    int type;
    int unk8;
    float x, y, w, h;
    int unk1C, unk20;
} ClipPrim;

typedef struct Text {
    void *grid;
    float x, y, z;
    float alpha;
    float color[3];
    int unk20;
    unsigned int flags;
} Text;

typedef struct Vertex {
    float x, y, z, rhw;
    uint32_t color;
    float u, v;
} Vertex;

typedef struct DynVB {
    void *vb;
    int size;
    int used;
} DynVB;

extern uint16_t gBandFilterType;
extern uint16_t gHandiCamFilterType;
extern uint16_t gLineFilterType;
extern uint16_t gWideScreenFilterType;
extern void *gDevice;
extern char gFilterRandom[];
extern int gHandiCamDrawMethod;
extern int gBandDrawMethod;
extern int gHandiCamFilterPrim;
extern int gBandFilterPrim;
extern char gBandPixels[];
extern int gLineDrawMethod;
extern int gLineFilterPrim;
extern int gWideScreenPrim;
extern int gBatteryX;
extern int gBatteryY;
extern int gTimecodeX;
extern int gTimecodeY;
extern float gTimecode;
extern float gBandTextX;
extern float gBandTextY;
extern float gBandTextDX;
extern float gBandTextDY;
extern void (*gRenderFuncs[])(void *);
extern char gScene[];
extern float gViewX;
extern float gViewY;
extern float gScreenW;
extern float gScreenH;
extern float gD3DUnkBCABEC;
extern DynVB gVBTL;
extern int *gD3DListB;
extern float gD3DViewMaxZ;
extern float gD3DMinZ;
extern int *gD3DListC;
extern float gD3DViewMinZ;
extern float gNearPlane;
extern int *gD3DStateList;
extern int *gD3DListA;

/* method idx of a COM object */
#define COM(obj, idx, type) ((type)(*(void ***)(obj))[idx])
#define DEV_GetTransform(d, a, b) COM(d, 12, long (__stdcall *)(void *, int, float *))(d, a, b)
#define DEV_SetRenderState(d, a, b) COM(d, 20, long (__stdcall *)(void *, int, int))(d, a, b)
#define DEV_DrawPrimitiveVB(d, a, b, c, e, f) \
    COM(d, 31, long (__stdcall *)(void *, int, void *, int, int, int))(d, a, b, c, e, f)
#define DEV_SetTexture(d, a, b) COM(d, 35, long (__stdcall *)(void *, int, void *))(d, a, b)
#define DEV_SetTextureStageState(d, a, b, c) COM(d, 37, long (__stdcall *)(void *, int, int, int))(d, a, b, c)
#define VB_Lock(v, a, b, c) COM(v, 3, long (__stdcall *)(void *, int, void *, unsigned int *))(v, a, b, c)
#define VB_Unlock(v) COM(v, 4, long (__stdcall *)(void *))(v)

static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kFour = 4.0f;
static const float kMinusFour = -4.0f;
static const float k128 = 128.0f;
static const float k255 = 255.0f;
static const float k50 = 50.0f;
static const float kHalf = 0.5f;
static const float kTexV = 0.0625f;
static const float kBandH = 0.05f;
static const float kBandRandom = 0.7f;
static const float kFramesPerMs = 0.024f;
static const float kBlinkPerMs = 0.001f;
static const float kBlinkMax = 1.5f;
static const float kBlinkOn = 0.75f;
static const float kBatteryW = 0.07f;
static const float kBatteryX = 0.1f;
static const float kBatteryY = 0.2f;
static const float kTimecodeX = 0.9f;
static const float kTimecodeY = 0.8f;
static const float kTextColor = 100.0f;

static const float kCornerX[4] = {0.1f, 0.9f, 0.1f, 0.9f};
static const float kCornerY[4] = {0.2f, 0.2f, 0.8f, 0.8f};
static const float kCornerDX[4] = {1, -1, 1, -1};
static const float kCornerDY[4] = {1, 1, -1, -1};

static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

static inline unsigned int fbits(float f)
{
    union {
        float f;
        unsigned int u;
    } x = {f};

    return x.u;
}

/* true for 0 and NaN, like the original compare */
static inline int is_zero(float f)
{
    return !(f < kZero || f > kZero);
}

void LineFilter_DrawPrim(FilterPrim *p)
{
    void (*fn)(void *) = gRenderFuncs[gLineDrawMethod];

    if (fn)
        fn(p->task);
}
HOOK(0x4A3530, LineFilter_DrawPrim);

void LineFilter_Draw(LineFilter *t)
{
    FilterPrim prim, *p;

    if (t->sorted) {
        p = SortRender_Add((void (*)(void *))LineFilter_DrawPrim, sizeof(FilterPrim), fbits(t->z));
        p->task = t;
        p->id = gLineFilterPrim;
        return;
    }
    prim.id = gLineFilterPrim;
    prim.task = t;
    LineFilter_DrawPrim(&prim);
}
HOOK(0x4A3550, LineFilter_Draw);

void WideScreenFilter_DrawPrim(FilterPrim *p)
{
    WideScreenFilter *t = p->task;
    float rect[4];

    rect[0] = t->x;
    rect[1] = t->y;
    rect[2] = t->w;
    rect[3] = t->h;
    Scene_SetRect(gScene, rect);
    Draw_RectZ(t->x, t->y + t->h - t->barH, t->w, t->barH, t->color[0], t->color[1], t->color[2], 0, -1, kOne);
    Draw_RectZ(t->x, t->y, t->w, t->barH, t->color[0], t->color[1], t->color[2], 0, -1, kOne);
}
HOOK(0x4A35B0, WideScreenFilter_DrawPrim);

void WideScreenFilter_Draw(WideScreenFilter *t)
{
    FilterPrim *p = Frame_Alloc(sizeof(FilterPrim));
    ClipPrim clip;

    p->id = gWideScreenPrim;
    p->task = t;
    RenderQueue_Add(0, p);
    clip.x = t->x;
    clip.y = t->y;
    clip.w = t->w;
    clip.h = t->h;
    Scene_QueueClipWindow(&clip, 0);
}
HOOK(0x4A3650, WideScreenFilter_Draw);

void BandFilter_DrawPrim(FilterPrim *p)
{
    void (*fn)(void *) = gRenderFuncs[gBandDrawMethod];

    if (fn)
        fn(p->task);
}
HOOK(0x4A36B0, BandFilter_DrawPrim);

static void draw_label(void *font, float x, float y, float z, const char *str)
{
    Text *text = Text_New(font, x, y, str);

    text->color[0] = kTextColor;
    text->color[1] = kTextColor;
    text->color[2] = kTextColor;
    text->flags |= 0x40000;
    text->z = z;
    Text_DrawAlt(text);
    Text_Free(text);
}

void BandFilter_Draw(BandFilter *t)
{
    FilterPrim *p = SortRender_Add((void (*)(void *))BandFilter_DrawPrim, sizeof(FilterPrim), fbits(t->z));
    char buf[52];
    char mode;
    float z;

    p->task = t;
    p->id = gBandFilterPrim;
    if (!is_zero(t->maxLevel)) {
        mode = Mem_GetMode();
        if (!mode)
            Mem_SetDefaultMode();
        crt_sprintf(buf, "CAMERA %d", t->camera);
        draw_label(t->font, gScreenW * gBandTextX + gBandTextDX + gViewX, gScreenH * gBandTextY + gBandTextDY + gViewY,
                   t->z, buf);
        if (!mode)
            Mem_ClearMode();
    }
    if (is_zero(t->maxLevel)) {
        z = t->z > gNearPlane ? t->z : gNearPlane;
        Draw_RectZ((int)round_ll(gViewX), (int)round_ll(gViewY), (int)round_ll(gScreenW), (int)round_ll(gScreenH),
                   t->color[0] * k128, t->color[1] * k128, t->color[2] * k128, 0x80000000, 0, z);
    }
}
HOOK(0x4A36D0, BandFilter_Draw);

void BandFilter_Run(BandFilter *t)
{
    long double fall = -t->level;
    float step = kMinusFour / (long double)t->period;

    if (!(fall > step))
        fall = step;
    t->level = fall + t->level;
    if (t->counter < t->period)
        t->counter++;
    else
        t->counter = 0;
    if (t->timer < t->interval) {
        t->timer++;
        return;
    }
    t->timer = (int)round_ll(Random_Float(gFilterRandom) * t->interval * kBandRandom);
    t->level = Random_Float(gFilterRandom) * t->maxLevel;
}
HOOK(0x4A38A0, BandFilter_Run);

/* NULL when the lock failed */
static Vertex *vb_lock(int n, int *start)
{
    Vertex *p = NULL;
    long hr;

    if (gVBTL.used + n > gVBTL.size) {
        hr = VB_Lock(gVBTL.vb, 0x2020, &p, NULL);
        *start = 0;
        gVBTL.used = n;
    } else {
        hr = VB_Lock(gVBTL.vb, 0x1020, &p, NULL);
        *start = gVBTL.used;
        p += gVBTL.used;
        gVBTL.used += n;
    }
    return hr ? NULL : p;
}

/* z and 1/w of the vertices for a screen overlay at depth z */
static void overlay_depth(float z, float *vz, float *rhw)
{
    float m[16];

    DEV_GetTransform(gDevice, 3, m);
    *rhw = kOne / (gD3DUnkBCABEC * z);
    *vz = (kOne - gD3DMinZ * *rhw) * m[10] * (gD3DViewMaxZ - gD3DViewMinZ) + gD3DViewMinZ;
    gD3DListA[1] = -1;
    gD3DListB[1] = -1;
    gD3DListC[1] = -1;
    gD3DStateList[1] = -1;
}

static void overlay_states(void)
{
    DEV_SetRenderState(gDevice, 0x2F, 0);
    DEV_SetRenderState(gDevice, 0x1C, 0);
    DEV_SetRenderState(gDevice, 0x1D, 0);
    DEV_SetRenderState(gDevice, 0x16, 1);
    DEV_SetRenderState(gDevice, 0x89, 0);
    DEV_SetRenderState(gDevice, 0x1A, 1);
    DEV_SetRenderState(gDevice, 0x0F, 0);
    DEV_SetRenderState(gDevice, 0x17, 4);
    DEV_SetRenderState(gDevice, 0x07, 0);
    DEV_SetRenderState(gDevice, 0x0E, 0);
}

static void vertex(Vertex *v, float x, float y, float z, float rhw)
{
    v->x = x;
    v->y = y;
    v->z = z;
    v->rhw = rhw;
}

static void uv(Vertex *v, float u, float tv)
{
    v->u = u;
    v->v = tv;
}

/* one textured quad across the screen per pair of band edges */
static Vertex *band_quads(Vertex *p, const float *ys, int pairs, float vz, float rhw, float h)
{
    int left = (int)round_ll(gViewX);
    float right = (long double)left + gScreenW;
    float u = gScreenW * kTexV;
    float tv = h * kTexV;

    for (; pairs; pairs--, ys += 2, p += 6) {
        vertex(&p[0], left, ys[0], vz, rhw);
        uv(&p[0], 0, 0);
        vertex(&p[1], right, ys[0], vz, rhw);
        uv(&p[1], u, 0);
        vertex(&p[2], right, ys[1], vz, rhw);
        uv(&p[2], u, tv);
        vertex(&p[3], left, ys[1], vz, rhw);
        uv(&p[3], 0, tv);
        vertex(&p[4], right, ys[1], vz, rhw);
        uv(&p[4], u, tv);
        vertex(&p[5], left, ys[0], vz, rhw);
        uv(&p[5], 0, 0);
    }
    return p;
}

static void band_texture(BandFilter *t, int second)
{
    char mode = Mem_GetMode();

    if (second)
        BandFilter_FillSolid(t);
    else
        BandFilter_FillStripes(t);
    BandFilter_AddNoise(t);
    if (!mode)
        Mem_SetDefaultMode();
    Texture_Download(&t->tex, NULL, 0x40, 0x10, 0x10, 3, gBandPixels);
    if (!mode)
        Mem_ClearMode();
    DEV_SetTexture(gDevice, 0, t->tex.surface);
}

/* bands of two noise textures, alternating down the screen */
void BandFilter_Render(BandFilter *t)
{
    float ys[100];
    float z, vz, rhw, h, top, bottom;
    long double y;
    Vertex *p;
    int n, start;

    z = t->z > gNearPlane ? t->z : gNearPlane;
    if (is_zero(t->maxLevel))
        return;
    overlay_depth(z, &vz, &rhw);
    overlay_states();
    DEV_SetTextureStageState(gDevice, 0, 0x0C, 1);
    DEV_SetTextureStageState(gDevice, 0, 0x12, 1);
    DEV_SetTextureStageState(gDevice, 0, 0x0B, 0);
    DEV_SetTextureStageState(gDevice, 0, 4, 2);
    DEV_SetTextureStageState(gDevice, 0, 5, 2);
    DEV_SetTextureStageState(gDevice, 0, 6, 0);
    DEV_SetTextureStageState(gDevice, 0, 1, 2);
    DEV_SetTextureStageState(gDevice, 0, 2, 2);
    DEV_SetTextureStageState(gDevice, 0, 3, 0);
    DEV_SetTextureStageState(gDevice, 1, 1, 1);
    DEV_SetRenderState(gDevice, 0x1B, 1);
    DEV_SetRenderState(gDevice, 0x13, 9);
    DEV_SetRenderState(gDevice, 0x14, 1);

    h = gScreenH * kBandH;
    y = (int)round_ll(gViewY);
    top = y;
    ys[0] = y;
    if (t->counter > 45)
        y += h * kHalf;
    ys[1] = y;
    y += h;
    bottom = (long double)(int)round_ll(gScreenH) + top;
    for (n = 2; y < bottom; n++) {
        ys[n] = y;
        y += h;
    }
    ys[n] = bottom;

    band_texture(t, 0);
    p = vb_lock(100, &start);
    if (!p)
        return;
    band_quads(p, &ys[1], n >> 1, vz, rhw, h);
    VB_Unlock(gVBTL.vb);
    DEV_DrawPrimitiveVB(gDevice, 4, gVBTL.vb, start, (n >> 1) * 6, 0);

    band_texture(t, 1);
    p = vb_lock(100, &start);
    if (!p)
        return;
    band_quads(p, &ys[0], (n + 1) >> 1, vz, rhw, h);
    VB_Unlock(gVBTL.vb);
    DEV_DrawPrimitiveVB(gDevice, 4, gVBTL.vb, start, ((n + 1) >> 1) * 6, 0);
}
HOOK(0x4A3940, BandFilter_Render);

/* scan lines every 4 pixels and white corner marks */
void HandiCamFilter_Render(HandiCamFilter *t)
{
    float z, vz, rhw, y;
    long double next, limit, a, b, jx, jy;
    uint32_t color;
    Vertex *p;
    int n, start, i, j, x0, x1, iy, cx, cy;

    z = t->z > gNearPlane ? t->z : gNearPlane;
    overlay_depth(z, &vz, &rhw);
    overlay_states();
    DEV_SetTextureStageState(gDevice, 0, 4, 3);
    DEV_SetTextureStageState(gDevice, 0, 5, 2);
    DEV_SetTextureStageState(gDevice, 0, 6, 0);
    DEV_SetTextureStageState(gDevice, 0, 1, 3);
    DEV_SetTextureStageState(gDevice, 0, 2, 2);
    DEV_SetTextureStageState(gDevice, 0, 3, 0);
    DEV_SetTextureStageState(gDevice, 1, 1, 1);
    DEV_SetTexture(gDevice, 0, NULL);
    DEV_SetRenderState(gDevice, 0x1B, 1);
    DEV_SetRenderState(gDevice, 0x13, 5);
    DEV_SetRenderState(gDevice, 0x14, 6);

    p = vb_lock(200, &start);
    if (!p)
        return;
    n = 0;
    y = gViewY;
    for (limit = (long double)y + kFour; (long double)gScreenH + gViewY >= limit; limit = next + kFour) {
        if (n >= 196) {
            VB_Unlock(gVBTL.vb);
            DEV_DrawPrimitiveVB(gDevice, 2, gVBTL.vb, start, n, 0);
            p = vb_lock(200, &start);
            if (!p)
                return;
            y = y - kFour;
            n = 0;
        } else {
            color = (uint32_t)round_ll(t->color[0] * k255) | 0x4C00;
            color = color << 8 | (uint32_t)round_ll(t->color[1] * k255);
            color = color << 8 | (uint32_t)round_ll(t->color[2] * k255);
            iy = (int)round_ll(y);
            x0 = (int)round_ll(gViewX);
            x1 = (int)round_ll((long double)gScreenW + gViewX);
            vertex(&p[0], x0, iy, vz, rhw);
            vertex(&p[1], x1, iy, vz, rhw);
            vertex(&p[2], x0, (long double)iy + kOne, vz, rhw);
            vertex(&p[3], x1, (long double)iy + kOne, vz, rhw);
            for (i = 0; i < 4; i++)
                p[i].color = color;
            p += 4;
            n += 4;
        }
        next = (long double)y + kFour;
        y = next;
    }
    VB_Unlock(gVBTL.vb);
    if (n > 0)
        DEV_DrawPrimitiveVB(gDevice, 2, gVBTL.vb, start, n, 0);

    p = vb_lock(200, &start);
    if (!p)
        return;
    for (i = 0; i < 4; i++) {
        a = kCornerDX[i] * k50;
        b = kCornerDY[i] * k50;
        cx = (int)round_ll(gScreenW * kCornerX[i] + gViewX);
        cy = (int)round_ll(gScreenH * kCornerY[i] + gViewY);
        for (j = 0; j < 2; j++, p += 4) {
            jx = (long double)j * kCornerDX[i];
            jy = (long double)j * kCornerDY[i];
            vertex(&p[0], cx, cy + jy, vz, rhw);
            vertex(&p[1], cx + a, cy + jy, vz, rhw);
            vertex(&p[2], cx + jx, cy, vz, rhw);
            vertex(&p[3], cx + jx, cy + b, vz, rhw);
            p[0].color = p[1].color = p[2].color = p[3].color = 0xFFFFFFFF;
        }
    }
    VB_Unlock(gVBTL.vb);
    DEV_DrawPrimitiveVB(gDevice, 2, gVBTL.vb, start, 32, 0);
}
HOOK(0x4A4300, HandiCamFilter_Render);

void HandiCamFilter_DrawPrim(FilterPrim *p)
{
    void (*fn)(void *) = gRenderFuncs[gHandiCamDrawMethod];

    if (fn)
        fn(p->task);
}
HOOK(0x4A4C90, HandiCamFilter_DrawPrim);

void HandiCamFilter_Run(HandiCamFilter *t)
{
    int now = Timer_GetMs();
    long double blink;

    gTimecode = (long double)(now - t->lastMs) * kFramesPerMs + gTimecode;
    blink = (long double)(now - t->lastMs) * kBlinkPerMs + t->blink;
    t->blink = blink;
    if (blink > kBlinkMax)
        t->blink = 0;
    t->lastMs = now;
}
HOOK(0x4A4CB0, HandiCamFilter_Run);

void HandiCamFilter_Draw(HandiCamFilter *t)
{
    FilterPrim *p = SortRender_Add((void (*)(void *))HandiCamFilter_DrawPrim, sizeof(FilterPrim), fbits(t->z));
    char buf[64];
    char mode;
    int frames, sec, min;
    float alpha, w, sh, left, right, top, bottom;
    long double y, h;

    p->id = gHandiCamFilterPrim;
    p->task = t;
    mode = Mem_GetMode();
    if (!mode)
        Mem_SetDefaultMode();
    /* 24 frames a second; the hours field is minutes / 60, always 0 */
    frames = (int)round_ll(gTimecode);
    sec = frames / 24;
    min = sec / 60 % 60;
    crt_sprintf(buf, "%2.2d : %2.2d : %2.2d : %2.2d", min / 60, min, sec % 60, frames % 24);
    draw_label(t->font, gScreenW * kTimecodeX + gViewX - (long double)gTimecodeX,
               gScreenH * kTimecodeY + gViewY - (long double)gTimecodeY, t->z, buf);
    if (!mode)
        Mem_ClearMode();

    alpha = t->blink >= kBlinkOn ? kOne : kZero;
    w = gScreenW * kBatteryW;
    sh = Sprite_Height(t->battery);
    h = sh / (long double)Sprite_Width(t->battery) * w;
    left = gScreenW * kBatteryX + (long double)gBatteryX + gViewX;
    y = gScreenH * kBatteryY + (long double)gBatteryY + gViewY;
    top = y;
    bottom = y + h;
    right = left + w;
    QVertex_DrawQuadEx(t->battery, left, bottom, right, bottom, left, top, right, top, k128, k128, k128, t->z, alpha, 0,
                       0x10C0000, 0);
}
HOOK(0x4A4D10, HandiCamFilter_Draw);

void CameraFilter_Close(void)
{
    Primitive_Unregister(gHandiCamFilterPrim);
    Primitive_Unregister(gBandFilterPrim);
    Primitive_Unregister(gWideScreenPrim);
    Primitive_Unregister(gLineFilterPrim);
    RenderMode_DeleteMethod(gHandiCamDrawMethod);
    RenderMode_DeleteMethod(gBandDrawMethod);
    RenderMode_DeleteMethod(gLineDrawMethod);
    TaskType_Free(gHandiCamFilterType);
    TaskType_Free(gBandFilterType);
    TaskType_Free(gWideScreenFilterType);
    TaskType_Free(gLineFilterType);
}
HOOK(0x4A4F60, CameraFilter_Close);

void LineFilter_SetLevel(LineFilter *t, float level)
{
    t->level = level;
}
HOOK(0x4A4FF0, LineFilter_SetLevel);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5CA114) void *gDevice;
ALIAS(gD3DDevice, gDevice, 0);
BSS(935BFC) int gHandiCamDrawMethod;
BSS(935BFC) int gBandDrawMethod;
BSS(935C08) int gHandiCamFilterPrim;
BSS(935C08) int gBandFilterPrim;
BSS(936010) int gLineDrawMethod;
BSS(936010) int gLineFilterPrim;
BSS(936010) int gWideScreenPrim;
BSS(936030) int gTimecodeX;
BSS(936030) int gTimecodeY;
BSS(936030) float gTimecode;
BSS(936030) float gBandTextX;
BSS(936030) float gBandTextY;
BSS(936030) float gBandTextDX;
BSS(936030) float gBandTextDY;
ALIAS(gBandFontA, gBandTextX, 0);
ALIAS(gBandFontB, gBandTextY, 0);
ALIAS(gBandFontC, gBandTextDX, 0);
ALIAS(gBandFontD, gBandTextDY, 0);
ALIAS(gHandiCamFontX, gTimecodeX, 0);
ALIAS(gHandiCamFontY, gTimecodeY, 0);
BSS(BCAD00) DynVB gVBTL;
ALIAS(gFilterVB, gVBTL, 0);
ALIAS(gFilterVBSize, gVBTL, 4);
ALIAS(gFilterVBUsed, gVBTL, 8);
ALIAS(gVBTLSize, gVBTL, 4);
ALIAS(gVBTLUsed, gVBTL, 8);
BSS(BCADC0) float gNearPlane;
