#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

/* the private data of the M16A2 weapon type: its M203 grenade launcher */
typedef struct M203Data {
    float speed;        /* km/h */
    float radius;
    float activate;
    char model[16];
} M203Data;

/* what the HUD render event passes */
typedef struct HudMsg {
    char *hud;
} HudMsg;

_Static_assert(sizeof(M203Data) == 0x1C, "M203Data");

#define gM16A2Type VAR(uint16_t, 0x5BE36C)

#define kKmh VAR(float, 0x5334A4)
#define kVelScale VAR(float, 0x5334A0)
#define kIconGap VAR(float, 0x534114)

ORIG(0x414A60, void, Matrix_SetYAxis, (Matrix *m, const Vec3f *dir));
ORIG(0x476670, void, M203_New, (void *a, void *gun, void *owner, Vec3d *pos, Matrix *m, Vec3f *vel,
                                      char *model, float radius, float activate));
ORIG(0x47CDA0, int, Script_EvalConst, (const char *expr));
ORIG(0x47AAB0, void, Gun_GetAmmoTextPos, (float *x, float *y, void *gun, char *hud));
ORIG(0x4B6E90, void, Text_SetString, (void *t, const char *s));
ORIG(0x4B7DF0, int, Text_GetWidth, (void *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (void *t));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B4E00, void, Draw_Sprite, (void *s, float x, float y, float r, float g, float b, int frame, int unk4,
                                   int unk8));

void M16A2_Create(Gun *g, void *arg);
void M16A2_Use(Gun *g, WeaponUseMsg *msg);
void M16A2_Render(Gun *g, HudMsg *msg);
void M16A2_ReadArgs(void *task, WeaponInitArgs *a);
void M16A2_GetDataSize(void *task, int *size);
void M16A2_Config(void *task, WeaponConfigMsg *m);

void M16A2_Open(void)
{
    gM16A2Type = TaskType_New("GunM16A2", sizeof(Gun), Gun_GetType(), 0);
    TaskType_SetHandler(gM16A2Type, 0, M16A2_Create);
    TaskType_SetHandler2(gM16A2Type, 2, Gun_Delete);
    TaskType_SetHandler(gM16A2Type, Human_GetEvent0(), M16A2_Use);
    TaskType_SetHandler(gM16A2Type, StatusScreen_GetEvent(), M16A2_Render);
    TaskType_SetHandler(gM16A2Type, WeaponType_GetInitEvent2(), M16A2_ReadArgs);
    TaskType_SetHandler(gM16A2Type, WeaponType_GetSizeEvent2(), M16A2_GetDataSize);
    TaskType_SetHandler(gM16A2Type, WeaponConfig_GetEvent(), M16A2_Config);
    Script_RegisterConstant("TASKTYPE_GUNM16A2", gM16A2Type);
}
HOOK(0x4733F0, M16A2_Open);

void M16A2_Create(Gun *g, void *arg)
{
    HANDLER(0, Gun_GetType())(g, arg);
    crt_sprintf(g->id, "WEAPON_ID_M16A2");
}
HOOK(0x4734D0, M16A2_Create);

/* the alternate fire launches an M203 grenade */
void M16A2_Use(Gun *g, WeaponUseMsg *msg)
{
    M203Data *d = g->info->data;
    long double s;
    Matrix m;
    Vec3f vel;
    Task *snd;

    HANDLER(Human_GetEvent0(), Gun_GetType())(g, msg);
    if (!msg->alt)
        return;
    Matrix_SetYAxis(&m, &g->dir);
    s = d->speed * kKmh;
    vel.x = s * g->dir.x * kVelScale;
    vel.y = s * g->dir.y * kVelScale;
    vel.z = s * g->dir.z * kVelScale;
    snd = Sound_Play(Level_GetDynamic(), "m203_launch_1");
    if (snd)
        Sound_SetPos(snd, &g->muzzle);
    M203_New(Level_GetDynamic(), g, g->obj.task.parent, &g->muzzle, &m, &vel, d->model, d->radius, d->activate);
}
HOOK(0x473510, M16A2_Use);

