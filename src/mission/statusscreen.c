/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "mission/mission.h"

ORIG(0x46B4D0, char *, ComputerRes_Get, (void));
ORIG(0x46C060, Task *, Computer_Get, (void));
ORIG(0x4894B0, int, LevelFlow_GetTicksLeft, (void));
ORIG(0x497570, void, Scene_Init, (void *scene, int on));
ORIG(0x4975F0, void, Scene_Free, (void *scene));
ORIG(0x497620, void, Scene_Select, (void *scene, int on));
ORIG(0x497680, void, Scene_SetRect, (void *scene, float *rect));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x4B50B0, void, QVertex_DrawFrame, (void *s, float x, float y, float r, float g, float b, float alpha, float z,
                                         int frame, unsigned int flags, int unk20));
ORIG(0x4B66A0, void, Sprite_Free, (void *s));
ORIG(0x4B6720, int, Sprite_Load, (void *s, const char *name));
ORIG(0x4C1780, void, Debug_Log, (void));

extern uint16_t gStatusScreenType;
extern uint8_t gStatusScreenEvent;
extern StatusScreen *gStatusScreen;
extern const char *gStatusPortraitNames[];
extern const char *gStatusActionNames[];
extern const char *gStatusActionSprites[];

static const float kBlinkStep = 0x1.111112p-4f; /* 1/15 */
static const float kOne = 1.0f;
static const float kTwo = 2.0f;
static const float kHalf = 0.5f;
static const float kIconY = 0.875f;
static const float kIconAlpha = 0x1.666666p-1f; /* 0.7 */
static const float kIconAlphaBase = 0x1.333334p-2f; /* 0.3 */
static const float kTimeX = 0.8125f;
static const float kTimeY = 0x1.555556p-5f; /* 1/24 */

static const struct {
    short offset;
    const char *name;
} kSprites[] = {
    {0x74, "STATUSSCREEN:cross.spr"},
    {0xEC, "STATUSSCREEN:clip20.spr"},
    {0x104, "STATUSSCREEN:barrel6.spr"},
    {0x11C, "STATUSSCREEN:shell7.spr"},
    {0x134, "STATUSSCREEN:spl_grenade.spr"},
    {0x14C, "STATUSSCREEN:M16_alternative.spr"},
    {0xD4, "STATUSSCREEN:rocket.spr"},
    {0x8C, "STATUSSCREEN:powerbarbackground.spr"},
    {0xA4, "STATUSSCREEN:powerbar.spr"},
    {0x1C4, "STATUSSCREEN:ak47.spr"},
    {0x1DC, "STATUSSCREEN:binoculars.spr"},
    {0x1F4, "STATUSSCREEN:colt.spr"},
    {0x20C, "STATUSSCREEN:deserteagle.spr"},
    {0x224, "STATUSSCREEN:druganov.spr"},
    {0x23C, "STATUSSCREEN:flashgrenade.spr"},
    {0x254, "STATUSSCREEN:glock.spr"},
    {0x26C, "STATUSSCREEN:health.spr"},
    {0x284, "STATUSSCREEN:jackhammer.spr"},
    {0x29C, "STATUSSCREEN:knife.spr"},
    {0x2B4, "STATUSSCREEN:LAW80.spr"},
    {0x2CC, "STATUSSCREEN:M16.spr"},
    {0x2E4, "STATUSSCREEN:Mp5.spr"},
    {0x2FC, "STATUSSCREEN:proximity.spr"},
    {0x314, "STATUSSCREEN:SAW.spr"},
    {0x32C, "STATUSSCREEN:spaz.spr"},
    {0x344, "STATUSSCREEN:splintgrenade.spr"},
    {0x35C, "STATUSSCREEN:stationary.spr"},
    {0x374, "STATUSSCREEN:uzi.spr"},
};

/* the delete handler frees the first nine in another order */
static const short kFreeOrder[] = {0xA4, 0x8C, 0xD4, 0xEC, 0x104, 0x11C, 0x134, 0x14C, 0x74};

