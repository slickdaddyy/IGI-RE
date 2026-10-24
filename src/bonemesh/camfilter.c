/* Camera filters: scan lines and noise, wide screen bars, the band filter
   texture and the hand held camera overlay. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "bonemesh/bonemesh.h"
#include "camfeed/camfeedtypes.h"

FN_EXTERN(BandFilter_Draw);
FN_EXTERN(BandFilter_DrawPrim);
FN_EXTERN(BandFilter_Render);
FN_EXTERN(BandFilter_Run);
FN_EXTERN(Debug_Log);
FN_EXTERN(HandiCamFilter_Draw);
FN_EXTERN(HandiCamFilter_DrawPrim);
FN_EXTERN(HandiCamFilter_Render);
FN_EXTERN(HandiCamFilter_Run);
FN_EXTERN(LineFilter_Draw);
FN_EXTERN(LineFilter_DrawPrim);
FN_EXTERN(Task_Nothing);
FN_EXTERN(WideScreenFilter_Draw);
FN_EXTERN(WideScreenFilter_DrawPrim);

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x490370, unsigned int, Timer_GetMs, (void));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B2320, void, Texture_Alloc, (void *tex, int a2, int pitch, int width, int height, int format, int a7, int a8));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B66A0, void, Sprite_Free, (void *slot));
ORIG(0x4B6720, int, Sprite_Load, (void *slot, const char *name));
ORIG(0x4B7000, void *, Sprite_New, (const char *name));
ORIG(0x4B7030, void, Sprite_Delete, (void *sprite));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));

typedef struct FilterVertex {
    float x, y, z, rhw;
    uint32_t color;
    float u, v;
} FilterVertex;

extern uint16_t gLineFilterType;
extern uint16_t gWideScreenFilterType;
extern uint16_t gBandFilterType;
extern int gBandCamera;
extern uint16_t gHandiCamFilterType;
extern int gHandiCamDrawMethod;
extern int gBandDrawMethod;
extern const char *gBandFont;
extern int gHandiCamFilterPrim;
extern int gBandFilterPrim;
extern int gBandPixels[];
extern char gFilterRandom[];
extern int gLineDrawMethod;
extern int gLineFilterPrim;
extern int gWideScreenPrim;
extern const char *gBatterySprite;
extern const char *gTimecodeFont;
extern int gBatteryX;
extern int gBatteryY;
extern int gTimecodeX;
extern int gTimecodeY;
extern float gTimecode;
extern float gBandTextX;
extern float gBandTextY;
extern float gBandTextDX;
extern int gBandTextDY;

extern void *gFilterVB;
extern int gFilterVBSize;
extern int gFilterVBUsed;
extern float gD3DMinZ;
extern float gLayer1MinZ;
extern float gLayer1MaxZ;
extern float gNearPlane;

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define TASK_DEBUG_LOG (FN(0x4C1780, Debug_Log))

void LineFilter_Render(LineFilter *t);

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(kM128, 0xC3000000);
F32(k128, 0x43000000);
F32(kM255, 0xC37F0000);
F32(k255, 0x437F0000);
F32(k256, 0x43800000);
F32(kM256, 0xC3800000);
F32(kSeedScale, 0x46A8C000); /* 21600 */
F32(kLineStep, 0x40800000);  /* 4 */
F32(kNoiseScale, 0x41200000); /* 10 */
F32(kStreak, 0x42480000);    /* 50 */

LineFilter *LineFilter_New(void *parent, float r, float g, float b, float depth, int sorted)
{
    LineFilter *t = (LineFilter *)Task_New(parent, gLineFilterType, NULL);

    t->color[0] = r;
    t->color[1] = g;
    t->color[2] = b;
    t->depth = depth;
    t->level = 0;
    t->sorted = sorted;
    return t;
}
HOOK(0x4A23E0, LineFilter_New);

