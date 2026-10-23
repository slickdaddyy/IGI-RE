#include "hook.h"
#include "igi.h"

/* Install and remove the Direct3D versions of the texture and QVertex
   render functions. */

ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x50BD80, void, sub_50BD80, (void));
ORIG(0x50EC00, void, sub_50EC00, (void));
ORIG(0x50DC30, void, sub_50DC30, (void));

void D3DTex_Init(void);
void D3DTex_Shutdown(void);

#define gQVertexDrawStrip VAR(int, 0x54E890)

void D3DTex_Register(void)
{
    sub_50BD80();
    D3DTex_Init();
}
HOOK(0x509910, D3DTex_Register);

void D3DTex_Unregister(void)
{
    D3DTex_Shutdown();
    Debug_Log();
}
HOOK(0x509920, D3DTex_Unregister);

void QVertex_InitDriver(void)
{
    gQVertexDrawStrip = RenderMode_CreateMethod("QVertex:DrawStrip");
    Debug_Log();
    sub_50EC00();
    sub_50DC30();
}
HOOK(0x509930, QVertex_InitDriver);

void QVertex_ShutdownDriver(void)
{
    Debug_Log();
    Debug_Log();
    Debug_Log();
    RenderMode_DeleteMethod(gQVertexDrawStrip);
}
HOOK(0x509960, QVertex_ShutdownDriver);
