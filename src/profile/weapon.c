#include <string.h>

#include "hook.h"
#include "igi.h"
#include "profile/param.h"
#include "profile/profile.h"
#include "core/pool.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x413BD0, void *, WeaponType_Find, (int id));
ORIG(0x413DB0, void *, WeaponType_GetCategoryEnum, (void));
ORIG(0x413DC0, void *, WeaponType_GetSightEnum, (void));
ORIG(0x413DD0, void *, WeaponType_GetAmmoEnum, (void));
ORIG(0x4D3710, void, Task_Nothing, (void *task, void *arg));

/* the task loaded from weapons/weaponconfig.qsc */
typedef struct WeaponConfigContainer {
    TaskHeader task;
    char unk20;
    char path[0x100];
    char unk121[3];
} WeaponConfigContainer;

/* everything after the task header is copied into the weapon type */
typedef struct WeaponConfig {
    TaskHeader task;
    int id;
    int unk24;
    int unk28;
    int unk2C;
    char unk30[0x100];
    char unk130[0x100];
    char unk230[0x100];
    char unk330[0x100];
    char unk430[0x100];
    char unk530[0x100];
    int unk630;
    int unk634;
    int unk638;
    char unk63C[0x10];
    char unk64C[0x10];
    int unk65C;
    int unk660;
    int unk664;
    int unk668;
    int unk66C;
    float unk670;
    float unk674;
    float unk678;
    float unk67C;
    float unk680;
    float unk684;
    float unk688;
    float unk68C;
    float unk690;
    float unk694;
    float unk698;
    int unk69C;
    int unk6A0;
    int unk6A4;
    int unk6A8;
    int unk6AC;
    int unk6B0;
    int unk6B4;
    int unk6B8;
    int unk6BC;
    int unk6C0;
    int unk6C4;
    int unk6C8;
    int unk6CC;
    char unk6D0[0x10];
    char unk6E0[0x10];
    float unk6F0;
    int unk6F4;
    int unk6F8;
    char unk6FC;
    char unk6FD[3];
    int unk700;
    int unk704;
} WeaponConfig;

/* what the WeaponConfig event handlers get */
typedef struct WeaponConfigApply {
    WeaponConfig *config;
    int *weapon;
} WeaponConfigApply;

_Static_assert(sizeof(WeaponConfigContainer) == 0x124, "WeaponConfigContainer");
_Static_assert(sizeof(WeaponConfig) == 0x708, "WeaponConfig");

#define WEAPON_CONFIG_SIZE (sizeof(WeaponConfig) - sizeof(TaskHeader))

typedef struct TaskType {
    const char *name;
    int size;
    uint16_t parent;
    uint16_t unk0A;
    Pool pool;
    char pooled;
    char used;
    char unk26[2];
    uint16_t mask;
    uint16_t unk2A;
    uint32_t flags;
} TaskType;

_Static_assert(sizeof(TaskType) == 0x30, "TaskType");

extern uint16_t gWeaponConfigContainerType;
extern uint16_t gWeaponConfigType;
extern uint8_t gWeaponConfigEvent;
extern char gDetectionRanges;
extern char gWeaponConfigPath[];
extern char gTaskTypeEnum;
extern ParamTable *gWeaponConfigContainerParams;
extern ParamTable *gWeaponConfigParams;
extern TaskType gTaskTypes[];

void WeaponConfigContainer_Create(WeaponConfigContainer *c);
void WeaponConfigContainer_GetParams(void *task, ParamTable **out);
void WeaponConfig_Create(WeaponConfig *c);
void WeaponConfig_Clear(WeaponConfig *c);
void WeaponConfig_Apply(WeaponConfig *c);
int TaskType_IsUsed(int type);
void WeaponConfig_GetParams(void *task, ParamTable **out);

uint8_t WeaponConfig_GetEvent(void)
{
    return gWeaponConfigEvent;
}
HOOK(0x4071D0, WeaponConfig_GetEvent);

/* runs the script, then sends every top level task it made the changed event */
void *WeaponConfig_Read(void *parent, const char *path)
{
    TaskHeader *root, *t, *next;

    strcpy(gWeaponConfigPath, path);
    root = Task_RunScript(parent, path, 0);
    if (root && root->head->next) {
        t = root->head;
        while (t) {
            next = t->next;
            gTaskStack[gTaskStackTop++] = next && next->next ? next : NULL;
            gTaskHandlers[Param_GetChangedEvent()][t->type](t, NULL);
            t = gTaskStack[--gTaskStackTop];
        }
    }
    return root;
}
HOOK(0x4071E0, WeaponConfig_Read);

