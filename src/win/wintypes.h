#ifndef WIN_WINTYPES_H
#define WIN_WINTYPES_H

/* A screen mode. The active one is at 0xC28B40. */
typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
    int backBuffers;
    int depth;
    char unk14;
    char unk15;
    char unk16;
    char unk17;
    int renderMode;
    char device[0x80];
    char desc[0x80];
} DisplayMode;
_Static_assert(sizeof(DisplayMode) == 0x11C, "DisplayMode");

#endif
