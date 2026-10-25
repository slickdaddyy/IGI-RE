#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "res/resource.h"
#include "res/sprite.h"
#include "core/coretypes.h"
#include "file/filevars.h"
#include "res/resvars.h"

ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int id));
ORIG(0x4B0F80, int, RenderMode_GetCurrent, (void));
ORIG(0x4B1000, void, RenderMode_Call, (int device, int id, void *arg));

void Palette_Register(void)
{
    gAllocPalette = RenderMode_CreateMethod("AllocPalette");
    gDeAllocPalette = RenderMode_CreateMethod("DeAllocPalette");
    gDownloadPalette = RenderMode_CreateMethod("DownloadPalette");
}
HOOK(0x4B7C80, Palette_Register);

void Palette_Unregister(void)
{
    RenderMode_DeleteMethod(gAllocPalette);
    RenderMode_DeleteMethod(gDeAllocPalette);
    RenderMode_DeleteMethod(gDownloadPalette);
}
HOOK(0x4B7CC0, Palette_Unregister);

void Palette_Create(Palette *p, void *colors, int count)
{
    struct {
        Palette *palette;
        int count;
        void *colors;
    } args;

    p->device = RenderMode_GetCurrent();
    args.palette = p;
    args.count = count;
    args.colors = colors;
    gRenderModeCurrent.methods[gAllocPalette](&args);
}
HOOK(0x4B7CF0, Palette_Create);

void Palette_Free(Palette *p)
{
    RenderMode_Call(p->device, gDeAllocPalette, p);
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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A84A40) int gDeAllocPalette;
BSS(A84A40) int gDownloadPalette;
BSS(A84A40) int gAllocPalette;
