#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

#define gMp5sdType VAR(uint16_t, 0x5BE374)

#define kZero VAR(float, 0x5333EC)
#define kAngle45 VAR(double, 0x5335E8)
#define kAngle30 VAR(double, 0x5339C0)
#define kZoomOut VAR(float, 0x533520)
#define kZoomIn VAR(float, 0x5336CC)
#define kHalf VAR(float, 0x533504)
#define kDegToRad VAR(float, 0x53367C)

ORIG(0x4F2030, int, sub_4F2030, (void));

void Mp5sd_Create(ZoomGun *g, void *arg);
void Mp5sd_Use(ZoomGun *g, WeaponUseMsg *msg);
void Mp5sd_Config(void *task, WeaponConfigMsg *m);

void Mp5sd_Unzoom(ZoomGun *g)
{
    g->zoomed = 0;
    HANDLER(sub_4F2030(), Gun_GetType())(g);
}
HOOK(0x473990, Mp5sd_Unzoom);

void Mp5sd_Open(void)
{
    gMp5sdType = TaskType_New("GunMP5SD", sizeof(ZoomGun), Gun_GetType(), 0);
    TaskType_SetHandler(gMp5sdType, 0, Mp5sd_Create);
    TaskType_SetHandler2(gMp5sdType, 2, Gun_Delete);
    TaskType_SetHandler(gMp5sdType, Human_GetEvent0(), Mp5sd_Use);
    TaskType_SetHandler(gMp5sdType, WeaponType_GetInitEvent2(), Gun_ReadZoomArg);
    TaskType_SetHandler(gMp5sdType, WeaponType_GetSizeEvent2(), sub_427D70);
    TaskType_SetHandler(gMp5sdType, WeaponConfig_GetEvent(), Mp5sd_Config);
    TaskType_SetHandler2(gMp5sdType, sub_4F2030(), Mp5sd_Unzoom);
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
    float *minFov = g->gun.info->data;
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

    if (m->info->data) {
        Mem_Free(m->info->data);
        m->info->data = NULL;
    }
    var = Config_Find(m->config, "Zoom - Min FOV (degrees)");
    if (!var)
        var = Config_AddReal(m->config, "Zoom - Min FOV (degrees)", 50.0f);
    t = x87_tan(var->v.f * kHalf * kDegToRad);
    m->info->data = Mem_Alloc(4, 4);
    *(float *)m->info->data = t;
}
HOOK(0x473C80, Mp5sd_Config);

void Mp5sd_Close(void)
{
    TaskType_Free(gMp5sdType);
}
HOOK(0x473D20, Mp5sd_Close);
