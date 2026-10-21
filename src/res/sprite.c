#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "res/resource.h"
#include "res/sprite.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1FF0, int, Texture_Count, (Chunk *form));
ORIG(0x4B2070, int, Texture_LoadAll, (Chunk *form, Texture *t, int count, int unk));
ORIG(0x4B22F0, void, Texture_Free, (Texture *t));
ORIG(0x4B23B0, void, Texture_Create, (Texture *t, Palette *p, int a, int w, int h, int format, int b, int c, void *pixels));

void Sprite_LoadTexture(Sprite *s, int i, void *pixels, int a, int w, int h, int format, int b, int c)
{
    Texture_Create(&s->textures[i], s->palette, a, w, h, format, b, c, pixels);
}
HOOK(0x4B64F0, Sprite_LoadTexture);

void Sprite_SetFrame(Sprite *s, int i, int texture, int w, int h, int x, int y, int advance, unsigned int flags,
                     float u0, float v0, float u1, float v1)
{
    SpriteFrame *f = &s->frames[i];

    f->x = x;
    f->y = y;
    f->w = w;
    f->h = h;
    f->advance = advance;
    f->flags = flags;
    f->texture = &s->textures[texture];
    f->uv[0] = u0;
    f->uv[1] = v0;
    f->uv[2] = u1;
    f->uv[3] = v1;
}
HOOK(0x4B6540, Sprite_SetFrame);

void Sprite_Init(Sprite *s, int format, int x, int y, int w, int h, int advance, unsigned int flags,
                 Palette *palette, int nframes, int ntextures)
{
    size_t size = (nframes + 1) * sizeof(SpriteFrame);
    SpriteFrame *f;

    s->palette = palette;
    s->nframes = nframes;
    s->map = NULL;
    s->format = format;
    f = Mem_Alloc(size + ntextures * sizeof(Texture), 4);
    s->ntextures = ntextures;
    s->frames = f;
    s->textures = (Texture *)((char *)f + size);
    memset(f, 0, size);
    f = &s->frames[nframes];
    f->flags = flags;
    f->x = x;
    f->y = y;
    f->w = w;
    f->h = h;
    f->advance = advance;
}
HOOK(0x4B65E0, Sprite_Init);

void Sprite_Free(Sprite *s)
{
    int i;

    if (s->palette) {
        Palette_Free(s->palette);
        Mem_Free(s->palette);
        s->palette = NULL;
    }
    if (s->map)
        Grid_Free(s->map);
    if (s->ntextures)
        for (i = 0; i < s->ntextures; i++)
            Texture_Free(&s->textures[i]);
    Mem_Free(s->frames);
    s->frames = NULL;
}
HOOK(0x4B66A0, Sprite_Free);

/* "FONT" ILFF forms */
typedef struct FontHeader {
    int proportional;
    int nframes;
    int unk8;
    int advance;
    int w, h;
} FontHeader;

typedef struct FontFrame {
    int flags;
    float uv[4];
    short texture, w, h, advance;
    short unk1C[3];
    short x, y;
    short unk26;
} FontFrame;

/* "LOOP" sprite files */
typedef struct LoopHeader1 {
    char magic[4];
    int version;
    int unk8;
    int palette;
    short unk10[3];
    unsigned short a, w, h;
} LoopHeader1;

typedef struct LoopHeader {
    char magic[4];
    int version;
    int unk8;
    int unkC;
    short unk10;
    short unk12;               /* passed on to the textures */
    int unk14;
    int palette;
    int charmap;
    int nframes;
    int unk24;
    int w, h;
    int format;
} LoopHeader;

typedef struct LoopFrame7 {
    int pixels;
    int a, w, h;
    int advance;
    int unk14[3];
    int x, y;
} LoopFrame7;

typedef struct LoopFrame9 {
    int pixels;
    int format;
    unsigned short a, w, h;
    short advance;
    short unk10[3];
    short x, y;
    short pad;
    int unk1C;
} LoopFrame9;

