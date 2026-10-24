#include "hook.h"
#include "igi.h"
#include "material/material.h"

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x407DE0, void, GameMaterial_SetActive, (void));
ORIG(0x407E30, void, GameMaterial_DefineQ, (void));
ORIG(0x407F50, void, GameMaterial_Define, (void));

#define MATERIAL_MAX 32

extern GameMaterial gMaterials[];
extern uint8_t gMaterialEvent0;
extern uint8_t gMaterialEvent1;
extern uint8_t gMaterialEvent2;

void Material_Init(void)
{
    int i;

    for (i = 0; i < MATERIAL_MAX; i++)
        gMaterials[i].used = 0;
    gMaterialEvent0 = TaskEvent_Alloc(0);
    gMaterialEvent1 = TaskEvent_Alloc(0);
    gMaterialEvent2 = TaskEvent_Alloc(0);
    Script_RegisterFunction("SetActiveMaterial", GameMaterial_SetActive, NULL, -1);
    Script_RegisterFunction("DefineQMaterial", GameMaterial_DefineQ, NULL, -1);
    Script_RegisterFunction("DefineGameMaterial", GameMaterial_Define, NULL, -1);
}
HOOK(0x407D60, Material_Init);
