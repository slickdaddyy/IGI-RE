#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"

/* A texture handle, owned by the render device that created it. */
typedef struct Texture {
    int device;
    int unk04[8];
} Texture;

/* What AllocTexture and DownloadTexture get from the engine. */
typedef struct TextureDesc {
    void *tex;
    int unk04;
    int unk08;
    int pitch;
    int unk10;
    int unk14;
    int width;
    int height;
    int format;
    int unk24;
    int fromFile;
    void *data;
} TextureDesc;

/* Header of a .tex file, version 2. */
typedef struct LoopHeader2 {
    int magic;
    int version;
    unsigned short unk08;
    unsigned short pad0A;
    int unk0C;
    unsigned short pitch;
    unsigned short width;
    unsigned short height;
    unsigned short format;
} LoopHeader2;

/* Header of a .tex file, version 11. */
typedef struct LoopHeader11 {
    int magic;
    int version;
    int format;
    int unk0C;
    int unk10;
    unsigned short unk14;
    unsigned short width;
    unsigned short height;
    unsigned short unk1A;
    unsigned short unk1C;
    unsigned short bytesPerPixel;
} LoopHeader11;

/* TEXH chunk of a resource file. */
typedef struct TexHeader {
    int format;
    int unk04;
    int unk08;
    unsigned short unk0C;
    unsigned short width;
    unsigned short height;
    unsigned short unk12;
    unsigned short unk14;
    unsigned short bitsPerPixel;
} TexHeader;

#define LOOP_MAGIC 0x504F4F4C
#define CHUNK_TEXH 0x48584554
#define CHUNK_BODY 0x59444F42

ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int id));
ORIG(0x4B0F80, int, RenderMode_GetCurrent, (void));
ORIG(0x4B1000, void, RenderMode_Call, (int device, int id, void *arg));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int flags));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4B7490, void *, Chunk_FirstChild, (void *file));
ORIG(0x4B7470, void *, Chunk_Next, (void *chunk));
ORIG(0x4B7580, int, Chunk_Id, (void *chunk));
ORIG(0x4B74D0, void *, Chunk_Data, (void *chunk, void *file));
ORIG(0x509910, void, D3DTex_Register, (void));
ORIG(0x509920, void, D3DTex_Unregister, (void));

extern void (*gRenderFuncs[])(void *);
extern char gTexFromFile;
extern float gTexParams[];

extern int gSetLODId;
extern int gGetTextureSizeId;
extern int gReloadTextureId;
extern int gRefreshTextureId;
extern int gBeginTextureListId;
extern int gEndTextureListId;
extern int gAllocTextureId;
extern int gDeAllocTextureId;
extern int gDownloadTextureId;
extern int gGetTMUsId;
extern int gSetActiveTMUId;
extern int gGetActiveTMUId;

void Texture_Create(Texture *tex, int a2, int pitch, int width, int height, int format, int a7, int a8, void *data);

int TexRes_Count(void *file)
{
    int count = 0;
    void *chunk = Chunk_FirstChild(file);

    while (chunk) {
        if (Chunk_Id(chunk) == CHUNK_TEXH) {
            chunk = Chunk_Next(chunk);
            if (!chunk || Chunk_Id(chunk) != CHUNK_BODY || !Chunk_Data(chunk, file))
                return 0;
            count++;
        }
        chunk = Chunk_Next(chunk);
    }
    return count;
}
HOOK(0x4B1FF0, TexRes_Count);

int TexRes_Load(void *file, Texture *tex, int max, int a4)
{
    int count = 0;
    void *chunk = Chunk_FirstChild(file);

    while (chunk && count < max) {
        if (Chunk_Id(chunk) == CHUNK_TEXH) {
            TexHeader *h = Chunk_Data(chunk, file);

            chunk = Chunk_Next(chunk);
            if (!h || !chunk || Chunk_Id(chunk) != CHUNK_BODY || !Chunk_Data(chunk, file))
                return 0;
            Texture_Create(tex, a4, h->bitsPerPixel * h->unk12 / 8, h->width, h->height, h->format,
                           h->unk0C, h->unk04, Chunk_Data(chunk, file));
            count++;
            tex++;
        }
        chunk = Chunk_Next(chunk);
    }
    return count;
}
HOOK(0x4B2070, TexRes_Load);

