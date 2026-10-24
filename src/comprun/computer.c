#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "comprun/comprun.h"
#include "comprun/compruntypes.h"

FN_EXTERN(Task_FalseHandler);

ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401BB0, void, Task_MoveAfter, (void *task, void *after));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x465800, void, HumanView_GetEye, (Vec3d *eye, void *view));
ORIG(0x465880, void, HumanView_GetMatrix, (float *m, void *view));
ORIG(0x46AB40, void, ComputerMap_ResetZoom, (void));
ORIG(0x46AB50, void, ComputerMap_SetStartTask, (void *task));
ORIG(0x46AFB0, void, ComputerMenu_OpenScreen, (int page));
ORIG(0x46B030, Task *, ComputerMenu_Get, (void));
ORIG(0x485320, void *, GameScreen_GetView, (void *task));
ORIG(0x489470, long double, LevelFlow_GetHour, (void));
ORIG(0x499FD0, void, Rain_Enable, (int v));
ORIG(0x499FE0, int, Rain_IsEnabled, (void));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const Vec3d *pos, const Matrix *axes, float fov, float aspect,
                                  float range, int frame, void *owner, float unk9, int unk10));
ORIG(0x4D9950, void, Camera_Init, (void *cam));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));
ORIG(0x4E8110, Task *, Viewport_New, (void *parent, float x, float y, float w, float h, void *world, void *cam,
                                  int unk8, int unk9, int unk10, int unk11, float unk12));
ORIG(0x4E8220, void, Viewport_SetList, (void *view, int layer, int on));
ORIG(0x4E8250, void, Viewport_SetDrawWorld, (void *view, int on));
ORIG(0x4E8270, void, Viewport_SetFog, (void *view, int v));
ORIG(0x4E8370, void, TerraDrawTask_New, (void *view, void *world));

extern uint16_t gComputerType;
extern Computer *gComputer;
extern int gComputerMapEnabled;
extern int gComputerSaved499;

#define Task_HandlerNothing45F920 (FN(0x45F920, Task_FalseHandler))

static const float kForward = 532.48f;
static const float kUp = 40.96f;
static const float kHalfPi = 0x1.921fb6p+0f;
static const double kQuarterPi = 0x1.921fb6p-1;

static inline long double x87_tan(long double x)
{
    long double r;

    __asm__("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"(x));
    return r;
}

void Computer_Create(Computer *t);
void Computer_RunHandler(Computer *t);
void Computer_Destroy(Computer *t);
Computer *Computer_Get(void);
int Computer_IsMapEnabled(void);

