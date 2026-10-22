/* HumanPlayerInput: reads the controls each frame and sends them to its
   parent human. Can record them to a file or play a recording back. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "soldier/soldier.h"

#define INPUT_FRAMES 9000

typedef struct HumanPlayerInput {
    Task task;
    char unk20[0x60];
    char record;
    char playback;
    char file[0x100];
    char unk182[2];
    float (*played)[40];
    float (*recorded)[40];
    int frame;
    int frames;
    char speed[0x50];
} HumanPlayerInput;

_Static_assert(offsetof(HumanPlayerInput, played) == 0x184, "played");
_Static_assert(offsetof(HumanPlayerInput, speed) == 0x194, "speed");
_Static_assert(sizeof(HumanPlayerInput) == 0x1E4, "HumanPlayerInput");

typedef struct InputMsg {
    int unk0;
    float *input;
} InputMsg;

typedef struct Profile {
    char unk0[0x538];
    int invertMouse;
    float mouseSpeed;
} Profile;

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x406220, Profile *, Profile_GetActive, (void));
ORIG(0x414FD0, int, Game_CheatKeyDown, (int key));
ORIG(0x415020, int, Game_CheatKeyPressed, (int key));
ORIG(0x46C0A0, void, sub_46C0A0, (int value));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B1700, void, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B18B0, void *, File_Load, (const char *name, unsigned int *size));
ORIG(0x4B1A30, void, File_Unload, (void *data));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1320, void, VarString_Init, (void *vs));
ORIG(0x4F1340, void, VarString_Free, (void *vs));
ORIG(0x4F1400, void, VarString_Bind, (void *vs, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (void *vs));
ORIG(0x4F16E0, long double, VarString_EvalReal, (void *vs, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));

#define gHumanPlayerInputType VAR(uint16_t, 0x5BDAC4)
#define gHumanPlayerInputParams VAR(void *, 0x5BDAC8)
#define gMouseDX VAR(float, 0xBC20F4)
#define gMouseDY VAR(float, 0xBC20F8)
#define gControls VAR(uint32_t, 0xBC210C)

static const float kMouseSpeedMin = 0x1.99999ap-5f;
static const float kMouseSpeedMax = 1.0f;

/* fistp with the current rounding mode */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* "LOCAL:" followed by the file name */
static void local_path(char *path, const char *name)
{
    static const char prefix[] = "LOCAL:";
    int i, n;

    memset(path, 0, 256);
    memcpy(path, prefix, sizeof(prefix));
    n = sizeof(prefix) - 1;
    for (i = 0; name[i]; i++)
        path[n + i] = name[i];
    path[n + i] = 0;
}

void HumanPlayerInput_CreateHandler(HumanPlayerInput *t);
int HumanPlayerInput_GetWeaponSelect(void);
void HumanPlayerInput_RunHandler(HumanPlayerInput *t);
void HumanPlayerInput_DeleteHandler(HumanPlayerInput *t);
void HumanPlayerInput_GetParams(HumanPlayerInput *t, void **out);
void HumanPlayerInput_ParamsChanged(HumanPlayerInput *t);
void HumanPlayerInput_ReadChannels(float *out, HumanPlayerInput *t);

void HumanPlayerInput_Open(void)
{
    HumanPlayerInput tmp;

    gHumanPlayerInputType = TaskType_New("HumanPlayerInput", sizeof(HumanPlayerInput), Task_GetRootType(), 0);
    TaskType_SetHandler(gHumanPlayerInputType, 0, HumanPlayerInput_CreateHandler);
    TaskType_SetHandler2(gHumanPlayerInputType, 1, HumanPlayerInput_RunHandler);
    TaskType_SetHandler2(gHumanPlayerInputType, 2, HumanPlayerInput_DeleteHandler);
    TaskType_SetHandler(gHumanPlayerInputType, Param_GetTableEvent(), HumanPlayerInput_GetParams);
    TaskType_SetHandler(gHumanPlayerInputType, Param_GetChangedEvent(), HumanPlayerInput_ParamsChanged);
    gHumanPlayerInputParams = ParamTable_New(4);
    ParamTable_Add(gHumanPlayerInputParams, &tmp, &tmp.record, Param_Bool8(), 7, NULL);
    ParamTable_Add(gHumanPlayerInputParams, &tmp, &tmp.playback, Param_Bool8(), 7, NULL);
    ParamTable_Add(gHumanPlayerInputParams, &tmp, tmp.file, Param_String256(), 7, NULL);
    ParamTable_Add(gHumanPlayerInputParams, &tmp, tmp.speed, Param_VarString(), 7, NULL);
}
HOOK(0x45DB70, HumanPlayerInput_Open);

void HumanPlayerInput_CreateHandler(HumanPlayerInput *t)
{
    t->record = 0;
    t->playback = 0;
    t->file[0] = 0;
    t->played = NULL;
    t->recorded = NULL;
    t->frame = 0;
    t->frames = 0;
    VarString_Init(t->speed);
}
HOOK(0x45DCB0, HumanPlayerInput_CreateHandler);

