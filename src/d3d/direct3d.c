#include <string.h>

#include "hook.h"
#include "d3d/d3d.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x48F0A0, HWND, AppContext_GethWnd, (void));
ORIG(0x48F130, int, AppContext_IsFullscreen, (void));
ORIG(0x48F140, void, AppContext_SetActive, (int active));
ORIG(0x48F170, int, AppContext_IsActive, (void));
ORIG(0x48F490, void, AppMain_AddWinProcCallback, (void *fn));
ORIG(0x48F4F0, void, AppMain_RemoveWinProcCallback, (void *fn));
ORIG(0x491CA0, void, Display_RunCallbacks, (D3DCallback *list));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x491EF0, void, Display_EnumDrivers, (DDEnumCtx *ctx));
ORIG(0x491DB0, int, Display_GetClipWindowPrim, (void));
ORIG(0x491DC0, int, Display_GetBlitPrim, (void));
ORIG(0x491DD0, int, Display_GetSnapshotPrim, (void));
ORIG(0x491DE0, int, Display_GetGFXCapsMethod, (void));
ORIG(0x491DF0, int, Display_GetOpenMethod, (void));
ORIG(0x491E00, int, Display_GetCloseMethod, (void));
ORIG(0x491E10, int, Display_GetSetModeMethod, (void));
ORIG(0x491E20, int, Display_GetUnsetModeMethod, (void));
ORIG(0x491E30, int, Display_GetVSyncMethod, (void));
ORIG(0x491E40, int, Display_GetSwapBuffersMethod, (void));
ORIG(0x491E50, int, Display_GetClearMethod, (void));
ORIG(0x491E60, int, Display_GetEnumMethod, (void));
ORIG(0x498460, void, FPU_SetTruncate, (void));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B0E00, int, RenderMode_Create, (const char *name));
ORIG(0x4B0E50, void, RenderMode_Delete, (int mode));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B0F80, int, RenderMode_GetCurrent, (void));
ORIG(0x4B73F0, void *, PtrList_New, (int size));
ORIG(0x4B7430, int, PtrList_Add, (void *list, void *p));
ORIG(0x4B7450, void, PtrList_Clear, (void *list, int index));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D3710, void, Task_Nothing, (void));
/* the exe's import thunk; DDRAW.dll is not linked into our DLL */
ORIG_STD(0x4AF752, HRESULT, DirectDrawCreateEx_, (GUID *guid, void **out, REFIID iid, IUnknown *outer));

#define IID_IDirect3DTnLHalDevice_ ((const GUID *)0x5349E0)
#define IID_IDirect3DRefDevice_ ((const GUID *)0x5349F0)
#define IID_IDirect3DHALDevice_ ((const GUID *)0x534A00)
#define IID_IDirect3DRGBDevice_ ((const GUID *)0x534A10)
#define IID_IDirect3D7_ ((const GUID *)0x534A20)
#define IID_IDirectDraw7_ ((const GUID *)0x534A50)

typedef struct DynVB {
    IDirect3DVertexBuffer7 *vb;
    int size;
    int used;
} DynVB;

typedef struct ClipRect {
    int unk0;
    int unk4;
    int unk8;
    float x;
    float y;
    float w;
    float h;
} ClipRect;

typedef struct BlitArgs {
    int unk0;
    int unk4;
    int unk8;
    int unkC;
    int x;
    int y;
    int w;
    int h;
    unsigned *pixels;
    int toPrimary;
} BlitArgs;

typedef struct SnapshotArgs {
    int unk0;
    int unk4;
    int unk8;
    unsigned char *pixels;
} SnapshotArgs;

typedef struct EnumArgs {
    struct {
        char name[0x80];
        char desc[0x80];
    } *drivers;
    int *count;
    int max;
} EnumArgs;

typedef struct ZBufferSearch {
    DDPIXELFORMAT pf;
    unsigned maxDepth;
} ZBufferSearch;

#define gScreenHeight VAR(int, 0x5CA044)
#define gDisplayEnumMode VAR(int, 0x5CA048)
#define gScreenWidth VAR(int, 0x5CA0D8)
#define gD3DRenderMode VAR(int, 0x5CA0E0)
#define gD3DReady VAR(char, 0x5CA0F8)
#define gD3DPrimary VAR(IDirectDrawSurface7 *, 0x5CA0FC)
#define gD3DBack VAR(IDirectDrawSurface7 *, 0x5CA100)
#define gD3DZBuffer VAR(IDirectDrawSurface7 *, 0x5CA104)
#define gD3DFogTable VAR(int, 0x5CA108)
#define gDDraw VAR(IDirectDraw7 *, 0x5CA10C)
#define gD3D VAR(IDirect3D7 *, 0x5CA110)
#define gD3DDevice VAR(IDirect3DDevice7 *, 0x5CA114)
#define gD3DSetModeCallbacks VAR(D3DCallback *, 0x5CA118)
#define gD3DUnsetModeCallbacks VAR(D3DCallback *, 0x5CA11C)
#define gD3DRestoreCallbacks VAR(D3DCallback *, 0x5CA120)
#define gTexturesCreated VAR(int, 0x5CA124)
#define gTextureBytes VAR(int, 0x5CA128)
#define gBytesToMb VAR(float, 0x5333C0)
#define gD3DDeviceDesc VAR(D3DDEVICEDESC7, 0xBCAC00)
#define gD3DFoundHal VAR(int, 0xBCACF4)
#define gD3DTnL VAR(int, 0xBCAD74)
#define gD3DZ24 VAR(int, 0xBCADA4)
#define gVBTL VAR(DynVB, 0xBCAD00)
#define gVBLit VAR(DynVB, 0xBCAD80)
#define gVBNormal VAR(DynVB, 0xBCAD90)
#define gD3DCallbackPool ((D3DCallback *)0xBCAD20)
#define gClearRed VAR(int, 0xBCADF0)
#define gClearGreen VAR(int, 0xBCADF4)
#define gClearBlue VAR(int, 0xBCADF8)
#define gDisplayModes ((DisplayMode *)0xBCAE20)
#define gDisplayModeCount VAR(int, 0xC10384)
#define gD3DListA VAR(int *, 0xBCADE8)
#define gD3DListB VAR(int *, 0xBCAD7C)
#define gD3DListC VAR(int *, 0xBCADAC)
#define gD3DStateList VAR(int *, 0xBCADC4)
#define gD3DClippingOnIndex VAR(int, 0xBCACEC)
#define gD3DClippingOffIndex VAR(int, 0xBCADE0)
#define gD3DMinZ VAR(float, 0xBCADA0)
#define gD3DMaxZ VAR(float, 0xBCADC8)
#define gD3DViewMinZ VAR(float, 0xBCADB4)
#define gD3DViewMaxZ VAR(float, 0xBCAD8C)

#define DX_ERROR "%s\n\nDirectX error message: %s"

static void __attribute__((noreturn)) DXError(const char *what, HRESULT hr)
{
    char buf[1024];

    crt_sprintf(buf, what);
    Error_Show(DX_ERROR, buf, DDraw_GetErrorString(hr));
    for (;;)
        ;
}

/* Picks the hardware device, preferring the TnL one. */
HRESULT __stdcall Direct3D_FindDeviceCB(char *desc, char *name, D3DDEVICEDESC7 *dev, D3DDEVICEDESC7 *out)
{
    if (!memcmp(&dev->deviceGUID, IID_IDirect3DHALDevice_, sizeof(GUID)))
        gD3DFoundHal = 1;
    if (!memcmp(&dev->deviceGUID, IID_IDirect3DTnLHalDevice_, sizeof(GUID))) {
        gD3DTnL = 1;
        gD3DFoundHal = 1;
    } else if (!gD3DFoundHal) {
        return 1;
    }
    *out = *dev;
    gD3DFogTable = (out->dpcTriCaps.dwRasterCaps >> 8) & 1;
    return gD3DTnL ? 0 : 1;
}
HOOK(0x492230, Direct3D_FindDeviceCB);

