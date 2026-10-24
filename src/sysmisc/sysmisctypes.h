#ifndef SYSMISC_SYSMISCTYPES_H
#define SYSMISC_SYSMISCTYPES_H

#include <stdint.h>

#include "res/restypes.h"

typedef struct DirEntry {
    char *name;
    unsigned long size;
    char isDir;
} DirEntry;

/* one step of a route: the next node and the distance */
typedef struct GraphStep {
    int next;           /* -1: no route */
    float dist;
} GraphStep;

/* what AllocTexture and DownloadTexture get from the engine */
typedef struct TextureDesc {
    Texture *tex;
    Palette *palette;
    int unk08;
    int pitch;
    int unk10;
    int unk14;
    int width;
    int height;
    int format;             /* 0x40: with mipmaps */
    unsigned flags;
    int mipmap;
    void *data;
} TextureDesc;

typedef struct PaletteDesc {
    Palette *palette;
    int count;
    uint32_t *colors;
} PaletteDesc;

#endif
