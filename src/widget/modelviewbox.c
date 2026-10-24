#include "hook.h"
#include "igi.h"
#include "widget/widget.h"

ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4C5B20, void *, World_New, (int size));
ORIG(0x4C5CE0, void, World_Free, (void *world));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void *model));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int unk));
ORIG(0x4CEC10, void *, VirModel_Find, (const char *name));
ORIG(0x4D9950, void, Camera_Init, (void *cam));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const float *pos, const float *axes, float fov, float aspect,
                                  float priority, int frame, void *owner, int unk9, int unk10));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));
ORIG(0x4E8110, Task *, Viewport_New, (void *parent, float x, float y, float w, float h, void *world, void *cam,
                                  int unk8, int unk9, int unk10, int unk11, float unk12));
ORIG(0x4E8370, void, TerraDrawTask_New, (void *view, void *world));
ORIG(0x4E8250, void, Viewport_SetDrawWorld, (void *view, char on));

/* a 3D view of a model in the menus, made from "minefield" */
typedef struct ModelViewBox {
    MenuWindow w;
    char isOn;
    char pad55[3];
    void *world;
    Task *view;
    char camera[0x60];
    void *model;
    Task *object;
} ModelViewBox;

_Static_assert(sizeof(ModelViewBox) == 0xC8, "ModelViewBox");

extern uint16_t gModelViewBoxType;
extern void *gModelViewBoxParams;
extern IntRange gModelViewBoxRange;
extern float gLightColor[];
extern const double gQuarterPi;

void ModelViewBox_Run(ModelViewBox *mv);
void ModelViewBox_Draw(ModelViewBox *mv);
void ModelViewBox_Delete(ModelViewBox *mv);
void ModelViewBox_Create(ModelViewBox *mv, void *arg);
void ModelViewBox_GetParams(void *task, void **out);
void ModelViewBox_Changed(ModelViewBox *mv, void *arg);
void ModelViewBox_Activate(ModelViewBox *mv);
void ModelViewBox_Deactivate(ModelViewBox *mv);

