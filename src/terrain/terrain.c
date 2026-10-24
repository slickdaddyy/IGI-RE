#include <string.h>

#include "hook.h"
#include "igi.h"
#include "terrain/terrain.h"
#include "coll/colltypes.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B2450, void, Texture_Download, (void *tex, void *palette, int pitch, int width, int height, int format,
                                        void *data));
ORIG(0x4B2610, void, Texture_SetActiveTMU, (int tmu));
ORIG(0x4B2670, int, Texture_GetTMUs, (void));
ORIG(0x4B5B60, int, Resource_Exists, (const char *name));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, unsigned int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *s));
ORIG(0x4B85B0, void, Script_Run, (void *s));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4CE3D0, void, Terrain_InvalidateLighting, (Terrain *t));
ORIG(0x4D11C0, void, QCube_Reset, (void));

extern const char gScriptEmptyString[];
extern char gHmpResident[];
extern char gBitResident[];
extern TerrainLightmap gLightmaps[];
extern TerrainMaterial gTerrainMaterials[];
extern uint8_t gTerrainTileMaps[][64];

_Static_assert(sizeof(TerrainLightmap) == 0x2C, "TerrainLightmap");

ScriptValue *CreateTerrainMaterial(ScriptValue *out, void *data, void *args);
ScriptValue *CreateTerrainTileMap(ScriptValue *out, void *data, void *args);

/* Uploads the lightmaps in <name>.lmp: an int size n, then n*n bytes, repeated. */
void Terrain_LoadLightmaps(Terrain *t)
{
    TerrainLightmap *lm;
    unsigned int size;
    int *p;
    int pos;

    if (!strlen(t->name) || !Resource_Exists(t->name))
        return;
    p = Resource_Load(t->name, &size);
    t->lmp = p;
    if (p) {
        lm = gLightmaps;
        for (pos = 0; pos < (int)size; lm++) {
            int n = *p++;

            pos += 4;
            if (n) {
                if (Texture_GetTMUs() > 1)
                    Texture_SetActiveTMU(1);
                Texture_Download(lm->texture, t->palette, n, n, n, 1, p);
                if (Texture_GetTMUs() > 1)
                    Texture_SetActiveTMU(-1);
                lm->size = n;
                lm->data = p;
            }
            p = (int *)((char *)p + n * n);
            pos += n * n;
        }
    }
    Resource_Unload(t->name);
}
HOOK(0x4C6160, Terrain_LoadLightmaps);

void Terrain_Load(Terrain *t, const char *path)
{
    char buf[128];
    unsigned int size, ctr_size, n;
    uint8_t *p;
    void *script;
    int i, j;

    QCube_Reset();

    strcpy(buf, path);
    strcat(buf, ".bit");
    t->bit = Resource_Load(buf, &size);
    p = (uint8_t *)t->bit + 0xC00;
    if (size < 0xC00) {
        t->bit = Mem_Alloc(0xC00, 4);
        memset(t->bit, 0, 0xC00);
        memset(gBitResident, 0, 0x100);
    } else {
        for (i = 0; i < 0x100; i++) {
            gBitResident[i] = 1;
            n = t->bit[i].size;
            if (n) {
                t->bit[i].data = p;
                p += (int)(n * n) / 8;
            }
            t->bit[i].flag = 0;
        }
    }

    strcpy(buf, path);
    strcat(buf, ".hmp");
    if (!Resource_Exists(buf)) {
        t->hmp = NULL;
    } else {
        t->hmp = Resource_Load(buf, &size);
        p = (uint8_t *)t->hmp + 0xC00;
        if (size < 0xC00) {
            t->hmp = Mem_Alloc(0xC00, 4);
            memset(t->hmp, 0, 0xC00);
            memset(gHmpResident, 0, 0x100);
        } else {
            for (i = 0; i < 0x100; i++) {
                gHmpResident[i] = 1;
                n = t->hmp[i].size;
                if (n) {
                    t->hmp[i].data = p;
                    p += (n + 1) * (n + 1);
                }
                t->hmp[i].flag = 0;
            }
        }
    }

    strcpy(buf, path);
    strcat(buf, ".cmd");
    t->cmd = Resource_Load(buf, NULL);

    strcpy(buf, path);
    strcat(buf, ".ctr");
    t->ctr = Resource_Load(buf, &ctr_size);

    strcpy(buf, path);
    strcat(buf, ".qsc");
    Script_RegisterFunction("CreateTerrainMaterial", CreateTerrainMaterial, NULL, 0xE2);
    Script_RegisterFunction("CreateTerrainTileMap", CreateTerrainTileMap, NULL, 0x41);
    script = Script_Load(buf);
    Script_Run(script);
    Script_Free(script);
    Script_UnregisterSymbol("CreateTerrainMaterial");
    Script_UnregisterSymbol("CreateTerrainTileMap");

    for (i = 0; i < (int)(ctr_size >> 5); i++)
        t->ctr[i].leaf = (const short *)((const char *)t->ctr[i].leaf + (uintptr_t)t->cmd);

    t->unkDC0 = Mem_Alloc(0x1000, 4);
    t->unkDC4 = Mem_Alloc(0x10000, 4);
    for (i = 0; i < 0x10000; i += 0x100)
        for (j = 0; j < 0x100; j++)
            t->unkDC4[i + j] = 0;
    for (i = 0; i < 0x1000; i += 0x10)
        t->unkDC0[i / 4] = 0;
    Terrain_InvalidateLighting(t);
}
HOOK(0x4C6240, Terrain_Load);

static ScriptValue *ret_zero(ScriptValue *out)
{
    out->i = 0;
    out->d = 0.0;
    out->s = gScriptEmptyString;
    return out;
}

/* CreateTerrainMaterial(id, unk, 32 x (int, real, real, real, int, real, int)) */
ScriptValue *CreateTerrainMaterial(ScriptValue *out, void *data, void *args)
{
    TerrainMaterial *m = &gTerrainMaterials[Script_GetArgInt(args, 0)];
    TerrainLayer *l = m->layers;
    int i;

    m->unk0 = Script_GetArgInt(args, 1);
    for (i = 3; i < 0xE3; i += 7, l++) {
        l->unk0 = Script_GetArgInt(args, i - 1);
        l->unk4 = Script_GetArgReal(args, i);
        l->unk8 = Script_GetArgReal(args, i + 1);
        l->scaleA = Script_GetArgReal(args, i + 2) * 0.5f;
        l->tileA = Script_GetArgInt(args, i + 3);
        l->scaleB = Script_GetArgReal(args, i + 4) * 0.5f;
        l->tileB = Script_GetArgInt(args, i + 5);
    }
    return ret_zero(out);
}
HOOK(0x4C6670, CreateTerrainMaterial);

/* CreateTerrainTileMap(id, 64 tile bytes) */
ScriptValue *CreateTerrainTileMap(ScriptValue *out, void *data, void *args)
{
    uint8_t *map = gTerrainTileMaps[Script_GetArgInt(args, 0)];
    int i;

    for (i = 0; i < 64; i++)
        map[i] = Script_GetArgInt(args, i + 1);
    return ret_zero(out);
}
HOOK(0x4C6760, CreateTerrainTileMap);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, C28FE0, 4) __typeof__(gTerrainMaterials[0]) store_C28FE0[32];
ALIAS(gTerrainMaterials, store_C28FE0, 0);