WideScreenFilter *WideScreenFilter_New(void *parent, float r, float g, float b, float unk2C, float w, float h, float x,
                                       float y, float barHeight)
{
    WideScreenFilter *t = (WideScreenFilter *)Task_New(parent, gWideScreenFilterType, NULL);

    t->color[0] = r;
    t->color[1] = g;
    t->color[2] = b;
    t->unk2C = unk2C;
    t->w = w;
    t->h = h;
    t->x = x;
    t->y = y;
    t->barHeight = barHeight;
    return t;
}
HOOK(0x4A2430, WideScreenFilter_New);

static float clamp01(float v)
{
    if (kZero.f > v)
        return kZero.f;
    if (kOne.f < v)
        return kOne.f;
    return v;
}

BandFilter *BandFilter_New(void *parent, float depth, float r, float g, float b, int period, int interval,
                           float maxLevel)
{
    BandFilter *t = (BandFilter *)Task_New(parent, gBandFilterType, NULL);
    char mode;

    t->depth = depth;
    t->level = 0;
    t->maxLevel = maxLevel;
    Texture_Alloc(&t->tex, 0, 0x40, 0x10, 0x10, 3, 0, 0);
    t->timer = interval;
    t->interval = interval;
    t->counter = 0;
    t->period = period;
    t->color[2] = clamp01(b);
    t->color[0] = clamp01(r);
    t->color[1] = clamp01(g);
    t->camera = gBandCamera;
    mode = Mem_GetMode();
    if (!mode)
        Mem_SetDefaultMode();
    t->font = NULL;
    t->font = Sprite_New(gBandFont);
    if (!mode)
        Mem_ClearMode();
    return t;
}
HOOK(0x4A2490, BandFilter_New);

void BandFilter_Delete(BandFilter *t)
{
    Sprite_Delete(t->font);
    Texture_Free(&t->tex);
}
HOOK(0x4A25E0, BandFilter_Delete);

/* colour of the light (scale 128) and dark (scale 255) bands */
static int band_color(const BandFilter *t, float neg, float pos)
{
    int c = (int)round_ll(t->color[0] * neg) << 8;

    c = (c + (int)round_ll(t->color[1] * neg)) << 8;
    return (int)round_ll(t->color[2] * pos) - c;
}

void BandFilter_FillStripes(BandFilter *t)
{
    int light = band_color(t, kM128.f, k128.f);
    int dark = band_color(t, kM255.f, k255.f);
    int *p = gBandPixels;
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 32; j++)
            *p++ = light;
        for (j = 0; j < 32; j++)
            *p++ = dark;
    }
}
HOOK(0x4A2600, BandFilter_FillStripes);

void BandFilter_FillSolid(BandFilter *t)
{
    int dark = band_color(t, kM255.f, k255.f);
    int i;

    for (i = 0; i < 0x100; i++)
        gBandPixels[i] = dark;
}
HOOK(0x4A26C0, BandFilter_FillSolid);

/* darkens level * 256 random pixels by a random factor. The index can
   reach 256, one past the table. */
void BandFilter_AddNoise(BandFilter *t)
{
    int n = (int)round_ll(t->level * k256.f);
    long double f;
    int *p;
    int v, lo, mid, hi, c;

    for (; n > 0; n--) {
        f = Random_Float(gFilterRandom);
        p = gBandPixels - (int)round_ll(f * kM256.f);
        v = *p;
        lo = v & 0xFF;
        mid = (v >> 8) & 0xFF;
        hi = (v >> 16) & 0xFF;
        f = Random_Float(gFilterRandom);
        c = (int)round_ll(hi * f) << 8;
        c = (c + (int)round_ll(mid * f)) << 8;
        *p = (c + (int)round_ll(lo * f)) << 8;
    }
}
HOOK(0x4A2720, BandFilter_AddNoise);

void BandFilter_SetColor(BandFilter *t, float r, float g, float b)
{
    t->color[0] = clamp01(r);
    t->color[1] = clamp01(g);
    t->color[2] = clamp01(b);
}
HOOK(0x4A2800, BandFilter_SetColor);