HRESULT __stdcall Direct3D_ListDeviceCB(char *desc, char *name, D3DDEVICEDESC7 *dev, DDEnumCtx *ctx)
{
    D3DDeviceEntry *e;

    if (ctx->listDevices != 1)
        return 1;
    if (ctx->deviceName && !crt_strstr(desc, ctx->deviceName))
        return 1;
    e = &ctx->devices[*ctx->deviceCount];
    strcpy(e->desc, desc);
    e->d3dDesc = *dev;
    e->driver = ctx->cur;
    e->hwRasterization = (dev->dwDevCaps >> 19) & 1;
    ++*ctx->deviceCount;
    return ctx->deviceName ? 0 : 1;
}
HOOK(0x4922B0, Direct3D_ListDeviceCB);

void Guid_ToString(char *buf, GUID g)
{
    crt_sprintf(buf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g.Data1, g.Data2, g.Data3, g.Data4[0],
                g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
}
HOOK(0x492370, Guid_ToString);

void Direct3D_VSync(void)
{
    HRESULT hr = IDirectDraw7_WaitForVerticalBlank(gDDraw, DDWAITVB_BLOCKEND, NULL);

    if (hr)
        DXError("Couldn't wait for vblank", hr);
}
HOOK(0x492400, Direct3D_VSync);

void Direct3D_Clear(void)
{
    HRESULT hr;

    if (!gD3DDevice)
        return;
    hr = IDirect3DDevice7_Clear(gD3DDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                (((gClearRed << 8) + gClearGreen) << 8) + gClearBlue, 1.0f, 0);
    if (hr)
        DXError("Couldn't clear screen", hr);
}
HOOK(0x492450, Direct3D_Clear);

HRESULT __stdcall Direct3D_EnumModeCB(DDSURFACEDESC2 *sd, DDDriver *drv);

void Direct3D_Enum(EnumArgs *args)
{
    DDEnumCtx ctx;
    DDDriver drivers[50];
    IDirectDraw7 *dd;
    HRESULT hr;
    int i, n;

    ctx.listDevices = 0;
    ctx.deviceName = NULL;
    ctx.driverName = NULL;
    ctx.drivers = drivers;
    ctx.count = args->count;
    Display_EnumDrivers(&ctx);
    n = *ctx.count;
    if (n >= args->max)
        n = args->max;
    for (i = 0; i < n; i++) {
        strcpy(args->drivers[i].desc, drivers[i].desc);
        strcpy(args->drivers[i].name, drivers[i].name);
        hr = DirectDrawCreateEx_(drivers[i].guid, (void **)&dd, IID_IDirectDraw7_, NULL);
        if (hr)
            DXError("Couldn't create DirectDraw object", hr);
        hr = IDirectDraw7_EnumDisplayModes(dd, 0, NULL, &drivers[i], (LPDDENUMMODESCALLBACK2)Direct3D_EnumModeCB);
        if (hr)
            DXError("Couldn't enumerate display modes.", hr);
        IDirectDraw7_Release(dd);
    }
    FPU_SetTruncate();
}
HOOK(0x4924D0, Direct3D_Enum);

HRESULT __stdcall Direct3D_EnumModeCB(DDSURFACEDESC2 *sd, DDDriver *drv)
{
    DisplayMode *m;
    unsigned depths;

    if (gDisplayModeCount >= 1000)
        return 0;
    if (!(sd->ddpfPixelFormat.dwFlags & DDPF_RGB))
        return 1;
    depths = drv->d3dDesc.dwDeviceRenderBitDepth;
    switch (sd->ddpfPixelFormat.dwRGBBitCount) {
    case 8:
        if (depths & DDBD_8)
            break;
        return 1;
    case 16:
        if (depths & DDBD_16)
            break;
        return 1;
    case 24:
        if (depths & DDBD_24)
            break;
        return 1;
    case 32:
        if (depths & DDBD_32)
            break;
        return 1;
    default:
        return 1;
    }
    m = &gDisplayModes[gDisplayModeCount];
    memset(m, 0, sizeof(*m));
    m->width = sd->dwWidth;
    m->height = sd->dwHeight;
    m->depth = sd->ddpfPixelFormat.dwRGBBitCount;
    m->renderMode = gDisplayEnumMode;
    m->backBuffers = 2;
    strcpy(m->desc, drv->desc);
    strcpy(m->device, drv->name);
    gDisplayModeCount++;
    return 1;
}
HOOK(0x492670, Direct3D_EnumModeCB);

HRESULT __stdcall Direct3D_ZBufferCapsCB(DDPIXELFORMAT *pf, char *out)
{
    if ((pf->dwFlags & (DDPF_ZBUFFER | DDPF_STENCILBUFFER)) == (DDPF_ZBUFFER | DDPF_STENCILBUFFER)) {
        crt_sprintf(out, "%s    %d bits z and stencil\n", out, pf->dwZBufferBitDepth);
        return 1;
    }
    if ((pf->dwFlags & DDPF_ZBUFFER) == DDPF_ZBUFFER)
        crt_sprintf(out, "%s    %d bits z\n", out, pf->dwZBufferBitDepth);
    return 1;
}
HOOK(0x492B70, Direct3D_ZBufferCapsCB);

static const char *YesNo(int on)
{
    return on ? "Yes" : "No";
}

void Direct3D_GetGFXCaps(char *out)
{
    const GUID *guid = gD3DTnL ? IID_IDirect3DTnLHalDevice_ : IID_IDirect3DHALDevice_;
    D3DDEVICEDESC7 desc;
    DDCAPS hal, hel;
    unsigned raster, tex;
    HRESULT hr;

    *(unsigned *)(out + 10000) = 0xABCD1234;
    hr = IDirect3DDevice7_GetCaps(gD3DDevice, &desc);
    if (hr)
        DXError("Couldn't get device caps", hr);
    hal.dwSize = sizeof(hal);
    hel.dwSize = sizeof(hel);
    hr = IDirectDraw7_GetCaps(gDDraw, &hal, &hel);
    if (hr)
        DXError("Couldn't get driver caps", hr);
    crt_sprintf(out, "Direct3D hardware caps\n----------------------\n\n");
    crt_sprintf(out, "%sRasterization caps\n", out);
    crt_sprintf(out, "%s  ZBuffer formats:\n", out);
    IDirect3D7_EnumZBufferFormats(gD3D, guid, (LPD3DENUMPIXELFORMATSCALLBACK)Direct3D_ZBufferCapsCB, out);
    raster = desc.dpcTriCaps.dwRasterCaps;
    crt_sprintf(out, "%s  WBuffer                %s\n", out, YesNo(raster & D3DPRASTERCAPS_WBUFFER));
    crt_sprintf(out, "%s  Depth bias             %s\n", out, YesNo(raster & D3DPRASTERCAPS_ZBIAS));
    crt_sprintf(out, "%s  ZFog                   %s\n", out, YesNo(raster & D3DPRASTERCAPS_ZFOG));
    crt_sprintf(out, "%s  WFog                   %s\n", out, YesNo(raster & D3DPRASTERCAPS_WFOG));
    crt_sprintf(out, "%s  Range fog              %s\n", out, YesNo(raster & D3DPRASTERCAPS_FOGRANGE));
    crt_sprintf(out, "%s  Per-pixel fog          %s\n", out, YesNo(raster & D3DPRASTERCAPS_FOGTABLE));
    crt_sprintf(out, "%s  Per-vertex fog         %s\n", out, YesNo(raster & D3DPRASTERCAPS_FOGVERTEX));
    crt_sprintf(out, "%s  Scene capture          %s\n", out, YesNo(raster & D3DPRASTERCAPS_ZBUFFERLESSHSR));
    crt_sprintf(out, "%s  Transp. sorting        %s\n", out, YesNo(raster & D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT));
    crt_sprintf(out, "%s  Antialiasing           %s\n", out, YesNo(raster & D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT));
    crt_sprintf(out, "%s  Anisotropic filtering  %s\n", out, YesNo(raster & D3DPRASTERCAPS_ANISOTROPY));
    crt_sprintf(out, "%s  Mipmap lod bias        %s\n", out, YesNo(raster & D3DPRASTERCAPS_MIPMAPLODBIAS));
    crt_sprintf(out, "%s  Subpixel rasterizing   %s\n\n", out, YesNo(raster & D3DPRASTERCAPS_SUBPIXEL));
    crt_sprintf(out, "%sTexture caps\n", out);
    tex = desc.dpcTriCaps.dwTextureCaps;
    crt_sprintf(out, "%s  Must have size 2^x     %s\n", out, YesNo(tex & D3DPTEXTURECAPS_POW2));
    crt_sprintf(out, "%s  Can have size not 2^x  %s\n\n", out, YesNo(tex & D3DPTEXTURECAPS_NONPOW2CONDITIONAL));
    crt_sprintf(out, "%sResources in use\n", out);
    crt_sprintf(out, "%s  Textures created       %d\n", out, gTexturesCreated);
    crt_sprintf(out, "%s  Texture footprint      %.2f mb\n", out, (double)((long double)gTextureBytes * gBytesToMb));
    crt_sprintf(out, "%s  VRAM usage             %.2f mb\n", out,
                (double)((long double)(hal.dwVidMemTotal - hal.dwVidMemFree) * gBytesToMb));
}
HOOK(0x4927B0, Direct3D_GetGFXCaps);

const char *DDraw_GetErrorString(HRESULT hr)
{
    switch ((unsigned)hr) {
    case 0x80004001: return "DDERR_UNSUPPORTED";
    case 0x80004005: return "DDERR_GENERIC";
    case 0x8007000E: return "DDERR_OUTOFMEMORY";
    case 0x80070057: return "DDERR_INVALIDPARAMS";
    case 0x88760005: return "DDERR_ALREADYINITIALIZED";
    case 0x8876000A: return "DDERR_CANNOTATTACHSURFACE";
    case 0x88760014: return "DDERR_CANNOTDETACHSURFACE";
    case 0x88760028: return "DDERR_CURRENTLYNOTAVAIL";
    case 0x88760037: return "DDERR_EXCEPTION";
    case 0x8876005A: return "DDERR_HEIGHTALIGN";
    case 0x8876005F: return "DDERR_INCOMPATIBLEPRIMARY";
    case 0x88760064: return "DDERR_INVALIDCAPS";
    case 0x8876006E: return "DDERR_INVALIDCLIPLIST";
    case 0x88760078: return "DDERR_INVALIDMODE";
    case 0x88760082: return "DDERR_INVALIDOBJECT";
    case 0x88760091: return "DDERR_INVALIDPIXELFORMAT";
    case 0x88760096: return "DDERR_INVALIDRECT";
    case 0x887600A0: return "DDERR_LOCKEDSURFACES";
    case 0x887600AA: return "DDERR_NO3D";
    case 0x887600B4: return "DDERR_NOALPHAHW";
    case 0x887600CD: return "DDERR_NOCLIPLIST";
    case 0x887600D2: return "DDERR_NOCOLORCONVHW";
    case 0x887600D4: return "DDERR_NOCOOPERATIVELEVELSET";
    case 0x887600D7: return "DDERR_NOCOLORKEY";
    case 0x887600DC: return "DDERR_NOCOLORKEYHW";
    case 0x887600DE: return "DDERR_NODIRECTDRAWSUPPORT";
    case 0x887600E1: return "DDERR_NOEXCLUSIVEMODE";
    case 0x887600E6: return "DDERR_NOFLIPHW";
    case 0x887600F0: return "DDERR_NOGDI";
    case 0x887600FA: return "DDERR_NOMIRRORHW";
    case 0x887600FF: return "DDERR_NOTFOUND";
    case 0x88760104: return "DDERR_NOOVERLAYHW";
    case 0x88760118: return "DDERR_NORASTEROPHW";
    case 0x88760122: return "DDERR_NOROTATIONHW";
    case 0x88760136: return "DDERR_NOSTRETCHHW";
    case 0x8876013C: return "DDERR_NOT4BITCOLOR";
    case 0x8876013D: return "DDERR_NOT4BITCOLORINDEX";
    case 0x88760140: return "DDERR_NOT8BITCOLOR";
    case 0x8876014A: return "DDERR_NOTEXTUREHW";
    case 0x8876014F: return "DDERR_NOVSYNCHW";
    case 0x88760154: return "DDERR_NOZBUFFERHW";
    case 0x8876015E: return "DDERR_NOZOVERLAYHW";
    case 0x88760168: return "DDERR_OUTOFCAPS";
    case 0x8876017C: return "DDERR_OUTOFVIDEOMEMORY";
    case 0x8876017E: return "DDERR_OVERLAYCANTCLIP";
    case 0x88760180: return "DDERR_OVERLAYCOLORKEYONLYONEACTIVE";
    case 0x88760183: return "DDERR_PALETTEBUSY";
    case 0x88760190: return "DDERR_COLORKEYNOTSET";
    case 0x8876019A: return "DDERR_SURFACEALREADYATTACHED";
    case 0x887601A4: return "DDERR_SURFACEALREADYDEPENDENT";
    case 0x887601AE: return "DDERR_SURFACEBUSY";
    case 0x887601B3: return "DDERR_CANTLOCKSURFACE";
    case 0x887601B8: return "DDERR_SURFACEISOBSCURED";
    case 0x887601C2: return "DDERR_SURFACELOST";
    case 0x887601CC: return "DDERR_SURFACENOTATTACHED";
    case 0x887601D6: return "DDERR_TOOBIGHEIGHT";
    case 0x887601E0: return "DDERR_TOOBIGSIZE";
    case 0x887601EA: return "DDERR_TOOBIGWIDTH";
    case 0x887601FE: return "DDERR_UNSUPPORTEDFORMAT";
    case 0x88760208: return "DDERR_UNSUPPORTEDMASK";
    case 0x88760219: return "DDERR_VERTICALBLANKINPROGRESS";
    case 0x8876021C: return "DDERR_WASSTILLDRAWING";
    case 0x88760230: return "DDERR_XALIGN";
    case 0x88760231: return "DDERR_INVALIDDIRECTDRAWGUID";
    case 0x88760232: return "DDERR_DIRECTDRAWALREADYCREATED";
    case 0x88760233: return "DDERR_NODIRECTDRAWHW";
    case 0x88760234: return "DDERR_PRIMARYSURFACEALREADYEXISTS";
    case 0x88760235: return "DDERR_NOEMULATION";
    case 0x88760236: return "DDERR_REGIONTOOSMALL";
    case 0x88760237: return "DDERR_CLIPPERISUSINGHWND";
    case 0x88760238: return "DDERR_NOCLIPPERATTACHED";
    case 0x88760239: return "DDERR_NOHWND";
    case 0x8876023A: return "DDERR_HWNDSUBCLASSED";
    case 0x8876023B: return "DDERR_HWNDALREADYSET";
    case 0x8876023C: return "DDERR_NOPALETTEATTACHED";
    case 0x8876023D: return "DDERR_NOPALETTEHW";
    case 0x8876023E: return "DDERR_BLTFASTCANTCLIP";
    case 0x8876023F: return "DDERR_NOBLTHW";
    case 0x88760240: return "DDERR_NODDROPSHW";
    case 0x88760241: return "DDERR_OVERLAYNOTVISIBLE";
    case 0x88760242: return "DDERR_NOOVERLAYDEST";
    case 0x88760243: return "DDERR_INVALIDPOSITION";
    case 0x88760244: return "DDERR_NOTAOVERLAYSURFACE";
    case 0x88760245: return "DDERR_EXCLUSIVEMODEALREADYSET";
    case 0x88760246: return "DDERR_NOTFLIPPABLE";
    case 0x88760247: return "DDERR_CANTDUPLICATE";
    case 0x88760248: return "DDERR_NOTLOCKED";
    case 0x88760249: return "DDERR_CANTCREATEDC";
    case 0x8876024A: return "DDERR_NODC";
    case 0x8876024B: return "DDERR_WRONGMODE";
    case 0x8876024C: return "DDERR_IMPLICITLYCREATED";
    case 0x8876024D: return "DDERR_NOTPALETTIZED";
    case 0x8876024E: return "DDERR_UNSUPPORTEDMODE";
    case 0x8876024F: return "DDERR_NOMIPMAPHW";
    case 0x88760250: return "DDERR_INVALIDSURFACETYPE";
    case 0x8876026C: return "DDERR_DCALREADYCREATED";
    case 0x88760280: return "DDERR_CANTPAGELOCK";
    case 0x88760294: return "DDERR_CANTPAGEUNLOCK";
    case 0x887602A8: return "DDERR_NOTPAGELOCKED";
    case 0x887602BC: return "D3DERR_BADMAJORVERSION";
    case 0x887602BD: return "D3DERR_BADMINORVERSION";
    case 0x887602C1: return "D3DERR_INVALID_DEVICE";
    case 0x887602C2: return "D3DERR_INITFAILED";
    case 0x887602C6: return "D3DERR_EXECUTE_CREATE_FAILED";
    case 0x887602C7: return "D3DERR_EXECUTE_DESTROY_FAILED";
    case 0x887602C8: return "D3DERR_EXECUTE_LOCK_FAILED";
    case 0x887602C9: return "D3DERR_EXECUTE_UNLOCK_FAILED";
    case 0x887602CA: return "D3DERR_EXECUTE_LOCKED";
    case 0x887602CB: return "D3DERR_EXECUTE_NOT_LOCKED";
    case 0x887602CC: return "D3DERR_EXECUTE_FAILED";
    case 0x887602CD: return "D3DERR_CLIPPED_FAILED";
    case 0x887602D0: return "D3DERR_TEXTURE_NO_SUPPORT";
    case 0x887602D1: return "D3DERR_TEXTURE_CREATE_FAILED";
    case 0x887602D2: return "D3DERR_TEXTURE_DESTROY_FAILED";
    case 0x887602D3: return "D3DERR_TEXTURE_LOCK_FAILED";
    case 0x887602D4: return "D3DERR_TEXTURE_UNLOCK_FAILED";
    case 0x887602D5: return "D3DERR_LOAD_FAILED";
    case 0x887602D6: return "D3DERR_SWAP_FAILED";
    case 0x887602D7: return "D3DERR_TEXTURE_LOCKED";
    case 0x887602D8: return "D3DERR_TEXTURE_NOT_LOCKED";
    case 0x887602D9: return "D3DERR_TEXTURE_GETSURF_FAILED";
    case 0x887602DA: return "D3DERR_MATRIX_CREATE_FAILED";
    case 0x887602DB: return "D3DERR_MATRIX_DESTROY_FAILED";
    case 0x887602DC: return "D3DERR_MATRIX_SETDATA_FAILED";
    case 0x887602DD: return "D3DERR_MAXTRIX_GETDATA_FAILED";
    case 0x887602DE: return "D3DERR_SETVIEWPORTDATA_FAILED";
    case 0x887602DF: return "D3DERR_INVALIDCURRENTVIEWPORT";
    case 0x887602E0: return "D3DERR_INVALIDPRIMITIVETYPE";
    case 0x887602E1: return "D3DERR_INVALIDVERTEXTYPE";
    case 0x887602E2: return "D3DERR_TEXTURE_BADSIZE";
    case 0x887602E4: return "D3DERR_MATERIAL_CREATE_FAILED";
    case 0x887602E5: return "D3DERR_MATERIAL_DESTROY_FAILED";
    case 0x887602E6: return "D3DERR_MATERIAL_SETDATA_FAILED";
    case 0x887602E7: return "D3DERR_MATERIAL_GETDATA_FAILED";
    case 0x887602E8: return "D3DERR_INVALIDPALETTE";
    case 0x887602E9: return "D3DERR_ZBUFF_NEEDS_SYSTEMMEMORY";
    case 0x887602EA: return "D3DERR_ZBUFF_NEEDS_VIDEOMEMORY";
    case 0x887602EB: return "D3DERR_SURFACENOTINVIDMEM";
    case 0x887602EE: return "D3DERR_LIGHT_SET_FAILED";
    case 0x887602EF: return "D3DERR_LIGHTHASVIEWPORT";
    case 0x887602F0: return "D3DERR_LIGHTNOTINTHISVIEWPORT";
    case 0x887602F8: return "D3DERR_SCENE_IN_SCENE";
    case 0x887602F9: return "D3DERR_SCENE_NOT_IN_SCENE";
    case 0x887602FA: return "D3DERR_SCENE_BEGIN_FAILED";
    case 0x887602FB: return "D3DERR_SCENE_END_FAILED";
    case 0x88760302: return "D3DERR_INBEGIN";
    case 0x88760303: return "D3DERR_NOTINBEGIN";
    case 0x88760304: return "D3DERR_NOVIEWPORTS";
    case 0x88760305: return "D3DERR_VIEWPORTDATANOTSET";
    case 0x88760306: return "D3DERR_VIEWPORTHASNODEVICE";
    }
    return "Unknown error";
}
HOOK(0x492BE0, DDraw_GetErrorString);

#define gD3DUnkBCABEC VAR(float, 0xBCABEC)
#define gD3DUnkBCABF0 VAR(float, 0xBCABF0)
#define gD3DUnkBCACF0 VAR(float, 0xBCACF0)
#define gD3DUnkBCAD70 VAR(float, 0xBCAD70)
#define gD3DUnkBCAD78 VAR(int, 0xBCAD78)
#define gD3DUnkBCAD9C VAR(float, 0xBCAD9C)
#define gD3DUnkBCADA8 VAR(float, 0xBCADA8)
#define gD3DUnkBCADB0 VAR(float, 0xBCADB0)
#define gD3DUnkBCADB8 VAR(float, 0xBCADB8)
#define gD3DUnkBCADBC VAR(float, 0xBCADBC)
#define gD3DUnkBCADC0 VAR(float, 0xBCADC0)
#define gD3DUnkBCADCC VAR(float, 0xBCADCC)
#define gD3DUnkBCADD0 VAR(float, 0xBCADD0)
#define gD3DUnkBCADD4 VAR(float, 0xBCADD4)
#define gD3DUnkBCADD8 VAR(float, 0xBCADD8)
#define gD3DUnkBCADDC VAR(float, 0xBCADDC)
#define gD3DUnkBCADE4 VAR(float, 0xBCADE4)

int Direct3D_DisplayOpen(void);
void Direct3D_SetMode(DisplayMode *mode);
void Direct3D_UnsetMode(void);
void Direct3D_SwapBuffers(void);
void Direct3D_Blit(BlitArgs *a);
void Direct3D_ClipWindow(ClipRect *r);
void Direct3D_Snapshot(SnapshotArgs *a);
void Direct3D_ClippingOn(void);
void Direct3D_ClippingOff(void);
void Direct3D_RestoreVertexBuffers(void);
int Direct3D_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
void Direct3D_AddRestoreCallback(void (*fn)(void));
char Direct3D_RemoveRestoreCallback(void (*fn)(void));

void Direct3D_Init(void)
{
    gD3DRenderMode = RenderMode_Create("Direct3D");
    RenderMode_SetMethod(gD3DRenderMode, Display_GetOpenMethod(), Direct3D_DisplayOpen);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetCloseMethod(), Task_Nothing);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetSetModeMethod(), Direct3D_SetMode);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetUnsetModeMethod(), Direct3D_UnsetMode);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetVSyncMethod(), Direct3D_VSync);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetSwapBuffersMethod(), Direct3D_SwapBuffers);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetClearMethod(), Direct3D_Clear);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetEnumMethod(), Direct3D_Enum);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetBlitPrim(), Direct3D_Blit);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetClipWindowPrim(), Direct3D_ClipWindow);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetSnapshotPrim(), Direct3D_Snapshot);
    RenderMode_SetMethod(gD3DRenderMode, Display_GetGFXCapsMethod(), Direct3D_GetGFXCaps);
    gD3DUnkBCAD78 = 1;
    gD3DReady = 1;
    memset(gD3DCallbackPool, 0, 10 * sizeof(D3DCallback));
    gD3DUnkBCABEC = 0.001f;
    gD3DUnkBCAD70 = 999.99994f;
    gD3DUnkBCADDC = 204.8f;
    gD3DUnkBCADD0 = 6144.0f;
    gD3DUnkBCADC0 = 409.6f;
    gD3DUnkBCADCC = 4.096e8f;
    gD3DUnkBCADD4 = 4.096e8f;
    gD3DUnkBCADBC = 4.096e8f;
    gD3DUnkBCADD8 = 0.20480001f;
    gD3DUnkBCADB8 = 6.144f;
    gD3DMinZ = 0.40960002f;
    gD3DMaxZ = 409600.03f;
    gD3DUnkBCADB0 = 409600.03f;
    gD3DUnkBCADA8 = 409600.03f;
    gD3DUnkBCABF0 = 0.0f;
    gD3DUnkBCACF0 = 0.05f;
    gD3DViewMinZ = 0.060000002f;
    gD3DViewMaxZ = 1.0f;
    gD3DUnkBCADE4 = 1.0f;
    gD3DUnkBCAD9C = 1.0f;
    gD3DListA = PtrList_New(10);
    gD3DListB = PtrList_New(10);
    gD3DListC = PtrList_New(10);
    gD3DStateList = PtrList_New(10);
    gD3DClippingOnIndex = PtrList_Add(gD3DStateList, Direct3D_ClippingOn);
    gD3DClippingOffIndex = PtrList_Add(gD3DStateList, Direct3D_ClippingOff);
    AppMain_AddWinProcCallback(Direct3D_WinProcCB);
    Direct3D_AddRestoreCallback(Direct3D_RestoreVertexBuffers);
}
HOOK(0x493580, Direct3D_Init);

