#include <string.h>

#include "hook.h"
#include "igi.h"
#include "res/sprite.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));

void Text_SetString(Text *t, const char *s);

Text *Text_Load(const char *name)
{
    Sprite *s = Mem_Alloc(sizeof(Sprite), 4);
    Text *t;

    if (!Sprite_Load(s, name)) {
        Mem_Free(s);
        return NULL;
    }
    t = Mem_Alloc(sizeof(Text), 4);
    t->grid = s->map;
    Grid_UpdateRow(s->map, 0);
    t->alpha = 1.0f;
    t->z = 1.0f;
    t->x = 0;
    t->color[0] = 128.0f;
    t->color[1] = 128.0f;
    t->color[2] = 128.0f;
    t->y = 0;
    t->unk20 = 0;
    t->flags = 0;
    return t;
}
HOOK(0x4B6DB0, Text_Load);

void Text_Delete(Text *t)
{
    Sprite *s = t->grid->sprite;

    Sprite_Free(s);
    Mem_Free(s);
    Mem_Free(t);
}
HOOK(0x4B6E30, Text_Delete);

void Text_Render(Text *t);
int Text_GetWidth(Text *t);
int Text_GetHeight(Text *t);

void Text_Draw(Text *t)
{
    Text_Render(t);
}
HOOK(0x4B6E60, Text_Draw);

int Text_Width(Text *t)
{
    return Text_GetWidth(t);
}
HOOK(0x4B6E70, Text_Width);

int Text_Height(Text *t)
{
    return Text_GetHeight(t);
}
HOOK(0x4B6E80, Text_Height);

void Text_SetString(Text *t, const char *s)
{
    Grid_Clear(t->grid);
    Grid_Print(t->grid, s, 0, 0);
}
HOOK(0x4B6E90, Text_SetString);

Text *Text_New(Sprite *s, float x, float y, const char *str)
{
    Text *t = Mem_Alloc(sizeof(Text), 4);

    memset(t, 0, sizeof(*t));
    t->grid = Grid_New(256, 1, s);
    t->color[0] = 128.0f;
    t->color[1] = 128.0f;
    t->color[2] = 128.0f;
    t->unk20 = 0;
    t->flags = 0;
    t->alpha = 1.0f;
    t->x = x;
    t->y = y;
    Text_SetString(t, str);
    return t;
}
HOOK(0x4B6EC0, Text_New);

void Text_Free(Text *t)
{
    Grid_Free(t->grid);
    Mem_Free(t);
}
HOOK(0x4B6F30, Text_Free);

void Text_DrawAlt(Text *t)
{
    Grid_Draw(t->grid, t->x, t->y, t->color[0], t->color[1], t->color[2], t->alpha, t->z, t->flags | 0x40000000,
              t->unk20);
}
HOOK(0x4B6F50, Text_DrawAlt);

/* fistp, so it follows the FPU rounding mode */
static int Round(float f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

void Text_GetRect(Text *t, float *rect)
{
    rect[0] = (float)Round(t->x);
    rect[1] = (float)Round(t->y);
    rect[2] = (float)Text_GetWidth(t);
    rect[3] = (float)Text_GetHeight(t);
}
HOOK(0x4B6F90, Text_GetRect);

void Text_Render(Text *t)
{
    Grid_Draw(t->grid, t->x, t->y, t->color[0], t->color[1], t->color[2], t->alpha, t->z, t->flags, t->unk20);
}
HOOK(0x4B7DB0, Text_Render);

int Text_GetWidth(Text *t)
{
    return Grid_Width(t->grid);
}
HOOK(0x4B7DF0, Text_GetWidth);

int Text_GetHeight(Text *t)
{
    return Grid_Height(t->grid);
}
HOOK(0x4B7E00, Text_GetHeight);
