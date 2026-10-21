#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "res/resource.h"
#include "res/sprite.h"

ORIG(0x4B0E70, int, Render_Register, (const char *name));
ORIG(0x4B0EB0, void, Render_Unregister, (int id));
ORIG(0x4B0F80, int, Render_GetDevice, (void));
ORIG(0x4B1000, void, Render_Call, (int device, int id, void *arg));

#define gRenderFuncs ((void (**)(void *))0xA94E84)
#define gAllocPalette VAR(int, 0xA84A48)
#define gDeAllocPalette VAR(int, 0xA84A40)
#define gDownloadPalette VAR(int, 0xA84A44)

void Palette_Register(void)
{
    gAllocPalette = Render_Register("AllocPalette");
    gDeAllocPalette = Render_Register("DeAllocPalette");
    gDownloadPalette = Render_Register("DownloadPalette");
}
HOOK(0x4B7C80, Palette_Register);

void Palette_Unregister(void)
{
    Render_Unregister(gAllocPalette);
    Render_Unregister(gDeAllocPalette);
    Render_Unregister(gDownloadPalette);
}
HOOK(0x4B7CC0, Palette_Unregister);

void Palette_Create(Palette *p, void *colors, int count)
{
    struct {
        Palette *palette;
        int count;
        void *colors;
    } args;

    p->device = Render_GetDevice();
    args.palette = p;
    args.count = count;
    args.colors = colors;
    gRenderFuncs[gAllocPalette](&args);
}
HOOK(0x4B7CF0, Palette_Create);

void Palette_Free(Palette *p)
{
    Render_Call(p->device, gDeAllocPalette, p);
}
HOOK(0x4B7D30, Palette_Free);

void Palette_Load(Palette *p, const char *name)
{
    char *d = Resource_Load(name, NULL);

    if (d[0] == 'L' && d[1] == 'O' && d[2] == 'O' && d[3] == 'P' && *(int *)(d + 4) == 3) {
        Palette_Create(p, d + 0x10, *(int *)(d + 0xC));
        Resource_Unload(name);
        return;
    }
    Error_Show("'%s' is not a valid palette file.", name);
    for (;;)
        ;
}
HOOK(0x4B7D50, Palette_Load);
