#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "computer/computer.h"

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B6EC0, void *, Text_New, (void *font, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (void *text));
ORIG(0x4B6F50, void, Text_DrawAlt, (void *text));
ORIG(0x4B7000, void *, Sprite_New, (const char *name));
ORIG(0x4B7030, void, Sprite_Delete, (void *sprite));
ORIG(0x4B70F0, int, Sprite_GetHeight, (void *font, const char *str));
ORIG(0x4B72E0, void *, SortRender_Add, (void (*fn)(void *), int size, unsigned int key));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));

/* the boot text of the map computer: up to five lines, each shown for
   its number of frames, with an optional callback when it ends */
typedef struct ComputerStartup {
    Task task;
    int step;
    int count;
    float barColor[3];
    float textColor[3];
    float timer;
    float frames[5];
    float depth;
    char *text[5];
    unsigned int flags;
    void (*done[5])(void *parent, int step);
    void *vb;
    void *font;
} ComputerStartup;

_Static_assert(offsetof(ComputerStartup, depth) == 0x58, "ComputerStartup");
_Static_assert(offsetof(ComputerStartup, flags) == 0x70, "ComputerStartup");
_Static_assert(sizeof(ComputerStartup) == 0x90, "ComputerStartup");

enum { STARTUP_RUNNING = 1, STARTUP_DONE = 2 };

typedef struct VBDesc {
    int size;
    int caps;
    int fvf;
    int count;
} VBDesc;

typedef struct SortDraw {
    int unk0;
    int id;
    int unk8;
    ComputerStartup *task;
} SortDraw;

extern uint16_t gStartupType;
extern int gStartupDraw;
extern int gStartupMethod;
extern void *gD3D;
extern void (*gRenderFuncs[])(void *);
extern float gScreenW;
extern float gScreenH;
extern void *gDevice;
extern float gViewX;
extern float gViewY;
extern float gD3DUnkBCABEC;
extern float gD3DMinZ;
extern float gD3DViewMinZ;
extern float gD3DViewMaxZ;
extern int *gD3DListA;
extern int *gD3DListB;
extern int *gD3DListC;
extern int *gD3DStateList;

/* method idx of a COM object */
#define COM(obj, idx, type) ((type)(*(void ***)(obj))[idx])

static const float kOne = 1.0f;
static const float kLeft = 0.15f;
static const float kTop = 0.65f;
static const float kTwo = 2.0f;
static const float k127 = 127.0f;
static const float k255 = 255.0f;
static const float kHalf = 0.5f;
static const float kBarH = 0.025f;

/* corners of the four box edges, as fractions of the box size */
static const float kEdgeX0[4] = {0, 0, 1, 1};
static const float kEdgeY1[4] = {0, 1, 1, 0};
static const float kEdgeY0[4] = {0, 0, 1, 1};
static const float kEdgeX1[4] = {1, 0, 0, 1};

typedef struct Vertex {
    float x, y, z, rhw;
    uint32_t color;
} Vertex;

#define DEV_GetTransform(d, a, b) COM(d, 12, long (__stdcall *)(void *, int, float *))(d, a, b)
#define DEV_SetRenderState(d, a, b) COM(d, 20, long (__stdcall *)(void *, int, int))(d, a, b)
#define DEV_DrawPrimitiveVB(d, a, b, c, e, f) \
    COM(d, 31, long (__stdcall *)(void *, int, void *, int, int, int))(d, a, b, c, e, f)
#define DEV_SetTexture(d, a, b) COM(d, 35, long (__stdcall *)(void *, int, void *))(d, a, b)
#define DEV_SetTextureStageState(d, a, b, c) COM(d, 37, long (__stdcall *)(void *, int, int, int))(d, a, b, c)
#define VB_Lock(v, a, b, c) COM(v, 3, long (__stdcall *)(void *, int, void *, unsigned int *))(v, a, b, c)
#define VB_Unlock(v) COM(v, 4, long (__stdcall *)(void *))(v)

void ComputerStartup_Draw(ComputerStartup *t);
void ComputerStartup_Run(ComputerStartup *t);
void ComputerStartup_Delete(ComputerStartup *t);
void ComputerStartup_SortDraw(SortDraw *d);
long ComputerStartup_Render(ComputerStartup *t);

