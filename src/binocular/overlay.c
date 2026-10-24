#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "binocular/binocular.h"
#include "qvertex/qvertextypes.h"
#include "coll/colltypes.h"
#include "win/wintypes.h"
#include "res/restypes.h"

FN_EXTERN(BinocularHolder_Changed);
FN_EXTERN(BinocularHolder_GetParams);
FN_EXTERN(BinocularOverlay_DrawHandler);
FN_EXTERN(BinocularOverlay_UseHandler);

/* what the collision line tests skip */
typedef struct CollFilter {
    int unk0;
    void *a;
    void *b;
    void *c;
    float unk10;
} CollFilter;

/* a bar that jumps to random levels and falls back */
typedef struct VUMeter {
    float level;
    float unk4;
    float decay;
    int timer;
    int period;
} VUMeter;

/* a human marked in the binocular view (0x2C bytes) */
typedef struct Target {
    char *human;
    int unk4;           /* the human's 0xCF5 flag */
    int unk8;           /* the human's 0xFC field is not 1 */
    int occluded;
    int seen;           /* seen this frame */
    float radius;
    float depth;
    int time;           /* last seen */
    int firstTime;
    float sx;
    float sy;
} Target;

/* "BinocularOverlay" (0x2E00 bytes), the view through the binoculars */
typedef struct BinocularOverlay {
    Task task;
    Text *text;             /* 0x20, the range */
    Text *letters[4];       /* 0x24, W N E S */
    Text *unk34;
    Matrix matrix;          /* 0x38 */
    Vec3d pos;              /* 0x60 */
    float fov;              /* 0x78 */
    float dir[3];           /* 0x7C */
    char ctx[0xA8];         /* 0x88, the view used for drawing */
    int visible;            /* 0x130, humans inside the view */
    int shown;              /* 0x134, humans marked */
    char rng[0xC];          /* 0x138 */
    int unk144;
    int unk148;
    char grid[0x2A04];      /* 0x14C */
    int unk2B50;
    VUMeter meters[12];     /* 0x2B54 */
    Target targets[10];     /* 0x2C44 */
    int unk2DFC;
} BinocularOverlay;

/* "BinocularOverlayHolder" (0x78 bytes): shows an overlay while "active" is true */
typedef struct BinocularHolder {
    Task task;
    Task **overlay;         /* 0x20 */
    VarString active;       /* 0x24 */
    char isActive;          /* 0x74 */
    char pad75[3];
} BinocularHolder;

_Static_assert(sizeof(VUMeter) == 0x14, "VUMeter");
_Static_assert(sizeof(Target) == 0x2C, "Target");
_Static_assert(offsetof(BinocularOverlay, ctx) == 0x88, "BinocularOverlay");
_Static_assert(offsetof(BinocularOverlay, grid) == 0x14C, "BinocularOverlay");
_Static_assert(offsetof(BinocularOverlay, meters) == 0x2B54, "BinocularOverlay");
_Static_assert(offsetof(BinocularOverlay, targets) == 0x2C44, "BinocularOverlay");
_Static_assert(sizeof(BinocularOverlay) == 0x2E00, "BinocularOverlay");
_Static_assert(sizeof(BinocularHolder) == 0x78, "BinocularHolder");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4169D0, char *, Game_AddSprite, (const char *name));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x460C60, int *, Human_GetList, (void));
ORIG(0x465800, void, HumanView_GetEye, (Vec3d *eye, void *view));
ORIG(0x46B4D0, char *, ComputerRes_Get, (void));
ORIG(0x472600, Task *, BinocularOverlay_New, (void *parent));
ORIG(0x485320, void *, GameScreen_GetView, (void *task));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x48F2D0, int, AppContext_IsUSA, (void));
ORIG(0x490370, int, Timer_GetMs, (void));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x497D70, void, TransContext_Create, (void *ctx, Matrix *m, Vec3d *pos, float fov));
ORIG(0x497E70, void, TransContext_Select, (void *ctx));
ORIG(0x497ED0, void, TransContext_Invalidate, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B2D00, void, Vec3_ToAngles, (const float *v, float *pitch, float *yaw));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B4A00, void, Draw_RectA, (float x, float y, float w, float h, float r, float g, float b, float a, int flags,
                                  int unk));