void Direct3D_FreeVertexBuffers(void);
void Direct3D_CreateVertexBuffers(void);

void Direct3D_RestoreVertexBuffers(void)
{
    Direct3D_FreeVertexBuffers();
    Direct3D_CreateVertexBuffers();
}
HOOK(0x493820, Direct3D_RestoreVertexBuffers);

static void CreateDynVB(DynVB *v, DWORD fvf, int size)
{
    D3DVERTEXBUFFERDESC d;
    HRESULT hr;

    d.dwSize = sizeof(d);
    d.dwCaps = gD3DTnL ? D3DVBCAPS_WRITEONLY : D3DVBCAPS_WRITEONLY | D3DVBCAPS_SYSTEMMEMORY;
    d.dwFVF = fvf;
    d.dwNumVertices = size;
    hr = IDirect3D7_CreateVertexBuffer(gD3D, &d, &v->vb, 0);
    if (hr)
        DXError("Couldn't create dynamic vertex buffer", hr);
    v->size = size;
}

void Direct3D_CreateVertexBuffers(void)
{
    void *p;

    CreateDynVB(&gVBNormal, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, 4000);
    CreateDynVB(&gVBLit, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, 1000);
    CreateDynVB(&gVBTL, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, 1000);
    IDirect3DVertexBuffer7_AddRef(gVBTL.vb);
    IDirect3DVertexBuffer7_Release(gVBTL.vb);
    IDirect3DVertexBuffer7_Lock(gVBTL.vb, 0, &p, NULL);
    IDirect3DVertexBuffer7_AddRef(gVBTL.vb);
    IDirect3DVertexBuffer7_Release(gVBTL.vb);
    IDirect3DVertexBuffer7_Unlock(gVBTL.vb);
}
HOOK(0x493830, Direct3D_CreateVertexBuffers);

