/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>

#include "hook.h"
#include "camera/camera.h"
#include "param/paramtypes.h"
#include "win/wintypes.h"
#include "editcam/editcamtypes.h"
#include "task/taskvars.h"
#include "vehbase/vehbasevars.h"

FN_EXTERN(Task_Nothing);

/* 0x278 bytes, parent Position */
typedef struct CutScene {
    Task task;
    Vec3d pos;
    Matrix matrix;
    char isRun;
    char isFinished;
    char registered;
    char pad63;
    int tick;
    float startTime;
    float timeScale;
    float screenSize;
    float fadeIn;
    float fadeOut;
    VarString runExpr;
    VarString resetExpr;
    VarString speedExpr;
    VarString runScript;
    VarString stopScript;
    int key;
    float unk210;
    int pad214;
    Vec3d camPos;
    Matrix camMatrix;
    void *camera;
    Task *lineFilter;
    Task *wideFilter;
    Task *bandFilter;
    Task *handiFilter;
    char noFilters;
    char wasRunning;
    char pad26E[2];
    int frame;
    int pad274;
} CutScene;

/* one entry of the key list built each frame */
typedef struct CutKey {
    EditCamera *key;
    float time;
} CutKey;

_Static_assert(offsetof(CutScene, runExpr) == 0x7C, "CutScene.runExpr");
_Static_assert(offsetof(CutScene, key) == 0x20C, "CutScene.key");
_Static_assert(offsetof(CutScene, camMatrix) == 0x230, "CutScene.camMatrix");
_Static_assert(offsetof(CutScene, frame) == 0x270, "CutScene.frame");
_Static_assert(sizeof(CutScene) == 0x278, "CutScene");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (Task *t));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (Task *task, uint16_t type));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x4A23E0, Task *, LineFilter_New, (void *parent, float r, float g, float b, float depth, int sorted));
ORIG(0x4A2430, Task *, WideScreenFilter_New, (void *parent, float r, float g, float b, float unk, float w, float h,
                                              float x, float y, float barHeight));
ORIG(0x4A2490, Task *, BandFilter_New, (void *parent, float depth, float r, float g, float b, int period,
                                        int interval, float maxLevel));
ORIG(0x4A2800, void, BandFilter_SetColor, (Task *f, float r, float g, float b));
ORIG(0x4A28F0, void, BandFilter_NextCamera, (void));
ORIG(0x4A2910, Task *, HandiCamFilter_New, (void *parent, float r, float g, float b, float depth));
ORIG(0x4A4FF0, void, LineFilter_SetLevel, (Task *f, float level));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const Vec3d *pos, const Matrix *axes, float fov, float aspect,
                                  float priority, int frame, void *owner, float unk9, int unk10));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
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
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));
ORIG(0x508610, uint16_t, EditCamera_GetType, (void));
ORIG(0x508620, void, EditCamera_AttachRotation, (Vec3d *pos, Matrix *rot, int target));
ORIG(0x508940, void, EditCamera_AttachPosition, (Vec3d *pos, Matrix *rot, void *unk48, void *unk60, int target));
ORIG(0x518130, void, Hermite_Init, (double *h, double p1, double p2, double m1, double m2));
ORIG(0x518190, long double, Hermite_Eval, (double *h, double u));

extern void *gCutSceneModel;
extern int gCutSceneTicksPerSec;
extern void *gCutSceneParams;
extern float gCutSceneTimeRange[];
extern uint16_t gCutSceneType;
extern char gCutSceneRandom[];
extern void *gCutSceneCamera;
extern void *gCutSceneFilterParent;

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

static const double kPi = 0x1.921fb54442d1bp+1;
static const double kNegPi = -0x1.921fb54442d1bp+1;
static const float kTwoPi = 0x1.921fb6p+2f;
static const double kHalfD = 0.5;
static const float kHalf = 0.5f;
static const float kUnits = 4096.0f;
static const float kShakeAngle = 0x1.1171bap-1f;
static const float kFilterDepth = 0x1.333334p+9f;
static const float kWideScreenUnk = 0x1.99999ap+8f;