ORIG(0x4B51F0, void, QVertex_DrawQuad, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                        float x3, float y3, float r, float g, float b, int frame, int flags, int unk));
ORIG(0x4B5730, void, QVertex_DrawLine, (float x0, float y0, float z0, float x1, float y1, float z1, float r0,
                                        float g0, float b0, float r1, float g1, float b1, float a, int flags,
                                        int unk));
ORIG(0x4B5860, void, QVertex_DrawPolyline, (const float *p, int n, float ox, float oy, float z, float r, float g,
                                            float b, float a, float angle, float sx, float sy, unsigned int flags,
                                            int unk));
ORIG(0x4B5F00, char *, Resource_Load, (const char *name, int flags));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6EC0, Text *, Text_New, (void *font, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B7DF0, int, Text_GetWidth, (Text *t));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *results, Vec3d *starts, Vec3d *ends, int count, char event,
                                  CollFilter *filter, unsigned int flags));
ORIG(0x4CE410, char, Coll_LineOfSight, (void *world, const Vec3d *from, const Vec3d *to, int event,
                                        CollFilter *filter, int a, int b, float *visibility));
ORIG(0x4D03D0, int, Mesh3D_Exclude, (float *v, float radius, void *out));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

void View_Project(View *v, float *out, float *depth, const Vec3d *p);
void View_ScaleSize(View *v, float *out, const float *size, float depth);

#define BINOCULAROVERLAY_DRAWHANDLER (FN(0x472400, BinocularOverlay_DrawHandler))
#define BINOCULAROVERLAY_USEHANDLER (FN(0x472440, BinocularOverlay_UseHandler))
#define BINOCULARHOLDER_GETPARAMS (FN(0x4723D0, BinocularHolder_GetParams))
#define BINOCULARHOLDER_CHANGED (FN(0x4723E0, BinocularHolder_Changed))

extern uint16_t gBinocularOverlayType;
extern uint16_t gBinocularHolderType;
extern char *gBinocularSprite;
extern void *gBinocularHolderParams;
extern const int gGridTemplate[];
extern const float gMarkerShape[];
extern const float gTargetShape[];
extern void (*gRenderFuncs[])(QVertexStrip *);
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern const char gScriptEmptyString[];

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(kHalf, 0x3F000000);
F32(kTwoPi, 0x40C90FDB);
F32(kHalfPi, 0x3FC90FDB);
F32(kTen, 0x41200000);
F32(kTwo, 0x40000000);
F32(kFour, 0x40800000);
F32(kSixtyFour, 0x42800000);
F32(kGrowRate, 0x3B83126F);     /* 0.004 per ms */
F32(kTextX, 0x3EC1999A);        /* 0.378125 */
F32(kTextY, 0x3F0CCCCD);        /* 0.55 */
F32(kEighth, 0x3E000000);
F32(kMeterY, 0x3F433333);       /* 0.7625 */
F32(kMeterStep, 0x3E400000);    /* 0.1875 */
F32(kTwelfth, 0x3DAAAAAB);
F32(kMeterGap, 0x3C19999A);     /* 0.009375 */
F32(kMarkerX, 0x3DC00000);      /* 0.09375 */
F32(kMarkerW, 0x3C800000);      /* 1/64 */
F32(kMarkerH, 0x3CAAAAAB);      /* 1/48 */
F32(kMarkerW2, 0xBC800000);
F32(kMarkerH2, 0xBCAAAAAB);
F32(kCompassX, 0x3F200000);     /* 0.625 */
F32(kCompassY, 0x3F4AAAAB);     /* 0.7917 */
F32(kCompassW, 0x3E900000);     /* 0.28125 */
F32(kCompassPad, 0x41D00000);   /* 26 */
F32(kInvTwoPi, 0x3E22F983);
F32(kTickLong, 0x41400000);     /* 12 */
F32(kTickMid, 0x41000000);      /* 8 */
static const double kMetric = 1000.0;
static const double kImperial = 0x1.c933333333334p+9; /* 914.4 rounded up */
static const double kWorldUnit = 4096.0;
static const double kInvWorldUnit = 1.0 / 4096.0;
static const double kFeet = 0x1.a3f28fca37f21p+1; /* 3.280839895 */

void BinocularOverlay_Create(BinocularOverlay *ov);
void BinocularOverlay_Delete(BinocularOverlay *ov);
void BinocularOverlay_StatusDisplayHandler(BinocularOverlay *ov);
void BinocularHolder_Create(BinocularHolder *h);
void BinocularHolder_Run(BinocularHolder *h);
void BinocularHolder_Delete(BinocularHolder *h);