/* the grenade count and icon next to the ammo display */
void M16A2_Render(Gun *g, HudMsg *msg)
{
    char *hud = msg->hud;
    void *list = NULL;
    Task *owner;
    AmmoEntry *ammo;
    TaskFn fn;
    int count = 0;
    float x, y;
    char buf[16];
    void **text = (void **)(hud + 0x4E0);

    Display_GetMode();
    Display_GetMode();
    HANDLER(StatusScreen_GetEvent(), Gun_GetType())(g, msg);
    owner = g->obj.task.parent;
    fn = HANDLER(AmmoList_GetEvent(), owner->type);
    if (fn)
        fn(owner, &list);
    ammo = AmmoList_Find(list, Script_EvalConst("AMMO_ID_M203"));
    if (ammo)
        count = ammo->count;
    Gun_GetAmmoTextPos(&x, &y, g, hud);
    crt_sprintf(buf, "%-4d", count);
    Text_SetString(*text, buf);
    x = x - Text_GetWidth(*text);
    ((float *)*text)[1] = x;
    MOV32(((float *)*text)[2], y);
    Text_DrawAlt(*text);
    x = x - kIconGap;
    x = x - Sprite_Width(hud + 0x14C);
    Draw_Sprite(hud + 0x14C, x, y, 128.0f, 128.0f, 128.0f, 0, 0, 0);
}
HOOK(0x473620, M16A2_Render);

void M16A2_ReadArgs(void *task, WeaponInitArgs *a)
{
    int i = a->next;
    float *out = a->data;

    out[0] = Script_GetArgReal(a->args, i);
    out[1] = Script_GetArgReal(a->args, i + 1);
    out[2] = Script_GetArgReal(a->args, i + 2);
    a->next = i + 3;
}
HOOK(0x473790, M16A2_ReadArgs);

void M16A2_GetDataSize(void *task, int *size)
{
    *size = sizeof(M203Data);
}
HOOK(0x4737E0, M16A2_GetDataSize);

void M16A2_Config(void *task, WeaponConfigMsg *m)
{
    ConfigVar *speed, *radius, *activate, *model;
    M203Data d;

    if (m->info->data) {
        Mem_Free(m->info->data);
        m->info->data = NULL;
    }
    memcpy(d.model, "108_05_1", 9);
    d.speed = 270.0f;
    d.radius = 5.0f;
    d.activate = 1.0f;
    speed = TagItem_Find(m->config, "M203 - Muzzle velocity (km/h)");
    if (!speed)
        speed = TagItem_AddReal32(m->config, "M203 - Muzzle velocity (km/h)", d.speed);
    radius = TagItem_Find(m->config, "M203 - Explosion radius (meters)");
    if (!radius)
        radius = TagItem_AddReal32(m->config, "M203 - Explosion radius (meters)", d.radius);
    activate = TagItem_Find(m->config, "M203 - Activate radius (meters)");
    if (!activate)
        activate = TagItem_AddReal32(m->config, "M203 - Activate radius (meters)", d.activate);
    model = TagItem_Find(m->config, "M203 - Model");
    if (!model)
        model = TagItem_AddString16(m->config, "M203 - Model", d.model);
    MOV32(d.speed, speed->v.f);
    MOV32(d.radius, radius->v.f);
    MOV32(d.activate, activate->v.f);
    strcpy(d.model, model->v.s);
    m->info->data = Mem_Alloc(sizeof(M203Data), 4);
    memcpy(m->info->data, &d, sizeof(M203Data));
}
HOOK(0x4737F0, M16A2_Config);

void M16A2_Close(void)
{
    TaskType_Free(gM16A2Type);
}
HOOK(0x473980, M16A2_Close);