void Direct3D_FreeVertexBuffers(void)
{
    if (gVBTL.vb) {
        IDirect3DVertexBuffer7_Release(gVBTL.vb);
        gVBTL.vb = NULL;
    }
    if (gVBLit.vb) {
        IDirect3DVertexBuffer7_Release(gVBLit.vb);
        gVBLit.vb = NULL;
    }
    if (gVBNormal.vb) {
        IDirect3DVertexBuffer7_Release(gVBNormal.vb);
        gVBNormal.vb = NULL;
    }
}
HOOK(0x493A10, Direct3D_FreeVertexBuffers);

void Direct3D_ClippingOn(void)
{
    HRESULT hr = IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_CLIPPING, TRUE);

    if (hr)
        DXError("Couldn't set renderstate", hr);
}
HOOK(0x493A60, Direct3D_ClippingOn);

void Direct3D_ClippingOff(void)
{
    HRESULT hr = IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_CLIPPING, FALSE);

    if (hr)
        DXError("Couldn't set renderstate", hr);
}
HOOK(0x493AC0, Direct3D_ClippingOff);

int Direct3D_DisplayOpen(void)
{
    return 1;
}
HOOK(0x493B20, Direct3D_DisplayOpen);

void Direct3D_UnsetMode(void)
{
    HRESULT hr;

    if (gD3DDevice) {
        IDirect3DDevice7_EndScene(gD3DDevice);
        while (IDirectDrawSurface7_GetBltStatus(gD3DBack, 0) == DDERR_WASSTILLDRAWING)
            ;
    }
    Direct3D_FreeVertexBuffers();
    Display_RunCallbacks(gD3DUnsetModeCallbacks);
    IDirect3DDevice7_SetTexture(gD3DDevice, 0, NULL);
    IDirect3DDevice7_SetTexture(gD3DDevice, 1, NULL);
    if (gD3DDevice) {
        IDirect3DDevice7_Release(gD3DDevice);
        gD3DDevice = NULL;
    }
    if (gD3DZBuffer) {
        IDirectDrawSurface7_DeleteAttachedSurface(gD3DBack, 0, gD3DZBuffer);
        IDirectDrawSurface7_Release(gD3DZBuffer);
        gD3DZBuffer = NULL;
    }
    if (gD3DBack) {
        IDirectDrawSurface7_Release(gD3DBack);
        gD3DBack = NULL;
    }
    if (gD3DPrimary) {
        IDirectDrawSurface7_Release(gD3DPrimary);
        gD3DPrimary = NULL;
    }
    if (gD3D) {
        IDirect3D7_Release(gD3D);
        gD3D = NULL;
    }
    if (gDDraw) {
        hr = IDirectDraw7_SetCooperativeLevel(gDDraw, AppContext_GethWnd(), DDSCL_NORMAL | DDSCL_FPUPRESERVE);
        if (hr)
            DXError("Couldn't set cooperative level", hr);
        IDirectDraw7_RestoreDisplayMode(gDDraw);
        IDirectDraw7_Release(gDDraw);
        gDDraw = NULL;
    }
}
HOOK(0x493B30, Direct3D_UnsetMode);