void BinocularOverlay_Open(void)
{
    BinocularHolder tmp;

    gBinocularOverlayType = TaskType_New("BinocularOverlay", 0x2E00, Task_GetRootType(), 0);
    TaskType_SetHandler(gBinocularOverlayType, 0, BinocularOverlay_Create);
    TaskType_SetHandler2(gBinocularOverlayType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gBinocularOverlayType, 2, BinocularOverlay_Delete);
    TaskType_SetHandler(gBinocularOverlayType, StatusScreen_GetEvent(), BinocularOverlay_StatusDisplayHandler);
    TaskType_SetHandler2(gBinocularOverlayType, Screen_GetDrawEvent(), BINOCULAROVERLAY_DRAWHANDLER);
    TaskType_SetHandler(gBinocularOverlayType, Human_GetEvent0(), BINOCULAROVERLAY_USEHANDLER);
    gBinocularSprite = Game_AddSprite("binoculars.spr");
    gBinocularHolderType = TaskType_New("BinocularOverlayHolder", 0x78, Task_GetRootType(), 0);
    TaskType_SetHandler(gBinocularHolderType, 0, BinocularHolder_Create);
    TaskType_SetHandler2(gBinocularHolderType, 1, BinocularHolder_Run);
    TaskType_SetHandler2(gBinocularHolderType, 2, BinocularHolder_Delete);
    TaskType_SetHandler(gBinocularHolderType, Param_GetTableEvent(), BINOCULARHOLDER_GETPARAMS);
    TaskType_SetHandler(gBinocularHolderType, Param_GetChangedEvent(), BINOCULARHOLDER_CHANGED);
    gBinocularHolderParams = ParamTable_New(1);
    ParamTable_Add(gBinocularHolderParams, &tmp, &tmp.active, Param_VarString(), 7, NULL);
}
HOOK(0x470FA0, BinocularOverlay_Open);

void VUMeter_Init(VUMeter *m, int period, float decay);
void BinocularOverlay_InitGrid(char *grid, int n);

void BinocularOverlay_Create(BinocularOverlay *ov)
{
    static const char kLetters[] = "WNES";
    char buf[2] = {0, 0};
    int i;

    for (i = 0; i < 12; i++)
        VUMeter_Init(&ov->meters[i], 30, 0.02f);
    ov->text = Text_New(*(void **)(ComputerRes_Get() + 0x10), 0, 0, gScriptEmptyString);
    ov->unk34 = Text_New(*(void **)(ComputerRes_Get() + 0x10), 0, 0, gScriptEmptyString);
    for (i = 0; i < 4; i++) {
        buf[0] = kLetters[i];
        ov->letters[i] = Text_New(*(void **)(ComputerRes_Get() + 0x10), 0, 0, buf);
    }
    ov->pos.x = 0;
    ov->pos.y = 0;
    ov->pos.z = 0;
    ov->dir[0] = 1.0f;
    ov->dir[1] = 0;
    ov->dir[2] = 0;
    Random_Init(ov->rng);
    BinocularOverlay_InitGrid(ov->grid, 0x18);
    ov->unk148 = 0;
    ov->unk144 = 0;
    for (i = 0; i < 10; i++)
        ov->targets[i] = (Target){0};
}
HOOK(0x471120, BinocularOverlay_Create);

/* three tables of 64 entries of 14 words, each filled from one template */
void BinocularOverlay_InitGrid(char *grid, int n)
{
    int i;

    for (i = 0; i < 64; i++) {
        __builtin_memcpy(grid + 0x1C00 + i * 0x38, gGridTemplate, 0x38);
        __builtin_memcpy(grid + 0xE00 + i * 0x38, gGridTemplate, 0x38);
        __builtin_memcpy(grid + i * 0x38, gGridTemplate, 0x38);
    }
    *(int *)(grid + 0x2A00) = n;
}
HOOK(0x471280, BinocularOverlay_InitGrid);

void VUMeter_Init(VUMeter *m, int period, float decay)
{
    m->level = 0;
    m->unk4 = 0;
    m->decay = 0;
    m->timer = 0;
    m->period = period;
    m->decay = decay;
}
HOOK(0x4712E0, VUMeter_Init);

void BinocularHolder_Create(BinocularHolder *h)
{
    h->isActive = 0;
    VarString_Init(&h->active);
    h->overlay = NULL;
}
HOOK(0x471310, BinocularHolder_Create);