void BandFilter_SetFont(const char *font, float a, float b, float c, int d)
{
    gBandFont = font;
    gBandTextX = a;
    gBandTextY = b;
    gBandTextDX = c;
    gBandTextDY = d;
}
HOOK(0x4A28B0, BandFilter_SetFont);

/* camera numbers cycle through 1..9 */
void BandFilter_NextCamera(void)
{
    if (gBandCamera < 9)
        gBandCamera++;
    else
        gBandCamera = 1;
}
HOOK(0x4A28F0, BandFilter_NextCamera);

HandiCamFilter *HandiCamFilter_New(void *parent, float r, float g, float b, float depth)
{
    char mode = Mem_GetMode();
    HandiCamFilter *t;

    if (!mode)
        Mem_SetDefaultMode();
    t = (HandiCamFilter *)Task_New(parent, gHandiCamFilterType, NULL);
    t->color[0] = r;
    t->color[1] = g;
    t->color[2] = b;
    t->depth = depth;
    t->blink = 0;
    t->lastMs = Timer_GetMs();
    t->font = NULL;
    t->font = Sprite_New(gTimecodeFont);
    Sprite_Load(t->battery, gBatterySprite);
    if (!mode)
        Mem_ClearMode();
    return t;
}
HOOK(0x4A2910, HandiCamFilter_New);

void HandiCamFilter_Delete(HandiCamFilter *t)
{
    Sprite_Free(t->battery);
    Sprite_Delete(t->font);
}
HOOK(0x4A29A0, HandiCamFilter_Delete);

void HandiCamFilter_SetSprite(const char *name, int x, int y)
{
    gBatterySprite = name;
    gBatteryX = x;
    gBatteryY = y;
}
HOOK(0x4A29C0, HandiCamFilter_SetSprite);

void HandiCamFilter_SetFont(const char *name, int x, int y)
{
    gTimecodeFont = name;
    gTimecodeX = x;
    gTimecodeY = y;
}
HOOK(0x4A29E0, HandiCamFilter_SetFont);

void CameraFilter_Init(void)
{
    Script_RegisterConstant("CAMERAFILTER_TYPE_NONE", 0);
    Script_RegisterConstant("CAMERAFILTER_TYPE_LINE", 1);
    Script_RegisterConstant("CAMERAFILTER_TYPE_BAND", 2);
    Script_RegisterConstant("CAMERAFILTER_TYPE_BAND_UPDATE", 3);
    Script_RegisterConstant("CAMERAFILTER_TYPE_HANDICAM", 4);
    gLineFilterType = TaskType_New("LineFilter", sizeof(LineFilter), Task_GetRootType(), 0);
    gWideScreenFilterType = TaskType_New("WideScreenFilter", sizeof(WideScreenFilter), Task_GetRootType(), 0);
    gBandFilterType = TaskType_New("BandFilter", sizeof(BandFilter), Task_GetRootType(), 0);
    gHandiCamFilterType = TaskType_New("HandiCamFilter", sizeof(HandiCamFilter), Task_GetRootType(), 0);
    TaskType_SetHandler2(gLineFilterType, Screen_GetDrawEvent(), FN(0x4A3550, LineFilter_Draw));
    TaskType_SetHandler2(gLineFilterType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gLineFilterType, 2, TASK_DEBUG_LOG);
    TaskType_SetHandler2(gWideScreenFilterType, Screen_GetDrawEvent(), FN(0x4A3650, WideScreenFilter_Draw));
    TaskType_SetHandler2(gWideScreenFilterType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gBandFilterType, Screen_GetDrawEvent(), FN(0x4A36D0, BandFilter_Draw));
    TaskType_SetHandler2(gBandFilterType, 1, FN(0x4A38A0, BandFilter_Run));
    TaskType_SetHandler2(gBandFilterType, 2, BandFilter_Delete);
    TaskType_SetHandler2(gHandiCamFilterType, Screen_GetDrawEvent(), FN(0x4A4D10, HandiCamFilter_Draw));
    TaskType_SetHandler2(gHandiCamFilterType, 1, FN(0x4A4CB0, HandiCamFilter_Run));
    TaskType_SetHandler2(gHandiCamFilterType, 2, HandiCamFilter_Delete);
    gLineDrawMethod = RenderMode_CreateMethod("DrawLineFilter");
    gBandDrawMethod = RenderMode_CreateMethod("DrawBandFilter");
    gHandiCamDrawMethod = RenderMode_CreateMethod("DrawHandiCamFilter");
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gLineDrawMethod, LineFilter_Render);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gBandDrawMethod, FN(0x4A3940, BandFilter_Render));
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gHandiCamDrawMethod, FN(0x4A4300, HandiCamFilter_Render));
    gLineFilterPrim = Primitive_Register("LineFilter");
    gWideScreenPrim = Primitive_Register("WideScreenFilter");
    gBandFilterPrim = Primitive_Register("BandFilter");
    gHandiCamFilterPrim = Primitive_Register("HandiCamFilter");
    Primitive_SetHandler(gLineFilterPrim, FN(0x4A3530, LineFilter_DrawPrim));
    Primitive_SetHandler(gWideScreenPrim, FN(0x4A35B0, WideScreenFilter_DrawPrim));
    Primitive_SetHandler(gBandFilterPrim, FN(0x4A36B0, BandFilter_DrawPrim));
    Primitive_SetHandler(gHandiCamFilterPrim, FN(0x4A4C90, HandiCamFilter_DrawPrim));
    Random_Init(gFilterRandom);
    gTimecode = Random_Float(gFilterRandom) * kSeedScale.f;
}
HOOK(0x4A2A00, CameraFilter_Init);

