#include <string.h>

#include "hook.h"
#include "igi.h"
#include "profile/param.h"
#include "profile/profile.h"
#include "humanai/humanaitypes.h"
#include "task/taskvars.h"
#include "math/mathvars.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4012A0, void *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401E30, void, Task_BroadcastChildren, (void *task, uint8_t slot, void *arg));
ORIG(0x452910, const char *, AIType_GetName, (int type));
ORIG(0x45D5A0, uint16_t, Soldier_GetType, (void));
ORIG(0x4886F0, void, SubLevel_Create, (void *task, void *arg));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4C1780, void, Debug_Log, (void *task, const char *name));
ORIG(0x4D3710, void, Task_Nothing, (void *task, void *arg));

#define AITYPE_COUNT 29
#define DIFFICULTY_COUNT 3

typedef struct HumanAIConfigItem {
    Task task;
    int difficulty;
    int aiType;
    char difficultyExpr[0x100];
    char aiTypeExpr[0x100];
    int unk228;
    float unk22C;
    float unk230;
    float unk234;
    float unk238;
    float unk23C;
    float unk240;
    float unk244;
    float unk248;
    float unk24C;
    float unk250;
    float unk254;
    float unk258;
    float unk25C;
    float unk260;
    float unk264;
    float unk268;
    float unk26C;
    float unk270;
    float unk274;
    float unk278;
    float unk27C;
    float unk280;
    float unk284;
    float unk288;
    float unk28C;
    float unk290;
    float unk294;
    float unk298;
    float unk29C;
    float unk2A0;
    float unk2A4;
} HumanAIConfigItem;

/* settings for every AI type and difficulty, from common/ai/settings.qsc */
typedef struct HumanAIConfig {
    Task task;
    char unk20;
    char unk21[3];
    HumanAIConfigItem *items[AITYPE_COUNT][DIFFICULTY_COUNT];
} HumanAIConfig;

/* the creation argument of a HumanAIConfigItem */
typedef struct HumanAIConfigNames {
    const char *difficulty;
    const char *aiType;
} HumanAIConfigNames;

_Static_assert(sizeof(HumanAIConfigItem) == 0x2A8, "HumanAIConfigItem");
_Static_assert(sizeof(HumanAIConfig) == 0x180, "HumanAIConfig");

extern uint16_t gHumanAIConfigType;
extern uint16_t gHumanAIConfigItemType;
extern ParamTable *gHumanAIConfigParams;
extern ParamRange gHumanAIIntRange;
extern ParamRange gHumanAIRealRange;
extern ParamTable *gHumanAIConfigItemParams;
extern HumanAIConfig *gHumanAIConfig;
extern float gFramesPerSecond;

void HumanAIConfig_GetParams(void *task, ParamTable **out);
void HumanAIConfigItem_Create(HumanAIConfigItem *item, HumanAIConfigNames *names);
void HumanAIConfigItem_GetParams(void *task, ParamTable **out);
void HumanAIConfigItem_Update(HumanAIConfigItem *item);
void HumanAIConfigItem_UpdateName(HumanAIConfigItem *item);
uint16_t HumanAIConfig_GetType(void);
uint16_t HumanAIConfigItem_GetType(void);

void HumanAIConfig_Init(void)
{
    HumanAIConfig hc;
    HumanAIConfigItem item;

    gHumanAIConfigType = TaskType_New("HumanAIConfig", sizeof(HumanAIConfig), Task_GetRootType(), 0);
    gHumanAIConfigItemType = TaskType_New("HumanAIConfigItem", sizeof(HumanAIConfigItem), Task_GetRootType(), 0);
    TaskType_SetHandler(gHumanAIConfigType, 0, SubLevel_Create);
    TaskType_SetHandler2(gHumanAIConfigType, 2, Task_Nothing);
    TaskType_SetHandler(gHumanAIConfigType, Param_GetTableEvent(), HumanAIConfig_GetParams);
    TaskType_SetHandler(gHumanAIConfigType, Param_GetChangedEvent(), Task_Nothing);
    gHumanAIConfigParams = ParamTable_New(1);
    ParamTable_Add(gHumanAIConfigParams, &hc, &hc.unk20, Param_PushButton(), 7, NULL);
    TaskType_SetHandler(gHumanAIConfigItemType, 0, HumanAIConfigItem_Create);
    TaskType_SetHandler2(gHumanAIConfigItemType, 1, Task_Nothing);
    TaskType_SetHandler2(gHumanAIConfigItemType, 2, Task_Nothing);
    TaskType_SetHandler(gHumanAIConfigItemType, Param_GetTableEvent(), HumanAIConfigItem_GetParams);
    TaskType_SetHandler(gHumanAIConfigItemType, Param_GetChangedEvent(), HumanAIConfigItem_Update);
    gHumanAIIntRange.i.min = 1;
    gHumanAIIntRange.i.max = 60;
    gHumanAIRealRange.f.min = 0;
    gHumanAIRealRange.f.max = 10.0f;
    gHumanAIConfigItemParams = ParamTable_New(0x22);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.aiTypeExpr, Param_String256(), 3, NULL);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.difficultyExpr, Param_String256(), 3, NULL);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk2A0, Param_Real32(), 7, &gHumanAIRealRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk2A4, Param_Real32(), 7, &gHumanAIRealRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk22C, Param_Real32(), 7, Param_Range02());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk230, Param_Real32(), 7, Param_Range02());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk268, Param_Real32(), 7, Param_Range0100());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk270, Param_Real32(), 7, Param_Range0100());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk26C, Param_Real32(), 7, Param_Range0100());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk234, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk238, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk23C, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk240, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk244, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk248, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk24C, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk228, Param_Int32(), 7, &gHumanAIIntRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk274, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk250, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk254, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk258, Param_Real32(), 7, Param_Range02());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk25C, Param_Real32(), 7, Param_Range02());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk260, Param_Real32(), 7, Param_Range02());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk264, Param_Real32(), 7, Param_Range02());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk278, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk27C, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk280, Param_Real32(), 7, Param_Range0100());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk284, Param_Real32(), 7, Param_Range0100());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk288, Param_Real32(), 7, &gHumanAIRealRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk28C, Param_Real32(), 7, &gHumanAIRealRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk290, Param_Real32(), 7, &gHumanAIRealRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk294, Param_Real32(), 7, &gHumanAIRealRange);
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk298, Param_Real32(), 7, Param_Range01());
    ParamTable_Add(gHumanAIConfigItemParams, &item, &item.unk29C, Param_Real32(), 7, &gHumanAIRealRange);
}
HOOK(0x406450, HumanAIConfig_Init);