void BinocularHolder_Run(BinocularHolder *h)
{
    if (VarString_HasExpr(&h->active))
        h->isActive = (char)x87_round(VarString_EvalReal(&h->active, h));
    if (h->isActive) {
        if (!h->overlay)
            h->overlay = TaskRef_Add(BinocularOverlay_New(GameScreen_GetView(*(void **)(Game_Get() + 0x44))));
    } else if (h->overlay && *h->overlay) {
        Task_Delete(*h->overlay);
        TaskRef_Remove(h->overlay);
        h->overlay = NULL;
    }
}
HOOK(0x471330, BinocularHolder_Run);

void BinocularOverlay_Delete(BinocularOverlay *ov)
{
    Text_Free(ov->text);
    Text_Free(ov->unk34);
    Text_Free(ov->letters[0]);
    Text_Free(ov->letters[1]);
    Text_Free(ov->letters[2]);
    Text_Free(ov->letters[3]);
}
HOOK(0x4713C0, BinocularOverlay_Delete);

void BinocularHolder_Delete(BinocularHolder *h)
{
    VarString_Free(&h->active);
    if (h->overlay && *h->overlay) {
        Task_Delete(*h->overlay);
        TaskRef_Remove(h->overlay);
        h->overlay = NULL;
    }
}
HOOK(0x471400, BinocularHolder_Delete);

void BinocularOverlay_Draw(BinocularOverlay *ov);

void BinocularOverlay_StatusDisplayHandler(BinocularOverlay *ov)
{
    TransContext_Create(ov->ctx, &ov->matrix, &ov->pos, ov->fov);
    TransContext_Select(ov->ctx);
    BinocularOverlay_Draw(ov);
    TransContext_Invalidate();
}
HOOK(0x471440, BinocularOverlay_StatusDisplayHandler);

void VUMeter_Update(VUMeter *m, void *rng);
void VUMeter_Draw(VUMeter *m, float x, float y, float w, float h, int vertical, int flip);
void BinocularOverlay_TrackMovement(BinocularOverlay *ov);
void BinocularOverlay_DrawCompass(BinocularOverlay *ov);

