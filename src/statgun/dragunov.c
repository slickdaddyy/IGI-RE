#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

extern uint16_t gDragunovType;
extern char *gSniperSightY;
extern char *gSniperSight;
extern char *gSniperSightX;

/* exe constants, read in place */
extern float kZero;
extern double kAngle45;
extern double kAngle30;
extern float kRecoilScale;
extern float kDragunovZoomOut;
extern float kDragunovZoomIn;
extern float kHalf;
extern float kDegToRad;

ORIG(0x4169D0, char *, Game_AddSprite, (const char *name));
ORIG(0x4659D0, void, HumanView_SetZoomed, (HumanView *v));
ORIG(0x4659E0, void, HumanTaskViewReset, (HumanView *v));
ORIG(0x4B51F0, void, QVertex_DrawQuad, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                        float x3, float y3, float r, float g, float b, int frame, int flags, int unk));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));

void Dragunov_Create(ZoomGun *g, void *arg);
void Dragunov_Use(ZoomGun *g, WeaponUseMsg *msg);
void Dragunov_Render(ZoomGun *g, void *msg);
void Dragunov_Config(void *task, WeaponConfigMsg *m);

void Dragunov_Open(void)
{
    gDragunovType = TaskType_New("GunDRAGUNOV", sizeof(ZoomGun), Gun_GetType(), 0);
    TaskType_SetHandler(gDragunovType, 0, Dragunov_Create);
    TaskType_SetHandler2(gDragunovType, 2, Gun_Delete);
    TaskType_SetHandler(gDragunovType, Human_GetEvent0(), Dragunov_Use);
    TaskType_SetHandler(gDragunovType, StatusScreen_GetEvent(), Dragunov_Render);
    TaskType_SetHandler(gDragunovType, WeaponType_GetInitEvent2(), Gun_ReadZoomArg);
    TaskType_SetHandler(gDragunovType, WeaponType_GetSizeEvent2(), RocketPod_GetArgSize);
    TaskType_SetHandler(gDragunovType, WeaponConfig_GetEvent(), Dragunov_Config);
    Script_RegisterConstant("TASKTYPE_GUNDRAGUNOV", gDragunovType);
    gSniperSight = Game_AddSprite("snipersight.spr");
    gSniperSightX = Game_AddSprite("snipersight_x.spr");
    gSniperSightY = Game_AddSprite("snipersight_y.spr");
}
HOOK(0x472D00, Dragunov_Open);

void Dragunov_Create(ZoomGun *g, void *arg)
{
    HANDLER(0, Gun_GetType())(g, arg);
    crt_sprintf(g->gun.id, "WEAPON_ID_DRAGUNOV");
    g->zoomed = 0;
}
HOOK(0x472E10, Dragunov_Create);

/* input 7 toggles the scope; inputs 14 and 15 zoom while it is up */
void Dragunov_Use(ZoomGun *g, WeaponUseMsg *msg)
{
    Human *h = msg->h;
    float *minFov = g->gun.info->data;
    HumanView *v = h->view;
    long double t;

    HANDLER(Human_GetEvent0(), Gun_GetType())(g, msg);
    g->zoomed = msg->zoomed;
    if (!msg->active)
        return;
    if (!g->zoomed) {
        if (h->input[7] != kZero && h->prevInput[7] == kZero)
            HumanView_SetZoomed(h->view);
        return;
    }
    msg->recoilPitch = msg->recoilPitch * kRecoilScale;
    msg->recoilYaw = msg->recoilYaw * kRecoilScale;
    if (h->input[7] != kZero && h->prevInput[7] == kZero)
        HumanTaskViewReset(h->view);
    if (h->input[14] == kZero && h->prevInput[7] == kZero) {
        if (h->input[15] == kZero)
            return;
        t = x87_tan(kAngle45);
        if (t > v->fov) {
            v->fov = v->fov * kDragunovZoomOut;
            v->unk1E8 = v->unk1E8 * kDragunovZoomOut;
        }
        if (t < v->fov) {
            v->fov = t;
            v->unk1E8 = x87_tan(kAngle30);
        }
    } else if (v->fov > *minFov) {
        v->fov = v->fov * kDragunovZoomIn;
        v->unk1E8 = v->unk1E8 * kDragunovZoomIn;
    }
}
HOOK(0x472E50, Dragunov_Use);