void ModelViewBox_Init(void)
{
    ModelViewBox mv;

    gModelViewBoxType = TaskType_New("ModelViewBox", sizeof(ModelViewBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gModelViewBoxType, 1, ModelViewBox_Run);
    TaskType_SetHandler2(gModelViewBoxType, Screen_GetDrawEvent(), ModelViewBox_Draw);
    TaskType_SetHandler2(gModelViewBoxType, 2, ModelViewBox_Delete);
    TaskType_SetHandler(gModelViewBoxType, 0, ModelViewBox_Create);
    TaskType_SetHandler(gModelViewBoxType, Param_GetTableEvent(), ModelViewBox_GetParams);
    TaskType_SetHandler(gModelViewBoxType, Param_GetChangedEvent(), ModelViewBox_Changed);
    TaskType_SetHandler2(gModelViewBoxType, Level_GetEventDA8(), ModelViewBox_Activate);
    TaskType_SetHandler2(gModelViewBoxType, Level_GetEventDAC(), ModelViewBox_Deactivate);
    gModelViewBoxRange.min = 0;
    gModelViewBoxRange.max = 640;
    gModelViewBoxParams = ParamTable_New(2);
    ParamTable_Add(gModelViewBoxParams, &mv, &mv.w.w, Param_Int32(), 7, &gModelViewBoxRange);
    ParamTable_Add(gModelViewBoxParams, &mv, &mv.w.h, Param_Int32(), 7, &gModelViewBoxRange);
}
HOOK(0x422B40, ModelViewBox_Init);

/* tan(pi / 4) with the x87 fptan, as a float */
static float fov(void)
{
    long double t;

    __asm__("fptan\n\tfstp %%st(0)" : "=t"(t) : "0"((long double)gQuarterPi));
    return t;
}

void ModelViewBox_Create(ModelViewBox *mv, void *arg)
{
    float pos[6] = {0};
    float axes[10] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    void *cam = mv->camera;

    CALL_BASE(0, mv, arg);
    mv->w.w = 300;
    mv->w.h = 200;
    mv->w.centreX = 1;
    mv->w.centreY = 1;
    mv->isOn = 0;
    mv->world = World_New(0x1000);
    Camera_Init(cam);
    Camera_Set(cam, pos, axes, fov(), 1.0f, 300.0f, Screen_GetFrame(), mv, 0, 0);
    mv->view = Viewport_New(((Task *)mv->w.menu)->parent, 0.0f, 0.0f, 1.0f, 1.0f, mv->world, cam, 0, 0, 0, 0, 1.0f);
    TerraDrawTask_New(mv->view, mv->world);
    Viewport_SetDrawWorld(mv->view, 0);
}
HOOK(0x422C90, ModelViewBox_Create);

void ModelViewBox_Delete(ModelViewBox *mv)
{
    Task_Delete(mv->object);
    Task_Delete(mv->view);
    World_Free(mv->world);
    ((void (*)(void *))HANDLER(Level_GetEventDAC(), mv->w.task.type))(mv);
}
HOOK(0x422DF0, ModelViewBox_Delete);

void ModelViewBox_Changed(ModelViewBox *mv, void *arg)
{
    Task *obj;

    gLightColor[0] = 0.5f;
    gLightColor[1] = 0.5f;
    gLightColor[2] = 0.5f;
    CALL_BASE(Param_GetChangedEvent(), mv, arg);
    mv->model = VirModel_Find("minefield");
    obj = Task_New(mv, RigidDynCubeObj_GetType(), NULL);
    ((int *)obj)[8] = 0;
    ((int *)obj)[10] = 0;
    ((int *)obj)[12] = 0;
    ((int *)obj)[9] = 0;
    ((int *)obj)[11] = 0;
    ((int *)obj)[13] = 0;
    RigidDynCubeObj_Init(mv->world, obj, mv->model);
    DynCube_ListAdd(mv->world, obj, 1);
    mv->object = obj;
}
HOOK(0x422E40, ModelViewBox_Changed);

void ModelViewBox_Run(ModelViewBox *mv)
{
    Flow_GetFrames();
    CALL_BASE(1, mv);
}
HOOK(0x422F10, ModelViewBox_Run);

void ModelViewBox_Draw(ModelViewBox *mv)
{
    float alpha;

    UI_ALPHA(mv->w.menu, alpha);
    MenuWindow_DrawEnd(&mv->w, 0.0f, alpha, 0);
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), MenuWindow_GetType()))(mv);
}
HOOK(0x422F30, ModelViewBox_Draw);

void ModelViewBox_GetParams(void *task, void **out)
{
    *out = gModelViewBoxParams;
}
HOOK(0x422FB0, ModelViewBox_GetParams);

/* builds the "<name>.isOn" symbol name but no longer registers it */
void ModelViewBox_Activate(ModelViewBox *mv)
{
    char name[64], buf[128];

    if (!mv->isOn) {
        mv->isOn = 1;
        Task_GetName(name, mv);
        crt_sprintf(buf, "%s.isOn", name);
    }
}
HOOK(0x422FC0, ModelViewBox_Activate);

void ModelViewBox_Deactivate(ModelViewBox *mv)
{
    char name[64], buf[128];

    if (mv->isOn) {
        Task_GetName(name, mv);
        crt_sprintf(buf, "%s.isOn", name);
        mv->isOn = 0;
    }
}
HOOK(0x423000, ModelViewBox_Deactivate);

void ModelViewBox_Shutdown(void)
{
    QTaskList_Free(gModelViewBoxParams);
    TaskType_Free(gModelViewBoxType);
    gModelViewBoxType = 0x181;
}
HOOK(0x423050, ModelViewBox_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B854) uint16_t gModelViewBoxType INIT(385);
PAD(igd, 53B854, 53B856, 0x2);
BSS(57BC28) void *gModelViewBoxParams;
PAD(igb, 57BC28, 57BC2C, 0x4);
BSS(57BC28) IntRange gModelViewBoxRange;