void Direct3D_ClipWindow(ClipRect *r)
{
    D3DCLIPSTATUS cs;
    HRESULT hr;

    cs.dwFlags = D3DCLIPSTATUS_EXTENTS2;
    cs.dwStatus = 0xF;
    cs.minx = r->x;
    cs.maxx = r->w + r->x;
    cs.miny = r->y;
    cs.maxy = r->h + r->y;
    cs.minz = gD3DMinZ;
    cs.maxz = gD3DMaxZ;
    hr = IDirect3DDevice7_SetClipStatus(gD3DDevice, &cs);
    if (hr)
        DXError("Couldn't set clipping window", hr);
}
HOOK(0x493CA0, Direct3D_ClipWindow);

void Guid_ToString(char *buf, GUID g);
HRESULT Direct3D_CreateDevice(int bpp, unsigned maxZ, int width, int height, DisplayMode *mode, const GUID *guid);
void Direct3D_ResetLists(void);

void Direct3D_SetMode(DisplayMode *mode)
{
    int fullscreen = AppContext_IsFullscreen();
    int found = 0;
    DDEnumCtx ctx;
    DDDriver drv;
    char guidName[0x80];
    D3DVIEWPORT7 vp;
    ClipRect clip;
    IDirectDrawClipper *clipper;
    const GUID *guid;
    MSG msg;
    HRESULT hr;
    const char *name;

    *Display_GetMode() = *mode;
    ctx.listDevices = 0;
    ctx.driverName = mode->device;
    ctx.drivers = &drv;
    ctx.count = &found;
    if (!strlen(mode->device)) {
        GUID none = {0};

        Guid_ToString(guidName, none);
        ctx.driverName = guidName;
    }
    name = ctx.driverName;
    strcpy(Display_GetMode()->device, name ? name : "");
    Display_EnumDrivers(&ctx);
    if (!found) {
        Error_Show("Direct3D device not found");
        for (;;)
            ;
    }
    guid = gD3DTnL ? IID_IDirect3DTnLHalDevice_ : IID_IDirect3DHALDevice_;
    if (!gDDraw) {
        hr = DirectDrawCreateEx_(drv.guid, (void **)&gDDraw, IID_IDirectDraw7_, NULL);
        if (hr)
            DXError("Couldn't create DirectDraw object", hr);
    }
    if (fullscreen)
        hr = IDirectDraw7_SetCooperativeLevel(gDDraw, AppContext_GethWnd(),
                                              DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE | DDSCL_FPUPRESERVE);
    else
        hr = IDirectDraw7_SetCooperativeLevel(gDDraw, AppContext_GethWnd(), DDSCL_NORMAL | DDSCL_FPUPRESERVE);
    if (hr)
        DXError("Couldn't set cooperative level", hr);
    hr = IDirectDraw7_QueryInterface(gDDraw, IID_IDirect3D7_, (void **)&gD3D);
    if (hr)
        DXError("Couldn't get the Direct3D interface", hr);

    /* fall back to 16 bit colour, then a 16 bit z-buffer, then 640x480 */
    hr = Direct3D_CreateDevice(-1, -1, -1, -1, mode, guid);
    if (hr == DDERR_INVALIDPIXELFORMAT)
        hr = Direct3D_CreateDevice(16, -1, -1, -1, mode, guid);
    else if (hr)
        hr = Direct3D_CreateDevice(-1, 16, -1, -1, mode, guid);
    if (hr) {
        hr = Direct3D_CreateDevice(16, 16, -1, -1, mode, guid);
        if (hr) {
            hr = Direct3D_CreateDevice(16, 16, 640, 480, mode, guid);
            Display_GetMode()->width = 640;
            Display_GetMode()->height = 480;
            if (hr)
                DXError("Couldn't create device", hr);
        }
    }

    hr = IDirect3DDevice7_GetCaps(gD3DDevice, &gD3DDeviceDesc);
    if (hr)
        DXError("Couldn't get device caps", hr);
    vp.dwX = 0;
    vp.dwY = 0;
    vp.dwWidth = gScreenWidth;
    vp.dwHeight = gScreenHeight;
    vp.dvMinZ = gD3DViewMinZ;
    vp.dvMaxZ = gD3DViewMaxZ;
    hr = IDirect3DDevice7_SetViewport(gD3DDevice, &vp);
    if (hr)
        DXError("Couldn't set viewport", hr);
    hr = IDirectDraw7_CreateClipper(gDDraw, 0, &clipper, NULL);
    if (hr)
        DXError("Couldn't create clipper", hr);
    hr = IDirectDrawClipper_SetHWnd(clipper, 0, AppContext_GethWnd());
    if (hr)
        DXError("Couldn't set clipper's hWnd", hr);
    hr = IDirectDrawSurface7_SetClipper(gD3DPrimary, clipper);
    if (hr)
        DXError("Couldn't set clipper", hr);
    IDirectDrawClipper_Release(clipper);
    if (gD3DDevice) {
        hr = IDirect3DDevice7_BeginScene(gD3DDevice);
        if (hr)
            DXError("BeginScene failed.", hr);
    }
    clip.x = 0;
    clip.y = 0;
    clip.w = (unsigned)gScreenWidth;
    clip.h = (unsigned)gScreenHeight;
    Direct3D_ClipWindow(&clip);
    Display_RunCallbacks(gD3DSetModeCallbacks);
    Direct3D_CreateVertexBuffers();
    while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    FPU_SetTruncate();
    Direct3D_ResetLists();
}
HOOK(0x493D50, Direct3D_SetMode);

