#include "igi.h"
#include "hook.h"
#include "orig.h"
#include "script/symbol.h"
#include "material/material.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4018C0, void, TaskEvent_Free, (char slot));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B85B0, void, Script_Run, (void *s));
ORIG(0x4B83D0, void, Script_Free, (void *s));
ORIG(0x4B8A20, int, Script_GetArgInt, (ScriptArgs *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (ScriptArgs *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (ScriptArgs *args, int i, char *buf, int size));
ORIG(0x4CE3E0, void, Terrain_SetMaterial, (void *obj, int i, void *material));

#define MATERIAL_COUNT 32

typedef void (*TaskHandler)(void *task, void *arg);

/* What the parse event handler gets: where to store, where to read from,
   and it returns how many script arguments it used. */
typedef struct MaterialParse {
    void *params;
    ScriptArgs *args;
    int arg;
    int used;
} MaterialParse;

typedef struct MaterialImpact {
    void *unk00;
    int unk04;
    float pos[3];
    int unk14;
} MaterialImpact;

extern unsigned char gMaterialSizeEvent;
extern unsigned char gMaterialParseEvent;
extern unsigned char gMaterialImpactEvent;
extern int gMaterialActive;
extern GameMaterial gMaterials[];
extern TaskHandler gTaskHandlers[][0x180];
extern const char gScriptNumberType[];

static ScriptValue *Material_Return(ScriptValue *out)
{
    out->i = 1;
    out->d = 1.0;
    out->s = gScriptNumberType;
    return out;
}

ScriptValue *GameMaterial_SetActive(ScriptValue *out, void *data, ScriptArgs *args)
{
    (void)data;
    gMaterialActive = Script_GetArgInt(args, 0);
    return Material_Return(out);
}
HOOK(0x407DE0, GameMaterial_SetActive);

ScriptValue *GameMaterial_DefineQ(ScriptValue *out, void *data, ScriptArgs *args)
{
    (void)data;
    gMaterials[gMaterialActive].unk00 = Script_GetArgReal(args, 0);
    gMaterials[gMaterialActive].bounce = Script_GetArgReal(args, 1);
    gMaterials[gMaterialActive].unk08 = Script_GetArgReal(args, 2);
    gMaterials[gMaterialActive].unk14 = Script_GetArgInt(args, 3) * (long double)(1.0f / 1048576);
    gMaterials[gMaterialActive].unk18 = Script_GetArgInt(args, 4) * (long double)(1.0f / 1048576);
    gMaterials[gMaterialActive].unk1C = Script_GetArgInt(args, 5) * (long double)(1.0f / 1048576);
    return Material_Return(out);
}
HOOK(0x407E30, GameMaterial_DefineQ);

/* Reads the effect type at script argument `arg` and lets the type parse
   its own parameters. Returns the index of the next argument. */
static int Material_ReadEffect(ScriptArgs *args, int arg, uint16_t *type, void **params)
{
    TaskHandler fn;
    MaterialParse parse;
    int size;

    *type = Script_GetArgInt(args, arg++);
    *params = NULL;
    fn = gTaskHandlers[gMaterialSizeEvent][*type];
    if (fn) {
        fn(NULL, &size);
        parse.params = Mem_Alloc(size, 4);
        parse.args = args;
        parse.arg = arg;
        *params = parse.params;
        gTaskHandlers[gMaterialParseEvent][*type](NULL, &parse);
        arg += parse.used;
    }
    return arg;
}

ScriptValue *GameMaterial_Define(ScriptValue *out, void *data, ScriptArgs *args)
{
    GameMaterial *m;
    int i, arg;

    (void)data;
    gMaterials[gMaterialActive].used = 1;
    Script_GetArgString(args, 0, gMaterials[gMaterialActive].name, 32);
    gMaterials[gMaterialActive].hardness = Script_GetArgReal(args, 1);
    gMaterials[gMaterialActive].ricochet = Script_GetArgReal(args, 2);
    gMaterials[gMaterialActive].unk48 = Script_GetArgReal(args, 3);
    gMaterials[gMaterialActive].unk4C = Script_GetArgInt(args, 4);
    gMaterials[gMaterialActive].unk50 = Script_GetArgReal(args, 5);
    arg = 6;
    for (i = 0; i < 6; i++)
        Script_GetArgString(args, arg++, gMaterials[gMaterialActive].unk54[i], 32);
    Script_GetArgString(args, arg++, gMaterials[gMaterialActive].unk114, 32);
    Script_GetArgString(args, arg++, gMaterials[gMaterialActive].unk134, 32);
    for (i = 0; i < 6; i++)
        Script_GetArgString(args, arg++, gMaterials[gMaterialActive].sounds[i], 32);
    for (i = 0; i < 2; i++)
        Script_GetArgString(args, arg++, gMaterials[gMaterialActive].unk214[i], 32);
    m = &gMaterials[gMaterialActive];
    arg = Material_ReadEffect(args, arg, &m->effect, &m->params);
    m = &gMaterials[gMaterialActive];
    Material_ReadEffect(args, arg, &m->backEffect, &m->backParams);
    return Material_Return(out);
}
HOOK(0x407F50, GameMaterial_Define);

void GameMaterial_Close(void)
{
    GameMaterial *m;

    for (m = gMaterials; m < gMaterials + MATERIAL_COUNT; m++) {
        if (!m->used)
            continue;
        if (m->params) {
            Mem_Free(m->params);
            m->params = NULL;
        }
        if (m->backParams) {
            Mem_Free(m->backParams);
            m->backParams = NULL;
        }
        m->used = 0;
    }
    TaskEvent_Free(gMaterialSizeEvent);
    TaskEvent_Free(gMaterialParseEvent);
    TaskEvent_Free(gMaterialImpactEvent);
}
HOOK(0x4082B0, GameMaterial_Close);

char GameMaterial_GetSizeEvent(void)
{
    return gMaterialSizeEvent;
}
HOOK(0x408320, GameMaterial_GetSizeEvent);

char GameMaterial_GetParseEvent(void)
{
    return gMaterialParseEvent;
}
HOOK(0x408330, GameMaterial_GetParseEvent);

char GameMaterial_GetImpactEvent(void)
{
    return gMaterialImpactEvent;
}
HOOK(0x408340, GameMaterial_GetImpactEvent);

void GameMaterial_LoadParameters(void)
{
    void *s = Script_Load("LOCAL:material/material.qsc");

    if (s) {
        Script_Run(s);
        Script_Free(s);
        return;
    }
    Error_Show("GameMaterial_LoadParameters: Couldn't load script \"%s\"", "LOCAL:material/material.qsc");
    for (;;)
        ;
}
HOOK(0x408350, GameMaterial_LoadParameters);

void GameMaterial_Attach(void *obj)
{
    int i;

    for (i = 0; i < MATERIAL_COUNT; i++)
        Terrain_SetMaterial(obj, i, &gMaterials[i]);
}
HOOK(0x408390, GameMaterial_Attach);

void GameMaterial_Impact(uint16_t type, int a, void *b, int c, const float *pos)
{
    MaterialImpact e;

    e.unk04 = a;
    e.unk00 = b;
    e.unk14 = c;
    e.pos[0] = pos[0];
    e.pos[1] = pos[1];
    e.pos[2] = pos[2];
    gTaskHandlers[gMaterialImpactEvent][type](NULL, &e);
}
HOOK(0x4083C0, GameMaterial_Impact);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(538518) unsigned char gMaterialSizeEvent INIT(255);
DATA(538518) unsigned char gMaterialParseEvent INIT(255);
DATA(538518) unsigned char gMaterialImpactEvent INIT(255);
PAD(igd, 538518, 53851B, 0x1);
DATA(538518) int gMaterialActive INIT(-1);
ALIAS(gMaterialEvent0, gMaterialSizeEvent, 0);
ALIAS(gMaterialEvent1, gMaterialParseEvent, 0);
ALIAS(gMaterialEvent2, gMaterialImpactEvent, 0);
PAD(igb, 569210, 569210, 0x4D08);
ALIAS(gMaterials, pad_569210, 0);
