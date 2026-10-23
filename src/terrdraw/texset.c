/* Texture sets: "LOOP" files of version 7 or 9 holding a palette and a
   list of textures. */
#include <stdint.h>

#include "hook.h"
#include "igi.h"

typedef struct TexSet {
    int format;
    int count;
    void *palette;
    char textures[][0x24];
} TexSet;

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B23B0, void, Texture_Create, (void *tex, void *palette, int pitch, int width, int height, int format,
                                      int unk7, int unk8, void *data));
ORIG(0x4B26A0, void, Texture_BeginFileLoad, (void));
ORIG(0x4B26B0, void, Texture_EndFileLoad, (void));
ORIG(0x4B5F00, char *, Resource_Load, (const char *name, int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4B7CF0, void, Palette_Create, (void *palette, void *colors, int count));
ORIG(0x4B7D30, void, Palette_Free, (void *palette));

static int is_loop(const char *p, int version)
{
    return p[0] == 'L' && p[1] == 'O' && p[2] == 'O' && p[3] == 'P' && *(const int *)(p + 4) == version;
}

static TexSet *new_set(const char *p, int *format, void **palette)
{
    int pal = *(const int *)(p + 0x18);
    TexSet *set;

    if (pal) {
        *palette = Mem_Alloc(0xC, 4);
        *format = *(const int *)(p + pal + 0xC);
        Palette_Create(*palette, (char *)p + pal + 0x10, *format);
    } else {
        *format = 2;
    }
    set = Mem_Alloc(*(const int *)(p + 0x20) * 0x24 + 0x30, 4);
    set->format = *format;
    set->palette = *palette;
    set->count = *(const int *)(p + 0x20);
    return set;
}

TexSet *TexSet_Load(const char *name)
{
    char *p, *e;
    TexSet *set;
    void *palette = NULL;
    int format;
    unsigned int i;

    Texture_BeginFileLoad();
    p = Resource_Load(name, NULL);
    if (!p) {
        Error_Show("Couldn't load texture file '%s'.", name);
        for (;;)
            ;
    }
    if (is_loop(p, 7)) {
        set = new_set(p, &format, &palette);
        e = p + 0x34;
        for (i = 0; i < *(unsigned int *)(p + 0x20); i++, e += 0x28) {
            Texture_Create(set->textures[i], set->palette, *(int *)(e + 4), *(int *)(e + 8), *(int *)(e + 0xC), format,
                           *(int16_t *)(p + 0x12), 0, p + *(int *)e);
        }
    } else if (is_loop(p, 9)) {
        set = new_set(p, &format, &palette);
        e = p + 0x34;
        for (i = 0; i < *(unsigned int *)(p + 0x20); i++, e += 0x20) {
            Texture_Create(set->textures[i], set->palette, *(uint16_t *)(e + 8), *(uint16_t *)(e + 0xA),
                           *(uint16_t *)(e + 0xC), *(int *)(e + 4), *(int16_t *)(p + 0x12), 0, p + *(int *)e);
        }
    } else {
        Error_Show("Texture file not of required type: '%s'.", name);
        for (;;)
            ;
    }
    Texture_EndFileLoad();
    Resource_Unload(name);
    return set;
}
HOOK(0x5175A0, TexSet_Load);

void TexSet_Free(TexSet *set)
{
    int i;

    if (set->palette) {
        Palette_Free(set->palette);
        Mem_Free(set->palette);
        set->palette = NULL;
    }
    for (i = 0; i < set->count; i++)
        Texture_Free(set->textures[i]);
    Mem_Free(set);
}
HOOK(0x5177F0, TexSet_Free);