HRESULT __stdcall Direct3D_FindZBufferCB(DDPIXELFORMAT *pf, ZBufferSearch *best);

/* The refresh rate passed to SetDisplayMode is always 0 (driver default). */
HRESULT Direct3D_CreateDevice(int bpp, unsigned maxZ, int width, int height, DisplayMode *mode, const GUID *guid)
{
    DDSURFACEDESC2 sd;
    DDSCAPS2 caps;
    ZBufferSearch zs;
    IDirect3DDevice7 *dev;
    HRESULT hr;
    HDC ic;

    if (width == -1)
        width = mode->width;
    if (height == -1)
        height = mode->height;
    if (!AppContext_IsFullscreen()) {
        ShowCursor(TRUE);
        ic = CreateICA("Display", NULL, NULL, NULL);
        GetDeviceCaps(ic, BITSPIXEL);
        DeleteDC(ic);
    } else {
        ShowCursor(FALSE);
        if (bpp == -1)
            bpp = mode->depth;
        hr = IDirectDraw7_SetDisplayMode(gDDraw, width, height, bpp, 0, 0);
        if (hr)
            return hr;
    }
    gScreenWidth = width;
    gScreenHeight = height;

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    if (AppContext_IsFullscreen()) {
        sd.dwBackBufferCount = mode->backBuffers;
        sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        sd.ddsCaps.dwCaps = DDSCAPS_LIVEVIDEO | DDSCAPS_3DDEVICE | DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
        hr = IDirectDraw7_CreateSurface(gDDraw, &sd, &gD3DPrimary, NULL);
        if (hr)
            return hr;
        memset(&caps, 0, sizeof(caps));
        caps.dwCaps = DDSCAPS_LIVEVIDEO | DDSCAPS_3DDEVICE | DDSCAPS_BACKBUFFER;
        hr = IDirectDrawSurface7_GetAttachedSurface(gD3DPrimary, &caps, &gD3DBack);
        if (hr) {
            IDirectDrawSurface7_Release(gD3DPrimary);
            return hr;
        }
    } else {
        sd.dwFlags = DDSD_CAPS;
        sd.ddsCaps.dwCaps = DDSCAPS_LIVEVIDEO | DDSCAPS_PRIMARYSURFACE;
        hr = IDirectDraw7_CreateSurface(gDDraw, &sd, &gD3DPrimary, NULL);
        if (hr)
            return hr;
        sd.dwWidth = gScreenWidth;
        sd.dwHeight = gScreenHeight;
        sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        sd.ddsCaps.dwCaps = DDSCAPS_LIVEVIDEO | DDSCAPS_3DDEVICE;
        sd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        sd.ddpfPixelFormat.dwFlags = DDPF_RGB;
        hr = IDirectDrawSurface7_GetPixelFormat(gD3DPrimary, &sd.ddpfPixelFormat);
        if (hr)
            goto release_primary;
        hr = IDirectDraw7_CreateSurface(gDDraw, &sd, &gD3DBack, NULL);
        if (hr) {
            IDirectDrawSurface7_Release(gD3DPrimary);
            return hr;
        }
    }

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    if (!memcmp(guid, IID_IDirect3DRGBDevice_, sizeof(GUID)) || !memcmp(guid, IID_IDirect3DRefDevice_, sizeof(GUID)))
        sd.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_SYSTEMMEMORY;
    else
        sd.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_VIDEOMEMORY;
    sd.dwHeight = gScreenHeight;
    sd.dwWidth = gScreenWidth;
    memset(&zs.pf, 0, sizeof(zs.pf));
    zs.maxDepth = maxZ;
    hr = IDirect3D7_EnumZBufferFormats(gD3D, guid, (LPD3DENUMPIXELFORMATSCALLBACK)Direct3D_FindZBufferCB, &zs);
    if (!zs.pf.dwZBufferBitDepth) {
        Error_Show("Couldn't find zbuffer format");
        for (;;)
            ;
    }
    if (hr) {
        IDirectDrawSurface7_Release(gD3DBack);
        IDirectDrawSurface7_Release(gD3DPrimary);
        return hr;
    }
    gD3DZ24 = zs.pf.dwZBufferBitDepth <= 24;
    sd.ddpfPixelFormat = zs.pf;
    hr = IDirectDraw7_CreateSurface(gDDraw, &sd, &gD3DZBuffer, NULL);
    if (hr) {
        IDirectDrawSurface7_Release(gD3DBack);
        IDirectDrawSurface7_Release(gD3DPrimary);
        return hr;
    }
    hr = IDirectDrawSurface7_AddAttachedSurface(gD3DBack, gD3DZBuffer);
    if (!hr) {
        hr = IDirect3D7_CreateDevice(gD3D, guid, gD3DBack, &dev);
        gD3DDevice = dev;
        if (!hr)
            return 0;
        IDirectDrawSurface7_DeleteAttachedSurface(gD3DBack, 0, gD3DZBuffer);
    }
    IDirectDrawSurface7_Release(gD3DZBuffer);
    IDirectDrawSurface7_Release(gD3DBack);
release_primary:
    IDirectDrawSurface7_Release(gD3DPrimary);
    return hr;
}
HOOK(0x4942A0, Direct3D_CreateDevice);