void HumanAIConfig_GetParams(void *task, ParamTable **out)
{
    *out = gHumanAIConfigParams;
}
HOOK(0x406B60, HumanAIConfig_GetParams);

void HumanAIConfigItem_Create(HumanAIConfigItem *item, HumanAIConfigNames *names)
{
    if (names) {
        strcpy(item->aiTypeExpr, names->aiType);
        strcpy(item->difficultyExpr, names->difficulty);
    } else {
        item->aiTypeExpr[0] = 0;
        item->difficultyExpr[0] = 0;
    }
    item->aiType = 0;
    item->difficulty = 0;
    item->unk228 = 30;
    item->unk22C = 0.33f;
    item->unk230 = 1.0f;
    item->unk234 = 0.3f;
    item->unk238 = 0.5f;
    item->unk23C = 0.5f;
    item->unk240 = 0.5f;
    item->unk244 = 0.5f;
    item->unk248 = 1.0f;
    item->unk24C = 0.25f;
    item->unk250 = 0.1f;
    item->unk254 = 0.2f;
    item->unk258 = 1.5f;
    item->unk25C = 1.0f;
    item->unk260 = 0.1f;
    item->unk264 = 0;
    item->unk268 = 10.0f;
    item->unk26C = 15.0f;
    item->unk270 = 15.0f;
    item->unk274 = 0.2f;
    item->unk278 = 0.33f;
    item->unk27C = 0.1f;
    item->unk280 = 10.0f;
    item->unk284 = 60.0f;
    item->unk288 = 0.5f;
    item->unk28C = 0.5f;
    item->unk290 = 0.3f;
    item->unk294 = 0.7f;
    item->unk298 = 0.4f;
    item->unk29C = 0.5f;
    item->unk2A0 = 1.0f;
    item->unk2A4 = 0.33f;
}
HOOK(0x406B70, HumanAIConfigItem_Create);

void HumanAIConfigItem_GetParams(void *task, ParamTable **out)
{
    *out = gHumanAIConfigItemParams;
}
HOOK(0x406D00, HumanAIConfigItem_GetParams);

void HumanAIConfigItem_Update(HumanAIConfigItem *item)
{
    item->aiType = Script_EvalExpr(item->aiTypeExpr);
    item->difficulty = Script_EvalExpr(item->difficultyExpr);
    HumanAIConfigItem_UpdateName(item);
}
HOOK(0x406D10, HumanAIConfigItem_Update);

void HumanAIConfigItem_UpdateName(HumanAIConfigItem *item)
{
    char buf[256];
    const char *ai = AIType_GetName(item->aiType);

    crt_sprintf(buf, "%s, %s", ai, Config_GetDifficultyName(item->difficulty));
    Debug_Log(item, buf);
}
HOOK(0x406D40, HumanAIConfigItem_UpdateName);

void HumanAIConfig_Shutdown(void)
{
    QTaskList_Free(gHumanAIConfigParams);
    QTaskList_Free(gHumanAIConfigItemParams);
    TaskType_Free(gHumanAIConfigType);
    TaskType_Free(gHumanAIConfigItemType);
}
HOOK(0x406D90, HumanAIConfig_Shutdown);

uint16_t HumanAIConfig_GetType(void)
{
    return gHumanAIConfigType;
}
HOOK(0x406DD0, HumanAIConfig_GetType);

