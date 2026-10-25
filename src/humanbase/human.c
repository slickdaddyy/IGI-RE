/* Task handlers of the Human base type (players and AI soldiers): drawing,
   collision queries, damage, pickups, camera info and the debug overlay. */
#pragma GCC optimize("excess-precision=standard")

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "humanbase/humanbase.h"
#include "transctx/transctxtypes.h"
#include "task/taskvars.h"
#include "bonemesh/bonemeshvars.h"
#include "humanbase/humanbasevars.h"

ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401F80, void, Task_ClearGlobalFlags, (uint32_t flags));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4028C0, int, Flow_GetRenders, (void));
ORIG(0x415020, int, Game_CheatKeyPressed, (int key));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x416890, uint8_t, Game_GetEvent14, (void));
ORIG(0x4168A0, int *, Game_GetTaskList0, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, Human *h, double *pos, float radius, int unk));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x416D80, char, PlayerXPHit, (void));
ORIG(0x450C80, uint8_t, HumanAI_GetEventSlot, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x450CE0, void, HumanAI_Broadcast, (void *noise));
ORIG(0x452920, double *, MoveItem_GetPos, (Task *node));
ORIG(0x463130, int, Human_GetTeam, (Task *h));
ORIG(0x463150, char, Human_IsPlayer, (Human *h));
ORIG(0x463310, void, Human_SetPlatform, (Human *h, void *obj));
ORIG(0x463550, void, Human_ClearPlatform, (Human *h));
ORIG(0x463640, void, Human_SetGround, (Human *h, Task *ground));
ORIG(0x463680, Task *, Human_GetGround, (Human *h));
ORIG(0x463700, void, Human_SetAttached, (Human *h, int a));
ORIG(0x463740, Task *, Human_GetAttached, (Human *h));
ORIG(0x463AD0, void, Human_Bleed, (Human *h, void *dir, int unk));
ORIG(0x464250, int, Human_GetMaskType, (void));
ORIG(0x465800, void, HumanView_GetEye, (double *pos, HumanView *v));
ORIG(0x465820, void, Human_GetEyePos, (void *out, Human *h));
ORIG(0x465880, void, HumanView_GetMatrix, (float *m, HumanView *v));
ORIG(0x46AB30, uint16_t, ComputerMap_GetZoom, (void));
ORIG(0x477BF0, uint16_t, Gun_GetType, (void));
ORIG(0x477C20, uint8_t, Gun_GetEvent2, (void));
ORIG(0x477C50, Task *, Gun_New, (Human *h, int a, int id, int ammo, char b));
ORIG(0x47B290, int, GunShot_FindNear, (double (*out)[3], int max, void *shot, double *pos));
ORIG(0x47CE00, void, AmmoList_Add, (void *ammo, int id, int count));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));
ORIG(0x4872B0, void, StatusScreen_ResetBlink, (void *a));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489C60, int, Human_GetHitZone, (Human *h, void *hit));
ORIG(0x489E10, void, HumanDamage_GetZone, (HumanDamage *d, int part));
ORIG(0x489F70, void, Human_ApplyLook, (Human *h, int frame));
ORIG(0x48A340, float *, Human_GetBonePos, (float *out, Human *h, int unk));
ORIG(0x467F30, void, ComputerObject_DrawMarker, (void *query));
ORIG(0x491CF0, int *, Display_GetMode, (void));
ORIG(0x497F20, void, TransContext_Project, (float *screen, float *depth, double *pos));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4AF9B0, int, List_Count, (void *list));
ORIG(0x4B27D0, long double, Math_ASinF, (float x));
ORIG(0x4B2840, long double, Math_ASin, (double x));
ORIG(0x4B2C70, int, Vec3_RayPlane, (float *out, const float *p, const float *d, const float *n));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const double *a, const double *b));
ORIG(0x4B3320, char, Vec3d_IsZero, (const double *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (float *m, float a));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4B4E00, void, Draw_Sprite, (void *s, float x, float y, float r, float g, float b, int frame, int unk4,
                                   int unk8));
ORIG(0x4B5560, void, QVertex_DrawQuadUV, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float u0, float v0, float u1, float v1, float u2,
                                          float v2, float u3, float v3, float r, float g, float b, float z, float a,
                                          int frame, unsigned int flags, int unk20));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));
ORIG(0x4B6E90, void, Text_SetString, (HumanText *t, const char *s));
ORIG(0x4B6EC0, HumanText *, Text_New, (void *font, float x, float y, const char *str));
ORIG(0x4B6F50, void, Text_DrawAlt, (HumanText *t));
ORIG(0x4B7E00, int, Text_GetHeight, (HumanText *t));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, double *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4CBE40, void, Coll_DropPointsEx, (void *world, CollHit *hits, double (*points)[3], int count, double *dz,
                                        uint8_t event, void *filter, unsigned int flags, int unk));
ORIG(0x4CDB20, void, Coll_LinesEx, (void *world, CollHit *hits, double (*starts)[3], double (*ends)[3], int count,
                                   uint8_t event, void *filter, unsigned int flags, int unk));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, double *pos, float radius));
ORIG(0x4CE410, int, Coll_LineOfSight, (void *t, const double *from, const double *to, char unk, void *unk2,
                                       unsigned int flags, unsigned int trees, float *visibility));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void *, VirModel_Find, (const char *name));