/* Keeps the deepest plain z-buffer format not deeper than best->maxDepth. */
HRESULT __stdcall Direct3D_FindZBufferCB(DDPIXELFORMAT *pf, ZBufferSearch *best)
{
    if (best->pf.dwZBufferBitDepth < pf->dwZBufferBitDepth && pf->dwFlags == DDPF_ZBUFFER &&
        pf->dwZBufferBitDepth <= best->maxDepth)
        best->pf = *pf;
    return 1;
}
HOOK(0x494690, Direct3D_FindZBufferCB);

void Direct3D_ResetLists(void)
{
    gD3DListA[1] = -1;
    gD3DListB[1] = -1;
    gD3DListC[1] = -1;
    gD3DStateList[1] = -1;
}
HOOK(0x4946D0, Direct3D_ResetLists);

void Direct3D_SwapBuffers(void)
{
    DDCAPS hal, hel;
    char buf[1024];
    RECT r;
    HRESULT hr;

    if (gD3DDevice) {
        if (IDirect3DDevice7_EndScene(gD3DDevice)) {
            hr = IDirect3D7_EvictManagedTextures(gD3D);
            if (hr)
                DXError("Couldn't evict textures", hr);
            IDirect3DDevice7_SetTexture(gD3DDevice, 0, NULL);
            IDirect3DDevice7_SetTexture(gD3DDevice, 1, NULL);
            if (IDirectDraw7_RestoreAllSurfaces(gDDraw)) {
                /* the message is built but never shown */
                hal.dwSize = sizeof(hal);
                hel.dwSize = sizeof(hel);
                IDirectDraw7_GetCaps(gDDraw, &hal, &hel);
                crt_sprintf(buf, "Couldn't restore surfaces\nVideo memory free: %d", hal.dwVidMemFree);
                goto done;
            }
            Display_RunCallbacks(gD3DRestoreCallbacks);
            Direct3D_ResetLists();
        }
        if (AppContext_IsFullscreen()) {
            hr = IDirectDrawSurface7_Flip(gD3DPrimary, NULL, 0);
            if (hr)
                DXError("Couldn't flip", hr);
        } else {
            if (AppContext_IsFullscreen()) {
                r.left = 0;
                r.top = 0;
                r.right = Display_GetMode()->width;
                r.bottom = Display_GetMode()->height;
            } else {
                GetClientRect(AppContext_GethWnd(), &r);
                ClientToScreen(AppContext_GethWnd(), (POINT *)&r.left);
                ClientToScreen(AppContext_GethWnd(), (POINT *)&r.right);
            }
            hr = IDirectDrawSurface7_Blt(gD3DPrimary, &r, gD3DBack, NULL, DDBLT_ASYNC, NULL);
            if (hr == DDERR_WASSTILLDRAWING)
                hr = IDirectDrawSurface7_Blt(gD3DPrimary, &r, gD3DBack, NULL, DDBLT_WAIT, NULL);
            if (hr)
                DXError("Couldn't update primary surface", hr);
        }
        hr = IDirect3DDevice7_BeginScene(gD3DDevice);
        if (hr)
            DXError("BeginScene failed.", hr);
    }
done:
    gVBNormal.used = 0;
    gVBLit.used = 0;
    gVBTL.used = 0;
}
HOOK(0x494700, Direct3D_SwapBuffers);

static int ScanForward(unsigned v, int prev)
{
    return v ? __builtin_ctz(v) : prev;
}

static int ScanReverse(unsigned v, int prev)
{
    return v ? 31 - __builtin_clz(v) : prev;
}

void Direct3D_Snapshot(SnapshotArgs *a)
{
    DDSURFACEDESC2 sd;
    DDPIXELFORMAT *pf = &sd.ddpfPixelFormat;
    unsigned char *dst = a->pixels, *src;
    int rlo, glo, blo, rhi, ghi, bhi, rbits, gbits, bbits;
    int x, y;
    HRESULT hr;

    sd.dwSize = sizeof(sd);
    hr = IDirectDrawSurface7_Lock(gD3DBack, NULL, &sd, DDLOCK_WAIT, NULL);
    if (hr)
        DXError("Failed to lock framebuffer.", hr);
    /* a zero mask leaves the previous result, like the bsf/bsr instructions */
    rlo = ScanForward(pf->dwRBitMask, 0);
    glo = ScanForward(pf->dwGBitMask, rlo);
    blo = ScanForward(pf->dwBBitMask, glo);
    rhi = ScanReverse(pf->dwRBitMask, blo);
    ghi = ScanReverse(pf->dwGBitMask, rhi);
    bhi = ScanReverse(pf->dwBBitMask, ghi);
    rbits = rhi - rlo + 1;
    gbits = ghi - glo + 1;
    bbits = bhi - blo + 1;
    if (!(pf->dwFlags & DDPF_RGB)) {
        Error_Show("Invalid framebuffer format");
        for (;;)
            ;
    }
    src = sd.lpSurface;
    if (pf->dwRGBBitCount == 32) {
        for (y = 0; y < (int)sd.dwHeight; y++) {
            for (x = 0; x < (int)sd.dwWidth; x++) {
                unsigned p = *(unsigned *)src;

                src += 4;
                *dst++ = (p >> (blo & 31)) << ((8 - bbits) & 31);
                *dst++ = (p >> (glo & 31)) << ((8 - gbits) & 31);
                *dst++ = (p >> (rlo & 31)) << ((8 - rbits) & 31);
            }
            src += sd.lPitch - (int)sd.dwWidth * 4;
        }
    } else if (pf->dwRGBBitCount == 16) {
        for (y = 0; y < (int)sd.dwHeight; y++) {
            for (x = 0; x < (int)sd.dwWidth; x++) {
                unsigned short p = *(unsigned short *)src;

                src += 2;
                *dst++ = (unsigned short)(p >> (blo & 31)) << ((8 - bbits) & 31);
                *dst++ = (unsigned short)(p >> (glo & 31)) << ((8 - gbits) & 31);
                *dst++ = (unsigned short)(p >> (rlo & 31)) << ((8 - rbits) & 31);
            }
            src += sd.lPitch - (int)sd.dwWidth * 2;
        }
    } else {
        Error_Show("Invalid framebuffer format");
        for (;;)
            ;
    }
    hr = IDirectDrawSurface7_Unlock(gD3DBack, NULL);
    if (hr)
        DXError("Failed to unlock framebuffer.", hr);
}
HOOK(0x494970, Direct3D_Snapshot);

