#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "weapon/weapon.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B1420, void, File_ScanDir, (const char *path, void *fn, void *user));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4F1CD0, void, ParamEnum_Init, (void *e));
ORIG(0x4F1DB0, void, ParamEnum_AddConstants, (void *e, const char *pattern));

#define gWeaponSizeEvent VAR(uint8_t, 0x5391AC)
#define gWeaponInitEvent VAR(uint8_t, 0x5391AD)
#define gWeaponFireEvent VAR(uint8_t, 0x5391AE)
#define gWeaponSizeEvent2 VAR(uint8_t, 0x5391AF)
#define gWeaponInitEvent2 VAR(uint8_t, 0x5391B0)
#define gWeaponTypeEvent VAR(uint8_t, 0x5391B1)
#define gWeaponEventB2 VAR(uint8_t, 0x5391B2)
#define gWeaponMaxId VAR(int, 0x5391B4)
#define gWeaponCategoryEnum ((void *)0x56E248)
#define gWeaponSightEnum ((void *)0x56E238)
#define gWeaponAmmoEnum ((void *)0x56E218)
#define gWeaponIdEnum ((void *)0x56E228)
#define gScriptEmptyString ((const char *)0x567C74)

typedef struct DirEntry {
    char *name;
    int unk04;
    char isDir;
} DirEntry;

ScriptValue *DefineWeaponType(ScriptValue *out, void *data, void *args);

/* runs weapon.qsc in every subfolder of LOCAL:weapons */
void WeaponType_Open(DirEntry *e)
{
    char path[256];
    void *script;

    if (!e->isDir)
        return;
    crt_sprintf(path, "%s/%s", e->name, "weapon.qsc");
    script = Script_Load(path);
    if (!script) {
        Error_Show("WeaponType_Open(): Couldn't load script: %s", "weapon.qsc");
        for (;;)
            ;
    }
    Script_Run(script);
    Script_Free(script);
}
HOOK(0x413A90, WeaponType_Open);

uint8_t WeaponType_GetSizeEvent(void)
{
    return gWeaponSizeEvent;
}
HOOK(0x413B00, WeaponType_GetSizeEvent);

uint8_t WeaponType_GetInitEvent(void)
{
    return gWeaponInitEvent;
}
HOOK(0x413B10, WeaponType_GetInitEvent);

uint8_t Weapon_GetFireEvent(void)
{
    return gWeaponFireEvent;
}
HOOK(0x413B20, Weapon_GetFireEvent);

uint8_t WeaponType_GetSizeEvent2(void)
{
    return gWeaponSizeEvent2;
}
HOOK(0x413B30, WeaponType_GetSizeEvent2);

uint8_t WeaponType_GetInitEvent2(void)
{
    return gWeaponInitEvent2;
}
HOOK(0x413B40, WeaponType_GetInitEvent2);

uint8_t Weapon_GetTypeEvent(void)
{
    return gWeaponTypeEvent;
}
HOOK(0x413B50, Weapon_GetTypeEvent);

uint8_t Weapon_GetEventB2(void)
{
    return gWeaponEventB2;
}
HOOK(0x413B60, Weapon_GetEventB2);

void WeaponType_LoadAll(void)
{
    gWeaponTypeCount = 0;
    File_ScanDir("LOCAL:weapons", WeaponType_Open, NULL);
}
HOOK(0x413B70, WeaponType_LoadAll);

WeaponType *WeaponType_Get(int i)
{
    return &gWeaponTypes[i];
}
HOOK(0x413B90, WeaponType_Get);

int WeaponType_GetCount(void)
{
    return gWeaponTypeCount;
}
HOOK(0x413BB0, WeaponType_GetCount);

int WeaponType_GetMaxId(void)
{
    return gWeaponMaxId;
}
HOOK(0x413BC0, WeaponType_GetMaxId);

WeaponType *WeaponType_Find(int id)
{
    int count = WeaponType_GetCount();
    int i;

    for (i = 0; i < count; i++) {
        WeaponType *w = WeaponType_Get(i);
        if (w->id == id)
            return w;
    }
    return NULL;
}
HOOK(0x413BD0, WeaponType_Find);

WeaponType *WeaponType_FindByKey(const char *key)
{
    int count = WeaponType_GetCount();
    int i;

    for (i = 0; i < count; i++) {
        WeaponType *w = WeaponType_Get(i);
        if (strcmp(w->key, key) == 0)
            return w;
    }
    return NULL;
}
HOOK(0x413C00, WeaponType_FindByKey);

/* asks a weapon task for its weapon type */
WeaponType *Weapon_GetType(void *task)
{
    TaskHandler h = gTaskHandlers[gWeaponTypeEvent][*(uint16_t *)((char *)task + 0x1C)];
    WeaponType *w = NULL;

    if (!h)
        return NULL;
    h(task, &w);
    return w;
}
HOOK(0x413C80, Weapon_GetType);

