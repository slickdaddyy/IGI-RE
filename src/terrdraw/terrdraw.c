/* Terrain drawing: blend modes, render chunks and the vertex buffer. */
#pragma GCC optimize("excess-precision=standard")

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "igi.h"
#include "terrdraw/terrdraw.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF910, void, List_AddHead, (TerrList *list, TerrNode *node));
ORIG(0x4B0B00, void, Pool_Init, (TerrPool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free, (TerrPool *pool));
ORIG(0x495170, void, Direct3D_AddSetModeCallback, (void (*fn)(void)));
ORIG(0x4951E0, void, Direct3D_AddUnsetModeCallback, (void (*fn)(void)));
ORIG(0x495200, void, Direct3D_AddRestoreCallback, (void (*fn)(void)));

#define gD3D VAR(IDirect3D7 *, 0x5CA110)
#define gD3DDevice VAR(IDirect3DDevice7 *, 0x5CA114)
#define gFogEnable VAR(DWORD, 0x5470B4)
#define gTerrFogColor VAR(DWORD, 0xA7E068)
#define gChunkHash ((TerrList *)0xA7E080)
#define gChunkHashEnd ((TerrList *)0xA7F85C)
#define gChunkPool VAR(TerrPool, 0xA7F878)
#define gTerrVB VAR(IDirect3DVertexBuffer7 *, 0xA7F890)
#define gD3DHardwareTL VAR(int, 0xBCAD74)

#define CHUNK_HASH_SIZE 0x1FD

void Terrain_SetBlendMode(int mode)
{
    switch (mode) {
    case 0:
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGCOLOR, gTerrFogColor);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGENABLE, gFogEnable);
        IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZFUNC, D3DCMP_LESS);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZWRITEENABLE, TRUE);
        break;
    case 1:
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGCOLOR, 0);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGENABLE, gFogEnable);
        IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZWRITEENABLE, FALSE);
        break;
    case 2:
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGCOLOR, 0xFFFFFFFF);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCCOLOR);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGENABLE, gFogEnable);
        IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
        IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZWRITEENABLE, FALSE);
        break;
    default:
        Error_Show("Unknown blendmode");
        for (;;)
            ;
    }
}
HOOK(0x5144E0, Terrain_SetBlendMode);

TerrChunk *TerrChunk_Get(int x, int y)
{
    TerrList *list = &gChunkHash[(unsigned int)(y * 0xB5AF + x * 0x9D) % CHUNK_HASH_SIZE];
    TerrChunk *c = (TerrChunk *)list->head;
    TerrNode *next;
    int i;

    for (next = c->node.next; next; next = next->next) {
        if (c->x == x && c->y == y)
            return c;
        c = (TerrChunk *)next;
    }
    if (gChunkPool.used == gChunkPool.count) {
        Error_Show("Out of terrain renderchunks");
        for (;;)
            ;
    }
    i = gChunkPool.used;
    c = (TerrChunk *)(gChunkPool.data + gChunkPool.free[i] * gChunkPool.stride);
    *(int *)((char *)c + gChunkPool.size) = i;
    gChunkPool.used = i + 1;
    c->unk2EE8 = 0;
    c->x = x;
    c->y = y;
    c->node.prev = NULL;
    c->node.next = NULL;
    List_AddHead(list, &c->node);
    return c;
}
HOOK(0x514680, TerrChunk_Get);

void TerrDraw_SetMode(void);
void TerrDraw_UnsetMode(void);
void TerrDraw_Restore(void);

void TerrDraw_Init(void)
{
    TerrList *l;

    Direct3D_AddSetModeCallback(TerrDraw_SetMode);
    Direct3D_AddUnsetModeCallback(TerrDraw_UnsetMode);
    Direct3D_AddRestoreCallback(TerrDraw_Restore);
    for (l = gChunkHash; l < gChunkHashEnd; l++) {
        l->head = (TerrNode *)&l->tail;
        l->tailpred = (TerrNode *)l;
        l->tail = NULL;
    }
}
HOOK(0x516600, TerrDraw_Init);

static void create_vertex_buffer(void)
{
    D3DVERTEXBUFFERDESC d;

    d.dwSize = sizeof(d);
    d.dwCaps = D3DVBCAPS_WRITEONLY;
    if (!gD3DHardwareTL)
        d.dwCaps = D3DVBCAPS_WRITEONLY | D3DVBCAPS_SYSTEMMEMORY;
    d.dwFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1;
    d.dwNumVertices = 0x8000;
    IDirect3D7_CreateVertexBuffer(gD3D, &d, &gTerrVB, 0);
}

void TerrDraw_SetMode(void)
{
    create_vertex_buffer();
    Pool_Init(&gChunkPool, sizeof(TerrChunk), 0x80, 0);
}
HOOK(0x516640, TerrDraw_SetMode);

void TerrDraw_UnsetMode(void)
{
    Pool_Free(&gChunkPool);
    IDirect3DVertexBuffer7_Release(gTerrVB);
    gTerrVB = NULL;
}
HOOK(0x5166C0, TerrDraw_UnsetMode);

void TerrDraw_Restore(void)
{
    IDirect3DVertexBuffer7_Release(gTerrVB);
    gTerrVB = NULL;
    create_vertex_buffer();
}
HOOK(0x5166F0, TerrDraw_Restore);