#define NSPRITES (int)(sizeof(kSprites) / sizeof(kSprites[0]))

void StatusScreen_Create(StatusScreen *s);
void StatusScreen_Run(StatusScreen *s);
void StatusScreen_Delete(StatusScreen *s);
void StatusScreen_Draw(StatusScreen *s);

void StatusScreen_Init(void)
{
    int i;

    gStatusScreenType = TaskType_New("StatusScreen", sizeof(StatusScreen), Task_GetRootType(), 0, 0);
    gStatusScreenEvent = TaskEvent_Alloc(0x181);
    TaskType_SetHandler(gStatusScreenType, 0, StatusScreen_Create);
    TaskType_SetHandler2(gStatusScreenType, 1, StatusScreen_Run);
    TaskType_SetHandler2(gStatusScreenType, 2, StatusScreen_Delete);
    TaskType_SetHandler2(gStatusScreenType, Screen_GetDrawEvent(), StatusScreen_Draw);
    Alias_Set("STATUSSCREEN:", "LOCAL:screens/game/status/");
    for (i = 0; i < 10; i++)
        gStatusActionSprites[i] = gStatusActionNames[i];
}
HOOK(0x486900, StatusScreen_Init);

void StatusScreen_Create(StatusScreen *s)
{
    float rect[4];
    int width, height, i;
    Text *t;

    width = Display_GetMode()->width;
    height = Display_GetMode()->height;
    rect[2] = width;
    rect[3] = height;
    gStatusScreen = s;
    rect[0] = 0.0f;
    rect[1] = 0.0f;
    Scene_Init(s->scene, 1);
    Scene_SetRect(s->scene, rect);
    s->res = Resource_Load("STATUSSCREEN:status.res", NULL);
    for (i = 0; i < NSPRITES; i++)
        Sprite_Load((char *)s + kSprites[i].offset, kSprites[i].name);
    for (i = 0; i < 4; i++)
        Sprite_Load(&s->portraits[i], gStatusPortraitNames[i]);
    for (i = 0; i < 10; i++)
        Sprite_Load(&s->actions[i], gStatusActionNames[i]);
    s->testText = Text_New(*(void **)(ComputerRes_Get() + 0x10), 320.0f, 240.0f, "Test");
    t = Text_New(*(void **)(ComputerRes_Get() + 0x10), 0.0f, 0.0f, gScriptEmptyString);
    s->timeText = t;
    t->flags |= 0x80;
    s->messages = StatusMessageScreen_New(s);
    s->blink = 0.0f;
    s->unk4EC = 0;
}
HOOK(0x4869B0, StatusScreen_Create);

void StatusScreen_Run(StatusScreen *s)
{
    long double b = s->blink + kBlinkStep;

    s->blink = b;
    if (b > kTwo)
        s->blink = 0.0f;
    CALL_CHILDREN(s, 1);
}
HOOK(0x486CD0, StatusScreen_Run);

void StatusScreen_Delete(StatusScreen *s)
{
    int i;

    Text_Free(s->timeText);
    Text_Free(s->testText);
    for (i = 0; i < 9; i++)
        Sprite_Free((char *)s + kFreeOrder[i]);
    for (i = 9; i < NSPRITES; i++)
        Sprite_Free((char *)s + kSprites[i].offset);
    for (i = 0; i < 4; i++)
        Sprite_Free(&s->portraits[i]);
    for (i = 0; i < 10; i++)
        Sprite_Free(&s->actions[i]);
    Resource_Flush(s->res);
    Scene_Free(s->scene);
}
HOOK(0x486D70, StatusScreen_Delete);

