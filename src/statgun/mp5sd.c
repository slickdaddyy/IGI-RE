#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

extern uint16_t gMp5sdType;

extern float kZero;
extern double kAngle45;
extern double kAngle30;
extern float kZoomOut;
extern float kZoomIn;
extern float kHalf;
extern float kDegToRad;

ORIG(0x4F2030, int, Weapon_GetDeselectEvent, (void));

void Mp5sd_Create(ZoomGun *g, void *arg);
void Mp5sd_Use(ZoomGun *g, WeaponUseMsg *msg);
void Mp5sd_Config(void *task, WeaponConfigMsg *m);

void Mp5sd_Unzoom(ZoomGun *g)
{
    g->zoomed = 0;
    HANDLER(Weapon_GetDeselectEvent(), Gun_GetType())(g);
}
HOOK(0x473990, Mp5sd_Unzoom);

void Mp5sd_Open(void)
{
    gMp5sdType = TaskType_New("GunMP5SD", sizeof(ZoomGun), Gun_GetType(), 0);
    TaskType_SetHandler(gMp5sdType, 0, Mp5sd_Create);
    TaskType_SetHandler2(gMp5sdType, 2, Gun_Delete);
    TaskType_SetHandler(gMp5sdType, Human_GetEvent0(), Mp5sd_Use);
    TaskType_SetHandler(gMp5sdType, WeaponType_GetInitEvent2(), Gun_ReadZoomArg);
    TaskType_SetHandler(gMp5sdType, WeaponType_GetSizeEvent2(), RocketPod_GetArgSize);
    TaskType_SetHandler(gMp5sdType, WeaponConfig_GetEvent(), Mp5sd_Config);
    TaskType_SetHandler2(gMp5sdType, Weapon_GetDeselectEvent(), Mp5sd_Unzoom);
    Script_RegisterConstant("TASKTYPE_GUNMP5SD", gMp5sdType);
}
HOOK(0x4739D0, Mp5sd_Open);

void Mp5sd_Create(ZoomGun *g, void *arg)
{
    HANDLER(0, Gun_GetType())(g, arg);
    crt_sprintf(g->gun.id, "WEAPON_ID_MP5SD");
    g->zoomed = 0;
}
HOOK(0x473AB0, Mp5sd_Create);

/* input 7 toggles the zoom, which then moves a step per frame */
void Mp5sd_Use(ZoomGun *g, WeaponUseMsg *msg)
{
    Human *h = msg->h;
    HumanView *v = h->view;
    float *minFov = g->gun.info->data2;
    long double fov;
    float e, t;
    int pressed;

    HANDLER(Human_GetEvent0(), Gun_GetType())(g, msg);
    if (!msg->active) {
        v->fov = x87_tan(kAngle45);
        v->unk1E8 = x87_tan(kAngle30);
        g->zoomed = 0;
        return;
    }
    fov = v->fov;
    MOV32(e, v->unk1E8);
    pressed = h->input[7] != kZero && h->prevInput[7] == kZero;
    if (g->zoomed) {
        if (pressed)
            g->zoomed = 0;
        if (fov > *minFov) {
            fov = fov * kZoomIn;
            e = e * kZoomIn;
        }
    } else {
        if (pressed)
            g->zoomed = 1;
        t = x87_tan(kAngle45);
        if (fov < t) {
            fov = fov * kZoomOut;
            e = e * kZoomOut;
        }
        if (fov > t) {
            fov = t;
            e = x87_tan(kAngle30);
        }
    }
    v->fov = fov;
    MOV32(v->unk1E8, e);
}
HOOK(0x473AF0, Mp5sd_Use);

void Mp5sd_Config(void *task, WeaponConfigMsg *m)
{
    ConfigVar *var;
    float t;

    if (m->info->data2) {
        Mem_Free(m->info->data2);
        m->info->data2 = NULL;
    }
    var = TagItem_Find(m->config, "Zoom - Min FOV (degrees)");
    if (!var)
        var = TagItem_AddReal32(m->config, "Zoom - Min FOV (degrees)", 50.0f);
    t = x87_tan(var->v.f * kHalf * kDegToRad);
    m->info->data2 = Mem_Alloc(4, 4);
    *(float *)m->info->data2 = t;
}
HOOK(0x473C80, Mp5sd_Config);

void Mp5sd_Close(void)
{
    TaskType_Free(gMp5sdType);
}
HOOK(0x473D20, Mp5sd_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533520) float kZoomOut INIT(1.25f);
DATA(5335E8) double kAngle45 INIT(0.7853981852531433);
ALIAS(gQuarterPi, kAngle45, 0);
DATA(5336CC) float kZoomIn INIT(0.8f);
ALIAS(kFourFifths, kZoomIn, 0);
ALIAS(kShadowFade, kZoomIn, 0);
ALIAS(kSlow, kZoomIn, 0);
ALIAS(kSoundMin, kZoomIn, 0);
DATA(5339C0) double kAngle30 INIT(0.5235987901687622);
BSS(5BE374) uint16_t gMp5sdType;
PAD(igb, 5BE374, 5BE376, 0x2);
