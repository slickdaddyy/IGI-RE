#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "compmap/compmap.h"

ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x505C80, uint16_t, MenuItem_GetType, (void));
ORIG(0x504DF0, uint8_t, MenuItemTask_GetListEvent, (void));
ORIG(0x504DC0, void, MenuItemTask_AddToList, (void *list, void *node));
ORIG(0x505C10, void, MenuItem_Setup, (void *item, int id, int a, int b, int c, int d, int e, void *handler));

static const float kTextZ = 0x1.053d70p+9f;

void ComputerButtonItem_Delete(ComputerButtonItem *it);
void ComputerButtonItem_Draw(ComputerButtonItem *it);
void ComputerTextItem_Delete(ComputerTextItem *it);
void ComputerTextItem_Draw(ComputerTextItem *it);
void ComputerItem_AddToList(ComputerItem *it, void *list);

void ComputerButtonItem_Init(void)
{
    uint16_t type = TaskType_New("ComputerButtonItem", sizeof(ComputerButtonItem), MenuItem_GetType(), 0);

    gButtonItemType = type;
    TaskType_SetHandler(type, 0, Task_Nothing);
    TaskType_SetHandler2(gButtonItemType, 1, Task_Nothing);
    TaskType_SetHandler2(gButtonItemType, 2, ComputerButtonItem_Delete);
    TaskType_SetHandler2(gButtonItemType, Screen_GetDrawEvent(), ComputerButtonItem_Draw);
    TaskType_SetHandler(gButtonItemType, MenuItemTask_GetListEvent(), ComputerItem_AddToList);
}
HOOK(0x46B510, ComputerButtonItem_Init);

void ComputerButtonItem_Delete(ComputerButtonItem *it)
{
    if (it->text)
        Text_Free(it->text);
}
HOOK(0x46B5A0, ComputerButtonItem_Delete);

void ComputerButtonItem_Draw(ComputerButtonItem *it)
{
    Sprite *s;

    if (it->base.hover && it->pressed)
        s = it->sprite[2];
    else if (it->base.hover)
        s = it->sprite[1];
    else
        s = it->sprite[0];
    QVertex_DrawFrame(s, it->x, it->y, 128.0f, 128.0f, 128.0f, 1.0f, kTextZ, 0, 0x400C0000, -1);
    if (it->text) {
        it->text->unk20 = -1;
        Text_DrawAlt(it->text);
    }
}
HOOK(0x46B5C0, ComputerButtonItem_Draw);

void ComputerButtonItem_Shutdown(void)
{
    TaskType_Free(gButtonItemType);
}
HOOK(0x46B650, ComputerButtonItem_Shutdown);

/* a sprite button with an optional text to the right of it */
ComputerButtonItem *ComputerButtonItem_New(Task *parent, int id, int a, int b, int c, int d, char e, float x,
                                           float y, char pressed, Sprite *font, const char *text, Sprite *normal,
                                           Sprite *hover, Sprite *down, void *handler)
{
    ComputerButtonItem *it = (ComputerButtonItem *)Task_New(parent, gButtonItemType, 0);
    float sw, sh, tw;
    long double th;

    MenuItem_Setup(it, id, a, b, c, d, e, handler);
    it->pressed = pressed;
    it->sprite[0] = normal;
    it->sprite[1] = hover;
    it->sprite[2] = down;
    sw = Sprite_Width(normal);
    sh = Sprite_Height(normal);
    if (strlen(text)) {
        it->text = Text_New(font, sw + x, y, text);
        it->text->flags |= 0x40000;
        it->text->z = kTextZ;
        tw = Text_GetWidth(it->text);
        th = Text_GetHeight(it->text);
    } else {
        th = 0;
        it->text = NULL;
        tw = 0;
    }
    it->x = x;
    it->base.x = x;
    it->base.w = tw + sw;
    it->y = y;
    it->base.self = it;
    it->base.y = y;
    it->base.h = sh > th ? sh : th;
    return it;
}
HOOK(0x46B660, ComputerButtonItem_New);

void ComputerButtonItem_SetPos(ComputerButtonItem *it, float x, float y)
{
    it->x = x;
    it->y = y;
    it->base.x = x;
    it->base.y = y;
}
HOOK(0x46B7D0, ComputerButtonItem_SetPos);

void ComputerTextItem_Init(void)
{
    uint16_t type = TaskType_New("ComputerTextItem", sizeof(ComputerTextItem), MenuItem_GetType(), 0);

    gTextItemType = type;
    TaskType_SetHandler(type, 0, Task_Nothing);
    TaskType_SetHandler2(gTextItemType, 1, Task_Nothing);
    TaskType_SetHandler2(gTextItemType, 2, ComputerTextItem_Delete);
    TaskType_SetHandler2(gTextItemType, Screen_GetDrawEvent(), ComputerTextItem_Draw);
    TaskType_SetHandler(gTextItemType, MenuItemTask_GetListEvent(), ComputerItem_AddToList);
}
HOOK(0x46B7F0, ComputerTextItem_Init);

void ComputerTextItem_Delete(ComputerTextItem *it)
{
    Text_Free(it->text);
}
HOOK(0x46B880, ComputerTextItem_Delete);

/* highlighted items are drawn half way to white */
void ComputerTextItem_Draw(ComputerTextItem *it)
{
    int c = it->color;

    it->text->unk20 = -1;
    Text_DrawAlt(it->text);
    if (it->base.hover) {
        it->text->color[0] = gItemColor[c][0] * 0.5f + 64.0f;
        it->text->color[1] = gItemColor[c][1] * 0.5f + 64.0f;
        it->text->color[2] = gItemColor[c][2] * 0.5f + 64.0f;
    } else {
        it->text->color[0] = gItemColor[c][0];
        it->text->color[1] = gItemColor[c][1];
        it->text->color[2] = gItemColor[c][2];
    }
}
HOOK(0x46B890, ComputerTextItem_Draw);