ComputerStartup *ComputerStartup_New(void *parent, int count, const float *frames, char **names, const float *textColor,
                                     const float *barColor, float depth)
{
    ComputerStartup *t = (ComputerStartup *)Task_New(parent, gStartupType, NULL);
    VBDesc desc;
    char m;
    int i;

    t->depth = depth;
    t->step = 0;
    t->count = count;
    t->timer = 0;
    t->flags = 0;
    memcpy(t->textColor, textColor, sizeof(t->textColor));
    memcpy(t->barColor, barColor, sizeof(t->barColor));
    for (i = 0; i < count; i++) {
        memcpy(&t->frames[i], &frames[i], sizeof(float));
        t->text[i] = Resource_Load(names[i], NULL);
        t->done[i] = NULL;
    }
    desc.size = sizeof(desc);
    desc.caps = 0x10000;
    desc.fvf = 0x44;
    desc.count = 50;
    t->vb = NULL;
    COM(gD3D, 5, long (__stdcall *)(void *, VBDesc *, void **, int))(gD3D, &desc, &t->vb, 0);
    m = Mem_GetMode();
    if (!m)
        Mem_SetDefaultMode();
    t->font = NULL;
    t->font = Sprite_New("LOCAL:computer/font3.fnt");
    if (!m)
        Mem_ClearMode();
    return t;
}
HOOK(0x466240, ComputerStartup_New);

void ComputerStartup_Delete(ComputerStartup *t)
{
    Sprite_Delete(t->font);
    COM(t->vb, 2, long (__stdcall *)(void *))(t->vb);
}
HOOK(0x466360, ComputerStartup_Delete);

void ComputerStartup_SetCallback(ComputerStartup *t, int step, void (*fn)(void *parent, int step))
{
    t->done[step] = fn;
}
HOOK(0x466390, ComputerStartup_SetCallback);

void ComputerStartup_Start(ComputerStartup *t)
{
    t->flags = (t->flags & ~STARTUP_DONE) | STARTUP_RUNNING;
    t->step = 0;
    t->timer = 0;
}
HOOK(0x4663B0, ComputerStartup_Start);

void ComputerStartup_Open(void)
{
    gStartupType = TaskType_New("ComputerStartup", sizeof(ComputerStartup), Task_GetRootType(), 0);
    TaskType_SetHandler2(gStartupType, Screen_GetDrawEvent(), ComputerStartup_Draw);
    TaskType_SetHandler2(gStartupType, 1, ComputerStartup_Run);
    TaskType_SetHandler2(gStartupType, 2, ComputerStartup_Delete);
    gStartupMethod = RenderMode_CreateMethod("ComputerStartup");
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gStartupMethod, ComputerStartup_Render);
    gStartupDraw = Primitive_Register("ComputerStartup");
    Primitive_SetHandler(gStartupDraw, ComputerStartup_SortDraw);
}
HOOK(0x4663D0, ComputerStartup_Open);

static uint32_t rgb(const float *c)
{
    uint32_t v = (uint32_t)round_ll(c[0] * k255) | 0xFFFFFF00;

    v = v << 8 | (uint32_t)round_ll(c[1] * k255);
    return v << 8 | (uint32_t)round_ll(c[2] * k255);
}

static void vertex(Vertex *v, float x, float y, float z, float rhw, uint32_t color)
{
    v->x = x;
    v->y = y;
    v->z = z;
    v->rhw = rhw;
    v->color = color;
}

