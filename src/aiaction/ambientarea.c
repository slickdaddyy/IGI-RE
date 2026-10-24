#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "aiaction/aiaction.h"
#include "hook.h"

ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4E6C60, void, Sound_SetVolume, (Task *sound, float unk, float level));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4F0F90, void *, Param_Real64x3, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

/* "AmbientArea" (0x210 bytes), a box that plays one of its sounds, picked
   at random, while the listener is inside, fading in and out */
typedef struct AmbientArea {
    Task task;
    Vec3d pos;
    Vec3d size;         /* 0x38 */
    Matrix matrix;      /* 0x50 */
    double volume;      /* 0x78 */
    double minDelay;    /* 0x80, seconds */
    double randDelay;   /* 0x88 */
    int delay;          /* 0x90, frames until a sound may start */
    char sounds[0x20];  /* 0x94, names split by space, ',' or ';' */
    char names[8][0x20]; /* 0xB4 */
    int count;          /* 0x1B4 */
    Task **sound;       /* 0x1B8 */
    VarString cond;     /* 0x1BC, plays only while non zero */
    char unk20C[4];
} AmbientArea;

_Static_assert(sizeof(AmbientArea) == 0x210, "AmbientArea");

extern uint16_t gAmbientAreaType;
extern void *gAmbientAreaParams;
extern void *gAmbientAreaModel;

static const double kHalfSize = 4096.0;
static const double kNegHalfSize = -4096.0;
static const double kFadeStep = 0.05;
static const double kFps = 30.0;

void AmbientArea_Create(AmbientArea *a);
void AmbientArea_Run(AmbientArea *a);
void AmbientArea_Delete(AmbientArea *a);
void AmbientArea_GetParams(void *t, void **out);
void AmbientArea_Changed(AmbientArea *a);

void AmbientArea_Init(void)
{
    AmbientArea a;

    gAmbientAreaModel = VirModel_Add("johan");
    gAmbientAreaType = TaskType_New("AmbientArea", sizeof(AmbientArea), EditorRigidObj_GetType(), 0);
    TaskType_SetHandler(gAmbientAreaType, 0, AmbientArea_Create);
    TaskType_SetHandler2(gAmbientAreaType, 1, AmbientArea_Run);
    TaskType_SetHandler2(gAmbientAreaType, 2, AmbientArea_Delete);
    TaskType_SetHandler(gAmbientAreaType, Param_GetTableEvent(), AmbientArea_GetParams);
    TaskType_SetHandler(gAmbientAreaType, Param_GetChangedEvent(), AmbientArea_Changed);
    gAmbientAreaParams = ParamTable_New(6);
    ParamTable_Add(gAmbientAreaParams, &a, &a.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gAmbientAreaParams, &a, &a.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAmbientAreaParams, &a, &a.size, Param_Real64x3(), 7, NULL);
    ParamTable_Add(gAmbientAreaParams, &a, &a.minDelay, Param_Real64(), 7, NULL);
    ParamTable_Add(gAmbientAreaParams, &a, &a.randDelay, Param_Real64(), 7, NULL);
    ParamTable_Add(gAmbientAreaParams, &a, a.sounds, Param_String256(), 7, NULL);
}
HOOK(0x44C130, AmbientArea_Init);

void AmbientArea_Create(AmbientArea *a)
{
    int i;

    a->size.x = 1.0;
    a->size.y = 1.0;
    a->size.z = 1.0;
    a->pos.x = 0.0;
    a->pos.y = 0.0;
    a->pos.z = 0.0;
    memset(&a->matrix, 0, sizeof(a->matrix));
    a->matrix.m[0] = 1.0f;
    a->matrix.m[4] = 1.0f;
    a->matrix.m[8] = 1.0f;
    a->volume = 0.0;
    a->minDelay = 0.0;
    a->randDelay = 0.0;
    a->delay = 0;
    a->count = 0;
    a->sounds[0] = 0;
    for (i = 0; i < 8; i++)
        a->names[i][0] = 0;
    a->sound = NULL;
    VarString_Init(&a->cond);
}
HOOK(0x44C2C0, AmbientArea_Create);

static void stop_sound(AmbientArea *a)
{
    if (a->sound) {
        if (*a->sound)
            Sound_Stop(*a->sound);
        TaskRef_Remove(a->sound);
        a->sound = NULL;
    }
}

static int random_delay(AmbientArea *a)
{
    return round_int((Random_Float(Game_GetRandom()) * a->randDelay + a->minDelay) * kFps);
}

