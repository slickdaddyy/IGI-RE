#include <string.h>

#include "hook.h"
#include "igi.h"
#include "ammotype/ammotype.h"
#include "qvertex/qvertextypes.h"
#include "win/wintypes.h"
#include "res/restypes.h"
#include "humanai/humanaitypes.h"
#include "guncasing/guncasingtypes.h"
#include "core/coretypes.h"
#include "file/filevars.h"
#include "guncasing/guncasingvars.h"
#include "math/mathvars.h"

/* The "Gun" task (0x1B0 bytes, 0x5BE3C6), base of the hand weapons. These
   are its handlers from 0x479780 on; the type is registered by 0x477E30. */

/* the message of the weapon fire event */
typedef struct FireMsg {
    char unk0;
    char fired;
    char unk2[0x1E];
    float recoilPitch;
    float recoilYaw;
    int unk28;
    Task *h;
} FireMsg;

/* the HUD sprites and text the human render handler passes on */
typedef struct Hud {
    char unk0[0x4E0];
    Text *text;
} Hud;

#define HUD_SPRITE(hud, off) ((void *)((char *)(hud) + (off)))

typedef struct TaskList {
    int count;
    int max;
} TaskList;

_Static_assert(offsetof(FireMsg, h) == 0x2C, "FireMsg");

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4137E0, void, HumanPlayer_SetWeaponAnim, (Task *h, int anim));
ORIG(0x413C80, WeaponType *, Weapon_GetType, (Task *weapon));
ORIG(0x414ED0, void, Model_GetLocalPoint, (Vec3f *out, void *obj, int i));
ORIG(0x4168C0, TaskList *, Game_GetTaskList2, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x450CE0, void, HumanAI_Broadcast, (AIMessage *m));
ORIG(0x460C10, uint8_t, Human_GetEvent1, (void));
ORIG(0x463130, int, Human_GetTeam, (Task *h));
ORIG(0x463150, char, Human_IsPlayer, (Task *h));
ORIG(0x46C980, Task *, GunPickup_New, (Task *parent, Vec3d *pos, float *matrix, int id, int rounds));
ORIG(0x4785F0, void, Gun_Reset, (Gun *g));
ORIG(0x4788D0, void, Gun_RemoveFromWorld, (Gun *g));
ORIG(0x489F50, char, HumanPlayer_AnimDone, (Task *h));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B4E00, void, Draw_Sprite, (void *s, float x, float y, float r, float g, float b, int frame, int unk4,
                                   int unk8));
ORIG(0x4B4F40, void, Draw_SpriteZ, (void *s, float x, float y, float r, float g, float b, int frame, int unk4,
                                    int unk8, float z));
ORIG(0x4B53B0, void, QVertex_DrawQuadEx, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float r, float g, float b, float z, float a, int frame,
                                          unsigned int flags, int unk20));
ORIG(0x4B5F00, const char *, Resource_Load, (const char *name, int flags));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B7DF0, int, Text_GetWidth, (Text *t));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model, int unk));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4F0EE0, Task *, Level_GetDynamic, (void));
ORIG(0x4F1FF0, uint8_t, Weapon_GetTriggerEvent, (void));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (void *slots, int i));
ORIG(0x4F2350, void, Weapon_UseAmmo, (Gun *g, char unlimited));

extern int gQVertexStrip;

extern const float kThree;
extern const float kEight;
extern const float kHudRight;  /* 0.984375 */
extern const float kHudBottom; /* 0.8958333 */

static const float kGray = 128.0f;

/* fistp in the current rounding mode */
static int x87_round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* asks the owner of the gun for its ammo list */
static AmmoList *owner_ammo(Gun *g)
{
    Task *owner = g->o.task.parent;
    AmmoList *list = NULL;
    TaskFn fn = HANDLER(AmmoList_GetEvent(), owner->type);

    if (fn)
        fn(owner, &list);
    return list;
}