/* the scope picture, black borders and cross hair lines */
void Dragunov_Render(ZoomGun *g, void *msg)
{
    int w = Display_GetMode()->width;
    int h = Display_GetMode()->height;
    float fw, fh, t, hw, hh, nt, r, b, y, x;
    int n;

    if (!g->zoomed) {
        HANDLER(StatusScreen_GetEvent(), Gun_GetType())(g, msg);
        return;
    }
    fw = w;
    n = Sprite_Width(gSniperSight + 0x18);
    t = fw / n;
    fh = h;
    hh = fh * kHalf;
    hw = fw * kHalf;
    nt = -t;
    QVertex_DrawQuad(gSniperSight + 0x18, nt, nt, hw, nt, nt, hh, hw, hh, 128.0f, 128.0f, 128.0f, 0, 0x1000000, 0);
    r = fw + t;
    QVertex_DrawQuad(gSniperSight + 0x18, r, nt, hw, nt, r, hh, hw, hh, 128.0f, 128.0f, 128.0f, 0, 0x1000000, 0);
    b = fh + t;
    QVertex_DrawQuad(gSniperSight + 0x18, nt, b, hw, b, nt, hh, hw, hh, 128.0f, 128.0f, 128.0f, 0, 0x1000000, 0);
    QVertex_DrawQuad(gSniperSight + 0x18, r, b, hw, b, r, hh, hw, hh, 128.0f, 128.0f, 128.0f, 0, 0x1000000, 0);
    n = Sprite_Height(gSniperSightX + 0x18);
    y = h / 2 - n / 2;
    t = n + y;
    QVertex_DrawQuad(gSniperSightX + 0x18, 0.0f, y, fw, y, 0.0f, t, fw, t, 128.0f, 128.0f, 128.0f, 0, 0x1000000, 0);
    n = Sprite_Width(gSniperSightY + 0x18);
    x = w / 2 - n / 2;
    t = n + x;
    QVertex_DrawQuad(gSniperSightY + 0x18, x, 0.0f, t, 0.0f, x, fh, t, fh, 128.0f, 128.0f, 128.0f, 0, 0x1000000, 0);
}
HOOK(0x473040, Dragunov_Render);

/* reads the narrowest view angle (degrees) of a zoomable gun */
void Gun_ReadZoomArg(void *task, WeaponInitArgs *a)
{
    int i = a->next;
    float *out = a->data;

    *out = x87_tan((long double)(Script_GetArgInt(a->args, i) / 2) * kDegToRad);
    a->next = i + 1;
}
HOOK(0x4732F0, Gun_ReadZoomArg);

void Dragunov_Config(void *task, WeaponConfigMsg *m)
{
    ConfigVar *var;
    float t;

    m->info->unk668 = 0.0f;
    m->info->unk664 = 0.0f;
    if (m->info->data) {
        Mem_Free(m->info->data);
        m->info->data = NULL;
    }
    var = TagItem_Find(m->config, "Zoom - Min FOV (degrees)");
    if (!var)
        var = TagItem_AddReal32(m->config, "Zoom - Min FOV (degrees)", 5.0f);
    t = x87_tan(var->v.f * kHalf * kDegToRad);
    m->info->data = Mem_Alloc(4, 4);
    *(float *)m->info->data = t;
}
HOOK(0x473330, Dragunov_Config);

void Dragunov_Close(void)
{
    TaskType_Free(gDragunovType);
}
HOOK(0x4733E0, Dragunov_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5339CC) float kDragunovZoomIn INIT(0.96f);
DATA(533A80) float kDragunovZoomOut INIT(1.04f);
BSS(5BE358) uint16_t gDragunovType;
PAD(igb, 5BE358, 5BE35A, 0x2);
BSS(5BE358) char *gSniperSightY;
BSS(5BE358) char *gSniperSight;
BSS(5BE358) char *gSniperSightX;