/* a black background, a box and the progress bar of the current line */
long ComputerStartup_Render(ComputerStartup *t)
{
    float m[16];
    unsigned int size;
    Vertex *v;
    float rhw, z, barH, halfW, left, top, y1, y2;
    long double frac, x1, x2;
    uint32_t c;
    int i, x0, y0, w, h, bw, bh;

    DEV_GetTransform(gDevice, 3, m);
    rhw = kOne / (gD3DUnkBCABEC * t->depth);
    z = (kOne - gD3DMinZ * rhw) * m[10] * (gD3DViewMaxZ - gD3DViewMinZ) + gD3DViewMinZ;
    gD3DListA[1] = -1;
    gD3DListB[1] = -1;
    gD3DListC[1] = -1;
    gD3DStateList[1] = -1;
    DEV_SetRenderState(gDevice, 0x2F, 0);
    DEV_SetRenderState(gDevice, 0x1C, 0);
    DEV_SetRenderState(gDevice, 0x1D, 0);
    DEV_SetRenderState(gDevice, 0x16, 1);
    DEV_SetRenderState(gDevice, 0x89, 0);
    DEV_SetRenderState(gDevice, 0x1A, 1);
    DEV_SetRenderState(gDevice, 0x0F, 0);
    DEV_SetRenderState(gDevice, 0x17, 4);
    DEV_SetRenderState(gDevice, 0x07, 1);
    DEV_SetRenderState(gDevice, 0x0E, 0);
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
    if (t->step == 0) {
        VB_Lock(t->vb, 0x2021, &v, &size);
        x0 = (int)round_ll(gViewX);
        y0 = (int)round_ll(gViewY);
        w = (int)round_ll(gScreenW);
        h = (int)round_ll(gScreenH);
        vertex(&v[0], x0, y0, z, rhw, 0xFF000000);
        vertex(&v[1], x0, (long double)h + y0, z, rhw, 0xFF000000);
        vertex(&v[2], (long double)w + x0, y0, z, rhw, 0xFF000000);
        vertex(&v[3], (long double)w + x0, (long double)h + y0, z, rhw, 0xFF000000);
        VB_Unlock(t->vb);
        DEV_DrawPrimitiveVB(gDevice, 5, t->vb, 0, 4, 0);
    }
    VB_Lock(t->vb, 0x2021, &v, &size);
    h = (int)round_ll(gScreenH);
    w = (int)round_ll(gScreenW);
    barH = (long double)h * kBarH;
    halfW = (long double)w * kHalf;
    left = (long double)w * kLeft;
    top = (long double)h * kTop;
    c = rgb(t->textColor);
    bw = (int)round_ll(halfW);
    bh = (int)round_ll(barH);
    for (i = 0; i < 4; i++) {
        vertex(v++, (long double)bw * kEdgeX0[i] + left, (long double)bh * kEdgeY0[i] + top, z, rhw, c);
        vertex(v++, (long double)bw * kEdgeX1[i] + left, (long double)bh * kEdgeY1[i] + top, z, rhw, c);
    }
    VB_Unlock(t->vb);
    DEV_DrawPrimitiveVB(gDevice, 2, t->vb, 0, 8, 0);
    VB_Lock(t->vb, 0x2021, &v, &size);
    c = rgb(t->barColor);
    frac = t->timer / t->frames[t->step];
    x1 = left + kOne;
    y1 = top + kOne;
    y2 = top + barH - kHalf;
    x2 = frac * (halfW - kOne) + left + kHalf;
    vertex(&v[0], x1, y1, z, rhw, c);
    vertex(&v[1], x1, y2, z, rhw, c);
    vertex(&v[2], x2, y1, z, rhw, c);
    vertex(&v[3], x2, y2, z, rhw, c);
    VB_Unlock(t->vb);
    return DEV_DrawPrimitiveVB(gDevice, 5, t->vb, 0, 4, 0);
}
HOOK(0x466480, ComputerStartup_Render);

void ComputerStartup_SortDraw(SortDraw *d)
{
    if ((d->task->flags & STARTUP_RUNNING) && gRenderFuncs[gStartupMethod])
        gRenderFuncs[gStartupMethod](d->task);
}
HOOK(0x466C60, ComputerStartup_SortDraw);

void ComputerStartup_Run(ComputerStartup *t)
{
    long double timer;
    int step;

    if (!(t->flags & STARTUP_RUNNING))
        return;
    timer = t->timer + kOne;
    step = t->step;
    t->timer = timer;
    if (!(timer > t->frames[step]))
        return;
    if (t->done[step])
        t->done[step](t->task.parent, step);
    if (t->step < t->count - 1) {
        t->timer = 0;
        t->step++;
        return;
    }
    t->flags = (t->flags & ~STARTUP_RUNNING) | STARTUP_DONE;
}
HOOK(0x466C90, ComputerStartup_Run);

void ComputerStartup_Draw(ComputerStartup *t)
{
    SortDraw *d;
    void *text;
    char m;
    float x, y0, y;
    int h;
    char *str;

    if (!(t->flags & STARTUP_RUNNING))
        return;
    d = SortRender_Add((void (*)(void *))ComputerStartup_SortDraw, sizeof(SortDraw), *(unsigned int *)&t->depth);
    d->id = gStartupDraw;
    d->task = t;
    m = Mem_GetMode();
    if (!m)
        Mem_SetDefaultMode();
    x = (int)round_ll(gScreenW) * kLeft;
    y0 = (int)round_ll(gScreenH) * kTop;
    str = t->text[t->step];
    h = Sprite_GetHeight(t->font, str);
    y = y0 - h - kTwo;
    text = Text_New(t->font, x, y, str);
    ((float *)text)[5] = t->textColor[0] * k127;
    ((float *)text)[6] = t->textColor[1] * k127;
    ((float *)text)[7] = t->textColor[2] * k127;
    ((unsigned int *)text)[9] |= 0x40000;
    ((float *)text)[3] = t->depth;
    ((int *)text)[8] = -1;
    Text_DrawAlt(text);
    Text_Free(text);
    if (!m)
        Mem_ClearMode();
}
HOOK(0x466CF0, ComputerStartup_Draw);

void ComputerStartup_Close(void)
{
    Primitive_Unregister(gStartupDraw);
    RenderMode_DeleteMethod(gStartupMethod);
    TaskType_Free(gStartupType);
}
HOOK(0x466E10, ComputerStartup_Close);