void ComputerItem_AddToList(ComputerItem *it, void *list)
{
    MenuItemTask_AddToList(list, it->node);
}
HOOK(0x46B940, ComputerItem_AddToList);

void ComputerTextItem_Shutdown(void)
{
    TaskType_Free(gTextItemType);
}
HOOK(0x46B960, ComputerTextItem_Shutdown);

ComputerTextItem *ComputerTextItem_New(Task *parent, int id, int a, int b, int c, int d, char e, float x, float y,
                                       Sprite *font, const char *text, int color, void *handler)
{
    ComputerTextItem *it = (ComputerTextItem *)Task_New(parent, gTextItemType, 0);
    float th;

    MenuItem_Setup(it, id, a, b, c, d, e, handler);
    it->text = Text_New(font, x, y, text);
    it->text->flags |= 0x40000;
    it->text->z = kTextZ;
    it->color = color;
    it->x = x;
    it->y = y;
    it->w = Text_GetWidth(it->text);
    th = Text_GetHeight(it->text);
    it->base.w = it->w;
    it->base.self = it;
    it->base.x = it->x;
    it->h = th;
    it->base.h = th;
    it->base.y = it->y;
    return it;
}
HOOK(0x46B970, ComputerTextItem_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 53F7D0, 4) const float store_53F7D0[10][3] INIT({{128.0f, 16.0f, 16.0f}, {16.0f, 96.0f, 16.0f}, {48.0f, 96.0f, 48.0f}, {128.0f, 128.0f, 128.0f}, {7.711968e-39f, 7.711934e-39f, 7.7119e-39f}, {7.711867e-39f, 7.711833e-39f, 7.711805e-39f}, {7.711777e-39f, 7.711749e-39f, 7.711721e-39f}, {7.711693e-39f, 7.711665e-39f, 7.711637e-39f}, {7.711609e-39f, 7.711581e-39f, 7.711553e-39f}, {7.711525e-39f, 7.711486e-39f, 7.711452e-39f}});
ALIAS(gItemColor, store_53F7D0, 0);
PAD_CONST(igd, 53F7D0, 53F848, 0x4) INIT({0x4C, 0xF8, 0x53});
DATA(53F84C) char str_53F84C[24] INIT("COMPUTER:h_scamera2.spr");
DATA(53F84C) char str_53F864[24] INIT("COMPUTER:h_scamera1.spr");
DATA(53F84C) char str_53F87C[25] INIT("COMPUTER:h_sentrygun.spr");
PAD(igd, 53F84C, 53F895, 0x3);
DATA(53F84C) char str_53F898[17] INIT("COMPUTER:h_9.spr");
PAD(igd, 53F84C, 53F8A9, 0x3);
DATA(53F84C) char str_53F8AC[17] INIT("COMPUTER:h_8.spr");
PAD(igd, 53F84C, 53F8BD, 0x3);
DATA(53F84C) char str_53F8C0[17] INIT("COMPUTER:h_7.spr");
PAD(igd, 53F84C, 53F8D1, 0x3);
DATA(53F84C) char str_53F8D4[17] INIT("COMPUTER:h_6.spr");
PAD(igd, 53F84C, 53F8E5, 0x3);
DATA(53F84C) char str_53F8E8[17] INIT("COMPUTER:h_5.spr");
PAD(igd, 53F84C, 53F8F9, 0x3);
DATA(53F84C) char str_53F8FC[17] INIT("COMPUTER:h_4.spr");
PAD(igd, 53F84C, 53F90D, 0x3);
DATA(53F84C) char str_53F910[17] INIT("COMPUTER:h_3.spr");
PAD(igd, 53F84C, 53F921, 0x3);
DATA(53F84C) char str_53F924[17] INIT("COMPUTER:h_2.spr");
PAD(igd, 53F84C, 53F935, 0x3);
DATA(53F84C) char str_53F938[17] INIT("COMPUTER:h_1.spr");
PAD(igd, 53F84C, 53F949, 0x3);
DATA(53F84C) char str_53F94C[17] INIT("COMPUTER:h_0.spr");
PAD(igd, 53F84C, 53F95D, 0x3);
DATA(53F84C) char str_53F960[20] INIT("COMPUTER:h_mine.spr");
DATA(53F84C) char str_53F974[23] INIT("COMPUTER:h_cabinet.spr");
PAD(igd, 53F84C, 53F98B, 0x1);
DATA(53F84C) char str_53F98C[24] INIT("COMPUTER:h_computer.spr");
DATA(53F84C) char str_53F9A4[22] INIT("COMPUTER:h_camera.spr");
PAD(igd, 53F84C, 53F9BA, 0x2);
DATA(53F84C) char str_53F9BC[21] INIT("COMPUTER:h_guard.spr");
PAD(igd, 53F84C, 53F9D1, 0x3);
DATA(53F84C) char str_53F9D4[22] INIT("COMPUTER:h_player.spr");
PAD(igd, 53F84C, 53F9EA, 0x2);
BSS(5BE2B4) uint16_t gButtonItemType;
PAD(igb, 5BE2B4, 5BE2B6, 0x6);
BSS(5BE2B4) uint16_t gTextItemType;
PAD(igb, 5BE2B4, 5BE2BE, 0x2);