/* range finder, compass, meters, frame and the marks on the humans in view */
void BinocularOverlay_Draw(BinocularOverlay *ov)
{
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    char buf[128];
    CollFilter filter;
    CollHit hit;
    Vec3d from, to;
    const char *label, *unit;
    float w, h, ty, x, y, mw, mh, step, q, hq, half, nq, wq, cx, cy, off, pitch, yaw;
    void *world;
    uint8_t event;
    int i;

    filter.a = ov;
    filter.b = ov->task.parent;
    from = ov->pos;
    to.x = ov->dir[0];
    to.y = ov->dir[1];
    filter.c = ov->task.parent->parent;
    filter.unk10 = 1.0f;
    to.x = (AppContext_IsUSA() ? kImperial : kMetric) * to.x * kWorldUnit;
    to.y = (AppContext_IsUSA() ? kImperial : kMetric) * to.y * kWorldUnit;
    {
        long double dz = (AppContext_IsUSA() ? kImperial : kMetric) * (double)ov->dir[2] * kWorldUnit;

        to.x = from.x + to.x;
        to.y = from.y + to.y;
        to.z = from.z + dz;
    }
    event = Game_GetEvent7();
    world = Game_GetWorld();
    Coll_Lines(world, &hit, &from, &to, 1, event, &filter, -1);
    if (Resource_Find("weapons/strings/Binoculars/range"))
        label = Resource_Load("weapons/strings/Binoculars/range", 0);
    else
        label = "RANGE";
    if (hit.found) {
        double dist = hit.dist * kInvWorldUnit;

        unit = AppContext_IsUSA() ? "ft" : "M";
        if (AppContext_IsUSA())
            dist = dist * kFeet;
        crt_sprintf(buf, "%s: %0.f%s", label, dist, unit);
    } else {
        unit = AppContext_IsUSA() ? "ft" : "M";
        crt_sprintf(buf, "%s: %0.f+%s", label, (double)(AppContext_IsUSA() ? 3000 : 1000), unit);
    }
    Text_SetString(ov->text, buf);
    w = width;
    ov->text->x = w * kTextX.f;
    h = height;
    ty = h * kTextY.f;
    ov->text->y = ty - (long double)Text_GetHeight(ov->text);
    Text_DrawAlt(ov->text);
    Vec3_ToAngles(ov->dir, &pitch, &yaw);
    BinocularOverlay_DrawCompass(ov);

    x = w * kEighth.f;
    y = h * kMeterY.f;
    mh = h * kEighth.f;
    step = w * kMeterStep.f * kTwelfth.f;
    mw = step - w * kMeterGap.f;
    for (i = 0; i < 12; i++) {
        VUMeter_Update(&ov->meters[i], ov->rng);
        VUMeter_Draw(&ov->meters[i], x, y, mw, mh, 0, 0);
        x = step + x;
    }

    q = w / (long double)Sprite_Width(gBinocularSprite + 0x18);
    hq = h + q;
    half = w * kHalf.f;
    nq = -q;
    QVertex_DrawQuad(gBinocularSprite + 0x18, nq, nq, half, nq, nq, hq, half, hq, 128.0f, 128.0f, 128.0f, 0,
                     0x1000000, 0);
    wq = w + q;
    QVertex_DrawQuad(gBinocularSprite + 0x18, wq, nq, half, nq, wq, hq, half, hq, 128.0f, 128.0f, 128.0f, 0,
                     0x1000000, 0);

    cy = height / 2;
    cx = width / 2;
    off = w * kMarkerX.f;
    QVertex_DrawPolyline(gMarkerShape, 4, cx - off, cy, 1.0f, 0, 100.0f, 0, 0.5f, 0, w * kMarkerW.f, h * kMarkerH.f,
                         0x80000, 0);
    QVertex_DrawPolyline(gMarkerShape, 4, off + cx, cy, 1.0f, 0, 100.0f, 0, 0.5f, 0, w * kMarkerW2.f,
                         h * kMarkerH2.f, 0x80000, 0);
    QVertex_DrawLine(0, cy, 1.0f, w, cy, 1.0f, 0, 32.0f, 0, 0, 32.0f, 0, 0.5f, 0x80000, 0);
    QVertex_DrawLine(cx, 0, 1.0f, cx, h, 1.0f, 0, 32.0f, 0, 0, 32.0f, 0, 0.5f, 0x80000, 0);
    BinocularOverlay_TrackMovement(ov);
}
HOOK(0x471480, BinocularOverlay_Draw);

void VUMeter_Update(VUMeter *m, void *rng)
{
    if (m->level > kZero.f) {
        long double t = (long double)m->level - m->decay;

        if (!(t >= kZero.f))
            m->level = kZero.f;
        else if (t > kOne.f)
            m->level = kOne.f;
        else
            m->level = t;
    }
    if (m->timer > 0) {
        m->timer--;
        return;
    }
    if (Random_Float(rng) > m->level)
        m->level = Random_Float(rng);
    m->timer = (int)x87_round((Random_Float(rng) + kOne.f) * m->period * kHalf.f) - 1;
}
HOOK(0x4719D0, VUMeter_Update);

void VUMeter_Draw(VUMeter *m, float x, float y, float w, float h, int vertical, int flip)
{
    float bw, bh;

    if (vertical) {
        bw = w * m->level;
        bh = h;
        if (flip)
            x = (kOne.f - m->level) * w + y; /* y, not x, like the original */
    } else {
        bw = w;
        bh = h * m->level;
        if (!flip)
            y = (kOne.f - m->level) * h + y;
    }
    Draw_RectA(x, y, bw, bh, m->level * kSixtyFour.f, 64.0f, 0, 0.5f, 0x80000, 0);
}
HOOK(0x471A90, VUMeter_Draw);

int BinocularOverlay_AddTarget(BinocularOverlay *ov, char *human, float depth, float radius, float *scr,
                               int occluded, int unk8, int unk4);

