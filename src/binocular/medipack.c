#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "binocular/binocular.h"

FN_EXTERN(ProxMine_Move);
FN_EXTERN(RocketPod_GetArgSize);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4071D0, uint8_t, WeaponConfig_GetEvent, (void));
ORIG(0x413B30, uint8_t, WeaponType_GetSizeEvent2, (void));
ORIG(0x413B40, uint8_t, WeaponType_GetInitEvent2, (void));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x460C10, uint8_t, Human_GetEvent1, (void));
ORIG(0x477BF0, uint16_t, Gun_GetType, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4F1FE0, uint8_t, Weapon_GetMoveEvent, (void));
ORIG(0x4FE650, char *, TagItem_AddReal32, (void *config, const char *name, float value));
ORIG(0x4FE730, char *, TagItem_Find, (void *config, const char *name));

#define WEAPON_HANDLER_4754C0 (FN(0x4754C0, ProxMine_Move))
#define ROCKETPOD_GETARGSIZE (FN(0x427D70, RocketPod_GetArgSize))

extern uint16_t gMedipackType;

void Medipack_Create(char *t, void *arg);
void Medipack_ParseArg(void *unused, ScriptArg *a);
void Medipack_Config(void *unused, ConfigArg *a);

void Medipack_Open(void)
{
    gMedipackType = TaskType_New("Medipack", 0x1B0, Gun_GetType(), 0);
    TaskType_SetHandler(gMedipackType, 0, Medipack_Create);
    TaskType_SetHandler(gMedipackType, Weapon_GetMoveEvent(), WEAPON_HANDLER_4754C0);
    TaskType_SetHandler(gMedipackType, Human_GetEvent0(), TASK_NOTHING);
    TaskType_SetHandler(gMedipackType, Human_GetEvent1(), TASK_NOTHING);
    TaskType_SetHandler(gMedipackType, WeaponType_GetInitEvent2(), Medipack_ParseArg);
    TaskType_SetHandler(gMedipackType, WeaponType_GetSizeEvent2(), ROCKETPOD_GETARGSIZE);
    TaskType_SetHandler(gMedipackType, WeaponConfig_GetEvent(), Medipack_Config);
    Script_RegisterConstant("TASKTYPE_MEDIPACK", gMedipackType);
}
HOOK(0x470410, Medipack_Open);

void Medipack_Create(char *t, void *arg)
{
    HANDLER(0, Gun_GetType())(t, arg);
    crt_sprintf(t + 0x104, "WEAPON_ID_MEDIPACK");
}
HOOK(0x470500, Medipack_Create);

void Medipack_ParseArg(void *unused, ScriptArg *a)
{
    int i = a->index;
    float *out = a->out;

    *out = Script_GetArgReal(a->args, i);
    a->index = i + 1;
}
HOOK(0x470540, Medipack_ParseArg);

void Medipack_Config(void *unused, ConfigArg *a)
{
    static const char kName[] = "Factor of human max damage to add for each round";
    F32(kDefault, 0x3C23D70A); /* 0.01 */
    char *e;
    uint32_t v;

    if (*(void **)(a->weaponType + 0x6E4)) {
        Mem_Free(*(void **)(a->weaponType + 0x6E4));
        *(void **)(a->weaponType + 0x6E4) = NULL;
    }
    e = TagItem_Find(a->config, kName);
    if (!e)
        e = TagItem_AddReal32(a->config, kName, kDefault.f);
    v = *(uint32_t *)(e + 0x124);
    *(void **)(a->weaponType + 0x6E4) = Mem_Alloc(4, 4);
    **(uint32_t **)(a->weaponType + 0x6E4) = v;
}
HOOK(0x470570, Medipack_Config);

void Medipack_Close(void)
{
    TaskType_Free(gMedipackType);
}
HOOK(0x470600, Medipack_Close);