void AmbientArea_Run(AmbientArea *a)
{
    Vec3d *ear = (Vec3d *)(Game_Get() + 0x88);
    char inside = 0;
    long double v;

    if (VarString_HasExpr(&a->cond) && !(char)round_int(VarString_EvalReal(&a->cond, a))) {
        v = a->volume - kFadeStep;
    } else {
        long double dx = ear->x - a->pos.x;
        long double dy = ear->y - a->pos.y;
        long double dz = ear->z - a->pos.z;
        const float *m = a->matrix.m;
        double lx = (m[6] * dz + m[3] * dy) + m[0] * dx;
        double ly = (m[7] * dz + m[4] * dy) + m[1] * dx;
        long double lz = (m[8] * dz + m[5] * dy) + m[2] * dx;

        if (a->size.x * kNegHalfSize < lx && a->size.x * kHalfSize > lx && a->size.y * kNegHalfSize < ly &&
            a->size.y * kHalfSize > ly && lz > a->size.z * kNegHalfSize && lz < a->size.z * kHalfSize) {
            v = a->volume + kFadeStep;
            inside = 1;
        } else {
            v = a->volume - kFadeStep;
        }
    }
    a->volume = v;
    if (!(v >= 0.0))
        a->volume = 0.0;
    else if (v > 1.0)
        a->volume = 1.0;

    if (a->volume == 0.0 || (a->sound && !*a->sound)) {
        a->delay = random_delay(a);
        stop_sound(a);
    } else if (a->sound) {
        if (inside)
            Sound_SetPos(*a->sound, ear);
        Sound_SetVolume(*a->sound, -1.0f, a->volume);
    } else if (inside) {
        int i = round_int(Random_Float(Game_GetRandom()) * a->count);

        if (!a->delay) {
            Task *s = Sound_Play(a, a->names[i]);

            if (s) {
                Sound_SetPos(s, ear);
                a->sound = TaskRef_Add(s);
                Sound_SetVolume(*a->sound, -1.0f, a->volume);
            }
        }
    }
    if (a->delay)
        a->delay--;
    CALL_CHILDREN(&a->task, 1);
}
HOOK(0x44C370, AmbientArea_Run);

void AmbientArea_Delete(AmbientArea *a)
{
    if (a->sound)
        TaskRef_Remove(a->sound);
    VarString_Free(&a->cond);
}
HOOK(0x44C700, AmbientArea_Delete);

void AmbientArea_GetParams(void *t, void **out)
{
    (void)t;
    *out = gAmbientAreaParams;
}
HOOK(0x44C730, AmbientArea_GetParams);

/* splits sounds into names */
void AmbientArea_Changed(AmbientArea *a)
{
    char *p = a->sounds, *start = p;
    char inWord = 0;

    a->count = 0;
    for (;; p++) {
        char c = *p;

        if (c == ' ' || c == ';' || c == ',' || c == 0) {
            if (inWord) {
                crt_strncpy(a->names[a->count], start, p - start);
                a->names[a->count][p - start] = 0;
                inWord = 0;
                a->count++;
            }
        } else {
            if (!inWord)
                start = p;
            inWord = 1;
        }
        if (!*p)
            break;
    }
    a->delay = random_delay(a);
    VarString_Bind(&a->cond, a);
}
HOOK(0x44C740, AmbientArea_Changed);

void AmbientArea_Close(void)
{
    QTaskList_Free(gAmbientAreaParams);
    TaskType_Free(gAmbientAreaType);
}
HOOK(0x44C810, AmbientArea_Close);

AmbientArea *AmbientArea_New(void *parent)
{
    AmbientArea *a;

    Mem_PushMode(1);
    a = (AmbientArea *)Task_New(parent, gAmbientAreaType, NULL);
    Mem_PopMode();
    return a;
}
HOOK(0x44C830, AmbientArea_New);

void AmbientArea_Set(AmbientArea *a, const Vec3d *pos, const Matrix *m, const Vec3d *size, const char *sounds)
{
    a->pos = *pos;
    a->size = *size;
    a->matrix = *m;
    strcpy(a->sounds, sounds);
}
HOOK(0x44C860, AmbientArea_Set);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57C734) uint16_t gAmbientAreaType;
PAD(igb, 57C734, 57C736, 0x2);
BSS(57C734) void *gAmbientAreaParams;
BSS(57C734) void *gAmbientAreaModel;