uint8_t Vehicle_GetClearHitEvent(void)
{
    return gVehicleClearHitEvent;
}
HOOK(0x4F4860, Vehicle_GetClearHitEvent);

void Vehicle_SetTaskList(void *list)
{
    gVehicles = list;
}
HOOK(0x4F4870, Vehicle_SetTaskList);

/* adds k, its EditCamera children and the following EditCamera siblings,
   depth first; each key starts where the previous one ended */
void CutScene_CollectKeys(CutKey *keys, int *count, EditCamera *k, float *time, float scale)
{
    Task *child;

    if (k->task.type != EditCamera_GetType())
        return;
    do {
        keys[*count].key = k;
        keys[*count].time = *time;
        *time = scale * k->duration + *time;
        (*count)++;
        child = Task_FindChild(&k->task, EditCamera_GetType());
        if (child)
            CutScene_CollectKeys(keys, count, (EditCamera *)child, time, scale);
        k = (EditCamera *)Task_FindNext(&k->task, EditCamera_GetType());
    } while (k && k->task.type == EditCamera_GetType());
}
HOOK(0x4F4880, CutScene_CollectKeys);

CutKey *CutScene_GetKey(CutKey *keys, int count, int i)
{
    if (i < 0)
        i = 0;
    if (i >= count)
        return &keys[count - 1];
    return &keys[i];
}
HOOK(0x4F4910, CutScene_GetKey);

/* keeps each angle within pi of the previous key's; a NaN hangs, like the original */
void CutScene_UnwrapAngles(CutKey *keys, int count)
{
    EditCamera *e = keys[0].key;
    float prev[3];
    int i, j;

    prev[0] = e->angles[0];
    prev[1] = e->angles[1];
    prev[2] = e->angles[2];
    for (i = 1; i < count; i++) {
        e = keys[i].key;
        for (j = 0; j < 3; j++) {
            while (e->angles[j] - prev[j] > kPi)
                e->angles[j] -= kTwoPi;
            while (!(e->angles[j] - prev[j] >= kNegPi))
                e->angles[j] += kTwoPi;
            prev[j] = e->angles[j];
        }
    }
}
HOOK(0x4F4940, CutScene_UnwrapAngles);

void CutScene_CreateHandler(CutScene *c);
void CutScene_RunHandler(CutScene *c);
void CutScene_DeleteHandler(CutScene *c);
void CutScene_GetParamTableHandler(void *task, void **out);
void CutScene_ParamChangedHandler(CutScene *c, const char *arg);
void CutScene_RegisterVarsHandler(CutScene *c);
void CutScene_UnregisterVarsHandler(CutScene *c);
void CutScene_ControlHandler(CutScene *c, int *tick);
void CutScene_InputInfoHandler(void *task, int *info);
void Task_ReturnZero(void *task, int *size);
void CutScene_LeaveWorldHandler(CutScene *c);