/* the child task of task that holds the weapon with this id */
void *Task_FindWeapon(void *task, int id)
{
    void **n = *(void ***)((char *)task + 8);

    if (!*n)
        return NULL;
    while (n) {
        WeaponType *w = Weapon_GetType(n);
        if (w && w->id == id)
            return n;
        n = *n;
        if (!n || !*n)
            return NULL;
    }
    return NULL;
}
HOOK(0x413CC0, Task_FindWeapon);

typedef struct WeaponFireArgs {
    int unk00[6];
    int unk18;
    int unk1C;
    WeaponType *type;
    int unk24;
    void *data;
    int unk2C;
} WeaponFireArgs;

void Weapon_Fire(int a0, int a1, int a2, int a3, int a4, int a5, WeaponType *type, int a7, int a8)
{
    WeaponFireArgs f;

    if (type->taskType == TASK_NO_TYPE)
        return;
    f.unk00[0] = a0;
    f.unk00[1] = a1;
    f.unk00[2] = a2;
    f.unk00[3] = a3;
    f.unk00[4] = a4;
    f.unk00[5] = a5;
    f.unk18 = 0;
    f.unk1C = 0;
    f.type = type;
    f.unk24 = a7;
    f.data = type->data;
    f.unk2C = a8;
    gTaskHandlers[gWeaponFireEvent][(uint16_t)type->taskType](NULL, &f);
}
HOOK(0x413D10, Weapon_Fire);

void *WeaponType_GetCategoryEnum(void)
{
    return gWeaponCategoryEnum;
}
HOOK(0x413DB0, WeaponType_GetCategoryEnum);

void *WeaponType_GetSightEnum(void)
{
    return gWeaponSightEnum;
}
HOOK(0x413DC0, WeaponType_GetSightEnum);

void *WeaponType_GetAmmoEnum(void)
{
    return gWeaponAmmoEnum;
}
HOOK(0x413DD0, WeaponType_GetAmmoEnum);

void *WeaponType_GetIdEnum(void)
{
    return gWeaponIdEnum;
}
HOOK(0x413DE0, WeaponType_GetIdEnum);

void WeaponType_Init(void)
{
    gWeaponSizeEvent = TaskEvent_Alloc(0);
    gWeaponInitEvent = TaskEvent_Alloc(0);
    gWeaponFireEvent = TaskEvent_Alloc(0);
    gWeaponSizeEvent2 = TaskEvent_Alloc(0);
    gWeaponInitEvent2 = TaskEvent_Alloc(0);
    gWeaponTypeEvent = TaskEvent_Alloc(0);
    gWeaponEventB2 = TaskEvent_Alloc(0);
    Script_RegisterFunction("DefineWeaponType", DefineWeaponType, NULL, -1);
    Script_RegisterConstant("WEAPONTYPE_GUN", 0);
    Script_RegisterConstant("WEAPONTYPE_PISTOL", 1);
    Script_RegisterConstant("WEAPONTYPE_SHOTGUN", 2);
    Script_RegisterConstant("WEAPONTYPE_KNIFE", 3);
    Script_RegisterConstant("WEAPONTYPE_GRENADE", 4);
    Script_RegisterConstant("WEAPONTYPE_BINOCULAR", 5);
    Script_RegisterConstant("WEAPONTYPE_MEDIPACK", 6);
    Script_RegisterConstant("WEAPONTYPE_PROXIMITYMINE", 7);
    Script_RegisterConstant("SIGHTDISPLAYTYPE_NONE", 0);
    Script_RegisterConstant("SIGHTDISPLAYTYPE_CROSSHAIR", 1);
    Script_RegisterConstant("AMMODISPLAYTYPE_NONE", 0);
    Script_RegisterConstant("AMMODISPLAYTYPE_NORMAL", 1);
    Script_RegisterConstant("AMMODISPLAYTYPE_CLIP", 2);
    Script_RegisterConstant("AMMODISPLAYTYPE_BARREL", 3);
    Script_RegisterConstant("AMMODISPLAYTYPE_SHELL", 4);
    Script_RegisterConstant("AMMODISPLAYTYPE_GRENADE", 5);
    ParamEnum_Init(gWeaponCategoryEnum);
    ParamEnum_AddConstants(gWeaponCategoryEnum, "WEAPONTYPE_.*");
    ParamEnum_Init(gWeaponSightEnum);
    ParamEnum_AddConstants(gWeaponSightEnum, "SIGHTDISPLAYTYPE_.*");
    ParamEnum_Init(gWeaponAmmoEnum);
    ParamEnum_AddConstants(gWeaponAmmoEnum, "AMMODISPLAYTYPE_.*");
    ParamEnum_Init(gWeaponIdEnum);
    ParamEnum_AddConstants(gWeaponIdEnum, "WEAPON_ID_.*");
}
HOOK(0x413DF0, WeaponType_Init);