/* the rounds of the gun's ammo type the owner carries */
static int carried(Gun *g)
{
    AmmoType *at = g->ammoType;
    AmmoList *list = owner_ammo(g);
    AmmoEntry *e = AmmoList_Find(list, at->id);

    return e ? e->count : 0;
}

void Gun_ShootHandler(Gun *g, FireMsg *m)
{
    WeaponType *w = g->wt;
    AIMessage noise;
    float x;
    long double y;

    HANDLER(Weapon_GetTriggerEvent(), g->o.task.type)(g, m);
    if (!m->fired)
        return;
    noise.type = 0x10;
    noise.flags = 0x46;
    memcpy(&noise.radius, &w->detectRange, 4);
    noise.pos = &g->muzzle;
    noise.from = (Task *)g;
    if (m->h) {
        noise.team = Human_GetTeam(m->h);
        noise.about = m->h;
    } else {
        noise.team = 1;
        noise.about = NULL;
    }
    HumanAI_Broadcast(&noise);
    /* a random point in the unit circle */
    do {
        x = rf((Random_Float(Game_GetRandom()) - kHalf) * 2);
        y = (Random_Float(Game_GetRandom()) - kHalf) * 2;
    } while (__builtin_sqrtl(y * y + (long double)x * x) > kOne);
    m->recoilYaw = y * (w->recoilYawRand * kDegToRad) + w->recoilYaw * kDegToRad;
    m->recoilPitch = w->recoilPitchRand * kDegToRad * x + w->recoilPitch * kDegToRad;
}
HOOK(0x479780, Gun_ShootHandler);

/* fills the clip from the ammo the owner carries */
void Gun_FillClipHandler(Gun *g, char *unlimited)
{
    WeaponType *w = g->wt;
    AmmoType *at = g->ammoType;
    AmmoList *list;
    AmmoEntry *e;
    int need;

    if (*unlimited) {
        g->rounds = w->clipSize;
        return;
    }
    list = owner_ammo(g);
    e = AmmoList_Find(list, at->id);
    if (!e || !e->count)
        return;
    need = w->clipSize - g->rounds;
    if (!need)
        return;
    if (e->count >= need) {
        e->count -= need;
        g->rounds += need;
    } else {
        g->rounds += e->count;
        e->count = 0;
    }
}
HOOK(0x4798C0, Gun_FillClipHandler);

/* first call starts the reload animation, the next calls refill the clip
   once it has played */
void Gun_ReloadHandler(Gun *g, ReloadMsg *m)
{
    Task *h = m->h;
    Task *c;

    Weapon_GetType(WeaponSlots_Get((char *)h + 0x4C4, 0));
    m->done = 0;
    if (g->reload == 0) {
        if (Human_IsPlayer(h))
            HumanPlayer_SetWeaponAnim(h, 7);
        g->reload = 1;
    } else if (g->reload == 1 && HumanPlayer_AnimDone(h)) {
        Weapon_UseAmmo(g, m->unlimited);
        m->done = 1;
        g->reload = 0;
    }
    c = g->o.attached->head;
    if (!c->next || !c)
        return;
    do {
        TaskFn fn;

        gTaskStack[gTaskStackTop++] = c->next && c->next->next ? c->next : NULL;
        fn = HANDLER(Human_GetEvent1(), c->type);
        if (fn)
            fn(c, m);
        c = gTaskStack[--gTaskStackTop];
    } while (c);
}
HOOK(0x479990, Gun_ReloadHandler);

/* rounds in the clip and rounds carried */
void Gun_GetAmmoHandler(Gun *g, int *out)
{
    AmmoType *at = g->ammoType;
    AmmoList *list;
    AmmoEntry *e;

    out[0] = g->rounds;
    out[1] = 0;
    if (!at)
        return;
    list = owner_ammo(g);
    if (!list)
        return;
    e = AmmoList_Find(list, at->id);
    if (e)
        out[1] = e->count;
}
HOOK(0x479AA0, Gun_GetAmmoHandler);