void WeaponConfig_Init(void)
{
    WeaponConfigContainer cont;
    WeaponConfig cfg;

    gWeaponConfigEvent = TaskEvent_Alloc(0);
    gWeaponConfigContainerType = TaskType_New("WeaponConfigContainer", sizeof(WeaponConfigContainer),
                                              Task_GetRootType(), 0);
    TaskType_SetHandler(gWeaponConfigContainerType, 0, WeaponConfigContainer_Create);
    TaskType_SetHandler(gWeaponConfigContainerType, Param_GetChangedEvent(), Task_Nothing);
    TaskType_SetHandler(gWeaponConfigContainerType, Param_GetTableEvent(), WeaponConfigContainer_GetParams);
    gWeaponConfigType = TaskType_New("WeaponConfig", sizeof(WeaponConfig), Task_GetRootType(), 0);
    TaskType_SetHandler(gWeaponConfigType, 0, WeaponConfig_Create);
    TaskType_SetHandler(gWeaponConfigType, Param_GetChangedEvent(), WeaponConfig_Apply);
    TaskType_SetHandler(gWeaponConfigType, Param_GetTableEvent(), WeaponConfig_GetParams);
    ParamEnum_Init(&gDetectionRanges);
    ParamEnum_AddConstants(&gDetectionRanges, "HUMANAI_DETECTIONEVENT_.*_RANGE");
    ParamEnum_Init(&gTaskTypeEnum);
    ParamEnum_AddConstants(&gTaskTypeEnum, "TASKTYPE_.*");
    gWeaponConfigContainerParams = ParamTable_New(2);
    ParamTable_Add(gWeaponConfigContainerParams, &cont, &cont.unk20, Param_PushButton(), 6, NULL);
    ParamTable_Add(gWeaponConfigContainerParams, &cont, &cont.path, Param_String256(), 2, NULL);
    gWeaponConfigParams = ParamTable_New(0x32);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.id, Param_Int32(), 3, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk30, Param_String256(), 3, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk130, Param_String256(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk330, Param_String256(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk230, Param_String256(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk630, Param_EnumInt32(), 7, WeaponType_GetCategoryEnum());
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk634, Param_EnumInt32(), 7, WeaponType_GetSightEnum());
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk638, Param_EnumInt32(), 7, WeaponType_GetAmmoEnum());
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk24, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk65C, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk674, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk678, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk67C, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk680, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk660, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk664, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk668, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk66C, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk684, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk688, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk68C, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk690, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk694, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk698, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk430, Param_String256(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk670, Param_Real32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk530, Param_String256(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk28, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk2C, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk63C, Param_String16(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk64C, Param_String16(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk69C, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6A0, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6A4, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6A8, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6AC, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6B0, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6B4, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6B8, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6C0, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6C4, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6BC, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6C8, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6CC, Param_Int32(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6D0, Param_String16(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6E0, Param_String16(), 7, NULL);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6F0, Param_EnumReal32(), 7, &gDetectionRanges);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6F4, Param_EnumInt32(), 7, &gTaskTypeEnum);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6F8, Param_EnumInt32(), 7, &gTaskTypeEnum);
    ParamTable_Add(gWeaponConfigParams, &cfg, &cfg.unk6FC, Param_Bool8(), 7, NULL);
}
HOOK(0x407290, WeaponConfig_Init);

void WeaponConfigContainer_Create(WeaponConfigContainer *c)
{
    c->unk20 = 0;
    strcpy(c->path, gWeaponConfigPath);
}
HOOK(0x407B50, WeaponConfigContainer_Create);

void WeaponConfigContainer_GetParams(void *task, ParamTable **out)
{
    *out = gWeaponConfigContainerParams;
}
HOOK(0x407B90, WeaponConfigContainer_GetParams);

void WeaponConfig_Create(WeaponConfig *c)
{
    WeaponConfig_Clear(c);
}
HOOK(0x407BA0, WeaponConfig_Create);

void WeaponConfig_Clear(WeaponConfig *c)
{
    memset(&c->id, 0, WEAPON_CONFIG_SIZE);
    c->unk6FC = 1;
    c->id = -1;
}
HOOK(0x407BB0, WeaponConfig_Clear);

/* copies the settings into the weapon type with the same id */
void WeaponConfig_Apply(WeaponConfig *c)
{
    WeaponConfigApply a;
    int *w = WeaponType_Find(c->id);
    int keep0, keep1;
    TaskHandler h;

    if (!w)
        return;
    keep0 = w[0x1B8];
    keep1 = w[0x1B9];
    a.config = c;
    a.weapon = w;
    memcpy(w, &c->id, WEAPON_CONFIG_SIZE);
    w[0x1B8] = keep0;
    w[0x1B9] = keep1;
    if (TaskType_IsUsed(c->unk6F8) && (h = gTaskHandlers[gWeaponConfigEvent][(uint16_t)c->unk6F8]) != NULL)
        h(NULL, &a);
    if (TaskType_IsUsed(c->unk6F4) && (h = gTaskHandlers[gWeaponConfigEvent][(uint16_t)c->unk6F4]) != NULL)
        h(NULL, &a);
}
HOOK(0x407BE0, WeaponConfig_Apply);

int TaskType_IsUsed(int type)
{
    if (type >= 0 && type < 0x180)
        return gTaskTypes[type].used;
    return 0;
}
HOOK(0x407CD0, TaskType_IsUsed);

void WeaponConfig_GetParams(void *task, ParamTable **out)
{
    *out = gWeaponConfigParams;
}
HOOK(0x407CF0, WeaponConfig_GetParams);

void WeaponConfig_Shutdown(void)
{
    ParamEnum_Free(&gTaskTypeEnum);
    ParamEnum_Free(&gDetectionRanges);
    QTaskList_Free(gWeaponConfigParams);
    QTaskList_Free(gWeaponConfigContainerParams);
    TaskType_Free(gWeaponConfigType);
    TaskType_Free(gWeaponConfigContainerType);
    TaskEvent_Free(gWeaponConfigEvent);
}
HOOK(0x407D00, WeaponConfig_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5384C0) uint16_t gWeaponConfigContainerType INIT(385);
DATA(5384C0) uint16_t gWeaponConfigType INIT(385);
BSS(5690D8) uint8_t gWeaponConfigEvent;
PAD(igb, 5690D8, 5690D9, 0x7);
BSS(5690D8) char gDetectionRanges;
PAD(igb, 5690D8, 5690E1, 0xF);
PAD(igb, 5690D8, 5690F0, 0x100);
ALIAS(gWeaponConfigPath, pad_5690F0, 0);
BSS(5690D8) char gTaskTypeEnum;
PAD(igb, 5690D8, 5691F1, 0x13);
BSS(5690D8) ParamTable *gWeaponConfigContainerParams;
BSS(5690D8) ParamTable *gWeaponConfigParams;