uint16_t HumanAIConfigItem_GetType(void)
{
    return gHumanAIConfigItemType;
}
HOOK(0x406DE0, HumanAIConfigItem_GetType);

static HumanAIConfigItem *NewItem(HumanAIConfig *hc, int ai, int difficulty)
{
    HumanAIConfigNames names;

    names.aiType = AIType_GetName(ai);
    names.difficulty = Config_GetDifficultyName(difficulty);
    return Task_New(hc, HumanAIConfigItem_GetType(), &names);
}

/* fills in a default item for every pair the script did not define */
void HumanAIConfig_Load(void *parent)
{
    HumanAIConfig *hc = Task_New(parent, HumanAIConfig_GetType(), NULL);
    HumanAIConfigItem *item;
    int ai, d;

    if (!hc) {
        Warning_Show("Unable to create HumanAIConfig task");
        return;
    }
    gHumanAIConfig = hc;
    memset(hc->items, 0, sizeof(hc->items));
    if (!Task_RunScript(hc, "LOCAL:common/ai/settings.qsc", 0)) {
        for (ai = 0; ai < AITYPE_COUNT; ai++)
            for (d = 0; d < DIFFICULTY_COUNT; d++)
                hc->items[ai][d] = NewItem(hc, ai, d);
        Task_BroadcastChildren(hc, Param_GetChangedEvent(), NULL);
        return;
    }
    item = hc->task.head->next ? (HumanAIConfigItem *)hc->task.head : NULL;
    Task_BroadcastChildren(hc, Param_GetChangedEvent(), NULL);
    while (item) {
        hc->items[item->aiType][item->difficulty] = item;
        item = (HumanAIConfigItem *)item->task.next;
        if (!item || !item->task.next)
            break;
    }
    for (ai = 0; ai < AITYPE_COUNT; ai++) {
        for (d = 0; d < DIFFICULTY_COUNT; d++) {
            if (hc->items[ai][d])
                continue;
            item = NewItem(hc, ai, d);
            hc->items[ai][d] = item;
            gTaskHandlers[Param_GetChangedEvent()][item->task.type](item, NULL);
        }
    }
}
HOOK(0x406DF0, HumanAIConfig_Load);

/* fistp, so it follows the FPU rounding mode */
static int Round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

#define COPY(dst, src) memcpy(&(dst), &(src), 4)

void HumanAIConfig_Apply(HumanAI *ai)
{
    HumanAIConfig *hc = gHumanAIConfig;
    char *owner = (char *)ai->task.parent;
    PlayerProfile *p;
    HumanAIConfigItem *c;
    int i;

    if (!hc) {
        Warning_Show("No HumanAIConfig task found");
        return;
    }
    p = Profile_GetActive();
    if (!p) {
        Warning_Show("No active playerprofile found");
        return;
    }
    c = hc->items[ai->aiType][p->difficulty];
    if (!c) {
        Warning_Show("No configuration found for AIType: #%d, Difficulty: #%d", AIType_GetName(ai->aiType),
                     p->difficulty);
        return;
    }
    ai->unk3D48 = Round((long double)c->unk230 * gFramesPerSecond);
    ai->unk3D44 = Round((long double)c->unk22C * gFramesPerSecond);
    ai->maxBurst = c->unk228;
    ai->maxTurns = Round((long double)c->unk268 * gFramesPerSecond);
    for (i = 0; i < 13; i++)
        COPY((&ai->evadeChance)[i], (&c->unk234)[i]);
    COPY(ai->grenadeChance, c->unk278);
    COPY(ai->grenadeChance2, c->unk27C);
    ai->grenadeMin = c->unk280 * k4096;
    ai->grenadeMax = c->unk284 * k4096;
    for (i = 0; i < 6; i++)
        COPY((&ai->throwSpeedMin)[i], (&c->unk288)[i]);
    ai->coverRange = c->unk270 * k4096;
    ai->fallFlatRange = c->unk26C * k4096;
    ai->aimSpeed = c->unk274;
    COPY(*(float *)(owner + 0x258), c->unk2A0);
    if (((Task *)owner)->type == Soldier_GetType())
        COPY(*(float *)(owner + 0xD04), c->unk2A4);
}
HOOK(0x406F70, HumanAIConfig_Apply);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5383D8) uint16_t gHumanAIConfigType INIT(385);
DATA(5383D8) uint16_t gHumanAIConfigItemType INIT(385);
BSS(5690B0) ParamTable *gHumanAIConfigParams;
PAD(igb, 5690B0, 5690B4, 0x4);
BSS(5690B0) ParamRange gHumanAIIntRange;
BSS(5690B0) ParamRange gHumanAIRealRange;
BSS(5690B0) ParamTable *gHumanAIConfigItemParams;
PAD(igb, 5690B0, 5690CC, 0x4);
BSS(5690B0) HumanAIConfig *gHumanAIConfig;