void Texture_ResetColorParams(float *p);
void Texture_EndFileLoad(void);

void Texture_Init(void)
{
    Texture_ResetColorParams(gTexParams);
    Texture_EndFileLoad();
    gAllocTextureId = RenderMode_CreateMethod("AllocTexture");
    gDeAllocTextureId = RenderMode_CreateMethod("DeAllocTexture");
    gDownloadTextureId = RenderMode_CreateMethod("DownloadTexture");
    gBeginTextureListId = RenderMode_CreateMethod("BeginTextureList");
    gEndTextureListId = RenderMode_CreateMethod("EndTextureList");
    gGetTMUsId = RenderMode_CreateMethod("GetTMUs");
    gSetActiveTMUId = RenderMode_CreateMethod("SetActiveTMU");
    gGetActiveTMUId = RenderMode_CreateMethod("GetActiveTMU");
    gRefreshTextureId = RenderMode_CreateMethod("RefreshTexture");
    gReloadTextureId = RenderMode_CreateMethod("ReloadTexture");
    gGetTextureSizeId = RenderMode_CreateMethod("GetTextureSize");
    gSetLODId = RenderMode_CreateMethod("SetLOD");
    D3DTex_Register();
}
HOOK(0x4B2180, Texture_Init);

void Texture_Shutdown(void)
{
    D3DTex_Unregister();
    RenderMode_DeleteMethod(gAllocTextureId);
    RenderMode_DeleteMethod(gDeAllocTextureId);
    RenderMode_DeleteMethod(gDownloadTextureId);
    RenderMode_DeleteMethod(gBeginTextureListId);
    RenderMode_DeleteMethod(gEndTextureListId);
    RenderMode_DeleteMethod(gGetTMUsId);
    RenderMode_DeleteMethod(gSetActiveTMUId);
    RenderMode_DeleteMethod(gGetActiveTMUId);
    RenderMode_DeleteMethod(gReloadTextureId);
    RenderMode_DeleteMethod(gRefreshTextureId);
    RenderMode_DeleteMethod(gGetTextureSizeId);
    RenderMode_DeleteMethod(gSetLODId);
}
HOOK(0x4B2250, Texture_Shutdown);

void Texture_Free(Texture *tex)
{
    RenderMode_Call(tex->device, gDeAllocTextureId, tex);
    memset(tex, 0, sizeof(*tex));
}
HOOK(0x4B22F0, Texture_Free);

void Texture_Alloc(Texture *tex, int a2, int pitch, int width, int height, int format, int a7, int a8)
{
    TextureDesc d;

    tex->device = RenderMode_GetCurrent();
    memset(&d, 0, sizeof(d));
    d.tex = tex;
    d.unk04 = a2;
    d.unk08 = a7;
    d.pitch = pitch;
    d.width = width;
    d.height = height;
    d.format = format;
    d.unk24 = a8;
    d.fromFile = gTexFromFile;
    d.data = NULL;
    gRenderFuncs[gAllocTextureId](&d);
}
HOOK(0x4B2320, Texture_Alloc);

void Texture_Create(Texture *tex, int a2, int pitch, int width, int height, int format, int a7, int a8, void *data)
{
    TextureDesc d;

    tex->device = RenderMode_GetCurrent();
    memset(&d, 0, sizeof(d));
    d.tex = tex;
    d.unk04 = a2;
    d.unk08 = a7;
    d.pitch = pitch;
    d.width = width;
    d.height = height;
    d.format = format;
    d.unk24 = a8;
    d.fromFile = gTexFromFile;
    d.data = data;
    gRenderFuncs[gAllocTextureId](&d);
    gRenderFuncs[gDownloadTextureId](&d);
}
HOOK(0x4B23B0, Texture_Create);

/* unk10 and unk14 are left unset, like the original. */
void Texture_Download(void *tex, int a2, int pitch, int width, int height, int format, void *data)
{
    TextureDesc d;

    d.tex = tex;
    d.unk04 = a2;
    d.unk08 = 0;
    d.pitch = pitch;
    d.width = width;
    d.height = height;
    d.format = format;
    d.unk24 = 0;
    d.fromFile = gTexFromFile;
    d.data = data;
    gRenderFuncs[gDownloadTextureId](&d);
}
HOOK(0x4B2450, Texture_Download);