/* draws the children, then lets the watched human draw its HUD, and the mission timer */
void StatusScreen_Draw(StatusScreen *s)
{
    char buf[64];
    Task *cam, *h;
    char *game;
    int width, height, min, sec;
    float x, y;

    cam = *(Task **)(Game_Get() + 0x5C);
    game = Game_Get() + 0x60;
    width = Display_GetMode()->width;
    height = Display_GetMode()->height;
    Scene_Select(s->scene, 1);
    CALL_CHILDREN(s, Screen_GetDrawEvent());
    if (*(Task **)(game + 0x54) == cam && !Computer_Get()) {
        TaskFn fn;
        void *arg = s;

        h = **(Task ***)((char *)cam + 0x68);
        fn = HANDLER(gStatusScreenEvent, h->type);
        if (fn)
            fn(h, &arg);
        if (gTickInfo[0x70]) {
            min = LevelFlow_GetTicksLeft() / 30 / 60;
            sec = LevelFlow_GetTicksLeft() / 30;
            x = width * kTimeX;
            y = height * kTimeY;
            sec %= 60;
            crt_sprintf(buf, "Time Left: %d minute%s, %d second%s", min, min == 1 ? "" : "s", sec,
                        sec == 1 ? "" : "s");
            Text_SetString(s->timeText, buf);
            s->timeText->x = x;
            s->timeText->y = y;
            Text_DrawAlt(s->timeText);
        }
    }
    Debug_Log();
}
HOOK(0x486F30, StatusScreen_Draw);

void StatusScreen_Close(void)
{
    TaskEvent_Free(gStatusScreenEvent);
    gStatusScreenEvent = 0xFF;
    TaskType_Free(gStatusScreenType);
    Alias_Remove("STATUSSCREEN:");
}
HOOK(0x487130, StatusScreen_Close);

Task *StatusScreen_New(void *parent)
{
    return Task_New(parent, gStatusScreenType, NULL);
}
HOOK(0x487160, StatusScreen_New);

StatusScreen *StatusScreen_Get(void)
{
    return gStatusScreen;
}
HOOK(0x487180, StatusScreen_Get);

uint8_t StatusScreen_GetEvent(void)
{
    return gStatusScreenEvent;
}
HOOK(0x487190, StatusScreen_GetEvent);

const char **StatusScreen_GetPortraitNames(void)
{
    return gStatusPortraitNames;
}
HOOK(0x4871A0, StatusScreen_GetPortraitNames);

const char **StatusScreen_GetActionSprites(void)
{
    return gStatusActionSprites;
}
HOOK(0x4871B0, StatusScreen_GetActionSprites);

/* the action icon in the middle of the screen, blinking */
void StatusScreen_DrawActionIcon(StatusScreen *s, void *sprite)
{
    int width, height, hw, hh;
    float x, y, cx, cy;
    long double v;

    width = Display_GetMode()->width;
    height = Display_GetMode()->height;
    if (!sprite)
        return;
    cx = width * kHalf;
    hw = Sprite_Width(sprite) / 2;
    x = cx - hw;
    cy = height * kIconY;
    hh = Sprite_Height(sprite) / 2;
    y = cy - hh;
    if (s->blink < kOne)
        v = s->blink;
    else
        v = kTwo - s->blink;
    QVertex_DrawFrame(sprite, x, y, 128.0f, 128.0f, 128.0f, v * kIconAlpha + kIconAlphaBase, 0.0f, 0, 0x80000, 0);
}
HOOK(0x4871C0, StatusScreen_DrawActionIcon);

