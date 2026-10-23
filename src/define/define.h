#ifndef DEFINE_DEFINE_H
#define DEFINE_DEFINE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)(void *task, void *arg);

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct SoundParams {
    float volume;
    float pitch;
    float pan;
    int unkC;
    double minDist;
    double maxDist;
    Vec3d pos;
    Vec3d vel;
    float start;
    int unk54;
} SoundParams;

/* what a task keeps to restart or query a sound of its own */
typedef struct SoundHandle {
    int bucket;
    int hash;
    int id;
    uint32_t flags; /* 2 positional, 0x10 loop */
    SoundParams params;
} SoundHandle;

/* what an object answers to the sound info event */
typedef struct SoundInfo {
    Vec3d *pos;
    float *matrix;
    int *values; /* count, then one value per index */
} SoundInfo;

/* argument of the play event: the listener object */
typedef struct SoundPlayArg {
    void *listener;
    char defs;
} SoundPlayArg;

/* argument of the list event: appends the definition as script text */
typedef struct SoundWriteArg {
    char *buf;
    int indent;
} SoundWriteArg;

typedef struct SoundFindArg {
    const char *name;
    Task *found;
} SoundFindArg;

/* argument of Create: the definition to copy, or NULL for a definition */
typedef struct SoundCreateArg {
    void *def;
    char isDef;
} SoundCreateArg;

/* Same layout as the script engine's value. Bytes 4 and 20 are never set. */
typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;

/* "SoundDefSound": DefineSound() */
typedef struct SoundDefSound {
    Task task;
    SoundHandle h;
    Vec3d pos;
    Vec3d offset;
    int volumeIndex;
    int pitchIndex;
    int sampleIndex;
    float volume;
    float volume2;
    float pitch;
    float minDist;
    float maxDist;
    float range;
    char loop;
    char isDef;
    char stop;
    char name[0x20];
    char sample[0x20];
    char pad11F;
    float time;
    float unk124;
    int bucket;
    int hash;
} SoundDefSound;

/* "SoundDefTriggerOnce": DefineTriggerOnce() */
typedef struct SoundDefTriggerOnce {
    Task task;
    SoundHandle h;
    Vec3d pos;
    Vec3d offset;
    float volume;
    float volume2;
    float pitch;
    float minDist;
    float maxDist;
    float range;
    int triggerIndex;
    int volumeIndex;
    int pitchIndex;
    int sampleIndex;
    char loop;
    char triggerOnZero;
    char isDef;
    char name[0x20];
    char sample[0x20];
    char active;
    char stop;
    char pad125[3];
    float time;
    float hold;
    int bucket;
    int hash;
} SoundDefTriggerOnce;

/* "SoundDefGraph": DefineGraph() */
typedef struct SoundDefGraph {
    Task task;
    SoundHandle h;
    Vec3d pos;
    Vec3d offset;
    float volume;
    float volume2;
    float pitch;
    float unkC4;
    float minDist;
    float maxDist;
    float range;
    char isDef;
    char stop;
    char padD6[2];
    int volumeIndex;
    int pitchIndex;
    int pitchAddIndex;
    int sampleIndex;
    float volumeGraph[11];
    float pitchGraph[11];
    char name[0x20];
    char sample[0x20];
    int bucket;
    int hash;
} SoundDefGraph;

_Static_assert(sizeof(SoundParams) == 0x58, "SoundParams");
_Static_assert(offsetof(SoundDefSound, pos) == 0x88, "SoundDefSound");
_Static_assert(offsetof(SoundDefSound, name) == 0xDF, "SoundDefSound");
_Static_assert(sizeof(SoundDefSound) == 0x130, "SoundDefSound");
_Static_assert(offsetof(SoundDefTriggerOnce, name) == 0xE3, "SoundDefTriggerOnce");
_Static_assert(offsetof(SoundDefTriggerOnce, time) == 0x128, "SoundDefTriggerOnce");
_Static_assert(sizeof(SoundDefTriggerOnce) == 0x138, "SoundDefTriggerOnce");
_Static_assert(offsetof(SoundDefGraph, volumeGraph) == 0xE8, "SoundDefGraph");
_Static_assert(sizeof(SoundDefGraph) == 0x188, "SoundDefGraph");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gScriptEmptyString ((const char *)0x567C74)

#define TASK_NO_TYPE 0x181

static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kDistScale = 4096.0f;

/* round like a store to a float / double in the original */
static inline float rf(long double x)
{
    volatile float f = x;

    return f;
}

static inline double rd(long double x)
{
    volatile double d = x;

    return d;
}

ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, void, Script_GetArgString, (void *args, int i, char *out, int size));
ORIG(0x496DB0, long double, Sound_GetDuration, (int bucket, int hash));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x4974A0, void, SoundParams_Init, (SoundParams *p));
ORIG(0x4E6050, int, Sound_GetRate, (void));
ORIG(0x4E65F0, uint8_t, Sound_GetListEvent, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6630, uint8_t, Sound_GetFindEvent, (void));
ORIG(0x4E6650, uint8_t, Sound_GetNameEvent, (void));
ORIG(0x4E6670, uint8_t, Sound_GetSetPosEvent, (void));
ORIG(0x4E6690, uint8_t, Sound_GetSetVolumeEvent, (void));
ORIG(0x4E66B0, uint8_t, Sound_GetSetPitchEvent, (void));
ORIG(0x4E66D0, uint8_t, Sound_GetSetGlobalEvent, (void));
ORIG(0x4E66F0, uint8_t, Sound_GetRangeEvent, (void));
ORIG(0x4E6710, uint8_t, Sound_GetDurationEvent, (void));
ORIG(0x4E6730, int, Sound_GetStopEvent, (void));
ORIG(0x4E6D20, void, Sound_AddTask, (void *task));
ORIG(0x4E6D40, void, Sound_RemoveTask, (void *task));
ORIG(0x4E6D60, void, SoundHandle_Play, (SoundHandle *h));
ORIG(0x4E6FA0, char, SoundHandle_IsPlaying, (const SoundHandle *h));
ORIG(0x4E6FF0, long double, SoundParams_ClampVolume, (SoundParams *p));
ORIG(0x4EBA80, uint16_t, SoundGroup_GetType, (void));
ORIG(0x4EBDD0, uint8_t, sub_4EBDD0, (void));
ORIG(0x4EBDE0, Vec3d *, sub_4EBDE0, (void *listener));

/* SoundDef_Write helpers, shared by the three definition types */
void SoundDef_Append(char *buf, const char *s);
void SoundDef_WriteIndent(SoundWriteArg *a);
void SoundDef_WriteEnd(Task *t, SoundWriteArg *a);

#endif
