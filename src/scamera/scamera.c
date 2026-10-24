#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "scamera/scamera.h"

FN_EXTERN(Car_CollFilter);

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x4169D0, void *, Game_AddSprite, (const char *name));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x467DB0, uint8_t, ComputerObject_GetEvent1, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489620, uint8_t, AIDetection_GetEvent1, (void));
ORIG(0x489640, int, AIDetection_AddMaskType, (uint16_t type));
ORIG(0x489680, void, AIDetection_Add, (void *obj));
ORIG(0x4896A0, void, AIDetection_Remove, (void *obj));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FB0, void *, Param_Int16, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, uint8_t, Level_GetEventDA8, (void));
ORIG(0x4F1220, uint8_t, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define gWorld VAR(void *, 0xA44344)
#define gAlarmLightSprite VAR(void *, 0x57C17C)
#define gSCameraType VAR(uint16_t, 0x57C180)
#define gSCameraParams VAR(void *, 0x57C184)
#define gViewConeType VAR(uint16_t, 0x57C188)
#define gLensList VAR(void *, 0x57C18C)
#define gLensType VAR(uint16_t, 0x57C190)
#define gSCameraDetectMask VAR(int, 0x57C198)

/* handlers that live outside this folder */
#define sub_42FC50 (FN(0x42FC50, Car_CollFilter))

void SCamera_Create(SCamera *c);
void SCamera_Run(SCamera *c);
void SCamera_Delete(SCamera *c);
void SCamera_GetParams(Task *t, void **out);
void SCamera_ParamsChanged(SCamera *c, char *arg);
void SCamera_AddSymbols(SCamera *c);
void SCamera_RemoveSymbols(SCamera *c);
void SCamera_GetDetectInfo(SCamera *c, char *out);
void SCamera_WorldRemove(SObj *o, void **arg);
void SCamera_WorldAdd(SCamera *c, void **arg);
void SCameraLens_Create(SCameraLens *l);
void SCameraLens_Run(SCameraLens *l);
void SCameraLens_Delete(SCameraLens *l);
void SCameraLens_ParamsChanged(SCameraLens *l);
void SCameraLens_Hit(SCameraLens *l, float *arg);
void SCameraLens_Blast(SCameraLens *l, void *blast);
void SCameraLens_IsOther(SCameraLens *l, char *arg);
void SCameraLens_Draw(SCameraLens *l);
void SCameraLens_DrawOnMap(SCameraLens *l, void **arg);
void SCameraLens_DrawViewCone(SCameraLens *l, void *arg);
void SCameraLens_WorldAdd(SCameraLens *l, void **arg);

#define FIELD(name) ((void *)offsetof(SCamera, name))

void SCamera_Open(void)
{
    gSCameraType = TaskType_New("SCamera", sizeof(SCamera), SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gSCameraType, 0, SCamera_Create);
    TaskType_SetHandler2(gSCameraType, 1, SCamera_Run);
    TaskType_SetHandler2(gSCameraType, 2, SCamera_Delete);
    TaskType_SetHandler(gSCameraType, Param_GetTableEvent(), SCamera_GetParams);
    TaskType_SetHandler(gSCameraType, Param_GetChangedEvent(), SCamera_ParamsChanged);
    TaskType_SetHandler2(gSCameraType, Level_GetEventDA8(), SCamera_AddSymbols);
    TaskType_SetHandler2(gSCameraType, Level_GetEventDAC(), SCamera_RemoveSymbols);
    TaskType_SetHandler(gSCameraType, AIDetection_GetEvent1(), SCamera_GetDetectInfo);
    TaskType_SetHandler(gSCameraType, AIDetection_GetEvent0(), sub_42FC50);
    TaskType_SetHandler(gSCameraType, World_GetEvent1(), SCamera_WorldRemove);
    TaskType_SetHandler(gSCameraType, World_GetEvent0(), SCamera_WorldAdd);
    gSCameraParams = ParamTable_New(15);
    ParamTable_Add(gSCameraParams, NULL, FIELD(place), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(yaw), Param_Angle(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(model), Param_String16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(pitch), Param_Angle(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(yaw_offset), Param_Angle(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(lens_model), Param_String16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(broken_model), Param_String16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(yaw_min), Param_Int16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(yaw_max), Param_Int16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(turn_speed), Param_Int16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(wait), Param_Real32(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(fov_x), Param_Int16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(fov_y), Param_Int16(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(range), Param_Real32(), 7, NULL);
    ParamTable_Add(gSCameraParams, NULL, FIELD(script), Param_VarString(), 7, NULL);

    gLensType = TaskType_New("SCameraLens", sizeof(SCameraLens), SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gLensType, 0, SCameraLens_Create);
    TaskType_SetHandler2(gLensType, 1, SCameraLens_Run);
    TaskType_SetHandler2(gLensType, 2, SCameraLens_Delete);
    TaskType_SetHandler(gLensType, Param_GetChangedEvent(), SCameraLens_ParamsChanged);
    TaskType_SetHandler(gLensType, Game_GetEvent9(), SCameraLens_Hit);
    TaskType_SetHandler(gLensType, Game_GetEvent11(), SCameraLens_Blast);
    TaskType_SetHandler(gLensType, AIDetection_GetEvent0(), SCameraLens_IsOther);
    TaskType_SetHandler(gLensType, 4, SCameraLens_Draw);
    TaskType_SetHandler(gLensType, ComputerObject_GetEvent0(), SCameraLens_DrawOnMap);
    TaskType_SetHandler(gLensType, ComputerObject_GetEvent1(), SCameraLens_DrawViewCone);
    TaskType_SetHandler(gLensType, World_GetEvent1(), SCamera_WorldRemove);
    TaskType_SetHandler(gLensType, World_GetEvent0(), SCameraLens_WorldAdd);
    gAlarmLightSprite = Game_AddSprite("alarmlight.spr");
    gLensList = QTaskList_New(0x40);

    /* the view cone ignores everything a magic object would react to */
    gViewConeType = TaskType_New("SCameraLensViewCone", 0x138, MagicObj_GetType(), 0);
    TaskType_SetHandler(gViewConeType, Game_GetEvent9(), NULL);
    TaskType_SetHandler(gViewConeType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gViewConeType, AIDetection_GetEvent0(), NULL);
    TaskType_SetHandler(gViewConeType, 12, NULL);
    TaskType_SetHandler(gViewConeType, 10, NULL);
    TaskType_SetHandler(gViewConeType, 11, NULL);
    TaskType_SetHandler(gViewConeType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gViewConeType, 9, NULL);
    TaskType_SetHandler(gViewConeType, 8, NULL);
    TaskType_SetHandler(gViewConeType, 12, NULL);
    TaskType_SetHandler(gViewConeType, QCube_GetNearClipEvent(), NULL);
    Script_RegisterConstant("TASKTYPE_SCAMERALENSVIEWCONE", gViewConeType);
    gSCameraDetectMask = AIDetection_AddMaskType(gSCameraType);
}
HOOK(0x440FD0, SCamera_Open);

void SCamera_Create(SCamera *c)
{
    Vec3d zero = {0, 0, 0};
    float scale;

    AIDetection_Add(c);
    RigidDynCubeObj_Init(gWorld, c, NULL);
    scale = sobj_scale(&c->o);
    DynCube_Add(gWorld, c, &zero, VirModel_GetRadius(c->o.model) * scale, 1);
    Sector_AddObject(c->o.world, c, DynCubeObj_GetPosition(c));
    c->place.x = 0;
    c->place.y = 0;
    c->place.z = 0;
    c->yaw = 0;
    memcpy(c->model, "313_01_1", 9);
    c->pitch = 0;
    c->yaw_offset = 0;
    memcpy(c->lens_model, "313_02_1", 9);
    memcpy(c->broken_model, "313_03_1", 9);
    c->wait = 1.0f;
    c->range = 10.0f;
    c->yaw_min = 45;
    c->yaw_max = 45;
    c->turn_speed = 45;
    c->fov_x = 90;
    c->fov_y = 90;
    c->lens = Task_New(c, gLensType, NULL);
    c->alarm = 0;
    c->on = 0;
    c->detect = 0;
    c->last_detect = 0;
    c->detect_time = 0;
    c->exploded = 0;
    c->registered = 0;
    VarString_Init(&c->script);
}
HOOK(0x4423F0, SCamera_Create);

void SCamera_Run(SCamera *c)
{
    EACH_CHILD(&c->o.task, 1);
    if (c->exploded)
        c->on = 0;
    else if (VarString_HasExpr(&c->script))
        c->on = round_int(VarString_EvalReal(&c->script, c));
}
HOOK(0x442610, SCamera_Run);

void SCamera_Delete(SCamera *c)
{
    AIDetection_Remove(c);
    if (Task_GetId(c) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), c->o.task.type);

        if (fn)
            fn(c);
    }
    VarString_Free(&c->script);
    Sector_RemoveObject(c);
    DynCube_Remove(c->o.world, c);
}
HOOK(0x4426D0, SCamera_Delete);

void SCamera_GetParams(Task *t, void **out)
{
    *out = gSCameraParams;
}
HOOK(0x442730, SCamera_GetParams);

void SCamera_ParamsChanged(SCamera *c, char *arg)
{
    float scale;

    RigidDynCubeObj_SetModel(c, VirModel_Find(c->model));
    scale = sobj_scale(&c->o);
    DynCube_Move(c->o.world, c, &c->place, VirModel_GetRadius(c->o.model) * scale);
    Sector_MoveObject(c->o.world, c, DynCubeObj_GetPosition(c));
    Matrix_SetRotZ(&c->o.matrix, c->yaw);
    VarString_Bind(&c->script, c);
    if (!arg || !*arg)
        EACH_CHILD(&c->o.task, Param_GetChangedEvent(), NULL);
}
HOOK(0x442740, SCamera_ParamsChanged);

void SCamera_AddSymbols(SCamera *c)
{
    char name[64], buf[128];

    if (c->registered)
        return;
    c->registered = 1;
    Task_GetName(name, c);
    crt_sprintf(buf, "%s.isDetection", name);
    Script_RegisterBool8(buf, &c->detect);
    crt_sprintf(buf, "%s.isLastDetection", name);
    Script_RegisterBool8(buf, &c->last_detect);
    crt_sprintf(buf, "%s.nDetectionTime", name);
    Script_RegisterInt2(buf, &c->detect_time);
    crt_sprintf(buf, "%s.isExploded", name);
    Script_RegisterBool8(buf, &c->exploded);
}
HOOK(0x442890, SCamera_AddSymbols);

void SCamera_RemoveSymbols(SCamera *c)
{
    char name[64], buf[128];

    if (!c->registered)
        return;
    Task_GetName(name, c);
    c->registered = 0;
    crt_sprintf(buf, "%s.isDetection", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastDetection", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nDetectionTime", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isExploded", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x442970, SCamera_RemoveSymbols);

/* what the AI detection code needs to know about a camera */
void SCamera_GetDetectInfo(SCamera *c, char *out)
{
    *(int *)out = SCamera_GetDetectMask();
    *(int *)(out + 0x20) = 1;
    out[0x25] = 1;
    out[0x24] = !c->exploded;
    memcpy(out + 8, &c->o.pos, sizeof(Vec3d));
}
HOOK(0x442A30, SCamera_GetDetectInfo);

/* arg: {world, ...}; shared by the camera and its lens */
void SCamera_WorldRemove(SObj *o, void **arg)
{
    EACH_CHILD(&o->task, World_GetEvent1(), arg);
    Sector_RemoveObject(o);
    DynCube_Remove(arg[0], o);
}
HOOK(0x443450, SCamera_WorldRemove);

/* arg: {unused, world, ...} */
void SCamera_WorldAdd(SCamera *c, void **arg)
{
    float scale = sobj_scale(&c->o);

    DynCube_Add(arg[1], c, &c->place, VirModel_GetRadius(c->o.model) * scale, 1);
    Sector_AddObject(c->o.world, c, DynCubeObj_GetPosition(c));
    EACH_CHILD(&c->o.task, World_GetEvent0(), arg);
}
HOOK(0x4434F0, SCamera_WorldAdd);

void SCamera_Close(void)
{
    QTaskList_Free(gSCameraParams);
    TaskType_Free(gViewConeType);
    TaskType_Free(gLensType);
    TaskType_Free(gSCameraType);
    QTaskList_Free(gLensList);
}
HOOK(0x443610, SCamera_Close);

void *SCameraLens_GetList(void)
{
    return gLensList;
}
HOOK(0x443660, SCameraLens_GetList);

int SCamera_GetDetectMask(void)
{
    return gSCameraDetectMask;
}
HOOK(0x443670, SCamera_GetDetectMask);