void Gun_ResetHandler(Gun *g)
{
    Vec3d zero = {0, 0, 0};
    float m, r;

    Gun_Reset(g);
    if (g->unkF1)
        return;
    m = g->o.scale[2] >= g->o.scale[1] ? g->o.scale[2] : g->o.scale[1];
    m = g->o.scale[0] > m ? g->o.scale[0] : m;
    r = rf(VirModel_GetRadius(g->o.model, 2) * m);
    DynCube_Add(Game_GetWorld(), g, &zero, r, 2);
    g->inWorld = 1;
}
HOOK(0x479B20, Gun_ResetHandler);

void Gun_RemoveFromWorldHandler(Gun *g)
{
    Gun_RemoveFromWorld(g);
}
HOOK(0x479C10, Gun_RemoveFromWorldHandler);

/* drops the gun as a pickup with the rounds left in the clip */
void Gun_DropHandler(Gun *g, Task **out)
{
    TaskList *a = Game_GetTaskList2();

    if (Game_GetTaskList2()->count >= a->max) {
        *out = NULL;
        return;
    }
    *out = GunPickup_New(Level_GetDynamic(), &g->o.pos, g->o.matrix.m, g->id, g->rounds);
    g->remove = 1;
}
HOOK(0x479C20, Gun_DropHandler);

void Gun_GetTypeHandler(Gun *g, WeaponType **out)
{
    *out = g->wt;
}
HOOK(0x479C80, Gun_GetTypeHandler);

extern const float kOff0X; /* 143.36 */
extern const float kOff0Y; /* 389.12 */
extern const float kOff0Z; /* 20.48 */
extern const float kOff1X; /* 409.6 */
extern const float kOff1Z; /* 40.96 */

void Gun_UnusedPointsHandler(void *obj, float *out)
{
    Vec3f v;

    Model_GetLocalPoint(&v, obj, 0);
    out[0] = v.x + kOff0X;
    out[1] = v.y - kOff0Y;
    out[2] = v.z - kOff0Z;
    Model_GetLocalPoint(&v, obj, 1);
    out[3] = v.x - kOff1X;
    memcpy(&out[4], &v.y, 4);
    out[5] = v.z + kOff1Z;
}
HOOK(0x479CA0, Gun_UnusedPointsHandler);

void Gun_DrawAmmoCount(Gun *g, Hud *hud);
void Gun_DrawAmmoClip(Gun *g, Hud *hud);
void Gun_DrawAmmoBar(Gun *g, Hud *hud);
void Gun_DrawAmmoItems(Gun *g, Hud *hud);

/* the weapon icon on the HUD, by weapon id */
static void *weapon_icon(Hud *hud, int id)
{
    switch (id) {
    case 1: return HUD_SPRITE(hud, 0x254);
    case 3: return HUD_SPRITE(hud, 0x20C);
    case 4: return HUD_SPRITE(hud, 0x2CC);
    case 5: return HUD_SPRITE(hud, 0x1C4);
    case 6:
    case 13: return HUD_SPRITE(hud, 0x374);
    case 7: return HUD_SPRITE(hud, 0x2E4);
    case 8: return HUD_SPRITE(hud, 0x32C);
    case 9: return HUD_SPRITE(hud, 0x284);
    case 10: return HUD_SPRITE(hud, 0x314);
    case 11: return HUD_SPRITE(hud, 0x224);
    case 12: return HUD_SPRITE(hud, 0x2B4);
    case 14: return HUD_SPRITE(hud, 0x344);
    case 15: return HUD_SPRITE(hud, 0x23C);
    case 16: return HUD_SPRITE(hud, 0x2FC);
    case 18: return HUD_SPRITE(hud, 0x1DC);
    case 19: return HUD_SPRITE(hud, 0x26C);
    case 21: return HUD_SPRITE(hud, 0x1F4);
    case 42: return HUD_SPRITE(hud, 0x35C);
    }
    return NULL;
}