void Texture_BeginFileLoad(void);

void Texture_LoadFile(Texture *tex, int a2, const char *name)
{
    char *p = Resource_Load(name, 0);
    LoopHeader2 *h2 = (LoopHeader2 *)p;
    LoopHeader11 *h11 = (LoopHeader11 *)p;

    if (h2->magic == LOOP_MAGIC && h2->version == 2) {
        Texture_BeginFileLoad();
        Texture_Create(tex, a2, h2->pitch, h2->width, h2->height, h2->format, h2->unk08, 0, p + 0x18);
    } else if (h11->magic == LOOP_MAGIC && h11->version == 11) {
        Texture_BeginFileLoad();
        Texture_Create(tex, a2, h11->bytesPerPixel * h11->unk1A, h11->width, h11->height, h11->format,
                       h11->unk14, h11->unk0C, p + 0x20);
    } else {
        Error_Show("'%s' is not a valid Texture file.", name);
        for (;;)
            ;
    }
    Resource_Unload(name);
    Texture_EndFileLoad();
}
HOOK(0x4B24C0, Texture_LoadFile);

void Texture_BeginList(int a, int b)
{
    int args[2] = { a, b };

    if (gRenderFuncs[gBeginTextureListId])
        gRenderFuncs[gBeginTextureListId](args);
}
HOOK(0x4B25B0, Texture_BeginList);

void Texture_EndList(void)
{
    if (gRenderFuncs[gEndTextureListId])
        gRenderFuncs[gEndTextureListId](NULL);
}
HOOK(0x4B25F0, Texture_EndList);

void Texture_SetActiveTMU(int tmu)
{
    if (gRenderFuncs[gSetActiveTMUId])
        gRenderFuncs[gSetActiveTMUId](&tmu);
}
HOOK(0x4B2610, Texture_SetActiveTMU);

int Texture_GetActiveTMU(void)
{
    int tmu = 0;

    if (gRenderFuncs[gGetActiveTMUId])
        gRenderFuncs[gGetActiveTMUId](&tmu);
    return tmu;
}
HOOK(0x4B2640, Texture_GetActiveTMU);

int Texture_GetTMUs(void)
{
    int n = 1;

    if (gRenderFuncs[gGetTMUsId])
        gRenderFuncs[gGetTMUsId](&n);
    return n;
}
HOOK(0x4B2670, Texture_GetTMUs);

void Texture_BeginFileLoad(void)
{
    gTexFromFile = 1;
}
HOOK(0x4B26A0, Texture_BeginFileLoad);

void Texture_EndFileLoad(void)
{
    gTexFromFile = 0;
}
HOOK(0x4B26B0, Texture_EndFileLoad);

int Texture_GetReloadId(void);

void Texture_ReloadAll(void)
{
    if (gRenderFuncs[Texture_GetReloadId()])
        gRenderFuncs[Texture_GetReloadId()](NULL);
}
HOOK(0x4B26C0, Texture_ReloadAll);

int Texture_GetRefreshId(void)
{
    return gRefreshTextureId;
}
HOOK(0x4B26E0, Texture_GetRefreshId);

int Texture_GetReloadId(void)
{
    return gReloadTextureId;
}
HOOK(0x4B26F0, Texture_GetReloadId);

int Texture_GetSizeId(void)
{
    return gGetTextureSizeId;
}
HOOK(0x4B2700, Texture_GetSizeId);

int Texture_GetSetLODId(void)
{
    return gSetLODId;
}
HOOK(0x4B2710, Texture_GetSetLODId);

void Texture_SetColorParams(const float *p)
{
    memcpy(gTexParams, p, 7 * sizeof(float));
}
HOOK(0x4B2720, Texture_SetColorParams);

void Texture_ResetColorParams(float *p)
{
    p[0] = 0.0f;
    p[1] = 0.0f;
    p[2] = 0.0f;
    p[3] = 1.0f;
    p[4] = 1.0f;
    p[5] = 1.0f;
    p[6] = 1.0f;
}
HOOK(0x4B2740, Texture_ResetColorParams);
