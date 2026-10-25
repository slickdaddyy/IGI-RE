#include "hook.h"
#include "igi.h"
#include "res/restypes.h"
#include "qvertex/qvertextypes.h"
#include "core/coretypes.h"
#include "file/filevars.h"
#include "math/mathvars.h"
#include "weapon/weaponvars.h"

typedef void (*DrawFunc)(QVertexStrip *cmd);

#define COLOR_SCALE (1.0f / 128)

static void SetColor(QVertex *v, float r, float g, float b)
{
    v->r = r;
    v->g = g;
    v->b = b;
}

/* Two corner vertices of an axis aligned rectangle, uv from (0,0) to (1,1). */
static void DrawRect(float x, float y, float w, float h, float r, float g, float b, float unk14, float z,
                     int unk4, int unk8)
{
    QVertex v[2];
    QVertexStrip cmd;

    r *= COLOR_SCALE;
    g *= COLOR_SCALE;
    b *= COLOR_SCALE;
    v[0].x = x;
    v[0].y = y;
    v[0].z = z;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].a = unk14;
    SetColor(&v[0], r, g, b);
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = z;
    v[1].u = 1.0f;
    v[1].v = 1.0f;
    v[1].a = unk14;
    SetColor(&v[1], r, g, b);
    cmd.lines = 0;
    cmd.flags = unk4;
    cmd.queue = unk8;
    cmd.count = 2;
    cmd.verts = v;
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    gRenderModeCurrent.methods[gQVertexStrip](&cmd);
}

void Draw_Rect(float x, float y, float w, float h, float r, float g, float b, int unk4, int unk8)
{
    DrawRect(x, y, w, h, r, g, b, 1.0f, 1.0f, unk4, unk8);
}
HOOK(0x4B4820, Draw_Rect);

void Draw_RectZ(float x, float y, float w, float h, float r, float g, float b, int unk4, int unk8, float z)
{
    DrawRect(x, y, w, h, r, g, b, 1.0f, z, unk4, unk8);
}
HOOK(0x4B4910, Draw_RectZ);

void Draw_RectA(float x, float y, float w, float h, float r, float g, float b, float unk14, int unk4, int unk8)
{
    DrawRect(x, y, w, h, r, g, b, unk14, 1.0f, unk4, unk8);
}
HOOK(0x4B4A00, Draw_RectA);

void Draw_RectAZ(float x, float y, float w, float h, float r, float g, float b, float unk14, float z, int unk4,
                 int unk8)
{
    DrawRect(x, y, w, h, r, g, b, unk14, z, unk4, unk8);
}
HOOK(0x4B4AF0, Draw_RectAZ);

/* Four vertices, each with its own color: top left, top right, bottom left, bottom right. */
void Draw_RectGradient(float x, float y, float w, float h,
                       float r0, float g0, float b0, float r1, float g1, float b1,
                       float r2, float g2, float b2, float r3, float g3, float b3,
                       float a0, float a1, float a2, float a3, float z, int unk4, int unk8)
{
    QVertex v[4];
    QVertexStrip cmd;
    float x2 = x + w, y2 = y + h;
    int i;

    for (i = 0; i < 4; i++) {
        v[i].x = i & 1 ? x2 : x;
        v[i].y = i & 2 ? y2 : y;
        v[i].z = z;
        v[i].u = i & 1 ? 1.0f : 0.0f;
        v[i].v = i & 2 ? 1.0f : 0.0f;
    }
    v[0].a = a0;
    v[1].a = a1;
    v[2].a = a2;
    v[3].a = a3;
    SetColor(&v[0], r0 * COLOR_SCALE, g0 * COLOR_SCALE, b0 * COLOR_SCALE);
    SetColor(&v[1], r1 * COLOR_SCALE, g1 * COLOR_SCALE, b1 * COLOR_SCALE);
    SetColor(&v[2], r2 * COLOR_SCALE, g2 * COLOR_SCALE, b2 * COLOR_SCALE);
    SetColor(&v[3], r3 * COLOR_SCALE, g3 * COLOR_SCALE, b3 * COLOR_SCALE);
    cmd.lines = 0;
    cmd.flags = unk4;
    cmd.queue = unk8;
    cmd.count = 4;
    cmd.verts = v;
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    gRenderModeCurrent.methods[gQVertexStrip](&cmd);
}
HOOK(0x4B4BE0, Draw_RectGradient);

static void DrawSprite(const Sprite *sheet, float x, float y, float r, float g, float b, int frame,
                       int unk4, int unk8, float z)
{
    const SpriteFrame *f = &sheet->frames[frame];
    QVertex v[2];
    QVertexStrip cmd;

    r *= COLOR_SCALE;
    g *= COLOR_SCALE;
    b *= COLOR_SCALE;
    v[0].x = f->x + x;
    v[0].y = f->y + y;
    v[0].z = z;
    v[0].u = f->uv[0];
    v[0].v = f->uv[1];
    v[0].a = 1.0f;
    SetColor(&v[0], r, g, b);
    v[1].x = (f->x + f->w) + x;
    v[1].y = (f->y + f->h) + y;
    v[1].z = z;
    v[1].u = f->uv[2];
    v[1].v = f->uv[3];
    v[1].a = 1.0f;
    SetColor(&v[1], r, g, b);
    cmd.lines = 0;
    cmd.flags = unk4;
    cmd.queue = unk8;
    cmd.count = 2;
    cmd.verts = v;
    gQVertexTexture = f->texture;
    gQVertexPalette = sheet->palette;
    gRenderModeCurrent.methods[gQVertexStrip](&cmd);
}

void Draw_Sprite(const Sprite *sheet, float x, float y, float r, float g, float b, int frame, int unk4,
                 int unk8)
{
    DrawSprite(sheet, x, y, r, g, b, frame, unk4, unk8, 1.0f);
}
HOOK(0x4B4E00, Draw_Sprite);

void Draw_SpriteZ(const Sprite *sheet, float x, float y, float r, float g, float b, int frame, int unk4,
                  int unk8, float z)
{
    DrawSprite(sheet, x, y, r, g, b, frame, unk4, unk8, z);
}
HOOK(0x4B4F40, Draw_SpriteZ);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A84A50) struct Texture *gQVertexTexture;
BSS(A84A58) struct Palette *gQVertexPalette;