static int Loop_Is(const char *d, int version)
{
    return d[1] == 'O' && d[2] == 'O' && d[3] == 'P' && ((const LoopHeader *)d)->version == version;
}

static Palette *Loop_Palette(const char *d, int offset, int *count)
{
    Palette *p = Mem_Alloc(sizeof(Palette), 4);

    *count = *(const int *)(d + offset + 0xC);
    Palette_Create(p, (char *)d + offset + 0x10, *count);
    return p;
}

Sprite *Sprite_Load(Sprite *s, const char *name)
{
    char *d = Resource_Load(name, NULL);
    const LoopHeader *h = (const LoopHeader *)d;
    Palette *pal = NULL;
    int format, i;

    if (!d)
        return NULL;
    if (Chunk_Id((Chunk *)d) == CHUNK_ID('I', 'L', 'F', 'F') && Chunk_FormType((Chunk *)d) == CHUNK_ID('F', 'O', 'N', 'T')) {
        Chunk *c = Chunk_FirstChild((Chunk *)d), *fnth, *tran, *anmf;
        FontHeader *fh;
        unsigned char *map;
        FontFrame *ff;
        unsigned int size;

        if (!c)
            goto corrupt;
        fnth = Chunk_Find(c, CHUNK_ID('F', 'N', 'T', 'H'));
        tran = Chunk_Find(c, CHUNK_ID('T', 'R', 'A', 'N'));
        anmf = Chunk_Find(c, CHUNK_ID('A', 'N', 'M', 'F'));
        if (!fnth || !tran || !anmf)
            goto corrupt;
        fh = Chunk_Data(fnth, (Chunk *)d);
        map = Chunk_Data(tran, (Chunk *)d);
        ff = Chunk_Data(anmf, (Chunk *)d);
        Chunk_GetInfo(&size, NULL, tran);
        Chunk_GetInfo(&size, NULL, anmf);
        if (!fh || !map || !ff)
            goto corrupt;
        Sprite_Init(s, 0, 0, 0, fh->w, fh->h, fh->advance, fh->proportional ? 0 : FRAME_FIXED, NULL, fh->nframes,
                    Texture_Count((Chunk *)d));
        if (s->ntextures <= 0)
            goto corrupt;
        Texture_LoadAll((Chunk *)d, s->textures, s->ntextures, 0);
        s->map = Grid_New(256, 1, s);
        for (i = 0; i < 256; i++, map += 2)
            Grid_Set(s->map, i, 0, *map);
        for (i = 0; i < s->nframes; i++, ff++)
            Sprite_SetFrame(s, i, ff->texture, ff->w, ff->h, ff->x, ff->y, ff->advance, ff->flags,
                            ff->uv[0], ff->uv[1], ff->uv[2], ff->uv[3]);
        return s;
    corrupt:
        Error_Show("ILF file corrupt: %s", name);
        for (;;)
            ;
    }
    if (d[0] != 'L')
        goto invalid;
    if (Loop_Is(d, 1)) {
        const LoopHeader1 *h1 = (const LoopHeader1 *)d;

        if (h1->palette)
            pal = Loop_Palette(d, h1->palette, &format);
        else
            format = 2;
        Sprite_Init(s, format, 0, 0, h1->w, h1->h, 0, FRAME_FIXED, pal, 1, 1);
        Sprite_LoadTexture(s, 0, d + sizeof(LoopHeader1), h1->a, h1->w, h1->h, format, 0, 0);
        Sprite_SetFrame(s, 0, 0, h1->w, h1->h, 0, 0, 0, 0, 0, 0, 1.0f, 1.0f);
    } else if (Loop_Is(d, 7)) {
        LoopFrame7 *f = (LoopFrame7 *)(d + sizeof(LoopHeader));

        if (h->palette)
            pal = Loop_Palette(d, h->palette, &format);
        else
            format = h->format;
        Sprite_Init(s, format, 0, 0, h->w, h->h, 0, h->nframes != 1 ? 0 : FRAME_FIXED, pal, h->nframes, h->nframes);
        for (i = 0; i < h->nframes; i++, f++) {
            Sprite_LoadTexture(s, i, d + f->pixels, f->a, f->w, f->h, format, h->unk12, 0);
            Sprite_SetFrame(s, i, i, f->w, f->h, f->x, f->y, f->advance, 0, 0, 0, 1.0f, 1.0f);
        }
        if (h->charmap)
            s->map = Grid_LoadCharMap(d + h->charmap, s);
    } else if (Loop_Is(d, 9)) {
        LoopFrame9 *f = (LoopFrame9 *)(d + sizeof(LoopHeader));

        if (h->palette)
            pal = Loop_Palette(d, h->palette, &format);
        else
            format = h->format;
        Sprite_Init(s, format, 0, 0, h->w, h->h, 0, h->nframes != 1 ? 0 : FRAME_FIXED, pal, h->nframes, h->nframes);
        for (i = 0; i < h->nframes; i++, f++) {
            Sprite_LoadTexture(s, i, d + f->pixels, f->a, f->w, f->h, f->format, h->unk12, f->unk1C);
            Sprite_SetFrame(s, i, i, f->w, f->h, f->x, f->y, f->advance, f->unk1C, 0, 0, 1.0f, 1.0f);
        }
        if (h->charmap)
            s->map = Grid_LoadCharMap(d + h->charmap, s);
    } else if (Loop_Is(d, 5) || Loop_Is(d, 10)) {
        Error_Show("The font '%s' must be packed. Ask Stein.", name);
        for (;;)
            ;
    } else {
    invalid:
        Error_Show("Invalid sprite file: %s", name);
        for (;;)
            ;
    }
    Resource_Unload(name);
    return s;
}
HOOK(0x4B6720, Sprite_Load);

