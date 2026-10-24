#ifndef RES_SPRITE_H
#define RES_SPRITE_H

#include "igi.h"
#include "res/restypes.h"

#define FRAME_FIXED 0x10000

/* Grid_Draw flags */
#define TEXT_CENTER 0x80
#define TEXT_RIGHT 0x100

int Sprite_Width(Sprite *s);
int Sprite_Height(Sprite *s);
int Sprite_Advance(Sprite *s);
int Sprite_IsFixed(Sprite *s);
void Sprite_SetFixed(Sprite *s, int on);
void Sprite_Free(Sprite *s);
Sprite *Sprite_Load(Sprite *s, const char *name);

Grid *Grid_LoadCharMap(const char *map, Sprite *s);
Grid *Grid_New(int cols, int rows, Sprite *s);
void Grid_Free(Grid *g);
void Grid_Set(Grid *g, int x, int y, int c);
char Grid_Get(Grid *g, int x, int y);
void Grid_UpdateRow(Grid *g, int row);
void Grid_Print(Grid *g, const char *text, int x, int y);
void Grid_Draw(Grid *g, float x, float y, float r, float gr, float b, float alpha, float z, unsigned int flags, int unk20);
int Grid_Width(Grid *g);
int Grid_Height(Grid *g);
void Grid_Clear(Grid *g);

void Palette_Create(Palette *p, void *colors, int count);
void Palette_Free(Palette *p);

void Text_Draw(Text *t);
int Text_Width(Text *t);
int Text_Height(Text *t);

void QVertex_DrawFrame(Sprite *s, float x, float y, float r, float g, float b, float alpha, float z, int frame, unsigned int flags, int unk20);

#endif