void Computer_Init(void)
{
    gComputerType = TaskType_New("Computer", sizeof(Computer), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gComputerType, 0, Computer_Create);
    TaskType_SetHandler2(gComputerType, 1, Computer_RunHandler);
    TaskType_SetHandler2(gComputerType, 2, Computer_Destroy);
    TaskType_SetHandler(gComputerType, Game_GetEvent7(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gComputerType, Game_GetEvent8(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gComputerType, Game_GetEvent13(), Task_HandlerNothing45F920);
}
HOOK(0x46BA60, Computer_Init);

void Computer_Create(Computer *t)
{
    static const Matrix ident = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
    Vec3d zero = {0.0, 0.0, 0.0};
    Matrix axes;
    void **model;
    void *world, *root;
    float unk9;
    int frame;
    char *game;

    gComputer = t;
    model = Computer_IsMapEnabled() ? VirModel_Find("mapcomputer") : NULL;
    RigidDynCubeObj_Init(Game_GetWorld(), t, model);
    DynCube_ListAdd(Game_GetWorld(), t, 1);
    t->view = NULL;
    Camera_Init(t->camera);
    axes = ident;
    unk9 = LevelFlow_GetHour();
    frame = Screen_GetFrame();
    Camera_Set(t->camera, &zero, &axes, x87_tan(kQuarterPi), 1.0f, 300.0f, frame, t, unk9, 0);
    world = Game_GetWorld();
    game = Game_Get();
    t->view = Viewport_New(*(void **)(game + 0x44), 0.0f, 0.0f, 1.0f, 1.0f, world, t->camera, 0, 0, 0, 0, 1.0f);
    Task_MoveAfter(t->view, GameScreen_GetView(*(void **)(Game_Get() + 0x44)));
    TerraDrawTask_New(t->view, Game_GetWorld());
    Viewport_SetList(t->view, 1, 0);
    Viewport_SetDrawWorld(t->view, 0);
    Viewport_SetList(t->view, 0, 0);
    Viewport_SetFog(t->view, 0);
    gComputerSaved499 = Rain_IsEnabled();
    Rain_Enable(0);
    root = GameScreen_GetView(*(void **)(Game_Get() + 0x44));
    Viewport_SetDrawWorld(root, 0);
    Viewport_SetList(root, -1, 0);
    Viewport_SetList(root, 0, 0);
    ComputerMap_ResetZoom();
    ComputerMap_SetStartTask(Computer_Get()->obj.task.parent);
    ComputerMenu_OpenScreen(Computer_IsMapEnabled() ? 1 : 2);
}
HOOK(0x46BB10, Computer_Create);

/* keeps the computer screen in front of the player's eyes */
void Computer_RunHandler(Computer *t)
{
    char *owner = (char *)t->obj.task.parent;
    const float *m = t->look.m;
    float dy, dz;
    long double dx;
    Matrix r;
    const float *q = r.m;
    float *o;
    Matrix out;

    HumanView_GetEye(&t->eye, *(void **)(owner + 0x4EC));
    HumanView_GetMatrix(t->look.m, *(void **)(owner + 0x4EC));
    dx = m[1] * kForward - m[2] * kUp;
    dy = m[4] * kForward - m[5] * kUp;
    dz = m[7] * kForward - m[8] * kUp;
    t->obj.pos.x = dx + t->eye.x;
    t->obj.pos.y = dy + t->eye.y;
    t->obj.pos.z = dz + t->eye.z;
    Matrix_SetRotX(&r, kHalfPi);
    o = out.m;
    o[0] = q[3] * m[1] + q[0] * m[0] + q[6] * m[2];
    o[1] = q[4] * m[1] + q[1] * m[0] + q[7] * m[2];
    o[2] = q[5] * m[1] + q[2] * m[0] + q[8] * m[2];
    o[3] = q[3] * m[4] + q[6] * m[5] + q[0] * m[3];
    out.count = t->look.count + r.count + 1;
    o[4] = q[4] * m[4] + q[7] * m[5] + q[1] * m[3];
    o[5] = q[5] * m[4] + q[8] * m[5] + q[2] * m[3];
    o[6] = q[6] * m[8] + q[0] * m[6] + q[3] * m[7];
    o[7] = q[7] * m[8] + q[1] * m[6] + q[4] * m[7];
    o[8] = q[8] * m[8] + q[2] * m[6] + q[5] * m[7];
    t->obj.matrix = out;
}
HOOK(0x46BD50, Computer_RunHandler);

void Computer_Destroy(Computer *t)
{
    Task *menu = ComputerMenu_Get();
    void *root;

    DynCube_ListRemove(Game_GetWorld(), t, 1);
    gComputer = NULL;
    if (menu)
        Task_Delete(menu);
    if (t->view)
        Task_Delete(t->view);
    root = GameScreen_GetView(*(void **)(Game_Get() + 0x44));
    Viewport_SetDrawWorld(root, 1);
    Viewport_SetList(root, -1, 1);
    Viewport_SetList(root, 0, 1);
    Rain_Enable(gComputerSaved499);
}
HOOK(0x46BF80, Computer_Destroy);

void Computer_Close(void)
{
    TaskType_Free(gComputerType);
}
HOOK(0x46C010, Computer_Close);

Task *Computer_New(void *parent)
{
    Task *t;

    Mem_PushMode(1);
    t = Task_New(parent, gComputerType, NULL);
    Mem_PopMode();
    return t;
}
HOOK(0x46C020, Computer_New);

void Computer_Delete(void)
{
    Task_Delete(gComputer);
}
HOOK(0x46C050, Computer_Delete);

Computer *Computer_Get(void)
{
    return gComputer;
}
HOOK(0x46C060, Computer_Get);

Task *Computer_GetView(void)
{
    if (gComputer)
        return gComputer->view;
    return NULL;
}
HOOK(0x46C070, Computer_GetView);

void *Computer_GetCamera(void)
{
    return gComputer->camera;
}
HOOK(0x46C090, Computer_GetCamera);

void Computer_SetMapEnabled(int on)
{
    gComputerMapEnabled = on;
}
HOOK(0x46C0A0, Computer_SetMapEnabled);

int Computer_IsMapEnabled(void)
{
    return gComputerMapEnabled;
}
HOOK(0x46C0B0, Computer_IsMapEnabled);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53FD98) int gComputerMapEnabled INIT(1);
DATA(53FD98) int gComputerSaved499 INIT(1);
BSS(5BE2C4) uint16_t gComputerType;
PAD(igb, 5BE2C4, 5BE2C6, 0x6);
BSS(5BE2C4) Computer *gComputer;