void StatusScreen_ResetBlink(StatusScreen *s)
{
    s->blink = 0.0f;
}
HOOK(0x4872B0, StatusScreen_ResetBlink);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_53FE48);
REF_EXTERN(str_540B98);
REF_EXTERN(str_540BB0);
REF_EXTERN(str_540BC8);
REF_EXTERN(str_540BE4);
REF_EXTERN(str_540C00);
REF_EXTERN(str_540C18);
REF_EXTERN(str_540C3C);
REF_EXTERN(str_540C54);
REF_EXTERN(str_540C78);
REF_EXTERN(str_540C98);
REF_EXTERN(str_540CBC);
REF_EXTERN(str_540CD4);
REF_EXTERN(str_540CEC);
DATA(53FE48) char str_53FE48[22] INIT("STATUSSCREEN:ammo.spr");
PAD(igd, 53FE48, 53FE5E, 0x2);
DATA(540B5C) uint8_t gStatusScreenEvent INIT(255);
PAD(igd, 540B5C, 540B5D, 0x3);
STORE(igd, 540B5C, 4) __typeof__(gStatusPortraitNames[0]) store_540B60[4] INIT({(__typeof__(store_540B60[0]))ref_str_540CEC, (__typeof__(store_540B60[1]))ref_str_540CD4, (__typeof__(store_540B60[2]))ref_str_53FE48, (__typeof__(store_540B60[3]))ref_str_540CBC});
ALIAS(gStatusPortraitNames, store_540B60, 0);
STORE(igd, 540B5C, 4) __typeof__(gStatusActionNames[0]) store_540B70[10] INIT({(__typeof__(store_540B70[0]))ref_str_540C98, (__typeof__(store_540B70[1]))ref_str_540C78, (__typeof__(store_540B70[2]))ref_str_540C54, (__typeof__(store_540B70[3]))ref_str_540C3C, (__typeof__(store_540B70[4]))ref_str_540C18, (__typeof__(store_540B70[5]))ref_str_540C00, (__typeof__(store_540B70[6]))ref_str_540BE4, (__typeof__(store_540B70[7]))ref_str_540BC8, (__typeof__(store_540B70[8]))ref_str_540BB0, (__typeof__(store_540B70[9]))ref_str_540B98});
ALIAS(gStatusActionNames, store_540B70, 0);
DATA(540B5C) char str_540B98[22] INIT("STATUSSCREEN:a_c4.spr");
PAD(igd, 540B5C, 540BAE, 0x2);
DATA(540B5C) char str_540BB0[24] INIT("STATUSSCREEN:a_wire.spr");
DATA(540B5C) char str_540BC8[26] INIT("STATUSSCREEN:a_ladder.spr");
PAD(igd, 540B5C, 540BE2, 0x2);
DATA(540B5C) char str_540BE4[25] INIT("STATUSSCREEN:a_fence.spr");
PAD(igd, 540B5C, 540BFD, 0x3);
DATA(540B5C) char str_540C00[24] INIT("STATUSSCREEN:a_door.spr");
DATA(540B5C) char str_540C18[36] INIT("STATUSSCREEN:action_pressbutton.spr");
DATA(540B5C) char str_540C3C[23] INIT("STATUSSCREEN:a_gun.spr");
PAD(igd, 540B5C, 540C53, 0x1);
DATA(540B5C) char str_540C54[33] INIT("STATUSSCREEN:action_picklock.spr");
PAD(igd, 540B5C, 540C75, 0x3);
DATA(540B5C) char str_540C78[31] INIT("STATUSSCREEN:action_comput.spr");
PAD(igd, 540B5C, 540C97, 0x1);
DATA(540B5C) char str_540C98[34] INIT("STATUSSCREEN:action_searchcab.spr");
PAD(igd, 540B5C, 540CBA, 0x2);
DATA(540B5C) char str_540CBC[24] INIT("STATUSSCREEN:weapon.spr");
DATA(540B5C) char str_540CD4[22] INIT("STATUSSCREEN:note.spr");
PAD(igd, 540B5C, 540CEA, 0x2);
DATA(540B5C) char str_540CEC[22] INIT("STATUSSCREEN:anya.spr");
PAD(igd, 540B5C, 540D02, 0x2);
BSS(5C8A48) StatusScreen *gStatusScreen;
STORE(igb, 5C8A48, 4) __typeof__(gStatusActionSprites[0]) store_5C8A4C[10];
ALIAS(gStatusActionSprites, store_5C8A4C, 0);
BSS(5C8A48) uint16_t gStatusScreenType;
PAD(igb, 5C8A48, 5C8A76, 0x2);