void HumanPlayerInput_RunHandler(HumanPlayerInput *t)
{
    Task *human = t->task.parent;
    float input[40];
    InputMsg msg;
    TaskHandler fn;

    if (t->playback && t->frames && t->frame < t->frames) {
        memcpy(input, t->played[t->frame], sizeof(input));
        t->frame++;
    } else {
        HumanPlayerInput_ReadChannels(input, t);
        if (t->record && t->frame < INPUT_FRAMES) {
            memcpy(t->recorded[t->frame], input, sizeof(input));
            t->frame++;
            t->frames++;
        }
    }
    if (VarString_HasExpr(t->speed))
        sub_46C0A0(x87_fistp(VarString_EvalReal(t->speed, t)));
    else
        sub_46C0A0(1);
    msg.unk0 = -1;
    msg.input = input;
    fn = (TaskHandler)TASK_HANDLER(Vehicle_GetControlEvent(), human->type);
    if (fn)
        fn(human, &msg);
}
HOOK(0x45DCF0, HumanPlayerInput_RunHandler);

void HumanPlayerInput_DeleteHandler(HumanPlayerInput *t)
{
    char path[256];
    void *f;

    if (t->record && t->frames) {
        local_path(path, t->file);
        f = File_Open(path, "w");
        if (f) {
            File_Write(f, t->recorded, t->frames * (int)sizeof(*t->recorded));
            File_Close(f);
        }
    } else if (t->playback && t->played) {
        File_Unload(t->played);
    }
    if (t->recorded) {
        Mem_Free(t->recorded);
        t->recorded = NULL;
    }
    VarString_Free(t->speed);
}
HOOK(0x45DE20, HumanPlayerInput_DeleteHandler);

void HumanPlayerInput_GetParams(HumanPlayerInput *t, void **out)
{
    (void)t;
    *out = gHumanPlayerInputParams;
}
HOOK(0x45DF40, HumanPlayerInput_GetParams);

void HumanPlayerInput_ParamsChanged(HumanPlayerInput *t)
{
    char path[256];
    unsigned int size;

    if (t->record) {
        t->recorded = Mem_Alloc(INPUT_FRAMES * sizeof(*t->recorded), 4);
    } else if (t->playback) {
        local_path(path, t->file);
        t->played = File_Load(path, &size);
        if (t->played)
            t->frames = size / sizeof(*t->played);
    }
    VarString_Bind(t->speed, t);
}
HOOK(0x45DF50, HumanPlayerInput_ParamsChanged);

void HumanPlayerInput_Close(void)
{
    QTaskList_Free(gHumanPlayerInputParams);
    TaskType_Free(gHumanPlayerInputType);
}
HOOK(0x45E040, HumanPlayerInput_Close);

/* the controls as 40 floats: mouse x and y, then one per action */
void HumanPlayerInput_ReadChannels(float *out, HumanPlayerInput *t)
{
    Profile *p = Profile_GetActive();
    uint32_t c;
    float k;

    (void)t;
    memset(out, 0, 40 * sizeof(float));
    out[0] = gMouseDX;
    c = gControls;
    out[1] = gMouseDY;
    out[2] = c & 1;
    out[3] = c & 2;
    out[4] = c & 4;
    out[5] = c & 8;
    if (p->invertMouse)
        out[1] = -out[1];
    k = p->mouseSpeed;
    if (!(k >= kMouseSpeedMin))
        k = kMouseSpeedMin;
    else if (k > kMouseSpeedMax)
        k = kMouseSpeedMax;
    out[0] = k * out[0];
    out[1] = k * out[1];
    out[6] = c & 0x100;
    out[7] = c & 0x4000;
    out[8] = c & 0x200;
    out[9] = c & 0x400;
    out[10] = c & 0x800;
    out[11] = c & 0x1000;
    out[12] = c & 0x8000;
    out[13] = HumanPlayerInput_GetWeaponSelect();
    out[14] = c & 0x100000;
    out[15] = c & 0x200000;
    out[20] = c & 0x40000;
    out[21] = c & 0x2000;
    out[22] = c & 0x80000;
    out[23] = c & 0x400000;
    out[24] = c & 0x800000;
    out[25] = c & 0x1000000;
    out[26] = c & 0x2000000;
    out[27] = c & 0x4000000;
    out[28] = c & 0x8000000;
    out[29] = c & 0x10000000;
    out[30] = c & 0x20000000;
    out[31] = c & 0x40000000;
    out[34] = c & 0x10000;
    out[32] = c & 0x20000;
    out[16] = Game_CheatKeyDown(0x4E);
    out[17] = Game_CheatKeyDown(0x4A);
    out[18] = Game_CheatKeyPressed(0x9C);
    out[19] = Game_CheatKeyPressed(0x37);
    out[33] = Game_CheatKeyPressed(0x21);
}
HOOK(0x45E060, HumanPlayerInput_ReadChannels);

/* the weapon to select directly; never used */
int HumanPlayerInput_GetWeaponSelect(void)
{
    return -1;
}
