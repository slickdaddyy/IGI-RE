#ifndef RES_SPRITE_H
#define RES_SPRITE_H

#include "igi.h"

/* Filled in by the render driver */
typedef struct Palette {
    int device;
    int unk4;
    int unk8;
} Palette;

typedef struct Texture {
    int device;
    int unk[8];
} Texture;

#define FRAME_FIXED 0x10000

typedef struct SpriteFrame {
    Texture *texture;
    unsigned int flags;
    short x, y;
    short w, h;
    short advance;
    short pad;
    float uv[4];
} SpriteFrame;

/* A set of images, or a font. frames[nframes] holds the size of the
   whole set and the font metrics. */
typedef struct Sprite {
    Palette *palette;
    Texture *textures;
    SpriteFrame *frames;
    struct Grid *map;          /* character -> frame + 1 */
    int ntextures;
    unsigned short nframes;
    unsigned char format;
} Sprite;

/* A block of characters drawn with a sprite, each cell is a frame + 1 */
typedef struct Grid {
    int proportional;
    short cols, rows;
    Sprite *sprite;
    short *widths;             /* pixel width of each row */
    unsigned char cells[];
} Grid;

typedef struct Text {
    Grid *grid;
    float x, y;
    float z;
    float alpha;
    float color[3];
    int unk20;
    unsigned int flags;
} Text;

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
