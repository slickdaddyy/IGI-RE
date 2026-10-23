#include "hook.h"
#include "flatwater/flatwater.h"

/* "MipMapControl": texture filtering settings of a level. Only the LOD bias
   is passed on to Direct3D. */

ORIG(0x492BE0, const char *, DDraw_GetErrorString, (long hr));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0F80, int, RenderMode_GetCurrent, (void));

typedef struct MipMapControl {
    Task task;
    float lodBias; /* 0x20, -8..7.75 */
    float unk24;   /* -8..7.75 */
    int mode;      /* 0x28, GR_MIPMAP_* */
    char unk2C;
    char pad2D[3];
} MipMapControl;

_Static_assert(sizeof(MipMapControl) == 0x30, "MipMapControl");

typedef struct D3DDevice {
    void **vtbl;
} D3DDevice;

typedef long(__stdcall *SetTextureStageStateFn)(D3DDevice *dev, int stage, int state, uint32_t value);

#define gMipMapType VAR(uint16_t, 0x54EEE0)
#define gMipMapModeRange ((int *)0xA83788)
#define gMipMapParams VAR(void *, 0xA83790)
#define gMipMapBiasRange ((float *)0xA83798)
#define gD3DDevice VAR(D3DDevice *, 0x5CA114)
#define gRenderUnk24 VAR(uint32_t, 0xBC79E8)
#define gRenderMipMapMode VAR(signed char, 0xBC79EC)
#define gRenderUnk2C VAR(char, 0xBC79ED)
#define gRenderLodBias VAR(float, 0xBC79F0)

#define D3DTSS_MIPMAPLODBIAS 19

void MipMapControl_Create(MipMapControl *m);
void MipMapControl_ParamsChanged(MipMapControl *m);
void MipMapControl_GetParams(void *task, void **out);

void MipMapControl_Open(void)
{
    MipMapControl m;

    gMipMapType = TaskType_New("MipMapControl", sizeof(MipMapControl), Task_GetRootType(), 0);
    TaskType_SetHandler(gMipMapType, 0, MipMapControl_Create);
    TaskType_SetHandler(gMipMapType, Param_GetChangedEvent(), MipMapControl_ParamsChanged);
    TaskType_SetHandler(gMipMapType, Param_GetTableEvent(), MipMapControl_GetParams);
    gMipMapParams = ParamTable_New(4);
    ParamTable_Add(gMipMapParams, &m, &m.unk2C, Param_Bool8(), 7, NULL);
    gMipMapModeRange[1] = 2;
    gMipMapModeRange[0] = 0;
    Script_RegisterConstant("GR_MIPMAP_DISABLE", 0);
    Script_RegisterConstant("GR_MIPMAP_NEAREST", 1);
    Script_RegisterConstant("GR_MIPMAP_NEAREST_DITHER", 2);
    ParamTable_Add(gMipMapParams, &m, &m.mode, Param_Int32(), 7, gMipMapModeRange);
    gMipMapBiasRange[1] = 7.75f;
    gMipMapBiasRange[0] = -8.0f;
    ParamTable_Add(gMipMapParams, &m, &m.lodBias, Param_Real32(), 7, gMipMapBiasRange);
    ParamTable_Add(gMipMapParams, &m, &m.unk24, Param_Real32(), 7, gMipMapBiasRange);
}
HOOK(0x51ECB0, MipMapControl_Open);

void MipMapControl_ParamsChanged(MipMapControl *m)
{
    int mode = Direct3D_GetRenderMode();
    long hr;

    if (RenderMode_GetCurrent() != mode)
        return;
    hr = ((SetTextureStageStateFn)gD3DDevice->vtbl[0x94 / 4])(gD3DDevice, 0, D3DTSS_MIPMAPLODBIAS,
                                                             *(uint32_t *)&m->lodBias);
    if (hr) {
        char text[1024];

        crt_sprintf(text, "Couldn't set texturestagestate");
        Error_Show("%s\n\nDirectX error message: %s", text, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
}
HOOK(0x51EE10, MipMapControl_ParamsChanged);

/* copies the renderer's current settings; the float goes through the x87 */
void MipMapControl_Create(MipMapControl *m)
{
    float bias;

    __asm__("flds %1\n\tfstps %0" : "=m"(bias) : "m"(gRenderLodBias));
    m->lodBias = bias;
    *(uint32_t *)&m->unk24 = gRenderUnk24;
    m->mode = gRenderMipMapMode;
    m->unk2C = gRenderUnk2C;
}
HOOK(0x51EE80, MipMapControl_Create);

void MipMapControl_GetParams(void *task, void **out)
{
    *out = gMipMapParams;
}
HOOK(0x51EEB0, MipMapControl_GetParams);

void MipMapControl_Close(void)
{
    QTaskList_Free(gMipMapParams);
    TaskType_Free(gMipMapType);
}
HOOK(0x51EEC0, MipMapControl_Close);