ORIG(0x4CFF70, int, Mesh3D_GetBoneMeshLOD, (Human *h));
ORIG(0x4D03D0, int, Mesh3D_Exclude, (float *v, float radius, void *out));
ORIG(0x4D06C0, int, Mesh3D_ExcludeBone, (Human *h, float *out));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void *model));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D3210, void, BoneObj_GetEvents, (int *out, void *anim));
ORIG(0x4D38C0, void, BoneObj_Update, (float *matrix, void *anim));
ORIG(0x4D64C0, int, Anim_GetId, (void *anim, int track));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4D9700, void, BoneDynCubeObj_SetModel, (Human *h, void *model));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, double *pos));
ORIG(0x4E7200, void, Sound_PlayDefined, (const char *name, int a, void *b));
ORIG(0x4E7C00, void, DebugText_VPrintf, (const char *fmt, va_list ap));
ORIG(0x4E7C40, void *, DebugText_GetFont, (void));
ORIG(0x4E81F0, char *, Viewport_GetCurrentCamera, (void));
ORIG(0x4EC0E0, void, AnimTrigger_Fire, (int a, Human *h, double *pos));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F2310, void, Weapon_Move, (Task *w, double *pos, float *m));
ORIG(0x4F9720, void *, Graph_GetNode, (void *graph, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (double *out, void *node));
ORIG(0x5042B0, char, AreaActivate_Contains, (void *criteria, double *pos));
ORIG(0x504A90, void, AreaActivate_RemoveCriteria, (int criteria));
ORIG(0x504AC0, unsigned int, AreaActivate_GetMask, (void *criteria));
ORIG(0x462BA0, void, Human_AvoidHumans, (Human *h, double *pos, double *to, float *vel));

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern uint8_t gHumanEvent0;
extern uint8_t gHumanEvent1;
extern const char *gHumanTeamNames[];
extern uint16_t gHumanType;
extern int gHumanDebugTime;
extern int gHumanCriteria;
extern void *gHumanParams;
extern int gHumanDebug;
extern void *gHumanList;

static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kHalf = 0.5f;
static const float kTwo = 2.0f;
static const float kPi = 0x1.921fb6p+1f;
static const float kThird = 0x1.555556p-2f;
static const float kCrushSpeed = 50.0f;
static const float kCrushDamage = 10000.0f;
static const double kLabelHeight = 4096.0;
static const double kLabelRange = 184320.0;
static const float kTextZ = 4096.0f;
static const float kHudX = 0.0625f;
static const float kHudY = 0x1.caaaaap-1f;
static const float kGray = 128.0f;

static const double kHalfD = 0.5;
static const double kOneD = 1.0;
static const double kTwoD = 2.0;
static const double kQuarterPiD = 0x1.921fb6p-1;
static const double kTurnDiv = 0x1.0aaaaaaaaaaabp+4;
static const double kUnitsToMeters = 0x1p-12;
static const float kQuarterPi = 0x1.921fb6p-1f;
static const float kTurnDead = 0x1.eb851ep-5f;
static const float kUseRange = 6.0f;
static const float kNoiseRadius = 8192.0f;
static const float kRadToDeg = 0x1.ca5dcp+5f;
static const float kFriction = 0x1.ccccccp-1f;
static const float kSlope90 = 90.0f;
static const float kSlope80 = 80.0f;
static const float kSlope60 = 60.0f;
static const float kSlope50 = 50.0f;
static const float kSlope45 = 45.0f;
static const float kSlide80 = 1137.0f;
static const float kSlide60 = 948.0f;
static const float kSlide50 = 0x1.c71c72p+5f;
static const float kSlide45 = 0x1.2f684cp+4f;
static const float kStepDown = 2048.0f;
static const double kWallPad = 0x1.47ae147ae147bp+3;
static const double kWallLow = 0x1.999999999999ap+8;
static const double kWallMid = 0x1.999999999999ap+10;
static const double kWallMidCrouch = 0x1.999999999999ap+9;
static const double kWallHigh = 0x1.ccccccccccccdp+12;
static const double kWallHighCrouch = 0x1.4cccccccccccdp+12;
static const float kWallSteep = 0x1.99999ap-4f;
static const float kFloorUp = 0x1.fff2e4p-1f;
static const float kCeilingDown = -0x1.fff2e4p-1f;
static const double kZeroD = 0.0;
static const double kFarDown = -4096000.0;
static const double kFarUp = 4096000.0;
static const double kRoofProbe = 8192.0;
static const double kPathJump = 100.0;
static const double kStepUp = 0x1.999999999999ap+7;
static const double kStepProbe = 2048.0;
static const double kNoGround = 1073741824.0;
static const float kTwoOverPi = 0x1.45f306p-1f;
static const float kSlideDown = 8192.0f;

static TaskHandler Handler(uint8_t slot, uint16_t type)
{
    return gTaskHandlers[slot][type];
}

/* fistp with the current rounding mode, low 32 bits */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static long double x87_tan(long double x)
{
    long double r;

    __asm__("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"(x));
    return r;
}

/* x87 "equal" test: also true for NaN */
static int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

/* True when the camera looks through this human's eyes. */
static int Human_IsViewer(Human *h, char *view, char *cam)
{
    return *(char **)(cam + 0x54) == view && **(Human ***)(view + 0x68) == h && *(int *)(view + 0xE8) == 1;
}

/* Draws the human (and the weapon in its hand) unless the camera looks
   through its own eyes. */
void Human_NearClipHandler(Human *h, void *arg)
{
    char *view = *(char **)(Game_Get() + 0x5C);
    char *cam = Viewport_GetCurrentCamera();
    HumanBoneMsg msg;
    Task *w;
    uint8_t ev;

    if (Human_IsViewer(h, view, cam))
        return;
    Human_WakeIfVisible(h);
    ev = QCube_GetNearClipEvent();
    Handler(ev, BoneDynCubeObj_GetType())(h, arg);
    w = WeaponSlots_Get(h->weapons, 0);
    if (!w)
        return;
    msg.firstPerson = 0;
    Handler(Gun_GetEvent2(), h->task.type)(h, &msg);
    Weapon_Move(w, msg.pos, msg.matrix);
    ev = QCube_GetNearClipEvent();
    Handler(ev, RigidDynCubeObj_GetType())(w, arg);
}
HOOK(0x45F050, Human_NearClipHandler);

void Human_WakeIfVisible(Human *h)
{
    float v[3], c[3];
    long double dx, dy, dz;
    char out[4];
    int lod;

    if (!h->frozen)
        return;
    Viewport_GetCurrentCamera();
    dx = h->pos[0] - gTransContext.pos.x;
    dy = h->pos[1] - gTransContext.pos.y;
    dz = h->pos[2] - gTransContext.pos.z;
    v[0] = gTransContext.matrix.m[2] * dz + gTransContext.matrix.m[1] * dy + gTransContext.matrix.m[0] * dx;
    v[1] = gTransContext.matrix.m[5] * dz + gTransContext.matrix.m[4] * dy + gTransContext.matrix.m[3] * dx;
    v[2] = gTransContext.matrix.m[8] * dz + gTransContext.matrix.m[7] * dy + gTransContext.matrix.m[6] * dx;
    c[0] = v[0];
    c[1] = v[1];
    c[2] = v[2];
    if (Mesh3D_Exclude(c, h->radius, out))
        return;
    lod = Mesh3D_GetBoneMeshLOD(h);
    if (lod != -1 && lod < 4)
        Human_Wake(h);
}
HOOK(0x45F140, Human_WakeIfVisible);

void Human_Wake(Human *h)
{
    if (!h->frozen)
        return;
    h->frozen = 0;
    BoneObj_Update(h->matrix.m, h->anim);
    Human_ApplyLook(h, Anim_GetId(h->anim, 0));
}
HOOK(0x45F240, Human_Wake);

/* Draw: the body, the weapon, then the debug text lines above the head. */
void Human_DrawHandler(Human *h, void *arg)
{
    char *view = *(char **)(Game_Get() + 0x5C);
    char *cam = Viewport_GetCurrentCamera();
    HumanBoneMsg msg;
    HumanText **t;
    char *line;
    double p[3];
    float scr[2], depth, unk = 0;
    int i, a, ix, iy;
    char mode;
    Task *w;

    if (h->unk74 && !Mesh3D_ExcludeBone(h, &unk))
        h->unk7C4 = Flow_GetRenders();
    if (Human_IsViewer(h, view, cam))
        return;
    Human_WakeIfVisible(h);
    gTaskHandlers[4][BoneDynCubeObj_GetType()](h, arg);
    w = WeaponSlots_Get(h->weapons, 0);
    if (w) {
        msg.firstPerson = 0;
        Handler(Gun_GetEvent2(), h->task.type)(h, &msg);
        Weapon_Move(w, msg.pos, msg.matrix);
        gTaskHandlers[4][RigidDynCubeObj_GetType()](w, 0);
    }
    memcpy(p, h->pos, sizeof(p));
    p[2] = p[2] + kLabelHeight;
    TransContext_Project(scr, &depth, p);
    if (!(depth > kZero) || !(depth < kLabelRange))
        return;
    t = h->texts;
    if (*t) {
        a = Text_GetHeight(*t) * 32 / 2;
        scr[0] = scr[0] - (long double)a;
        a = Text_GetHeight(*t) * 32 / 2;
        scr[1] = scr[1] - (long double)a;
    }
    line = h->lines[0];
    for (i = 0; i < 32; i++, t++, line += sizeof(h->lines[0])) {
        if (*t) {
            Text_SetString(*t, line);
            iy = x87_fistp(scr[1]);
            line[0] = 0;
            ix = x87_fistp(scr[0]);
            (*t)->x = ix;
            (*t)->y = iy;
            (*t)->flags |= 0x40000;
            (*t)->z = kTextZ;
            Text_DrawAlt(*t);
            scr[1] = (long double)Text_GetHeight(*t) + scr[1];
        } else if (DebugText_GetFont()) {
            mode = Mem_GetMode();
            if (!mode)
                Mem_SetDefaultMode();
            *t = Text_New(DebugText_GetFont(), 0, 0, (const char *)0x567C74);
            if (!mode)
                Mem_ClearMode();
        }
    }
}
HOOK(0x45F280, Human_DrawHandler);

void Human_CollLinesHandler(Human *h, CollLinesMsg *msg)
{
    double *starts = msg->starts, *ends = msg->ends;
    TaskHandler f;

    if (msg->filter != 0xFF && (f = Handler(msg->filter, h->task.type))) {
        f(h, msg->result);
        if (!*msg->result)
            return;
    }
    if (Human_LinesHitBounds(h, starts, ends, msg->count)) {
        Human_Wake(h);
        gTaskHandlers[9][BoneDynCubeObj_GetType()](h, msg);
    }
}
HOOK(0x45F500, Human_CollLinesHandler);

/* True when a segment passes within radius of the human and its bounding
   box overlaps the cube of that radius. */
int Human_LinesHitBounds(Human *h, const double *s, const double *e, int count)
{
    float r = h->radius;
    float a[3], b[3], d[3], n[3], lo1, lo2, hi[3], dot, nr;
    long double lo0, len2, dd, dist;
    int i;

    for (i = 0; i < count; i++, s += 3, e += 3) {
        a[0] = s[0] - h->pos[0];
        a[1] = s[1] - h->pos[1];
        a[2] = s[2] - h->pos[2];
        b[0] = e[0] - h->pos[0];
        b[1] = e[1] - h->pos[1];
        b[2] = e[2] - h->pos[2];
        lo0 = !(a[0] >= b[0]) ? a[0] : b[0];
        lo1 = !(a[1] >= b[1]) ? a[1] : b[1];
        lo2 = !(a[2] >= b[2]) ? a[2] : b[2];
        hi[0] = a[0] > b[0] ? a[0] : b[0];
        hi[1] = a[1] > b[1] ? a[1] : b[1];
        hi[2] = a[2] > b[2] ? a[2] : b[2];
        d[0] = e[0] - s[0];
        d[1] = e[1] - s[1];
        d[2] = e[2] - s[2];
        n[0] = -a[0];
        n[1] = -a[1];
        n[2] = -a[2];
        dot = n[2] * d[2] + n[1] * d[1] + n[0] * d[0];
        len2 = (long double)n[0] * n[0] + (long double)n[1] * n[1] + (long double)n[2] * n[2];
        dd = (long double)d[0] * d[0] + (long double)d[1] * d[1] + (long double)d[2] * d[2];
        dist = len2 - (long double)dot * dot * (kOne / dd);
        if (!((long double)r * r >= dist))
            continue;
        if (lo0 > r || lo1 > r || lo2 > r)
            continue;
        nr = -r;
        if (!(hi[0] >= nr) || !(hi[1] >= nr))
            continue;
        if (hi[2] >= nr)
            return 1;
    }
    return 0;
}
HOOK(0x45F580, Human_LinesHitBounds);

void Human_CollLineHandler(Human *h, CollLineMsg *msg)
{
    double *start = msg->start, *end = msg->end;
    TaskHandler f;

    if (msg->filter != 0xFF && (f = Handler(msg->filter, h->task.type))) {
        f(h, msg->result);
        if (!*msg->result)
            return;
    }
    if (Human_LinesHitBounds(h, start, end, 1)) {
        Human_Wake(h);
        gTaskHandlers[8][BoneDynCubeObj_GetType()](h, msg);
    }
}
HOOK(0x45F800, Human_CollLineHandler);

void Human_GetParamTableHandler(Human *h, void **out)
{
    *out = gHumanParams;
}
HOOK(0x45F880, Human_GetParamTableHandler);

void Human_ParamChangedHandler(Human *h, void *arg)
{
    void *model = VirModel_Find(h->modelName);

    if (model) {
        BoneDynCubeObj_SetModel(h, model);
        BoneObj_Update(h->matrix.m, h->anim);
    }
    DynCube_Move(Game_GetWorld(), h, h->placePos, h->radius);
    h->baseZ = h->yaw;
    h->rotZ = h->yaw;
    Matrix_SetRotZ(h->matrix.m, h->yaw);
}
HOOK(0x45F890, Human_ParamChangedHandler);

void Human_GetWeaponsHandler(Human *h, void ***out)
{
    *out = (void **)h->weapons;
}
HOOK(0x45F900, Human_GetWeaponsHandler);

void Human_GetAmmoHandler(Human *h, void **out)
{
    *out = h->ammo;
}
HOOK(0x45F910, Human_GetAmmoHandler);

/* Shared by many task types: answers "no". */
void Task_FalseHandler(void *task, char *out)
{
    *out = 0;
}
HOOK(0x45F920, Task_FalseHandler);

static void Human_AddDamage(Human *h, long double x)
{
    long double t = x + h->damage.total;
    uint32_t bits;

    h->damage.total = t;
    bits = h->sm.bits;
    if (t >= h->damage.max) {
        bits |= HUMAN_DEAD;
        h->dead = 1;
    }
    h->sm.bits = bits;
}

/* msg: {unk, damage, dir[3], ...} */
void Human_HitHandler(Human *h, int *msg)
{
    HumanDamage *d = &h->damage;
    long double x;
    int zone;

    Human_Bleed(h, msg + 2, msg[0]);
    if (Human_IsPlayer(h) && PlayerXPHit())
        return;
    if (!h->vulnerable)
        return;
    zone = Human_GetHitZone(h, msg);
    d->hitZone = zone;
    HumanDamage_GetZone(d, zone);
    x = (long double)d->part[zone].mult * ((float *)msg)[1];
    d->part[zone].dmg = x + d->part[zone].dmg;
    Human_AddDamage(h, x);
}
HOOK(0x45F930, Human_HitHandler);

void Human_BlastHandler(Human *h, void *msg)
{
    HumanDamage *d = &h->damage;
    long double x;

    if (Human_IsPlayer(h) && PlayerXPHit())
        return;
    if (!h->vulnerable)
        return;
    d->hitZone = 13;
    HumanDamage_GetZone(d, 13);
    x = Game_BlastFalloff(msg, h, h->pos, h->radius * kThird, 1);
    if (is_zero(x))
        return;
    x = x * d->part[13].mult;
    d->part[13].dmg = x + d->part[13].dmg;
    Human_AddDamage(h, x);
}
HOOK(0x45F9D0, Human_BlastHandler);

/* msg: {object, velocity}. An object moving at 50 or more that reaches the
   human kills it. */
void Human_CrushHandler(Human *h, void **msg)
{
    double *p = DynCubeObj_GetPosition(h);
    char *obj = msg[0];
    double a[3], b[3], d[3], lim;
    char active = h->vulnerable;
    CollHit hit;
    CollLinesMsg q;
    float *s, *v, m, dmg;

    memcpy(a, p, sizeof(a));
    memcpy(b, obj + 0x20, sizeof(b));
    d[0] = b[0] - a[0];
    d[1] = b[1] - a[1];
    d[2] = b[2] - a[2];
    if (!active)
        return;
    s = (float *)(obj + 0x98);
    m = !(s[2] >= s[1]) ? s[1] : s[2];
    if (s[0] > m)
        m = s[0];
    lim = VirModel_GetRadius(*(void **)(obj + 0x6C)) * m;
    if (Vec3d_Length(d) > lim)
        return;
    q.starts = a;
    q.ends = b;
    q.count = 1;
    q.filter = 0xFF;
    q.result = 0;
    q.unk14 = 0;
    q.unk18 = 0;
    q.magic = 0x41424344;
    q.hits = &hit;
    hit.found = 0;
    hit.material = 0;
    hit.back = 0;
    gTaskHandlers[9][RigidDynCubeObj_GetType()](msg[0], &q);
    if (hit.found && !hit.back)
        return;
    v = msg[1];
    dmg = 0;
    if (__builtin_sqrtl((long double)v[1] * v[1] + (long double)v[2] * v[2] + (long double)v[0] * v[0]) >= kCrushSpeed)
        dmg = kCrushDamage;
    h->damage.hitZone = 13;
    if (Human_IsPlayer(h) && PlayerXPHit())
        return;
    if (h->ai && h->ai->invulnerable)
        return;
    h->damage.part[13].dmg = dmg + h->damage.part[13].dmg;
    Human_AddDamage(h, dmg);
}
HOOK(0x45FA80, Human_CrushHandler);

void Human_GetHumanCameraInfoHandler(Human *h, HumanCameraInfo *info)
{
    HumanView *v = h->view;

    HumanView_GetEye(&info->pos.x, v);
    HumanView_GetMatrix(info->matrix.m, h->view);
    info->fov = v->fovX;
    memcpy(&info->body, h->matrix.m, sizeof(info->body));
    info->unk6C = v->fovY;
    info->mode = h->camMode;
    if (v->camOverride) {
        memcpy(&info->pos, v->camPos, sizeof(info->pos));
        memcpy(&info->matrix, &v->camRot, sizeof(info->matrix));
        info->mode = 4;
    }
    if (!is_zero(h->input[19]) && is_zero(h->prevInput[19]))
        h->camToggle ^= 1;
    if (h->camToggle)
        info->mode = 2;
}
HOOK(0x45FCD0, Human_GetHumanCameraInfoHandler);

/* Passes the AI event on to the children, unless this human sent it. */
void Human_AIEventHandler(Human *h, Human **msg)
{
    Task *t, *n;
    TaskHandler f;

    if (h == msg[5] && h == msg[6])
        return;
    t = h->task.head;
    if (!t->next)
        return;
    while (t) {
        n = t->next;
        gTaskStack[gTaskStackTop++] = n && n->next ? n : 0;
        f = Handler(HumanAI_GetEventSlot(), t->type);
        if (f)
            f(t, msg);
        t = gTaskStack[--gTaskStackTop];
    }
}
HOOK(0x45FDA0, Human_AIEventHandler);

void Human_DetectInfoHandler(Human *h, int *msg)
{
    msg[0] = Human_GetMaskType();
    msg[8] = h->team;
    ((char *)msg)[0x24] = !h->dead;
    ((char *)msg)[0x25] = h->vulnerable;
    Human_GetEyePos(msg + 2, h);
}
HOOK(0x45FE30, Human_DetectInfoHandler);

/* Keeps the nearest team mate in front of msg->pos (distance ignores height). */
typedef struct HumanNearMsg {
    Human *who;
    double *pos;
    float *matrix;
    int unkC;
    double best;
    double local[3];
    int team;
} HumanNearMsg;

void Human_NearestAllyHandler(Human *h, HumanNearMsg *msg)
{
    long double dx, dy, dz, len;
    double l[3], v[3];
    float *m;

    if (msg->who == h || msg->team != h->team)
        return;
    dx = h->pos[0] - msg->pos[0];
    dy = h->pos[1] - msg->pos[1];
    dz = h->pos[2] - msg->pos[2];
    m = msg->matrix;
    l[0] = m[6] * dz + m[3] * dy + m[0] * dx;
    l[1] = m[7] * dz + m[4] * dy + m[1] * dx;
    l[2] = m[8] * dz + m[5] * dy + m[2] * dx;
    v[0] = l[0];
    v[1] = 0;
    v[2] = l[2];
    len = Vec3d_Length(v);
    if (!(msg->best >= 0.0) || !(len >= msg->best)) {
        msg->best = len;
        msg->local[0] = l[0];
        msg->local[1] = l[1];
        msg->local[2] = l[2];
    }
}
HOOK(0x45FE70, Human_NearestAllyHandler);

/* msg: {ammo id, count} */
void WeaponAmmoPickup(Human *h, int *msg)
{
    if (h->dead)
        return;
    AmmoList_Add(h->ammo, msg[0], msg[1]);
    Sound_PlayDefined("weaponpickup_1", 0, 0);
}
HOOK(0x45FF80, WeaponAmmoPickup);

/* msg: {weapon id, ammo}. Adds ammo when the weapon is already carried,
   else creates it. */
void WeaponGunPickup(Human *h, int *msg)
{
    Task *t;
    uint16_t type;
    int *id, *ammo;

    if (h->dead)
        return;
    t = h->task.head;
    if (t->next) {
        for (;;) {
            type = Gun_GetType();
            if (TaskType_IsA(t->type, type)) {
                id = *(int **)((char *)t + 0x128);
                if (id && *id == msg[0]) {
                    ammo = *(int **)((char *)t + 0x12C);
                    if (ammo)
                        AmmoList_Add(h->ammo, *ammo, msg[1]);
                    goto sound;
                }
            }
            t = t->next;
            if (!t || !t->next)
                break;
        }
    }
    Gun_New(h, 0, msg[0], msg[1], 1);
sound:
    Sound_PlayDefined("weaponpickup_1", 0, 0);
}
HOOK(0x45FFC0, WeaponGunPickup);

Task *Human_GetWeaponHandler(Human *h)
{
    return WeaponSlots_Get(h->weapons, 0);
}
HOOK(0x460070, Human_GetWeaponHandler);

void Human_FirstPersonHandler(Human *h, int *msg)
{
    if (*msg && h->view->inWorld) {
        *msg = 1;
        return;
    }
    *msg = 0;
}
HOOK(0x460090, Human_FirstPersonHandler);

/* Weapon position: the hand of the view model in first person, else the
   hand bone of the body (then firstPerson is cleared). */
void Human_GetWeaponBoneHandler(Human *h, HumanBoneMsg *msg)
{
    HumanView *v = h->view;
    float *b;
    float x, y, z;

    if (msg->firstPerson && v->inWorld) {
        b = (float *)((char *)v->motion + 0x1A4);
        x = b[0];
        y = b[1];
        z = b[2];
        msg->pos[0] = x + v->pos[0];
        msg->pos[1] = y + v->pos[1];
        msg->pos[2] = z + v->pos[2];
        memcpy(msg->matrix, (char *)v->motion + 0x1B0, sizeof(msg->matrix));
        return;
    }
    b = (float *)((char *)h->motion + 0xB70);
    x = b[0];
    y = b[1];
    z = b[2];
    msg->pos[0] = x + h->pos[0];
    msg->pos[1] = y + h->pos[1];
    msg->pos[2] = z + h->pos[2];
    memcpy(msg->matrix, (char *)h->motion + 0xB7C, sizeof(msg->matrix));
    msg->firstPerson = 0;
}
HOOK(0x4600C0, Human_GetWeaponBoneHandler);

static void Human_GetViewBone(Human *h, double *out, int bone)
{
    HumanView *v = h->view;
    float *b;
    float x, y, z;

    if (!v->inWorld)
        return;
    b = (float *)((char *)v->motion + bone);
    x = b[0];
    y = b[1];
    z = b[2];
    out[0] = x + v->pos[0];
    out[1] = y + v->pos[1];
    out[2] = z + v->pos[2];
    memcpy(out + 3, (char *)v->motion + bone + 0xC, 40);
}

void Human_GetViewBarrelHandler(Human *h, double *out)
{
    Human_GetViewBone(h, out, 0x1AE8);
}
HOOK(0x4601A0, Human_GetViewBarrelHandler);

void Human_GetViewPartHandler(Human *h, double *out)
{
    Human_GetViewBone(h, out, 0x1C74);
}
HOOK(0x460210, Human_GetViewPartHandler);

typedef struct HumanSeeQuery {
    double *pos;
    float *matrix;
    int unk8;
    double *targetPos;
    float *targetMatrix;
    float radius;
    int unk18;
    int unk1C;
    int kind;
} HumanSeeQuery;

/* A watcher at q->pos asks whether it sees this human. */
void Human_MapMarkerHandler(Human *h, HumanSeeQuery *q)
{
    struct {
        float unk0;
        int unk4;
        int unk8;
        Human *h;
    } filter;
    float d[3], l[3], vis;
    float *m;
    void *world;
    uint8_t ev;

    if (!ComputerMap_GetZoom() || ComputerMap_GetZoom() == 3)
        return;
    q->targetPos = h->pos;
    q->targetMatrix = h->matrix.m;
    q->unk1C = 1;
    q->radius = h->radius;
    q->kind = h->team == 0 || h->team == 2 ? 3 : 0;
    if (!h->vulnerable)
        return;
    d[0] = h->pos[0] - q->pos[0];
    d[1] = h->pos[1] - q->pos[1];
    d[2] = h->pos[2] - q->pos[2];
    m = q->matrix;
    l[0] = d[2] * m[6] + d[1] * m[3] + d[0] * m[0];
    l[1] = d[2] * m[7] + d[1] * m[4] + d[0] * m[1];
    l[2] = d[2] * m[8] + d[1] * m[5] + d[0] * m[2];
    if (Mesh3D_Exclude(l, q->radius, 0))
        return;
    filter.unk0 = l[0];
    filter.unk4 = 0;
    filter.unk8 = 0;
    filter.h = h;
    vis = 0;
    ev = AIDetection_GetEvent0();
    world = Game_GetWorld();
    Coll_LineOfSight(world, q->pos, q->targetPos, ev, &filter, -1, -1, &vis);
    if (!is_zero(vis))
        ComputerObject_DrawMarker(q);
}
HOOK(0x460280, Human_MapMarkerHandler);

/* HUD: the weapon's and the attached object's overlays, then the health bar. */
void Human_RenderHandler(Human *h, void **msg)
{
    char *hud = msg[0];
    char *view = *(char **)(Game_Get() + 0x5C);
    int width = Display_GetMode()[1];
    int height = Display_GetMode()[2];
    void *s = hud + 0x8C, *bar = hud + 0xA4, *local;
    float hp, x0, y0, x1, y1, xa, ya, xb, yb, t;
    long double f;
    TaskHandler fn;
    uint16_t type;
    Task *w;

    if (*(int *)(view + 0xE8) != 1)
        return;
    w = WeaponSlots_Get(h->weapons, 0);
    if (w && (fn = Handler(StatusScreen_GetEvent(), w->type)))
        fn(w, msg);
    if (!h->unk7B8 && Human_GetAttached(h)) {
        local = msg[0];
        type = Human_GetAttached(h)->type;
        fn = Handler(Game_GetEvent6(), type);
        if (fn)
            fn(Human_GetAttached(h), &local);
    } else {
        StatusScreen_ResetBlink(msg[0]);
    }
    f = kOne - h->damage.total / h->damage.max;
    hp = f;
    if (!(f >= kZero))
        hp = 0;
    x0 = (long double)width * kHudX;
    x1 = x0 - (long double)Sprite_Width(s) * kHalf;
    y0 = (long double)height * kHudY;
    y1 = y0 - (long double)Sprite_Height(s) * kHalf;
    Draw_Sprite(s, x1, y1, kGray, kGray, kGray, 0, 0, 0);
    if (is_zero(hp))
        return;
    x1 = x0 - kTwo;
    y1 = y0 - kTwo;
    xa = x1 - (long double)Sprite_Width(bar) * kHalf;
    ya = y1 - (long double)Sprite_Height(bar) * kHalf;
    xb = (long double)Sprite_Width(bar) * kHalf + x1;
    yb = (long double)Sprite_Height(bar) * kHalf + y1;
    t = kOne - hp;
    ya = (long double)Sprite_Height(bar) * t + ya;
    QVertex_DrawQuadUV(bar, xa, ya, xb, ya, xa, yb, xb, yb, 0, t, kOne, t, 0, kOne, kOne, kOne, kGray, kGray, kGray, 0,
                       kOne, 0, 0x80000, 0);
}
HOOK(0x460400, Human_RenderHandler);

void Human_GetSoundInfoHandler(Human *h, void **out)
{
    out[0] = h->pos;
    out[1] = h->matrix.m;
    out[2] = 0;
}
HOOK(0x4606D0, Human_GetSoundInfoHandler);

typedef struct GunshotMsg {
    void *shot;
    int unk4;
    Task **shooter;
} GunshotMsg;

/* A shot passed by: the player hears it whiz past, AI hears a noise. */
void Human_GunshotHandler(Human *h, GunshotMsg *msg)
{
    double pos[3], pts[32][3];
    AIMessage noise;
    Task *a;
    void *snd;
    int team, n, i;

    if (msg->shooter) {
        a = *msg->shooter;
        team = 1;
        if (a == &h->task)
            return;
        if (a && TaskType_IsA(a->type, Human_GetType()))
            team = ((Human *)a)->team;
        if (h->team == team)
            return;
    }
    memcpy(pos, h->pos, sizeof(pos));
    n = GunShot_FindNear(pts, 32, msg->shot, pos);
    for (i = 0; i < n; i++) {
        if (Human_IsPlayer(h)) {
            if (Flow_GetFrames() > h->missSoundTime) {
                snd = Sound_Play(h, "gunshotmiss");
                if (snd)
                    Sound_SetPos(snd, pts[i]);
                h->missSoundTime = Flow_GetFrames() + 5;
            }
            continue;
        }
        noise.type = 0x14;
        noise.team = 1;
        noise.flags = 0x46;
        noise.radius = kNoiseRadius;
        noise.pos = (Vec3d *)pts[i];
        noise.from = msg->shot;
        if (!msg->shooter) {
            noise.about = 0;
        } else {
            noise.about = *msg->shooter;
            if (noise.about)
                noise.team = Human_GetTeam(noise.about);
        }
        HumanAI_Broadcast(&noise);
    }
}
HOOK(0x4606F0, Human_GunshotHandler);

void Human_RegisterVarsHandler(Human *h)
{
    char name[0x40], buf[0x80];

    if (h->scriptVars)
        return;
    h->scriptVars = 1;
    Task_GetName(name, h);
    crt_sprintf(buf, "%s.isDead", name);
    Script_RegisterBool8(buf, &h->dead);
    crt_sprintf(buf, "%s.eTeam", name);
    Script_RegisterInt2(buf, &h->team);
}
HOOK(0x460850, Human_RegisterVarsHandler);

void Human_UnregisterVarsHandler(Human *h)
{
    char name[0x40], buf[0x80];

    if (!h->scriptVars)
        return;
    Task_GetName(name, h);
    crt_sprintf(buf, "%s.isDead", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.eTeam", name);
    Script_RemoveSymbol(buf);
    h->scriptVars = 0;
}
HOOK(0x4608D0, Human_UnregisterVarsHandler);

void Human_ActivateHandler(Human *h, void *arg)
{
    uint8_t ev;

    if (h->vulnerable)
        return;
    h->vulnerable = 1;
    ev = World_GetEvent0();
    Handler(ev, BoneDynCubeObj_GetType())(h, arg);
}
HOOK(0x460950, Human_ActivateHandler);

void Human_DeactivateHandler(Human *h, void *arg)
{
    uint8_t ev;

    if (!h->vulnerable)
        return;
    h->vulnerable = 0;
    ev = World_GetEvent1();
    Handler(ev, BoneDynCubeObj_GetType())(h, arg);
}
HOOK(0x4609A0, Human_DeactivateHandler);

void Human_MapSelectHandler(Human *h, char *out)
{
    *out = h->team == 1;
}
HOOK(0x4609F0, Human_MapSelectHandler);

static Human *Human_ListItem(int i)
{
    return ((Human **)((char *)Human_GetList() + 8))[i];
}

/* CRITERIA_HUMAN<team>: counts humans of matching teams inside the area. */
void Human_CountTeamCriteria(void **msg)
{
    int n = *(int *)Human_GetList();
    unsigned int mask = AreaActivate_GetMask(msg[0]);
    Human *t;
    int i;

    for (i = 0; i < n; i++) {
        t = Human_ListItem(i);
        if ((unsigned int)gHumanTeamCriteria[t->team] & mask && AreaActivate_Contains(msg[0], t->pos))
            ((int *)msg)[1]++;
    }
}
HOOK(0x460A10, Human_CountTeamCriteria);

/* CRITERIA_HUMAN: counts humans inside the area. */
void Human_CountCriteria(void **msg)
{
    int n = *(int *)Human_GetList();
    int i;

    for (i = 0; i < n; i++)
        if (AreaActivate_Contains(msg[0], Human_ListItem(i)->pos))
            ((int *)msg)[1]++;
}
HOOK(0x460A70, Human_CountCriteria);

void Human_PreDeleteHandler(Human *h, void *arg)
{
    struct {
        int unk0;
        void *world;
        int unk8;
    } m;

    if (h->vulnerable)
        return;
    m.unk0 = 0;
    m.world = Game_GetWorld();
    m.unk8 = 2;
    Handler(World_GetEvent0(), h->task.type)(h, &m);
}
HOOK(0x460AC0, Human_PreDeleteHandler);

void Human_Close(void)
{
    int i;

    QTaskList_Free(gHumanList);
    QTaskList_Free(gHumanParams);
    if (gHumanEvent0 != 0xFF)
        TaskEvent_Free(gHumanEvent0);
    if (gHumanEvent1 != 0xFF)
        TaskEvent_Free(gHumanEvent1);
    TaskEvent_Free(gHumanGunshotEvent);
    TaskEvent_Free(gHumanPunchEvent);
    TaskType_Free(gHumanType);
    AreaActivate_RemoveCriteria(gHumanCriteria);
    for (i = 0; i < 8; i++)
        AreaActivate_RemoveCriteria(gHumanTeamCriteria[i]);
    for (i = 0; i < 2; i++)
        Script_UnregisterSymbol(gHumanTeamNames[i]);
    Task_ClearGlobalFlags(1);
}
HOOK(0x460B20, Human_Close);

uint16_t Human_GetType(void)
{
    return gHumanType;
}
HOOK(0x460BE0, Human_GetType);

uint8_t Human_GetEvent0(void)
{
    if (gHumanEvent0 == 0xFF)
        gHumanEvent0 = TaskEvent_Alloc(0);
    return gHumanEvent0;
}
HOOK(0x460BF0, Human_GetEvent0);

uint8_t Human_GetEvent1(void)
{
    if (gHumanEvent1 == 0xFF)
        gHumanEvent1 = TaskEvent_Alloc(0);
    return gHumanEvent1;
}
HOOK(0x460C10, Human_GetEvent1);

uint8_t Human_GetGunshotEvent(void)
{
    return gHumanGunshotEvent;
}
HOOK(0x460C30, Human_GetGunshotEvent);

uint8_t Human_GetPunchEvent(void)
{
    return gHumanPunchEvent;
}
HOOK(0x460C40, Human_GetPunchEvent);

void *Human_GetParamTable(void)
{
    return gHumanParams;
}
HOOK(0x460C50, Human_GetParamTable);

void *Human_GetList(void)
{
    return gHumanList;
}
HOOK(0x460C60, Human_GetList);

void Human_SetAI(Human *h, HumanAI *ai)
{
    h->ai = ai;
}
HOOK(0x460C70, Human_SetAI);

/* Debug text for the player, toggled with cheat key 0x44 (once per frame). */
void Human_printf(Human *h, const char *fmt, ...)
{
    va_list ap;

    if (!Human_IsPlayer(h))
        return;
    if (Game_CheatKeyPressed(0x44) && gHumanDebugTime != Flow_GetRenders()) {
        gHumanDebug ^= 1;
        gHumanDebugTime = Flow_GetRenders();
    }
    if (!gHumanDebug)
        return;
    va_start(ap, fmt);
    DebugText_VPrintf(fmt, ap);
    va_end(ap);
}
HOOK(0x460C80, Human_printf);

void Human_MarkDelete(Human *h)
{
    h->deleteMe = 1;
}
HOOK(0x460CE0, Human_MarkDelete);

typedef struct HumanCarrierMsg {
    double *pos;
    float *matrix;
    double *offset;
} HumanCarrierMsg;

/* Works out how far the object the human stands on moved it this frame:
   movePos, moveMatrix and moveAngle. */
void Human_UpdatePlatformMove(Human *h)
{
    HumanCarrierMsg q;
    Task *obj = h->attach;
    long double dx, r0, x;
    double dy, dz, r1, r2, y, z;
    float *m;
    int i;

    h->moveAngle[1] = 0;
    h->moveAngle[0] = 0;
    for (i = 0; i < 3; i++)
        h->movePos[i] = 0;
    for (i = 0; i < 10; i++)
        ((float *)&h->moveMatrix)[i] = 0;
    h->moveAngle[2] = 0;
    if (!obj && !(obj = Human_GetGround(h)))
        return;
    if (!Handler(Game_GetEvent14(), obj->type))
        return;
    Handler(Game_GetEvent14(), obj->type)(obj, &q);
    if (!q.pos)
        return;
    m = q.matrix;
    dx = q.pos[0] - h->pos[0];
    dy = q.pos[1] - h->pos[1];
    dz = q.pos[2] - h->pos[2];
    r0 = m[2] * dz + m[1] * dy + m[0] * dx;
    r1 = m[5] * dz + m[4] * dy + m[3] * dx;
    r2 = m[8] * dz + m[7] * dy + m[6] * dx;
    x = dx - r0;
    y = dy - r1;
    z = dz - r2;
    x = x + q.offset[0];
    y = y + q.offset[1];
    z = z + q.offset[2];
    h->movePos[0] = x;
    h->movePos[1] = y;
    h->movePos[2] = z;
    memcpy(&h->moveMatrix, m, sizeof(h->moveMatrix));
    Matrix_GetEuler(m, &h->moveAngle[0], &h->moveAngle[1], &h->moveAngle[2]);
}
HOOK(0x460CF0, Human_UpdatePlatformMove);

typedef struct HumanUseQuery {
    Human *h;
    int unk4;
    double pos[3];
    float matrix[9];
    int unk44;
    float bodyMatrix[10];
    float unk70;
    float unk74;
    int unk78;
    float range;
    int found;
} HumanUseQuery;

typedef struct HumanControlMsg {
    Human *h;
    int unk4;
    double pos[3];
    HumanAnimEvent animEvent;
    float *input;
    float *prevInput;
    int count;
} HumanControlMsg;

static Task *Game_TaskListItem(int *list, int i)
{
    return ((Task **)(list + 2))[i];
}

void Human_UpdateUse(Human *h)
{
    HumanUseQuery q;
    HumanControlMsg c;
    TaskHandler fn;
    uint16_t type;
    Task *t;
    int i;

    Human_SetAttached(h, 0);
    if (!(h->sm.bits & HUMAN_ON_GROUND))
        return;
    if (!h->unk7B8) {
        q.h = h;
        HumanView_GetEye(q.pos, h->view);
        HumanView_GetMatrix(q.matrix, h->view);
        memcpy(q.bodyMatrix, h->matrix.m, sizeof(q.bodyMatrix));
        q.unk70 = q.unk74 = Human_IsPlayer(h) ? kQuarterPi : kPi;
        q.unk78 = 0;
        q.range = kUseRange;
        q.found = 0;
        i = 0;
        if (*Game_GetTaskList0() > 0) {
            do {
                type = Game_TaskListItem(Game_GetTaskList0(), i)->type;
                fn = Handler(Game_GetEvent2(), type);
                fn(Game_TaskListItem(Game_GetTaskList0(), i), &q);
                i++;
            } while (i < *Game_GetTaskList0());
        }
        Human_SetAttached(h, q.found);
    }
    if (!h->climbRef || !(t = *h->climbRef))
        return;
    c.h = h;
    memcpy(c.pos, h->pos, sizeof(c.pos));
    c.animEvent = h->animEvent;
    c.input = h->input;
    c.prevInput = h->prevInput;
    c.count = h->unk7BC;
    Handler(Game_GetEvent4(), t->type)(t, &c);
    h->unk7BC++;
}
HOOK(0x460EE0, Human_UpdateUse);

/* Turns the body: follows the object it is attached to, else turns by the
   turn input (scaled down near zero) plus the carrier's turn. */
void Human_UpdateBody(Human *h)
{
    HumanView *v = h->view;
    Task *obj = h->attach;
    long double x, ax, s;
    float r[10], a, ex, ey, ez;
    float *b, *m;

    if (obj) {
        m = h->attachMatrix.m;
        b = (float *)((char *)obj + 0x70);
        r[0] = m[6] * b[2] + b[1] * m[3] + m[0] * b[0];
        r[1] = m[7] * b[2] + b[1] * m[4] + m[1] * b[0];
        r[2] = m[8] * b[2] + b[1] * m[5] + m[2] * b[0];
        r[3] = m[0] * b[3] + m[6] * b[5] + m[3] * b[4];
        r[4] = m[1] * b[3] + m[7] * b[5] + m[4] * b[4];
        r[5] = m[2] * b[3] + m[8] * b[5] + m[5] * b[4];
        r[6] = m[6] * b[8] + m[3] * b[7] + m[0] * b[6];
        r[7] = m[7] * b[8] + m[4] * b[7] + m[1] * b[6];
        r[8] = m[8] * b[8] + m[5] * b[7] + m[2] * b[6];
        ((int *)r)[9] = h->attachMatrix.count + *(int *)((char *)obj + 0x94) + 1;
        Matrix_GetEuler(r, &ex, &ey, &ez);
        h->baseX = ex;
        h->rotX = ex;
        h->baseY = ey;
        h->rotY = ey;
        h->baseZ = ez;
        h->rotZ = ez + v->recoilYaw;
    } else {
        s = x87_tan(kQuarterPiD);
        x = h->input[0];
        if (!(s < v->fovX || s > v->fovX))
            x = (x + h->prevInput[0]) * kHalfD;
        if (h->frozenView)
            x = kZero;
        h->baseX = 0;
        h->rotX = 0;
        h->baseY = 0;
        h->rotY = 0;
        a = -(x * kPi) * v->fovX;
        ax = !(x >= kZero) ? -x : x;
        if (!(ax >= kTurnDead))
            a = a / (kTwoD - ax * kTurnDiv);
        s = (a + h->moveAngle[2]) + h->baseZ;
        h->baseZ = s;
        h->rotZ = s + v->recoilYaw;
    }
    Matrix_SetEuler(h->matrix.m, h->rotX, h->rotY, h->rotZ);
}
HOOK(0x4610C0, Human_UpdateBody);

/* Plays the sound an animation event asks for at its point on the body. */
void Human_FireAnimTrigger(Human *h)
{
    HumanAnimEvent *e = &h->animEvent;
    long double x;
    float y, z;
    double p[3];

    e->sound = -1;
    BoneObj_GetEvents(&e->unk0, h->anim);
    if (e->sound == -1)
        return;
    x = h->matrix.m[2] * e->offset[2] + h->matrix.m[1] * e->offset[1] + h->matrix.m[0] * e->offset[0];
    y = h->matrix.m[5] * e->offset[2] + h->matrix.m[4] * e->offset[1] + h->matrix.m[3] * e->offset[0];
    z = h->matrix.m[8] * e->offset[2] + h->matrix.m[7] * e->offset[1] + h->matrix.m[6] * e->offset[0];
    p[0] = x + h->pos[0];
    p[1] = y + h->pos[1];
    p[2] = z + h->pos[2];
    AnimTrigger_Fire(e->sound, h, p);
}
HOOK(0x4613D0, Human_FireAnimTrigger);

void Human_CollideWalls(Human *h, double *from, double *to, float *vel, uint32_t *bits);
void Human_ProbeGround(Human *h, double *from, double *to, float *vel, uint32_t *bits, float reach);

/* Applies velocity, root motion, slope sliding, push and the carrier's
   motion, runs the collision steps and moves the human. */
void Human_UpdateMove(Human *h)
{
    uint32_t bits = h->sm.bits;
    char steep = 0;
    float v[3], n[3], s0, s1, f1, f2, p1, p2, slope, dist;
    long double len, inv, t0, f0, sum, x;
    double pos[3], to[3];

    v[0] = h->vel[0] + h->delta[0];
    v[1] = h->vel[1] + h->delta[1];
    v[2] = h->vel[2] + h->delta[2];
    if (!h->ground.found || !(bits & HUMAN_ON_GROUND))
        goto still;
    n[0] = h->ground.normal.x;
    n[1] = h->ground.normal.y;
    n[2] = h->ground.normal.z;
    if (!is_zero(n[0]) || !is_zero(n[1]) || !is_zero(n[2])) {
        len = __builtin_sqrtl((long double)n[0] * n[0] + (long double)n[1] * n[1] + (long double)n[2] * n[2]);
        inv = kOneD / len;
        n[0] = n[0] * inv;
        n[1] = n[1] * inv;
        n[2] = n[2] * inv;
    }
    slope = kSlope90 - Math_ASinF(n[2]) * kRadToDeg;
    s0 = n[0];
    s1 = n[1];
    if (slope > kSlope80) {
        s0 = s0 * kSlide80;
        s1 = s1 * kSlide80;
        steep = 1;
        h->slide[0] = s0;
        h->slide[1] = s1;
        h->slide[2] = 0;
    } else if (slope > kSlope60) {
        s0 = s0 * kSlide60;
        s1 = s1 * kSlide60;
        steep = 1;
        h->slide[0] = s0;
        h->slide[1] = s1;
        h->slide[2] = 0;
    } else if (slope > kSlope50) {
        t0 = s0 * kSlide50;
        s1 = s1 * kSlide50;
        steep = 1;
        h->slide[0] = t0 + h->slide[0];
        h->slide[1] = s1 + h->slide[1];
    } else if (slope > kSlope45) {
        t0 = s0 * kSlide45;
        s1 = s1 * kSlide45;
        steep = 1;
        h->slide[0] = t0 + h->slide[0];
        h->slide[1] = s1 + h->slide[1];
    } else {
still:
        h->slide[0] = 0;
        h->slide[1] = 0;
        h->slide[2] = 0;
    }
    f0 = h->slide[0] * kFriction;
    memcpy(pos, h->pos, sizeof(pos));
    h->slide[0] = f0;
    f1 = h->slide[1] * kFriction;
    h->slide[1] = f1;
    f2 = h->slide[2] * kFriction;
    h->slide[2] = f2;
    bits &= ~HUMAN_ROOF;
    sum = f0 + v[0];
    v[1] = f1 + v[1];
    v[2] = f2 + v[2];
    x = h->push[0] * kFriction;
    h->push[0] = x;
    p1 = h->push[1] * kFriction;
    h->push[1] = p1;
    p2 = h->push[2] * kFriction;
    h->push[2] = p2;
    v[0] = x + sum;
    v[1] = p1 + v[1];
    v[2] = p2 + v[2];
    x = h->movePos[0] + pos[0];
    pos[0] = x;
    pos[1] = h->movePos[1] + pos[1];
    pos[2] = h->movePos[2] + pos[2];
    to[0] = v[0] + x;
    to[1] = v[1] + pos[1];
    to[2] = v[2] + pos[2];
    if (!h->unk694) {
        if (h->unk696)
            Human_CollideWalls(h, pos, to, v, &bits);
        if (h->unk695) {
            dist = 0;
            if (bits & HUMAN_ON_GROUND)
                dist = kStepDown;
            if (steep)
                dist = kSlideDown;
            Human_ProbeGround(h, pos, to, v, &bits, dist);
        }
        if (h->unk698)
            Human_AvoidHumans(h, pos, to, v);
    }
    if (bits & HUMAN_ROOF) {
        Human_printf(h, "Roof!\n");
        Human_printf(h, "Distance to roof from feet: %.2f\n", h->roofDistance * kUnitsToMeters);
    }
    h->sm.bits = bits;
    DynCube_Move(Game_GetWorld(), h, to, h->radius);
    Human_printf(h, "PosX: %.2f\n", to[0]);
    Human_printf(h, "PosY: %.2f\n", to[1]);
    Human_printf(h, "PosZ: %.2f\n", to[2]);
    h->outVel[0] = v[0];
    h->outVel[1] = v[1];
    h->outVel[2] = v[2];
    h->outVel[0] = h->movePos[0] + h->outVel[0];
    h->outVel[1] = h->movePos[1] + h->outVel[1];
    h->outVel[2] = h->movePos[2] + h->outVel[2];
    Human_printf(h, "SpeedX: %.2f\n", (double)v[0]);
    Human_printf(h, "SpeedY: %.2f\n", (double)v[1]);
    Human_printf(h, "SpeedZ: %.2f\n", (double)v[2]);
}
HOOK(0x4614B0, Human_UpdateMove);

typedef struct HumanFilter {
    int unk0;
    Human *h;
} HumanFilter;

/* z of the unit normal of a hit (the raw z when the normal is zero) */
static long double Hit_UpZ(const CollHit *hit)
{
    float x = hit->normal.x, y = hit->normal.y, z = hit->normal.z;

    if (is_zero(x) && is_zero(y) && is_zero(z))
        return z;
    return z * (kOneD / __builtin_sqrtl((long double)x * x + (long double)y * y + (long double)z * z));
}

/* Keeps the nearest wall among the hits, making its normal horizontal. */
static CollHit *Hit_PickWall(CollHit *hit, int n, CollHit *best)
{
    long double z, inv;

    for (; n; n--, hit++) {
        if (!hit->found)
            continue;
        z = Hit_UpZ(hit);
        if (!(z >= kZero))
            z = -z;
        if (z >= kWallSteep)
            continue;
        hit->normal.z = 0;
        if (!is_zero(hit->normal.x) || !is_zero(hit->normal.y)) {
            inv = kOneD / __builtin_sqrtl((long double)hit->normal.y * hit->normal.y +
                                          (long double)hit->normal.x * hit->normal.x);
            hit->normal.x = hit->normal.x * inv;
            hit->normal.y = hit->normal.y * inv;
            hit->normal.z = hit->normal.z * inv;
        }
        if (!best || !(hit->dist >= best->dist))
            best = hit;
    }
    return best;
}

/* Moves from -> to, sliding along walls hit at three heights (at most 11
   tries). In the air it also lands on floors and stops at ceilings.
   vel gets the distance actually moved. */
void Human_CollideWalls(Human *h, double *from, double *to, float *vel, uint32_t *bits)
{
    float buf[3], foot[3], rel[3];
    double dir[3], c[3], starts[3][3], ends[3][3], nd[3], len, dist, ex, k;
    long double xd, zd, px;
    CollHit hits[3], *best;
    HumanFilter filter;
    int checks = 0, i;
    char again;
    uint8_t ev;
    void *world;

    memcpy(foot, Human_GetBonePos(buf, h, 0), sizeof(foot));
    *bits &= ~8u;
    do {
        again = 0;
        dir[0] = to[0] - from[0];
        dir[1] = to[1] - from[1];
        dir[2] = to[2] - from[2];
        if (Vec3d_IsZero(dir))
            break;
        len = Vec3d_Length(dir) + kWallPad;
        Vec3d_Normalize(dir);
        ex = len * dir[0];
        memcpy(c, dir, sizeof(c));
        c[1] = c[1] * len;
        c[2] = c[2] * len;
        checks++;
        starts[0][2] = kWallLow - foot[2];
        k = h->crouch ? kWallMidCrouch : kWallMid;
        starts[1][2] = k - foot[2];
        k = h->crouch ? kWallHighCrouch : kWallHigh;
        starts[2][2] = k - foot[2];
        for (i = 0; i < 3; i++) {
            starts[i][0] = 0;
            starts[i][1] = 0;
            ends[i][0] = ex;
            ends[i][1] = c[1];
            ends[i][2] = starts[i][2] + c[2];
        }
        for (i = 0; i < 3; i++) {
            starts[i][0] = starts[i][0] + from[0];
            starts[i][1] = starts[i][1] + from[1];
            starts[i][2] = from[2] + starts[i][2];
            ends[i][0] = ends[i][0] + from[0];
            ends[i][1] = ends[i][1] + from[1];
            ends[i][2] = from[2] + ends[i][2];
        }
        filter.h = h;
        memset(hits, 0, sizeof(hits));
        ev = Game_GetEvent8();
        world = Game_GetWorld();
        Coll_LinesEx(world, hits, starts, ends, 3, ev, &filter, -1, 1);
        dist = len - kWallPad;
        if (!(*bits & HUMAN_ON_GROUND)) {
            best = Hit_PickWall(hits, 3, 0);
            if (hits[0].found && !best && Hit_UpZ(&hits[0]) > kFloorUp) {
                memcpy(to, &hits[0].pos, sizeof(hits[0].pos));
                to[2] = (foot[2] - kWallLow) + to[2];
            }
            if (hits[2].found && !best) {
                if (Hit_UpZ(&hits[2]) >= kCeilingDown) {
                    memcpy(to, &hits[2].pos, 2 * sizeof(double));
                    h->vel[0] = 0;
                    h->vel[1] = 0;
                }
                h->vel[2] = 0;
                to[2] = (hits[2].pos.z - (kWallHigh - foot[2])) - kWallPad;
                goto next;
            }
        } else {
            best = Hit_PickWall(hits + 1, 2, 0);
        }
        if (!best)
            goto next;
        nd[0] = best->normal.x;
        nd[1] = best->normal.y;
        nd[2] = best->normal.z;
        Vec3d_Normalize(nd);
        xd = dist * dir[0];
        memcpy(c, dir, sizeof(c));
        c[1] = c[1] * dist;
        zd = c[2] * dist;
        c[2] = zd;
        if (!(zd * nd[2] + c[1] * nd[1] + xd * nd[0] >= kZeroD)) {
            rel[0] = to[0] - best->pos.x;
            rel[1] = to[1] - best->pos.y;
            rel[2] = to[2] - best->pos.z;
            Vec3_RayPlane(buf, rel, &best->normal.x, &best->normal.x);
            to[0] = buf[0] + best->pos.x;
            to[1] = buf[1] + best->pos.y;
            to[2] = buf[2] + best->pos.z;
            again = 1;
            px = nd[0] * kWallPad;
            nd[1] = nd[1] * kWallPad;
            nd[2] = nd[2] * kWallPad;
            to[0] = px + to[0];
            to[1] = nd[1] + to[1];
            to[2] = nd[2] + to[2];
            *bits |= 8;
        }
next:
        if (checks > 10) {
            memcpy(to, from, 2 * sizeof(double));
            break;
        }
    } while (again);
    vel[0] = to[0] - from[0];
    vel[1] = to[1] - from[1];
    vel[2] = to[2] - from[2];
    Human_printf(h, "Checks: %d\n", checks);
}
HOOK(0x461A70, Human_CollideWalls);

/* Finds the ground under to (and the roof above). Soldiers walking a path
   take the height from the path instead. Puts the human on the ground when
   it is within reach, and keeps the ground hit and slope. */
void Human_ProbeGround(Human *h, double *from, double *to, float *vel, uint32_t *bits, float reach)
{
    float buf[3], foot[3], top[3];
    double pts[2][3], dz[2], d[3], v[3], end[3], len, zdiff, k;
    long double gd, t, a, dx, l;
    CollHit hits[2], hit;
    CollLinesMsg q;
    HumanFilter filter;
    HumanAI *ai;
    Graph *path;
    TaskHandler fn;
    Task *task, *n0;
    void *node, *world;
    char grounded = 0;
    uint8_t ev;

    h->groundDistance = kNoGround;
    memcpy(foot, Human_GetBonePos(buf, h, 0), sizeof(foot));
    k = h->crouch ? kWallMidCrouch : kWallMid;
    memcpy(pts[0], to, 2 * sizeof(double));
    pts[0][2] = (k - foot[2]) + to[2];
    ai = h->ai;
    if (ai) {
        memset(hits, 0, sizeof(hits));
        h->groundDistance = 0;
        if (!ai->graph)
            goto check;
        node = Graph_GetNode(ai->graph, ai->node);
        memcpy(&hits[0].pos, to, sizeof(hits[0].pos));
        hits[0].pos.z = hits[0].pos.z - foot[2];
        if (node) {
            hits[0].material = *(void **)((char *)node + 0x138);
            hits[0].found = 1;
            hits[0].normal.x = 0;
            hits[0].normal.y = 0;
            hits[0].normal.z = kOne;
        }
        if (List_Count(&ai->moves) <= 0) {
            node = Graph_GetNode(ai->graph, ai->node);
            if (node) {
                Graph_GetNodePos(d, node);
                hits[0].pos.z = d[2];
            }
        } else if ((n0 = (Task *)ai->moves.head)->next) {
            if (Vec3d_IsZero(h->pathCur)) {
                memcpy(h->pathPrev, &hits[0].pos, sizeof(h->pathPrev));
                memcpy(h->pathCur, &hits[0].pos, sizeof(h->pathCur));
            }
            if (Vec3d_Distance(MoveItem_GetPos(n0), h->pathCur) > kPathJump) {
                memcpy(h->pathPrev, h->pathCur, sizeof(h->pathPrev));
                memcpy(h->pathCur, MoveItem_GetPos(n0), sizeof(h->pathCur));
            }
            d[0] = h->pathCur[0] - hits[0].pos.x;
            d[1] = h->pathCur[1] - hits[0].pos.y;
            d[2] = h->pathCur[2] - hits[0].pos.z;
            if (Vec3d_IsZero(d)) {
                hits[0].pos.z = h->pathCur[2];
            } else {
                len = Vec3d_Length(d);
                zdiff = h->pathCur[2] - h->pathPrev[2];
                t = len / Vec3d_Distance(h->pathCur, h->pathPrev);
                if (!(t >= kZeroD))
                    t = kZeroD;
                else if (len / Vec3d_Distance(h->pathCur, h->pathPrev) > kOneD)
                    t = kOneD;
                else
                    t = len / Vec3d_Distance(h->pathCur, h->pathPrev);
                hits[0].pos.z = (kOneD - t) * zdiff + h->pathPrev[2];
            }
        }
        path = ai->graph;
        if (!path->attached)
            Human_ClearPlatform(h);
        else if (!h->attach)
            Human_SetPlatform(h, path->task.parent);
        grounded = 1;
    } else {
        memcpy(pts[1], to, sizeof(pts[1]));
        dz[0] = kFarDown;
        dz[1] = kRoofProbe;
        filter.h = h;
        ev = Game_GetEvent13();
        world = Game_GetWorld();
        Coll_DropPointsEx(world, hits, pts, 2, dz, ev, &filter, -1, 1);
        if (hits[1].found && !(hits[1].normal.z >= kZero)) {
            memcpy(top, Human_GetBonePos(buf, h, 0), sizeof(top));
            gd = top[2] + hits[1].dist;
            h->roofDistance = gd;
            if (!(gd >= kWallHigh))
                *bits |= HUMAN_ROOF;
        }
        if (!hits[0].found) {
            dz[0] = kFarUp;
            ev = Game_GetEvent13();
            world = Game_GetWorld();
            Coll_DropPointsEx(world, hits, pts, 1, dz, ev, &filter, -1, 1);
        }
    }
check:
    task = hits[0].task;
    if (hits[0].found && !h->ai) {
        if (task) {
            memcpy(d, &hits[0].pos, sizeof(d));
            d[2] = hits[0].pos.z + kStepUp;
            memcpy(end, d, sizeof(end));
            end[2] = d[2] - kStepProbe;
            q.starts = d;
            q.ends = end;
            q.hits = &hit;
            hit.found = 0;
            hit.task = 0;
            q.filter = 0xFF;
            q.result = 0;
            q.count = 1;
            q.unk14 = 0;
            q.unk18 = 0;
            q.magic = 0x41424344;
            fn = gTaskHandlers[10][task->type];
            if (fn) {
                fn(task, &q);
                if (hit.found)
                    hits[0].material = hit.material;
                task = hits[0].task;
            }
        }
        gd = (to[2] - hits[0].pos.z) - foot[2];
        h->groundDistance = gd;
        if (!(gd > reach))
            goto ground;
    }
    if (!grounded) {
        *bits &= ~(uint32_t)HUMAN_ON_GROUND;
        Human_SetGround(h, 0);
        h->movePos[0] = 0;
        h->movePos[1] = 0;
        h->movePos[2] = 0;
        goto done;
    }
ground:
    *bits |= HUMAN_ON_GROUND;
    to[2] = foot[2] + hits[0].pos.z;
    if (task) {
        Human_SetGround(h, task);
    } else {
        Human_SetGround(h, 0);
        h->movePos[0] = 0;
        h->movePos[1] = 0;
        h->movePos[2] = 0;
    }
    v[2] = 0;
    dx = to[0] - from[0];
    v[0] = dx;
    v[1] = to[1] - from[1];
    if (is_zero(dx) && is_zero(v[1])) {
        a = kZeroD;
    } else {
        l = Vec3d_Length(v);
        t = to[2] - from[2];
        a = Math_ASin(t / __builtin_sqrtl(t * t + l * l));
    }
    h->slopeAngle = a;
    h->slope = a * kTwoOverPi;
done:
    memcpy(&h->ground, &hits[0], sizeof(h->ground));
    if (!hits[0].found) {
        h->groundValid = 0;
        return;
    }
    memcpy(h->groundPos, &hits[0].pos, sizeof(h->groundPos));
    h->groundDist = h->groundDistance;
    memcpy(h->groundUp, &hits[0].normal, sizeof(h->groundUp));
    if (!is_zero(h->groundUp[0]) || !is_zero(h->groundUp[1]) || !is_zero(h->groundUp[2])) {
        t = kOneD / __builtin_sqrtl((long double)h->groundUp[0] * h->groundUp[0] +
                                    (long double)h->groundUp[1] * h->groundUp[1] +
                                    (long double)h->groundUp[2] * h->groundUp[2]);
        h->groundUp[0] = h->groundUp[0] * t;
        h->groundUp[1] = h->groundUp[1] * t;
        h->groundUp[2] = h->groundUp[2] * t;
    }
    h->groundValid = 1;
}
HOOK(0x462390, Human_ProbeGround);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53F314) uint8_t gHumanEvent0 INIT(255);
DATA(53F314) uint8_t gHumanEvent1 INIT(255);
DATA(53F314) uint8_t gHumanGunshotEvent INIT(255);
DATA(53F314) uint8_t gHumanPunchEvent INIT(255);
STORE(igb, 5BDAD8, 4) int store_5BDAD8[8];
ALIAS(gHumanTeamCriteria, store_5BDAD8, 0);
BSS(5BDAFC) int gHumanDebugTime;
BSS(5BDC1C) int gHumanDebug;