int Sprite_Width(Sprite *s)
{
    return s->frames[s->nframes].w;
}
HOOK(0x4B6CE0, Sprite_Width);

int Sprite_Height(Sprite *s)
{
    return s->frames[s->nframes].h;
}
HOOK(0x4B6D00, Sprite_Height);

int Sprite_Advance(Sprite *s)
{
    return s->frames[s->nframes].advance;
}
HOOK(0x4B6D20, Sprite_Advance);

int Sprite_IsFixed(Sprite *s)
{
    return s->frames[s->nframes].flags >> 16 & 1;
}
HOOK(0x4B6D40, Sprite_IsFixed);

void Sprite_SetFixed(Sprite *s, int on)
{
    if (on)
        s->frames[s->nframes].flags |= FRAME_FIXED;
    else
        s->frames[s->nframes].flags &= ~FRAME_FIXED;
}
HOOK(0x4B6D60, Sprite_SetFixed);

Sprite *Sprite_New(const char *name)
{
    Sprite *s = Mem_Alloc(sizeof(Sprite), 4);

    if (Sprite_Load(s, name))
        return s;
    Mem_Free(s);
    return NULL;
}
HOOK(0x4B7000, Sprite_New);

void Sprite_Delete(Sprite *s)
{
    Sprite_Free(s);
    Mem_Free(s);
}
HOOK(0x4B7030, Sprite_Delete);

int Sprite_StringWidth(Sprite *s, const char *text)
{
    int len = strlen(text);
    int advance = Sprite_Advance(s);
    int w = 0, i;

    for (i = 0; i < len; i++) {
        unsigned char c = Grid_Get(s->map, (unsigned char)text[i], 0);

        w += c ? s->frames[c - 1].advance : advance;
    }
    return w;
}
HOOK(0x4B7050, Sprite_StringWidth);

int Sprite_GetHeight(Sprite *s)
{
    return Sprite_Height(s);
}
HOOK(0x4B70F0, Sprite_GetHeight);