/* finds the humans inside the view and draws a growing mark on each */
void BinocularOverlay_TrackMovement(BinocularOverlay *ov)
{
    Task *player = &ov->task;
    int *list, i;

    ov->shown = 0;
    ov->visible = 0;
    while (!TaskType_IsA(player->type, Human_GetType())) {
        player = player->parent;
        if (!player)
            return;
    }
    list = Human_GetList();
    for (i = 0; i < 10; i++)
        ov->targets[i].seen = 0;
    for (i = 0; i < list[0]; i++) {
        char *hm = ((char **)(list + 2))[i];
        Vec3d eye, pos;
        long double dx, dy, dz;
        float v[3], scr[2], depth, radius, visibility;
        CollFilter filter;
        uint8_t event;
        void *world;

        if (!hm || hm == (char *)player || !hm[0xCF6] || hm[0xCF5])
            continue;
        HumanView_GetEye(&eye, *(void **)(hm + 0x4EC));
        pos = *(Vec3d *)(hm + 0x20);
        dx = pos.x - gView.pos.x;
        dy = pos.y - gView.pos.y;
        dz = pos.z - gView.pos.z;
        radius = *(float *)(hm + 0xA0);
        v[0] = gView.m[2] * dz + gView.m[1] * dy + gView.m[0] * dx;
        v[1] = gView.m[5] * dz + gView.m[4] * dy + gView.m[3] * dx;
        v[2] = gView.m[8] * dz + gView.m[7] * dy + gView.m[6] * dx;
        if (Mesh3D_Exclude(v, radius, NULL))
            continue;
        filter.a = player;
        filter.b = player;
        filter.c = hm;
        ov->visible++;
        visibility = 0;
        event = AIDetection_GetEvent0();
        world = Game_GetWorld();
        Coll_LineOfSight(world, &gView.pos, &eye, event, &filter, -1, -1, &visibility);
        View_Project(&gView, scr, &depth, &pos);
        if (!(depth > kZero.f))
            continue;
        BinocularOverlay_AddTarget(ov, hm, depth, radius, scr, visibility == kZero.f, *(int *)(hm + 0xFC) != 1,
                                   hm[0xCF5]);
    }
    for (i = 0; i < 10; i++) {
        Target *t = &ov->targets[i];
        int now = Timer_GetMs();
        float r, g, b, angle, scale, size[2], out[2];
        long double grow;
        int age;

        if (!t->seen) {
            if (now - t->time > 3000)
                t->human = NULL;
            continue;
        }
        if (t->occluded)
            continue;
        ov->shown++;
        if (t->unk4) {
            r = 0;
            g = 0;
            b = 128.0f;
        } else if (t->unk8) {
            r = 0;
            g = 128.0f;
            b = 0;
        } else {
            r = 64.0f;
            g = 0;
            b = 0;
        }
        age = now - t->firstTime;
        if (age < 0)
            age = 0;
        else if (age > 250)
            age = 250;
        grow = age * kGrowRate.f;
        angle = grow * kHalfPi.f;
        scale = (kOne.f - grow) * kTen.f + kOne.f;
        size[0] = t->radius;
        size[1] = t->radius;
        View_ScaleSize(&gView, out, size, t->depth);
        QVertex_DrawPolyline(gTargetShape, 5, t->sx, t->sy, 1.0f, r, g, b, 0.5f, angle, out[0] * scale,
                             out[1] * scale, 0, 0);
    }
}
HOOK(0x471B60, BinocularOverlay_TrackMovement);

/* keeps the human in its slot, a free slot, or the slot of a farther one */
int BinocularOverlay_AddTarget(BinocularOverlay *ov, char *human, float depth, float radius, float *scr,
                               int occluded, int unk8, int unk4)
{
    Target *t;
    int i;

    for (i = 0; i < 10; i++) {
        if (ov->targets[i].human == human) {
            t = &ov->targets[i];
            goto found;
        }
    }
    for (i = 0; i < 10; i++) {
        if (!ov->targets[i].human) {
            t = &ov->targets[i];
            t->firstTime = Timer_GetMs();
            goto found;
        }
    }
    for (i = 0; i < 10; i++) {
        if (ov->targets[i].depth > depth) {
            t = &ov->targets[i];
            t->firstTime = Timer_GetMs();
            goto found;
        }
    }
    return 0;
found:
    t->human = human;
    t->depth = depth;
    t->seen = 1;
    t->radius = radius;
    t->occluded = occluded;
    t->time = Timer_GetMs();
    t->unk8 = unk8;
    t->unk4 = unk4;
    t->sx = scr[0];
    t->sy = scr[1];
    return 1;
}
HOOK(0x471F60, BinocularOverlay_AddTarget);