/* locks room for 200 vertices in the filter buffer, discarding it when
   full; 0 on failure */
#define LOCK_BATCH(v, first)                                            \
    (gFilterVBUsed + 200 > gFilterVBSize                                \
         ? (hr = VB_Lock(gFilterVB, 0x2020, &(v), NULL), (first) = 0,   \
            gFilterVBUsed = 200, !hr && (v))                            \
         : (hr = VB_Lock(gFilterVB, 0x1020, &(v), NULL),                \
            (first) = gFilterVBUsed, (v) += (first),                    \
            gFilterVBUsed = (first) + 200, !hr && (v)))

static uint32_t line_color(const LineFilter *t)
{
    uint32_t c = (uint32_t)round_ll(t->color[0] * k255.f) | 0x4C00;

    c = c << 8 | (uint32_t)round_ll(t->color[1] * k255.f);
    return c << 8 | (uint32_t)round_ll(t->color[2] * k255.f);
}

/* every 4 pixels a line across the view, then level * 10 noise streaks
   per row (one every 1 / (level * 10) rows when that is below 1) */
void LineFilter_Render(LineFilter *t)
{
    float m[16];
    float depth, rhw, z, level, y, ny, fy, step;
    long double s, lim;
    FilterVertex *v;
    uint32_t c, gray;
    long hr;
    int first, count, n, k;

    depth = t->depth > gNearPlane ? t->depth : gNearPlane;
    DEV_GetTransform(gD3DDevice, 3, m);
    rhw = kOne.f / (gWorldScale * depth);
    z = (kOne.f - gD3DMinZ * rhw) * m[10] * (gLayer1MaxZ - gLayer1MinZ) + gLayer1MinZ;
    gD3DTextureStates->current = -1;
    gD3DBlendStates->current = -1;
    gD3DRenderStates->current = -1;
    gD3DClipStates->current = -1;
    DEV_SetRenderState(gD3DDevice, 0x2F, 0);
    DEV_SetRenderState(gD3DDevice, 0x1C, 0);
    DEV_SetRenderState(gD3DDevice, 0x1D, 0);
    DEV_SetRenderState(gD3DDevice, 0x16, 1);
    DEV_SetRenderState(gD3DDevice, 0x89, 0);
    DEV_SetRenderState(gD3DDevice, 0x1A, 1);
    DEV_SetRenderState(gD3DDevice, 0x0F, 0);
    DEV_SetRenderState(gD3DDevice, 0x17, 4);
    DEV_SetRenderState(gD3DDevice, 0x07, 1);
    DEV_SetRenderState(gD3DDevice, 0x0E, 0);
    DEV_SetTextureStageState(gD3DDevice, 0, 4, 3);
    DEV_SetTextureStageState(gD3DDevice, 0, 5, 2);
    DEV_SetTextureStageState(gD3DDevice, 0, 6, 0);
    DEV_SetTextureStageState(gD3DDevice, 0, 1, 3);
    DEV_SetTextureStageState(gD3DDevice, 0, 2, 2);
    DEV_SetTextureStageState(gD3DDevice, 0, 3, 0);
    DEV_SetTextureStageState(gD3DDevice, 1, 1, 1);
    DEV_SetTexture(gD3DDevice, 0, NULL);
    DEV_SetRenderState(gD3DDevice, 0x1B, 1);
    DEV_SetRenderState(gD3DDevice, 0x13, 5);
    DEV_SetRenderState(gD3DDevice, 0x14, 6);
    level = t->level;
    if (!LOCK_BATCH(v, first))
        return;
    count = 0;
    y = gScene.y;
    while (!(gScene.h + gScene.y < y + kLineStep.f)) {
        if (count < 200) {
            fy = (int)round_ll(y);
            v->x = (int)round_ll(gScene.x);
            v->y = fy;
            v->z = z;
            v->rhw = rhw;
            v->color = line_color(t);
            v++;
            v->x = (int)round_ll(gScene.w + gScene.x);
            v->y = fy;
            v->z = z;
            v->rhw = rhw;
            v->color = line_color(t);
            v++;
            count += 2;
        } else {
            VB_Unlock(gFilterVB);
            DEV_DrawPrimitiveVB(gD3DDevice, 2, gFilterVB, first, count, 0);
            if (!LOCK_BATCH(v, first))
                return;
            y = y - kLineStep.f;
            count = 0;
        }
        y = y + kLineStep.f;
    }
    VB_Unlock(gFilterVB);
    if (count > 0)
        DEV_DrawPrimitiveVB(gD3DDevice, 2, gFilterVB, first, count, 0);
    if (!(level > kZero.f))
        return;
    s = level * kNoiseScale.f;
    step = kOne.f;
    if (s < kOne.f) {
        step = kOne.f / s;
        s = kOne.f;
    }
    n = (int)round_ll(s);
    if (!LOCK_BATCH(v, first))
        return;
    count = 0;
    y = gScene.y;
    ny = step + y;
    while (!(gScene.h + gScene.y < ny)) {
        for (k = 0; k < n; k++) {
            if (count < 200) {
                fy = (int)round_ll(y);
                v->x = Random_Float(gFilterRandom) * gScene.w + gScene.x;
                v->y = fy;
                v->z = z;
                v->rhw = rhw;
                gray = (uint32_t)round_ll(Random_Float(gFilterRandom) * k255.f);
                c = (uint32_t)round_ll(level * k255.f);
                c = ((c << 8 | gray) << 8 | gray) << 8 | gray;
                v->color = c;
                v++;
                count++;
                v->x = Random_Float(gFilterRandom) * kStreak.f + v[-1].x;
                lim = gScene.w + gScene.x;
                if (lim <= v->x)
                    v->x = lim;
                v->y = fy;
                v->z = z;
                v->rhw = rhw;
                v->color = c;
                v++;
                count++;
            } else {
                VB_Unlock(gFilterVB);
                DEV_DrawPrimitiveVB(gD3DDevice, 2, gFilterVB, first, count, 0);
                if (!LOCK_BATCH(v, first))
                    return;
                count = 0;
            }
        }
        y = ny;
        ny = step + y;
    }
    VB_Unlock(gFilterVB);
    if (count > 0)
        DEV_DrawPrimitiveVB(gD3DDevice, 2, gFilterVB, first, count, 0);
}
HOOK(0x4A2CD0, LineFilter_Render);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(534354) char str_534354[15] INIT("runtime error ");
PAD(igd, 534354, 534363, 0x1);
DATA(534354) char str_534364[3] INIT("\r\n");
PAD(igd, 534354, 534367, 0x1);
DATA(534354) char str_534368[14] INIT("TLOSS error\r\n");
PAD(igd, 534354, 534376, 0x2);
DATA(534354) char str_534378[13] INIT("SING error\r\n");
PAD(igd, 534354, 534385, 0x3);
DATA(534354) char str_534388[15] INIT("DOMAIN error\r\n");
PAD(igd, 534354, 534397, 0x1);
DATA(534354) char str_534398[37] INIT("R6028\r\n- unable to initialize heap\r\n");
PAD(igd, 534354, 5343BD, 0x3);
DATA(534354) char str_5343C0[53] INIT("R6027\r\n- not enough space for lowio initialization\r\n");
PAD(igd, 534354, 5343F5, 0x3);
DATA(534354) char str_5343F8[53] INIT("R6026\r\n- not enough space for stdio initialization\r\n");
PAD(igd, 534354, 53442D, 0x3);
DATA(534354) char str_534430[38] INIT("R6025\r\n- pure virtual function call\r\n");
PAD(igd, 534354, 534456, 0x2);
DATA(534354) char str_534458[53] INIT("R6024\r\n- not enough space for _onexit/atexit table\r\n");
PAD(igd, 534354, 53448D, 0x3);
DATA(534354) char str_534490[41] INIT("R6019\r\n- unable to open console device\r\n");
PAD(igd, 534354, 5344B9, 0x3);
DATA(534354) char str_5344BC[33] INIT("R6018\r\n- unexpected heap error\r\n");
PAD(igd, 534354, 5344DD, 0x3);
DATA(534354) char str_5344E0[45] INIT("R6017\r\n- unexpected multithread lock error\r\n");
PAD(igd, 534354, 53450D, 0x3);
DATA(534354) char str_534510[44] INIT("R6016\r\n- not enough space for thread data\r\n");
DATA(534354) char str_53453C[33] INIT("\r\nabnormal program termination\r\n");
PAD(igd, 534354, 53455D, 0x3);
DATA(534354) char str_534560[44] INIT("R6009\r\n- not enough space for environment\r\n");
DATA(534354) char str_53458C[42] INIT("R6008\r\n- not enough space for arguments\r\n");
PAD(igd, 534354, 5345B6, 0x2);
DATA(534354) char str_5345B8[37] INIT("R6002\r\n- floating point not loaded\r\n");
PAD(igd, 534354, 5345DD, 0x3);
DATA(5346DC) char str_5346DC[2] INIT("(");
PAD(igd, 5346DC, 5346DE, 0xE) INIT({0x6E, 0x00, 0x75, 0x00, 0x6C, 0x00, 0x6C, 0x00, 0x29});
DATA(5346DC) char str_5346EC[7] INIT("(null)");
PAD(igd, 5346DC, 5346F3, 0x1);
DATA(543D4C) uint16_t gLineFilterType INIT(385);
DATA(543D4C) uint16_t gWideScreenFilterType INIT(385);
DATA(543D4C) uint16_t gBandFilterType INIT(385);
PAD(igd, 543D4C, 543D52, 0x2);
DATA(543D4C) int gBandCamera INIT(1);
DATA(543D4C) uint16_t gHandiCamFilterType INIT(385);
PAD(igd, 543D4C, 543D5A, 0x2);
STORE(igb, 935BF0, 1) char store_935BF0[12];
ALIAS(gFilterRandom, store_935BF0, 0);
BSS(935C04) const char *gBandFont;
STORE(igb, 935C10, 4) int store_935C10[256];
ALIAS(gBandPixels, store_935C10, 0);
BSS(936020) const char *gBatterySprite;
BSS(936020) const char *gTimecodeFont;