void Direct3D_Blit(BlitArgs *a)
{
    IDirectDrawSurface7 *target = a->toPrimary ? gD3DPrimary : gD3DBack;
    IDirectDrawSurface7 *tmp;
    DDSURFACEDESC2 sd;
    DDPIXELFORMAT *pf = &sd.ddpfPixelFormat;
    RECT r, dst, src;
    int pitch, i;
    void *buf;
    HRESULT hr;

    memset(&sd, 0, sizeof(sd));
    pf->dwSize = sizeof(*pf);
    hr = IDirectDrawSurface7_GetPixelFormat(target, pf);
    if (hr)
        DXError("Couldn't read pixel format", hr);
    pitch = (unsigned)(a->w * pf->dwRGBBitCount) >> 3;
    Mem_PushMode(1);
    buf = Mem_Alloc(a->h * pitch, 4);
    if ((pf->dwFlags & DDPF_RGB) && pf->dwRGBBitCount == 32 && pf->dwRBitMask == 0xFF0000 && pf->dwGBitMask == 0xFF00 &&
        pf->dwBBitMask == 0xFF) {
        for (i = 0; i < a->h * a->w; i++)
            ((unsigned *)buf)[i] = a->pixels[i] | 0xFF000000;
    } else if ((pf->dwFlags & DDPF_RGB) && pf->dwRGBBitCount == 16 && pf->dwRBitMask == 0xF800 &&
               pf->dwGBitMask == 0x7E0 && pf->dwBBitMask == 0x1F) {
        for (i = 0; i < a->h * a->w; i++) {
            unsigned p = a->pixels[i];

            ((unsigned short *)buf)[i] = (((((p >> 19) & 0x1F) << 6) + ((p >> 10) & 0x3F)) << 5) + ((p >> 3) & 0x1F);
        }
    }
    sd.dwSize = sizeof(sd);
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_LPSURFACE | DDSD_PIXELFORMAT;
    sd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_TEXTURE;
    sd.dwHeight = a->h;
    sd.dwWidth = a->w;
    sd.lPitch = pitch;
    sd.lpSurface = buf;
    hr = IDirectDraw7_CreateSurface(gDDraw, &sd, &tmp, NULL);
    if (hr)
        DXError("Couldn't create texture surface", hr);
    if (a->toPrimary) {
        GetClientRect(AppContext_GethWnd(), &r);
        ClientToScreen(AppContext_GethWnd(), (POINT *)&r.left);
        ClientToScreen(AppContext_GethWnd(), (POINT *)&r.right);
    } else {
        r.left = 0;
        r.top = 0;
        r.right = 0;
        r.bottom = 0;
    }
    dst.left = a->x + r.left;
    dst.top = a->y + r.top;
    dst.right = dst.left + a->w;
    dst.bottom = dst.top + a->h;
    src.left = 0;
    src.top = 0;
    src.right = a->w;
    src.bottom = a->h;
    IDirectDrawSurface7_Blt(target, &dst, tmp, &src, 0, NULL);
    IDirectDrawSurface7_Release(tmp);
    Mem_Free(buf);
    Mem_PopMode();
}
HOOK(0x494C50, Direct3D_Blit);

int Direct3D_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    MINMAXINFO *mmi = (MINMAXINFO *)lparam;
    RECT r;

    if (RenderMode_GetCurrent() != gD3DRenderMode || !gDDraw)
        return 1;
    if (msg == WM_ACTIVATE) {
        if (!LOWORD(wparam) || HIWORD(wparam)) {
            if (AppContext_IsActive()) {
                InvalidateRect(hwnd, NULL, FALSE);
                AppContext_SetActive(0);
                return 1;
            }
        }
        if (!AppContext_IsActive())
            AppContext_SetActive(1);
    } else if (msg == WM_GETMINMAXINFO) {
        r.left = 0;
        r.top = 0;
        r.right = Display_GetMode()->width;
        r.bottom = Display_GetMode()->height;
        AdjustWindowRect(&r, AppContext_IsFullscreen() ? WS_POPUP : WS_OVERLAPPEDWINDOW, FALSE);
        mmi->ptMinTrackSize.x = 100;
        mmi->ptMinTrackSize.y = 100;
        mmi->ptMaxTrackSize.x = r.right - r.left;
        mmi->ptMaxTrackSize.y = r.bottom - r.top;
    }
    return 1;
}
HOOK(0x494F50, Direct3D_WinProcCB);

void Direct3D_Shutdown(void)
{
    Direct3D_RemoveRestoreCallback(Direct3D_RestoreVertexBuffers);
    AppMain_RemoveWinProcCallback(Direct3D_WinProcCB);
    PtrList_Clear(gD3DStateList, gD3DClippingOnIndex);
    PtrList_Clear(gD3DStateList, gD3DClippingOffIndex);
    QTaskList_Free(gD3DListA);
    QTaskList_Free(gD3DListB);
    QTaskList_Free(gD3DListC);
    QTaskList_Free(gD3DStateList);
    RenderMode_Delete(gD3DRenderMode);
}
HOOK(0x495060, Direct3D_Shutdown);

int Direct3D_GetRenderMode(void)
{
    return gD3DRenderMode;
}
HOOK(0x4950E0, Direct3D_GetRenderMode);

/* Transposes a 3x3 rotation and adds the translation. */
void Direct3D_MakeMatrix(D3DMATRIX *out, const float *rot, const float *pos)
{
    out->_11 = rot[0];
    out->_12 = rot[3];
    out->_13 = rot[6];
    out->_14 = 0;
    out->_21 = rot[1];
    out->_22 = rot[4];
    out->_23 = rot[7];
    out->_24 = 0;
    out->_31 = rot[2];
    out->_32 = rot[5];
    out->_33 = rot[8];
    out->_34 = 0;
    if (pos) {
        out->_41 = pos[0];
        out->_42 = pos[1];
        out->_43 = pos[2];
    } else {
        out->_41 = 0;
        out->_42 = 0;
        out->_43 = 0;
    }
    out->_44 = 1.0f;
}
HOOK(0x4950F0, Direct3D_MakeMatrix);

void Direct3D_AddCallback(void (*fn)(void), D3DCallback **list)
{
    int i;

    for (i = 0; i < 10; i++) {
        if (!gD3DCallbackPool[i].fn) {
            gD3DCallbackPool[i].next = *list;
            gD3DCallbackPool[i].fn = fn;
            *list = &gD3DCallbackPool[i];
            return;
        }
    }
    Error_Show("No more Direct3D callbacks available.");
    for (;;)
        ;
}
HOOK(0x495190, Direct3D_AddCallback);

void Direct3D_AddSetModeCallback(void (*fn)(void))
{
    Direct3D_AddCallback(fn, &gD3DSetModeCallbacks);
}
HOOK(0x495170, Direct3D_AddSetModeCallback);

void Direct3D_AddUnsetModeCallback(void (*fn)(void))
{
    Direct3D_AddCallback(fn, &gD3DUnsetModeCallbacks);
}
HOOK(0x4951E0, Direct3D_AddUnsetModeCallback);

void Direct3D_AddRestoreCallback(void (*fn)(void))
{
    Direct3D_AddCallback(fn, &gD3DRestoreCallbacks);
}
HOOK(0x495200, Direct3D_AddRestoreCallback);

/* The node stays marked as used. */
char Direct3D_RemoveCallback(void (*fn)(void), D3DCallback **list)
{
    D3DCallback *prev = NULL, *n;

    for (n = *list; n; prev = n, n = n->next) {
        if (n->fn == fn) {
            if (prev)
                prev->next = n->next;
            else
                *list = n->next;
            return 1;
        }
    }
    return 0;
}
HOOK(0x495220, Direct3D_RemoveCallback);

char Direct3D_RemoveRestoreCallback(void (*fn)(void))
{
    return Direct3D_RemoveCallback(fn, &gD3DRestoreCallbacks);
}
HOOK(0x495260, Direct3D_RemoveRestoreCallback);
