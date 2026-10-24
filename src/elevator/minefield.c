#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

/* blows up the object with the given id while a script condition is true */
typedef struct MineField {
    Task task;
    Vec3d pos;
    VarString cond;
    float unk88;
    float unk8C;
    float unk90;
    float unk94;
    int unk98;
    int unk9C;
    char sound[16];
    char unkB0[16];
    int targetId;
    float delay;
    char drop;
    char padC9[3];
    int timer;
} MineField;

_Static_assert(offsetof(MineField, cond) == 0x38, "MineField");
_Static_assert(offsetof(MineField, targetId) == 0xC0, "MineField");
_Static_assert(sizeof(MineField) == 0xD0, "MineField");

extern uint16_t gMineFieldType;
extern void *gMineFieldParams;
extern RangeI gMineFieldTargetRange;
extern RangeI gMineField9CRange;
extern RangeF gMineField88Range;
extern RangeI gMineField98Range;
extern RangeF gMineField90Range;
extern RangeF gMineFieldDelayRange;
extern RangeF gMineField8CRange;
extern RangeF gMineField94Range;

static const float kFrames = 30.0f;
static const float kUnits = 4096.0f;
static const double kZero = 0.0;

void MineField_Create(MineField *m);
void MineField_Run(MineField *m);
void MineField_Delete(MineField *m);
void MineField_GetParams(void *task, void **out);
void MineField_Changed(MineField *m);

void MineField_Init(void)
{
    MineField m;

    gMineFieldType = TaskType_New("MineField", sizeof(MineField), EditorRigidObj_GetType(), 0);
    TaskType_SetHandler(gMineFieldType, 0, MineField_Create);
    TaskType_SetHandler2(gMineFieldType, 1, MineField_Run);
    TaskType_SetHandler2(gMineFieldType, 2, MineField_Delete);
    TaskType_SetHandler(gMineFieldType, Param_GetTableEvent(), MineField_GetParams);
    TaskType_SetHandler(gMineFieldType, Param_GetChangedEvent(), MineField_Changed);
    gMineFieldParams = ParamTable_New(12);
    gMineField88Range = (RangeF){0.0f, 100.0f};
    gMineField8CRange = (RangeF){0.0f, 100.0f};
    gMineField90Range = (RangeF){0.0f, 10.0f};
    gMineField94Range = (RangeF){0.0f, 60.0f};
    gMineFieldDelayRange = (RangeF){-1.0f, 10000.0f};
    gMineField98Range = (RangeI){0, 1000};
    gMineField9CRange = (RangeI){0, 100};
    gMineFieldTargetRange = (RangeI){-1, 0x1000};
    ParamTable_Add(gMineFieldParams, &m, &m.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gMineFieldParams, &m, &m.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gMineFieldParams, &m, &m.unk88, Param_Real32(), 7, &gMineField88Range);
    ParamTable_Add(gMineFieldParams, &m, &m.unk8C, Param_Real32(), 7, &gMineField8CRange);
    ParamTable_Add(gMineFieldParams, &m, &m.unk90, Param_Real32(), 7, &gMineField90Range);
    ParamTable_Add(gMineFieldParams, &m, &m.unk94, Param_Real32(), 7, &gMineField94Range);
    ParamTable_Add(gMineFieldParams, &m, &m.unk98, Param_Int32(), 7, &gMineField98Range);
    ParamTable_Add(gMineFieldParams, &m, &m.unk9C, Param_Int32(), 7, &gMineField9CRange);
    ParamTable_Add(gMineFieldParams, &m, m.sound, Param_String16(), 7, NULL);
    ParamTable_Add(gMineFieldParams, &m, &m.targetId, Param_Int32(), 7, &gMineFieldTargetRange);
    ParamTable_Add(gMineFieldParams, &m, &m.delay, Param_Real32(), 7, &gMineFieldDelayRange);
    ParamTable_Add(gMineFieldParams, &m, &m.drop, Param_Bool8(), 7, NULL);
}
HOOK(0x43A840, MineField_Init);

void MineField_Create(MineField *m)
{
    m->unk88 = 3.0f;
    m->unk90 = 3.0f;
    m->unk8C = 1.5f;
    m->unk94 = 0.0f;
    m->unk98 = 3;
    m->unk9C = 3;
    m->sound[0] = 0;
    m->delay = 5.0f;
    m->timer = 0;
    m->drop = 1;
    m->targetId = 0;
    VarString_Init(&m->cond);
}
HOOK(0x43AB50, MineField_Create);

void MineField_Explode(MineField *m);

void MineField_Run(MineField *m)
{
    long double v;

    if (m->timer == -1)
        return;
    if (m->timer != 0) {
        m->timer--;
        return;
    }
    if (!VarString_HasExpr(&m->cond))
        return;
    v = VarString_EvalReal(&m->cond, m);
    if (v < kZero || v > kZero) {
        MineField_Explode(m);
        m->timer = (int)round_ll(m->delay * kFrames);
    }
}
HOOK(0x43ABC0, MineField_Run);

void MineField_Explode(MineField *m)
{
    Task *t = Task_FindById(m->targetId);
    ObjInfo info;
    TaskFn fn;

    if (!t)
        return;
    fn = HANDLER(7, t->type);
    if (!fn)
        return;
    info.name = NULL;
    fn(t, &info);
    if (m->drop)
        Game_DropToGround(t, &info);
    Explosion_Spawn(&info.pos, 1, (int)round_ll(m->unk94 * kFrames), m->unk90, m->unk88 * kUnits, m->unk8C * kUnits,
                    6144.0f, m->unk98, m->unk9C, 0, "1009_01_1", m->sound, -1, 0, 102400.0f, 0);
}
HOOK(0x43AC40, MineField_Explode);

void MineField_Delete(MineField *m)
{
    VarString_Free(&m->cond);
}
HOOK(0x43AD20, MineField_Delete);

void MineField_GetParams(void *task, void **out)
{
    *out = gMineFieldParams;
}
HOOK(0x43AD30, MineField_GetParams);

void MineField_Changed(MineField *m)
{
    VarString_Bind(&m->cond, m);
}
HOOK(0x43AD40, MineField_Changed);

void MineField_Close(void)
{
    QTaskList_Free(gMineFieldParams);
    TaskType_Free(gMineFieldType);
}
HOOK(0x43AD60, MineField_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53C2D0) uint16_t gMineFieldType INIT(385);
PAD(igd, 53C2D0, 53C2D2, 0x2);
BSS(57BEB0) RangeI gMineFieldTargetRange;
BSS(57BEB0) RangeI gMineField9CRange;
BSS(57BEB0) RangeF gMineField88Range;
BSS(57BEB0) RangeI gMineField98Range;
BSS(57BEB0) RangeF gMineField90Range;
BSS(57BEB0) RangeF gMineFieldDelayRange;
BSS(57BEB0) RangeF gMineField8CRange;
BSS(57BEB0) RangeF gMineField94Range;
PAD(igb, 57BEB0, 57BEF0, 0x4);
BSS(57BEB0) void *gMineFieldParams;
