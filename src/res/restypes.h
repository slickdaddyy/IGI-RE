#ifndef RES_RESTYPES_H
#define RES_RESTYPES_H

/* Filled in by the render driver */
typedef struct Palette {
    int device;
    int unk4;
    int unk8;
} Palette;

typedef struct Texture {
    int device;
    int unk4[7];
    struct IDirectDrawSurface7 *surface;
} Texture;

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

typedef struct PtrList {
    unsigned int magic;
    int current;
    int count;
    int capacity;
    unsigned int magic2;
    void *items[];
} PtrList;

#endif