/* the heading strip with a tick every 1/64 turn and the letters */
void BinocularOverlay_DrawCompass(BinocularOverlay *ov)
{
    int sw = Display_GetMode()->width;
    int sh = Display_GetMode()->height;
    float pitch, yaw, x0, y0, width, bh, hw, hh, step, off, cy, cx, left, right;
    QVertex v[3];
    QVertexStrip strip;
    long double bottom;
    int i;

    Vec3_ToAngles(ov->dir, &pitch, &yaw);
    x0 = sw * kCompassX.f;
    y0 = sh * kCompassY.f;
    width = sw * kCompassW.f;
    i = Text_GetHeight(ov->letters[0]);
    bh = (long double)i + i + kCompassPad.f;
    hw = width * kHalf.f;
    hh = bh * kHalf.f;
    step = width * kMarkerW.f;
    if (yaw < kZero.f)
        yaw = yaw + kTwoPi.f;
    off = width * yaw * kInvTwoPi.f;
    cy = hh + y0;
    for (i = 0; i < 64; i++) {
        Text *text = NULL;
        long double half, sum;
        float len, x, top, low;

        if (!(i & 0xF)) {
            len = kTickLong.f;
            text = ov->letters[i >> 4];
        } else if (!(i & 7)) {
            len = kTickMid.f;
        } else {
            len = kFour.f;
        }
        half = len * kHalf.f;
        low = half + hh + y0;
        x = off + x0;
        top = cy - half;
        QVertex_DrawLine(x, top, 1.0f, x, low, 1.0f, 0, 64.0f, 0, 0, 64.0f, 0, 1.0f, 0, 0);
        if (text) {
            float tw = Text_GetWidth(text);
            int th = Text_GetHeight(text);
            float tx = x - tw * kHalf.f;

            text->x = tx;
            text->y = top - (long double)th - kTwo.f;
            Text_DrawAlt(text);
            text->x = tx;
            text->y = low + kTwo.f;
            Text_DrawAlt(text);
        }
        sum = (long double)off + step;
        off = sum;
        if (!(sum < width))
            off = off - width;
    }

    cx = hw + x0;
    for (i = 0; i < 3; i++) {
        v[i].z = 1.0f;
        v[i].u = 0;
        v[i].v = 0;
        v[i].a = 1.0f;
        v[i].r = 0;
        v[i].g = 64.0f;
        v[i].b = 0;
    }
    v[0].y = y0 + kFour.f;
    left = cx - kFour.f;
    right = cx + kFour.f;
    v[1].x = left;
    v[2].x = right;
    v[0].x = cx;
    v[1].y = y0;
    v[2].y = y0;
    strip.lines = 0;
    strip.flags = 0x800000;
    strip.queue = 0;
    strip.count = 3;
    strip.verts = v;
    gQVertexTexture = NULL;
    gRenderFuncs[gQVertexStrip](&strip);
    bottom = (long double)bh + y0;
    v[0].x = cx;
    v[0].y = bottom - kFour.f;
    v[1].x = left;
    v[1].y = bottom;
    v[2].x = right;
    v[2].y = bottom;
    gRenderFuncs[gQVertexStrip](&strip);
}
HOOK(0x472050, BinocularOverlay_DrawCompass);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD_CONST(igd, 534038, 534038, 0x38) INIT({0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x40});
ALIAS(gGridTemplate, pad_534038, 0);
PAD_CONST(igd, 534038, 534070, 0x40) INIT({0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x20, 0xC0, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x20, 0xC0, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x20, 0x40, 0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0xC0, 0xBF, 0x00, 0x00, 0x80, 0x3F});
ALIAS(gMarkerShape, pad_534070, 0);
PAD_CONST(igd, 534038, 5340B0, 0x64) INIT({0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0xBC, 0xAB, 0xAA, 0xAA, 0xBC, 0x00, 0x00, 0x80, 0x3C, 0xAB, 0xAA, 0xAA, 0x3C, 0x00, 0x00, 0xC0, 0x3D, 0x9A, 0x99, 0x19, 0x3C, 0x00, 0x00, 0x40, 0x3E, 0x33, 0x33, 0x43, 0x3F, 0xCD, 0xCC, 0x0C, 0x3F, 0x9A, 0x99, 0xC1, 0x3E, 0x21, 0x7F, 0xA3, 0xFC, 0x28, 0x3F, 0x0A, 0x40, 0x34, 0x33, 0x33, 0x33, 0x33, 0x93, 0x8C, 0x40, 0x6F, 0x12, 0x83, 0x3B});
ALIAS(gTargetShape, pad_5340B0, 0);
DATA(5401F8) uint16_t gBinocularOverlayType INIT(385);
DATA(5401F8) uint16_t gBinocularHolderType INIT(385);
BSS(5BE344) char *gBinocularSprite;