/* allocates and fills the private data of one of the weapon's task types */
static void init_task_data(void **data, int type, uint8_t sizeEvent, uint8_t initEvent, void *args, int *next)
{
    WeaponInitArgs init;
    int size;

    gTaskHandlers[sizeEvent][(uint16_t)type](NULL, &size);
    init.data = *data = Mem_Alloc(size, 4);
    init.args = args;
    init.next = *next;
    gTaskHandlers[initEvent][(uint16_t)type](NULL, &init);
    *next = init.next;
}

ScriptValue *DefineWeaponType(ScriptValue *out, void *data, void *args)
{
    int id = Script_GetArgInt(args, 0);
    char name[128];
    WeaponType *w;
    int i, next;

    gWeaponTypes[gWeaponTypeCount].id = id;
    for (i = 0; i < gWeaponTypeCount; i++) {
        if (id == gWeaponTypes[i].id) {
            Error_Show("Weapon ID %d not unique", id);
            for (;;)
                ;
        }
    }
    if (gWeaponMaxId < id)
        gWeaponMaxId = id;
    Script_GetArgString(args, 1, name, sizeof(name));
    w = &gWeaponTypes[gWeaponTypeCount];
    Script_RegisterConstant(name, w->id);
    strcpy(w->name, name);
    Script_GetArgString(args, 2, w->unk110, 0x100);
    Script_GetArgString(args, 3, w->unk310, 0x100);
    Script_GetArgString(args, 4, w->unk210, 0x100);
    w->unk6DC = 1;
    w->category = Script_GetArgInt(args, 5);
    w->sightDisplay = Script_GetArgInt(args, 6);
    w->ammoDisplay = Script_GetArgInt(args, 7);
    w->unk004 = Script_GetArgInt(args, 8);
    w->unk63C = Script_GetArgInt(args, 9);
    for (i = 0; i < 4; i++)
        w->unk654[i] = (float)Script_GetArgReal(args, 10 + i);
    for (i = 0; i < 4; i++)
        w->unk640[i] = Script_GetArgInt(args, 14 + i);
    for (i = 0; i < 6; i++)
        w->unk664[i] = (float)Script_GetArgReal(args, 18 + i);
    Script_GetArgString(args, 24, w->unk410, 0x100);
    w->unk650 = (float)Script_GetArgReal(args, 25);
    Script_GetArgString(args, 26, w->unk510, 0x100);
    w->unk008 = Script_GetArgInt(args, 27);
    w->unk00C = Script_GetArgInt(args, 28);
    Script_GetArgString(args, 29, w->key, 0x10);
    Script_GetArgString(args, 30, w->unk62C, 0x10);
    for (i = 0; i < 6; i++)
        w->anim[i] = Script_GetArgInt(args, 31 + i);
    w->anim2[0] = Script_GetArgInt(args, 37);
    w->anim2[1] = Script_GetArgInt(args, 38);
    w->anim2[3] = Script_GetArgInt(args, 39);
    w->anim2[4] = Script_GetArgInt(args, 40);
    w->anim2[2] = Script_GetArgInt(args, 41);
    w->anim2[5] = Script_GetArgInt(args, 42);
    w->anim2[6] = Script_GetArgInt(args, 43);
    Script_GetArgString(args, 44, w->unk6B0, 0x10);
    Script_GetArgString(args, 45, w->unk6C0, 0x10);
    w->detectRange = (float)Script_GetArgReal(args, 46);

    /* each task type may take extra arguments after the fixed ones */
    next = 48;
    w->taskType = Script_GetArgInt(args, 47);
    w->data = NULL;
    if (w->taskType != TASK_NO_TYPE && gTaskHandlers[gWeaponSizeEvent][(uint16_t)w->taskType])
        init_task_data(&w->data, w->taskType, gWeaponSizeEvent, gWeaponInitEvent, args, &next);
    w->taskType2 = Script_GetArgInt(args, next);
    w->data2 = NULL;
    next++;
    if (w->taskType2 != TASK_NO_TYPE && gTaskHandlers[gWeaponSizeEvent2][(uint16_t)w->taskType2])
        init_task_data(&w->data2, w->taskType2, gWeaponSizeEvent2, gWeaponInitEvent2, args, &next);
    gWeaponTypeCount++;
    out->i = 1;
    out->d = 1.0;
    out->s = gScriptEmptyString;
    return out;
}
HOOK(0x413F90, DefineWeaponType);