void CutScene_Open(void)
{
    CutScene c;

    gCutSceneTimeRange[0] = 0.0f;
    gCutSceneTimeRange[1] = 20.0f;
    gCutSceneModel = VirModel_Add("1721_01_1");
    gCutSceneType = TaskType_New("CutScene", sizeof(CutScene), EditorRigidObj_GetType(), 0, 0);
    TaskType_SetHandler(gCutSceneType, 0, CutScene_CreateHandler);
    TaskType_SetHandler2(gCutSceneType, 1, CutScene_RunHandler);
    TaskType_SetHandler2(gCutSceneType, 2, CutScene_DeleteHandler);
    TaskType_SetHandler(gCutSceneType, Param_GetTableEvent(), CutScene_GetParamTableHandler);
    TaskType_SetHandler(gCutSceneType, Param_GetChangedEvent(), CutScene_ParamChangedHandler);
    TaskType_SetHandler2(gCutSceneType, Level_GetActivateEvent(), CutScene_RegisterVarsHandler);
    TaskType_SetHandler2(gCutSceneType, Level_GetDeactivateEvent(), CutScene_UnregisterVarsHandler);
    TaskType_SetHandler(gCutSceneType, Vehicle_GetControlEvent(), CutScene_ControlHandler);
    TaskType_SetHandler(gCutSceneType, Vehicle_GetInputInfoEvent(), CutScene_InputInfoHandler);
    TaskType_SetHandler(gCutSceneType, Vehicle_GetStateSizeEvent(), Task_ReturnZero);
    TaskType_SetHandler(gCutSceneType, Vehicle_GetKeysEvent(), TASK_NOTHING);
    TaskType_SetHandler(gCutSceneType, Vehicle_GetLoadEvent(), TASK_NOTHING);
    TaskType_SetHandler(gCutSceneType, Vehicle_GetSaveEvent(), TASK_NOTHING);
    TaskType_SetHandler(gCutSceneType, World_GetEvent1(), CutScene_LeaveWorldHandler);
    gCutSceneParams = ParamTable_New(14);
    ParamTable_Add(gCutSceneParams, &c, &c.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.runExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.resetExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.speedExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.startTime, Param_Real32(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.isRun, Param_Bool8(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.timeScale, Param_Real32(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.screenSize, Param_Real32(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.fadeIn, Param_Real32(), 7, gCutSceneTimeRange);
    ParamTable_Add(gCutSceneParams, &c, &c.fadeOut, Param_Real32(), 7, gCutSceneTimeRange);
    ParamTable_Add(gCutSceneParams, &c, &c.unk210, Param_Real32(), 7, gCutSceneTimeRange);
    ParamTable_Add(gCutSceneParams, &c, &c.runScript, Param_VarString(), 7, NULL);
    ParamTable_Add(gCutSceneParams, &c, &c.stopScript, Param_VarString(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_CUTSCENE", gCutSceneType);
    Random_Init(gCutSceneRandom);
}
HOOK(0x4F4B00, CutScene_Open);

void CutScene_CreateHandler(CutScene *c)
{
    VarString_Init(&c->runExpr);
    VarString_Init(&c->resetExpr);
    VarString_Init(&c->speedExpr);
    VarString_Init(&c->runScript);
    VarString_Init(&c->stopScript);
    c->pos.x = 0;
    c->pos.y = 0;
    c->pos.z = 0;
    c->matrix = (Matrix){{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    c->registered = 0;
    c->screenSize = 1.0f;
    c->timeScale = 1.0f;
    c->camera = gCutSceneCamera;
    c->isRun = 1;
    c->wasRunning = 0;
    c->isFinished = 0;
    c->tick = 0;
    c->startTime = 0.0f;
    c->unk210 = 0.0f;
    c->key = -1;
    c->lineFilter = NULL;
    c->bandFilter = NULL;
    c->wideFilter = NULL;
    c->handiFilter = NULL;
    c->frame = -1;
}
HOOK(0x4F4EE0, CutScene_CreateHandler);

void CutScene_RegisterVarsHandler(CutScene *c)
{
    char name[64];
    char buf[128];

    if (c->registered)
        return;
    c->registered = 1;
    Task_GetName(name, c);
    crt_sprintf(buf, "%s.isRun", name);
    Script_RegisterBool8(buf, &c->isRun);
    crt_sprintf(buf, "%s.isFinished", name);
    Script_RegisterBool8(buf, &c->isFinished);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RegisterInt(buf, &c->tick);
}
HOOK(0x4F4FB0, CutScene_RegisterVarsHandler);

void CutScene_UnregisterVarsHandler(CutScene *c)
{
    char name[64];
    char buf[128];

    if (!c->registered)
        return;
    Task_GetName(name, c);
    c->registered = 0;
    crt_sprintf(buf, "%s.isRun", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isFinished", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x4F5050, CutScene_UnregisterVarsHandler);

static void delete_filter(Task **f)
{
    if (*f) {
        Task_Delete(*f);
        *f = NULL;
    }
}

void CutScene_DeleteHandler(CutScene *c)
{
    if (Task_GetId(c) >= 0) {
        TaskFn f = gTaskHandlers[Level_GetDeactivateEvent()][c->task.type];

        if (f)
            ((void (*)(void *))f)(c);
    }
    VarString_Free(&c->runExpr);
    VarString_Free(&c->resetExpr);
    VarString_Free(&c->speedExpr);
    VarString_Free(&c->runScript);
    VarString_Free(&c->stopScript);
    delete_filter(&c->lineFilter);
    delete_filter(&c->bandFilter);
    delete_filter(&c->handiFilter);
    delete_filter(&c->wideFilter);
}
HOOK(0x4F50E0, CutScene_DeleteHandler);

void CutScene_GetParamTableHandler(void *task, void **out)
{
    *out = gCutSceneParams;
}
HOOK(0x4F51C0, CutScene_GetParamTableHandler);

void CutScene_Update(CutScene *c);

void CutScene_RunHandler(CutScene *c)
{
    CALL_CHILDREN(&c->task, 1);
    if (VarString_HasExpr(&c->runExpr))
        c->isRun = (char)round_ll(VarString_EvalReal(&c->runExpr, c));
    if (VarString_HasExpr(&c->resetExpr) && (char)round_ll(VarString_EvalReal(&c->resetExpr, c))) {
        c->tick = 0;
        c->isFinished = 0;
    }
    if (c->wasRunning && !c->isRun)
        c->frame = Screen_GetFrame();
    if (c->isRun) {
        c->noFilters = 0;
        CutScene_Update(c);
        if (VarString_HasExpr(&c->speedExpr)) {
            int n = c->tick + (int)round_ll(VarString_EvalReal(&c->speedExpr, c) * gCutSceneTicksPerSec);

            c->tick = n;
            if (n < 0)
                c->tick = 0;
        } else {
            c->tick++;
        }
    } else if (c->frame != Screen_GetFrame()) {
        /* the filters only go once the scene missed a frame */
        delete_filter(&c->wideFilter);
        delete_filter(&c->lineFilter);
        delete_filter(&c->bandFilter);
        delete_filter(&c->handiFilter);
    }
    if (!c->wasRunning) {
        if (c->isRun && VarString_HasExpr(&c->runScript))
            VarString_EvalReal(&c->runScript, c);
        if (!c->wasRunning) {
            c->wasRunning = c->isRun;
            return;
        }
    }
    if (!c->isRun && VarString_HasExpr(&c->stopScript))
        VarString_EvalReal(&c->stopScript, c);
    c->wasRunning = c->isRun;
}
HOOK(0x4F51D0, CutScene_RunHandler);

void CutScene_Interpolate(CutScene *c, CutKey **k, double t, double end, char changed);

void CutScene_Update(CutScene *c)
{
    CutKey keys[100];
    CutKey *k[4];
    int count = 0;
    int cur = -1;
    int i;
    char found = 0;
    char changed = 0;
    float end = 0.0f;
    float t = (long double)c->tick / gCutSceneTicksPerSec;

    if (!Task_FindChild(&c->task, EditCamera_GetType()))
        return;
    CutScene_CollectKeys(keys, &count, (EditCamera *)Task_FindChild(&c->task, EditCamera_GetType()), &end,
                         c->timeScale);
    if (!count)
        return;
    CutScene_UnwrapAngles(keys, count);
    for (i = 0; i < count && !found; i++) {
        if (keys[i].time > t)
            found = 1;
        else
            cur = i;
    }
    if (t > end) {
        c->isFinished = 1;
        c->key = -1;
        c->tick = (int)round_ll((long double)gCutSceneTicksPerSec * end);
    }
    if (c->key != cur) {
        c->key = cur;
        changed = 1;
    }
    k[0] = CutScene_GetKey(keys, count, cur - 1);
    k[1] = CutScene_GetKey(keys, count, cur);
    k[2] = CutScene_GetKey(keys, count, cur + 1);
    k[3] = CutScene_GetKey(keys, count, cur + 2);
    if (!k[0]->key->smooth)
        k[0] = k[1];
    if (!k[1]->key->smooth)
        k[2] = k[1];
    if (!k[2]->key->smooth)
        k[3] = k[2];
    CutScene_Interpolate(c, k, t, end, changed);
}
HOOK(0x4F5400, CutScene_Update);

long double CutScene_Spline(double p0, double p1, double p2, double p3, double t0, double t1, double t2, double t3,
                            double t);

#define SPLINE(f)                                                                                           \
    CutScene_Spline(k[0]->key->f, k[1]->key->f, k[2]->key->f, k[3]->key->f, k[0]->time, k[1]->time, k[2]->time, \
                    k[3]->time, t)

/* places the camera between keys 1 and 2 and drives the screen filters */
void CutScene_Interpolate(CutScene *c, CutKey **k, double t, double end, char changed)
{
    EditCamera *e1;
    float w[4];
    float ax, ay, az, fov, unk98, shake, level, span, mix, screen, s4, s5, s6;
    float fadeIn = 1.0f;
    float fadeOut = 1.0f;
    float color[3];
    long double f, out, d;
    Vec3d pos;
    Matrix rot, shk, m;
    DisplayMode *mode;
    float width, height;
    int i;

    for (i = 0; i < 4; i++) {
        w[i] = k[i]->key->unk98;
        if (!(w[i] >= 0.0f))
            w[i] = c->unk210;
    }
    pos.x = SPLINE(pos.x);
    pos.y = SPLINE(pos.y);
    pos.z = SPLINE(pos.z);
    ax = SPLINE(angles[0]);
    ay = SPLINE(angles[1]);
    az = SPLINE(angles[2]);
    fov = SPLINE(fov);
    unk98 = CutScene_Spline(w[0], w[1], w[2], w[3], k[0]->time, k[1]->time, k[2]->time, k[3]->time, t);
    shake = SPLINE(shake);
    level = SPLINE(filterLevel);

    d = k[2]->time - k[1]->time;
    span = d;
    if (x87_eq(d, 0.0f)) {
        e1 = k[1]->key;
        color[0] = e1->color[0];
        color[1] = e1->color[1];
        color[2] = e1->color[2];
    } else {
        if (c->bandFilter && x87_eq(level, 0.0f)) {
            if (!(span > kHalf)) {
                f = 1.0f;
            } else {
                f = (t - k[1]->time) / (span - kHalf);
                if (f < 0.0f)
                    f = 0.0f;
                else if (!(f <= 1.0f))
                    f = 1.0f;
            }
        } else {
            f = (t - k[1]->time) / span;
        }
        for (i = 0; i < 3; i++)
            color[i] = (k[2]->key->color[i] - k[1]->key->color[i]) * f + k[1]->key->color[i];
    }
    Matrix_SetEuler(&rot, ax, ay, az);

    e1 = k[1]->key;
    if (e1->posAttach >= 0) {
        if (!changed && !e1->posFollow) {
            pos = c->camPos;
        } else {
            EditCamera_AttachPosition(&pos, &rot, &e1->attachPos, &e1->attachRot, e1->posAttach);
            c->camPos = pos;
        }
    }
    e1 = k[1]->key;
    if (e1->rotAttach >= 0) {
        if (!changed && !e1->rotFollow) {
            rot = c->camMatrix;
        } else {
            EditCamera_AttachRotation(&pos, &rot, e1->rotAttach);
            c->camMatrix = rot;
        }
    }

    if (!x87_eq(c->fadeIn, 0.0f) && c->fadeIn > t)
        fadeIn = t / c->fadeIn;
    if (x87_eq(c->fadeOut, 0.0f)) {
        out = fadeOut;
    } else {
        d = end - t;
        if (!(d >= c->fadeOut))
            out = d / c->fadeOut;
        else
            out = fadeOut;
    }
    if (!(fadeIn >= out))
        mix = fadeIn;
    else
        mix = out;
    if (mix < 0.0f)
        mix = 0.0f;
    else if (!(mix <= 1.0f))
        mix = 1.0f;

    pos.x = (kHalf - Random_Float(gCutSceneRandom)) * shake * kUnits + pos.x;
    pos.y = (kHalf - Random_Float(gCutSceneRandom)) * shake * kUnits + pos.y;
    pos.z = (kHalf - Random_Float(gCutSceneRandom)) * shake * kUnits + pos.z;
    s4 = (kHalf - Random_Float(gCutSceneRandom)) * shake * kShakeAngle;
    s5 = (kHalf - Random_Float(gCutSceneRandom)) * shake * kShakeAngle;
    s6 = (kHalf - Random_Float(gCutSceneRandom)) * shake * kShakeAngle;
    Matrix_SetEuler(&shk, s6, s5, s4);
    m.m[0] = shk.m[6] * rot.m[2] + shk.m[3] * rot.m[1] + shk.m[0] * rot.m[0];
    m.m[1] = shk.m[7] * rot.m[2] + shk.m[4] * rot.m[1] + shk.m[1] * rot.m[0];
    m.m[2] = shk.m[8] * rot.m[2] + shk.m[5] * rot.m[1] + shk.m[2] * rot.m[0];
    m.m[3] = rot.m[5] * shk.m[6] + rot.m[4] * shk.m[3] + rot.m[3] * shk.m[0];
    m.m[4] = rot.m[5] * shk.m[7] + rot.m[4] * shk.m[4] + rot.m[3] * shk.m[1];
    m.m[5] = rot.m[5] * shk.m[8] + rot.m[4] * shk.m[5] + rot.m[3] * shk.m[2];
    m.m[6] = rot.m[8] * shk.m[6] + rot.m[7] * shk.m[3] + rot.m[6] * shk.m[0];
    m.m[7] = rot.m[8] * shk.m[7] + rot.m[7] * shk.m[4] + rot.m[6] * shk.m[1];
    m.m[8] = rot.m[8] * shk.m[8] + rot.m[7] * shk.m[5] + rot.m[6] * shk.m[2];
    m.count = shk.count + rot.count + 1;
    rot = m;
    screen = (1.0f - mix) + mix * c->screenSize;
    Camera_Set(gCutSceneCamera, &pos, &rot, fov, 1.0f, 1000.0f, Screen_GetFrame(), NULL, unk98, 0);

    if (c->noFilters)
        return;
    if (gCutSceneFilterParent && changed) {
        delete_filter(&c->lineFilter);
        e1 = k[1]->key;
        if (e1->filter == 1)
            c->lineFilter = LineFilter_New(gCutSceneFilterParent, e1->color[0], e1->color[1], e1->color[2],
                                           kFilterDepth, 1);
        if (k[1]->key->filter == 3)
            BandFilter_NextCamera();
        delete_filter(&c->bandFilter);
        e1 = k[1]->key;
        if (e1->filter == 2 || e1->filter == 3)
            c->bandFilter = BandFilter_New(gCutSceneFilterParent, kFilterDepth, e1->color[0], e1->color[1],
                                           e1->color[2], 60, 90, level);
        delete_filter(&c->handiFilter);
        e1 = k[1]->key;
        if (e1->filter == 4)
            c->handiFilter = HandiCamFilter_New(gCutSceneFilterParent, e1->color[0], e1->color[1], e1->color[2],
                                                kFilterDepth);
    }
    if (c->bandFilter)
        BandFilter_SetColor(c->bandFilter, color[0], color[1], color[2]);
    if (c->lineFilter)
        LineFilter_SetLevel(c->lineFilter, level);
    if (!gCutSceneFilterParent || x87_eq(screen, 1.0f)) {
        delete_filter(&c->wideFilter);
        return;
    }
    mode = Display_GetMode();
    height = mode->height + 1.0f;
    mode = Display_GetMode();
    width = mode->width + 1.0f;
    if (!c->wideFilter)
        c->wideFilter = WideScreenFilter_New(gCutSceneFilterParent, 0, 0, 0, kWideScreenUnk, width, height, 0, 0,
                                             (1.0f - screen) * height * kHalf + 1.0f);
}
HOOK(0x4F55D0, CutScene_Interpolate);

/* Catmull-Rom style curve through p1 (time t1) and p2 (time t2), tangents
   scaled for uneven key times */
long double CutScene_Spline(double p0, double p1, double p2, double p3, double t0, double t1, double t2, double t3,
                            double t)
{
    double h[4];
    double m1, m2;
    double u = 0.0;
    long double half = (p3 - p1) * kHalfD;
    long double d = t2 - t0;

    if (x87_eq(d, 0.0))
        m1 = 0.0;
    else
        m1 = (t1 - t0) * 2 / d * ((p2 - p0) * kHalfD);
    d = t3 - t1;
    if (x87_eq(d, 0.0))
        m2 = 0.0;
    else
        m2 = (t2 - t1) * 2 / d * half;
    Hermite_Init(h, p1, p2, m1, m2);
    d = t2 - t1;
    if (!x87_eq(d, 0.0))
        u = (t - t1) / d;
    return Hermite_Eval(h, u);
}
HOOK(0x4F6290, CutScene_Spline);

void CutScene_ParamChangedHandler(CutScene *c, const char *arg)
{
    if (!(c->fadeIn >= 0.0f))
        c->fadeIn = 0.0f;
    if (!(c->fadeOut >= 0.0f))
        c->fadeOut = 0.0f;
    c->tick = (int)round_ll((long double)gCutSceneTicksPerSec * c->startTime);
    if (!arg || !*arg)
        CALL_CHILDREN(&c->task, Param_GetChangedEvent(), 0);
    VarString_Bind(&c->runExpr, c);
    VarString_Bind(&c->resetExpr, c);
    VarString_Bind(&c->speedExpr, c);
    VarString_Bind(&c->runScript, c);
    VarString_Bind(&c->stopScript, c);
}
HOOK(0x4F63C0, CutScene_ParamChangedHandler);

/* also used by Human and the Graph parameter */
void Task_ReturnZero(void *task, int *size)
{
    *size = 0;
}
HOOK(0x4F64E0, Task_ReturnZero);

void CutScene_InputInfoHandler(void *task, int *info)
{
    info[1] = 0;
}
HOOK(0x4F64F0, CutScene_InputInfoHandler);

/* jumps to a tick and places the camera without touching the filters */
void CutScene_ControlHandler(CutScene *c, int *tick)
{
    c->tick = *tick;
    c->noFilters = 1;
    CutScene_Update(c);
}
HOOK(0x4F6500, CutScene_ControlHandler);

void CutScene_LeaveWorldHandler(CutScene *c)
{
    delete_filter(&c->wideFilter);
    delete_filter(&c->lineFilter);
    delete_filter(&c->bandFilter);
    delete_filter(&c->handiFilter);
}
HOOK(0x4F6520, CutScene_LeaveWorldHandler);

void CutScene_Close(void)
{
    QTaskList_Free(gCutSceneParams);
    TaskType_Free(gCutSceneType);
}
HOOK(0x4F6590, CutScene_Close);

void CutScene_SetCamera(void *camera)
{
    gCutSceneCamera = camera;
}
HOOK(0x4F65B0, CutScene_SetCamera);

void CutScene_SetFilterParent(void *parent)
{
    gCutSceneFilterParent = parent;
}
HOOK(0x4F65C0, CutScene_SetFilterParent);

void CutScene_SetTicksPerSecond(int ticks)
{
    gCutSceneTicksPerSec = ticks;
}
HOOK(0x4F65D0, CutScene_SetTicksPerSecond);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A76C50) void *gCutSceneModel;
BSS(A76C50) int gCutSceneTicksPerSec;
BSS(A76C50) void *gCutSceneParams;
PAD(igb, A76C50, A76C5C, 0x4);
STORE(igb, A76C50, 4) float store_A76C60[2];
ALIAS(gCutSceneTimeRange, store_A76C60, 0);
BSS(A76C50) uint16_t gCutSceneType;
PAD(igb, A76C50, A76C6A, 0x6);
STORE(igb, A76C50, 1) char store_A76C70[12];
ALIAS(gCutSceneRandom, store_A76C70, 0);
BSS(A76C50) void *gCutSceneCamera;
BSS(A76C50) void *gCutSceneFilterParent;