/* weapon name, icon, ammo and crosshair */
void Gun_HudHandler(Gun *g, Hud **msg)
{
    Hud *hud = *msg;
    WeaponType *w = g->wt;
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    float textW = 0, iconW = 0;
    void *icon;

    switch (w->ammoDisplay) {
    case 1:
    case 5:
        Gun_DrawAmmoCount(g, hud);
        break;
    case 2:
        Gun_DrawAmmoClip(g, hud);
        break;
    case 3:
        Gun_DrawAmmoBar(g, hud);
        break;
    case 4:
        Gun_DrawAmmoItems(g, hud);
        break;
    }
    if (w->ammoDisplay) {
        char path[260];
        const char *s;
        float x = rf(width * kHudRight);
        float y = rf(height * kHudBottom - kTwo);

        crt_sprintf(path, "weapons/strings/%s", w->unk110);
        s = Resource_Find(path) ? Resource_Load(path, 0) : w->unk110;
        Text_SetString(hud->text, s);
        x = rf(x - Text_GetWidth(hud->text));
        y = rf(y - Text_GetHeight(hud->text));
        hud->text->x = x;
        hud->text->y = y;
        Text_DrawAlt(hud->text);
        textW = Text_GetWidth(hud->text);
    }
    icon = weapon_icon(hud, w->id);
    if (icon) {
        float x = rf(width * kHudRight);
        float y;

        x = rf(x - Sprite_Width(icon));
        y = rf(height * kHudBottom - kTwo - kTwo);
        y = rf(y - Sprite_Height(icon));
        y = rf(y - Text_GetHeight(hud->text));
        Draw_Sprite(icon, x, y, kGray, kGray, kGray, 0, 0, 0);
        iconW = Sprite_Width(icon);
    }
    if (w->ammoDisplay) {
        QVertex v[2];
        QVertexStrip strip;
        long double m;

        gQVertexTexture = NULL;
        gQVertexPalette = NULL;
        strip.lines = 1;
        strip.flags = 0x80000;
        strip.queue = -1;
        strip.count = 2;
        strip.verts = v;
        v[0].x = rf(width * kHudRight);
        v[0].y = rf(height * kHudBottom);
        v[0].z = 1.0f;
        v[0].u = 0;
        v[0].v = 0;
        v[0].a = 0.5f;
        v[0].r = 0;
        v[0].g = 0.9f;
        v[0].b = 0;
        v[1].z = 1.0f;
        v[1].u = 0;
        v[1].v = 0;
        v[1].a = 0.5f;
        v[1].r = 0;
        v[1].g = 0.9f;
        v[1].b = 0;
        if (textW > iconW)
            m = textW + kThree;
        else
            m = iconW;
        v[1].x = v[0].x - m;
        v[1].y = v[0].y;
        gRenderModeCurrent.methods[gQVertexStrip](&strip);
    }
    if (w->sightDisplay == 1) {
        void *cross = HUD_SPRITE(hud, 0x74);
        float cx = rf(Display_GetMode()->width * kHalf);
        float cy = rf(Display_GetMode()->height * kHalf);
        float hw = rf(Sprite_Width(cross) * kHalf);
        int sh = Sprite_Height(cross);

        Draw_SpriteZ(cross, cx - x87_round(hw), cy - x87_round(sh * kHalf), kGray, kGray, kGray, 0, 0, 0, 1.0f);
    }
}
HOOK(0x479D20, Gun_HudHandler);

