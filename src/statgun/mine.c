#include "hook.h"
#include "statgun/statgun.h"

FN_EXTERN(Mine_DeleteHandler);
FN_EXTERN(Mine_GetParamTableHandler);
FN_EXTERN(Mine_MapHandler);
FN_EXTERN(Mine_ParamChangedHandler);
FN_EXTERN(Mine_ProximityHandler);
FN_EXTERN(Mine_RunHandler);

/* "Mine" (0x250 bytes), a placed mine, an ExplodeObject */
typedef struct Mine {
    Task task;
    char unk20[0x150];
    char model[0xC8];   /* 0x170 */
    int unk238;
    int unk23C;
    float unk240;
    float unk244;
    char unk248;
    char pad249[7];
} Mine;

_Static_assert(offsetof(Mine, unk238) == 0x238, "Mine");
_Static_assert(sizeof(Mine) == 0x250, "Mine");

extern uint16_t gMineType;
extern void *gMineParams;
extern void *gMineList;

#define HANDLER_475CC0 (FN(0x475CC0, Mine_RunHandler))
#define HANDLER_475D30 (FN(0x475D30, Mine_DeleteHandler))
#define HANDLER_475D80 (FN(0x475D80, Mine_GetParamTableHandler))
#define HANDLER_475D90 (FN(0x475D90, Mine_ParamChangedHandler))
#define HANDLER_475DF0 (FN(0x475DF0, Mine_ProximityHandler))
#define HANDLER_475F30 (FN(0x475F30, Mine_MapHandler))

ORIG(0x481340, uint16_t, ExplodeObject_GetType, (void));
ORIG(0x481350, void *, ExplodeObject_GetParams, (void));
ORIG(0x481360, void, ExplodeObject_Setup, (void *o, float a, float b, float c, float d, int e, int f, int g,
                                          const char *sound));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4167D0, uint8_t, Game_GetEvent0, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *base, int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4168B0, void *, Game_GetTaskList1, (void));
ORIG(0x476030, void *, Mine_GetList, (void));
ORIG(0x4168F0, void *, Game_GetCutsceneList, (void));

void Mine_Create(Mine *m, void *arg);

void Mine_Open(void)
{
    Mine m;

    gMineType = TaskType_New("Mine", sizeof(Mine), ExplodeObject_GetType(), 0);
    TaskType_SetHandler(gMineType, 0, Mine_Create);
    TaskType_SetHandler2(gMineType, 1, HANDLER_475CC0);
    TaskType_SetHandler2(gMineType, 2, HANDLER_475D30);
    TaskType_SetHandler(gMineType, Param_GetTableEvent(), HANDLER_475D80);
    TaskType_SetHandler(gMineType, Param_GetChangedEvent(), HANDLER_475D90);
    TaskType_SetHandler(gMineType, Game_GetEvent0(), HANDLER_475DF0);
    TaskType_SetHandler(gMineType, ComputerObject_GetEvent0(), HANDLER_475F30);
    gMineList = QTaskList_New(0x40);
    gMineParams = ParamTable_Extend(ExplodeObject_GetParams(), 2);
    ParamTable_Add(gMineParams, &m, &m.unk244, Param_Real32(), 7, NULL);
    ParamTable_Add(gMineParams, &m, &m.unk240, Param_Real32(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_MINE", gMineType);
}
HOOK(0x475AC0, Mine_Open);

void Mine_Create(Mine *m, void *arg)
{
    QTaskList_Add(Game_GetTaskList1(), m);
    QTaskList_Add(Mine_GetList(), m);
    QTaskList_Add(Game_GetCutsceneList(), m);
    HANDLER(0, ExplodeObject_GetType())(m, arg);
    ExplodeObject_Setup(m, 1.0f, 2.0f, 0.0f, 1.0f, 0, 5, 1, "explo_proxy");
    crt_sprintf(m->model, "136_01_1");
    m->unk238 = 0;
    m->unk23C = 0;
    m->unk240 = 0.75f;
    m->unk244 = 3.0f;
    m->unk248 = 0;
}
HOOK(0x475C10, Mine_Create);