/* "%-4d" rounds carried, right aligned at the bottom */
void Gun_DrawAmmoCount(Gun *g, Hud *hud)
{
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    int count = carried(g);
    char buf[16];
    float x;

    crt_sprintf(buf, "%-4d", count);
    Text_SetString(hud->text, buf);
    x = rf(width * kHudRight - kFour);
    hud->text->x = x - Text_GetWidth(hud->text);
    hud->text->y = height * kHudBottom + kFour;
    Text_DrawAlt(hud->text);
}
HOOK(0x47A260, Gun_DrawAmmoCount);

/* rounds carried and a clip sprite whose frame shows the rounds left;
   weapon 12 shows a loaded round instead and counts it */
void Gun_DrawAmmoClip(Gun *g, Hud *hud)
{
    WeaponType *w = g->wt;
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    int rounds = g->rounds;
    int count = carried(g);
    int frame;
    void *spr;
    char buf[16];
    float x, y;

    frame = 60 - rounds * 60 / w->clipSize;
    if (frame < 0)
        frame = 0;
    if (w->id == 12) {
        frame = 0;
        spr = rounds ? HUD_SPRITE(hud, 0xD4) : NULL;
        if (g->rounds)
            count++;
    } else {
        spr = HUD_SPRITE(hud, 0xEC);
    }
    crt_sprintf(buf, "%-4d", count);
    Text_SetString(hud->text, buf);
    x = rf(width * kHudRight - kFour);
    hud->text->x = x - Text_GetWidth(hud->text);
    y = rf(height * kHudBottom + kFour);
    hud->text->y = y;
    Text_DrawAlt(hud->text);
    if (spr) {
        x = rf(x - kEight);
        x = rf(x - Sprite_Width(spr));
        x = rf(x - Text_GetWidth(hud->text));
        Draw_Sprite(spr, x, y, kGray, kGray, kGray, frame, 0, 0);
    }
}
HOOK(0x47A360, Gun_DrawAmmoClip);

void Gun_DrawAmmoBar(Gun *g, Hud *hud)
{
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    int rounds = g->rounds;
    int count = carried(g);
    void *spr = HUD_SPRITE(hud, 0x104);
    int frame, sh;
    char buf[16];
    float x, x0, x1, y, y1, sw;

    frame = g->unk19C + (6 - rounds) * 10;
    if (frame < 0)
        frame = 0;
    crt_sprintf(buf, "%-4d", count);
    Text_SetString(hud->text, buf);
    x = rf(width * kHudRight - kFour);
    x0 = rf(width * kHudRight - kFour - kEight);
    y = rf(height * kHudBottom + kFour);
    x0 = rf(x0 - Sprite_Width(spr));
    x0 = rf(x0 - Text_GetWidth(hud->text));
    sw = Sprite_Width(spr);
    sh = Sprite_Height(spr);
    y1 = rf(sh + y);
    x1 = rf(sw + x0);
    QVertex_DrawQuadEx(spr, x0, y, x0, y1, x1, y, x1, y1, kGray, kGray, kGray, 0, 0, frame, 0, 0);
    hud->text->x = x - Text_GetWidth(hud->text);
    hud->text->y = y;
    Text_DrawAlt(hud->text);
}
HOOK(0x47A540, Gun_DrawAmmoBar);

void Gun_DrawAmmoItems(Gun *g, Hud *hud)
{
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    int rounds = g->rounds;
    int count = carried(g);
    void *spr = HUD_SPRITE(hud, 0x11C);
    int frame;
    char buf[16];
    float x, x0, y;

    frame = 7 - rounds;
    if (frame < 0)
        frame = 0;
    crt_sprintf(buf, "%-4d", count);
    Text_SetString(hud->text, buf);
    x = rf(width * kHudRight - kFour);
    x0 = rf(x - kEight);
    x0 = rf(x0 - Sprite_Width(spr));
    x0 = rf(x0 - Text_GetWidth(hud->text));
    y = rf(height * kHudBottom + kFour);
    Draw_Sprite(spr, x0, y, kGray, kGray, kGray, frame, 0, 0);
    hud->text->x = x - Text_GetWidth(hud->text);
    hud->text->y = y;
    Text_DrawAlt(hud->text);
}
HOOK(0x47A750, Gun_DrawAmmoItems);

/* hits on the gun itself, its owner or what the owner is mounted on are
   ignored */
void Gun_CollFilter(Gun *g, char *msg)
{
    Task *a = *(Task **)(msg + 8);
    Task *b = *(Task **)(msg + 0xC);
    Task *owner = g->o.task.parent;

    if (a == &g->o.task || a == owner || b == &g->o.task || b == owner) {
        *msg = 0;
        return;
    }
    if (owner && TaskType_IsA(owner->type, MagicObj_GetType()) && a == *(Task **)((char *)owner + 0x130)) {
        *msg = 0;
        return;
    }
    *msg = 1;
}
HOOK(0x47A8E0, Gun_CollFilter);

void Gun_GetShotSourceHandler(Gun *g, void **out)
{
    out[0] = *g->shots;
    out[1] = &g->point;
}
HOOK(0x47A940, Gun_GetShotSourceHandler);

void Gun_RemoveHandler(Gun *g, void *arg)
{
    if (g->loopSound && *g->loopSound) {
        Task_Delete(*g->loopSound);
        TaskRef_Remove(g->loopSound);
        g->loopSound = NULL;
    }
    if (g->inWorld) {
        uint8_t slot = World_GetEvent1();

        HANDLER(slot, RigidDynCubeObj_GetType())(g, arg);
        g->inWorld = 0;
    }
}
HOOK(0x47A960, Gun_RemoveHandler);

void Gun_AddHandler(Gun *g, void *arg)
{
    if (!g->unkF1 && g->unkF4 && !g->inWorld) {
        uint8_t slot = World_GetEvent0();

        HANDLER(slot, RigidDynCubeObj_GetType())(g, arg);
        g->inWorld = 1;
    }
}
HOOK(0x47A9E0, Gun_AddHandler);

void Gun_Close(void)
{
    QTaskList_Free(gGunParams);
    TaskEvent_Free(gGunEvents[4]);
    TaskEvent_Free(gGunEvents[3]);
    TaskEvent_Free(gGunEvents[2]);
    TaskEvent_Free(gGunEvents[1]);
    TaskEvent_Free(gGunEvents[0]);
    TaskType_Free(gGunCasingType);
    TaskType_Free(gGunType);
}
HOOK(0x47AA40, Gun_Close);

/* where the ammo text of a mounted gun goes */
void Gun_GetAmmoTextPos(float *x, float *y, Gun *g, Hud *hud)
{
    WeaponType *w = g->wt;
    int width = Display_GetMode()->width;
    int height = Display_GetMode()->height;
    int count = carried(g);
    void *spr;
    char buf[16];

    crt_sprintf(buf, "%-4d", count);
    Text_SetString(hud->text, buf);
    spr = w->id == 12 ? HUD_SPRITE(hud, 0x14C) : HUD_SPRITE(hud, 0xEC);
    *x = width * kHudRight - kFour - kEight;
    *y = height * kHudBottom + kFour;
    *x = *x - Sprite_Width(spr);
    *x = *x - Text_GetWidth(hud->text);
}
HOOK(0x47AAB0, Gun_GetAmmoTextPos);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53352C) const float kThree INIT(3.0f);
ALIAS(kBrakeTail, kThree, 0);
DATA(533588) const float kTwo INIT(2.0f);
DATA(5335C8) const float kFour INIT(4.0f);
DATA(533730) const float kOff1Z INIT(40.96f);
ALIAS(kGripLimit, kOff1Z, 0);
DATA(533924) const float kHudBottom INIT(0.8958333f);
DATA(53414C) const float kOff0Y INIT(389.12f);
DATA(53414C) const float kOff0X INIT(143.36f);
DATA(53414C) const float kHudRight INIT(0.984375f);
