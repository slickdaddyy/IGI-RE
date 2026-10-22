#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"

ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416A70, void *, Game_GetPlayer, (int i));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, const Vec3d *starts, const Vec3d *ends, int count,
                                  uint8_t unk, void *unk2, unsigned int flags));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (void *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, const Vec3d *pos));
ORIG(0x4E6C60, void, sub_4E6C60, (void *sound, float unk, float level));
ORIG(0x4E7BD0, void, Debug_printf, (const char *fmt, ...));
ORIG(0x4E8C50, uint16_t, Sound_GetType, (void));
ORIG(0x4F80F0, long double, sub_4F80F0, (const Vec3d *pos));
ORIG(0x4F8140, void, sub_4F8140, (void **hits, int *counts, int *n));

/* plays the ambient rain loop and the sounds of the surfaces around the
   listener ("EnvironmentSound", 0x5C bytes) */
typedef struct EnvSound {
    Task task;
    char echo;
    char surfaces;
    char unk22;
    char unk23;
    char name[16];
    float volume;
    void *sound;
    float left[4];
    float right[4];
} EnvSound;

/* the object the sounds follow */
typedef struct Listener {
    char unk0[8];
    float unk8;
    char unkC[8];
    float unk14;
    char unk18[0x10];
    Vec3d pos;
} Listener;

/* the parts of a sound task used here */
typedef struct SoundTask {
    Task task;
    char unk20[0xA8];
    float volume;
    char unkCC[0x33];
    char name[1];
} SoundTask;

/* the parts of a collision hit used here (0x70 bytes) */
typedef struct Hit {
    char unk0[8];
    Vec3d pos;
    char unk20[0x2C];
    char *material;
    char unk50[8];
    int found;
    int unk5C;
    double dist;
    char unk68[8];
} Hit;

_Static_assert(sizeof(EnvSound) == 0x5C, "EnvSound");
_Static_assert(offsetof(Listener, pos) == 0x28, "Listener");
_Static_assert(offsetof(SoundTask, name) == 0xFF, "SoundTask");
_Static_assert(sizeof(Hit) == 0x70, "Hit");

#define gEnvSoundParams VAR(void *, 0x57BDA8)
#define gEnvSoundListener VAR(Listener *, 0x57BDAC)
#define gEnvSoundType VAR(uint16_t, 0x57BDB0)

static const float kZeroF = 0.0f;
static const float kOneF = 1.0f;
static const float kEchoFade = 0x1.6c16c2p-7f;
static const float kFade = 0x1.47ae14p-6f;
static const float kRise = 0x1.eb852p-4f;
static const double kEchoRange = 40960.0;
static const double kEchoUnit = 1.0 / 4096.0;
static const double kHalf = 0.5;

void EnvironmentSound_Create(EnvSound *t);
void EnvironmentSound_Run(EnvSound *t);
void EnvironmentSound_GetParams(void *t, void **out);
void EnvironmentSound_Reset(EnvSound *t);
void EnvironmentSound_Changed(EnvSound *t);

void EnvironmentSound_Init(void)
{
    EnvSound e;

    gEnvSoundType = TaskType_New("EnvironmentSound", sizeof(EnvSound), Task_GetRootType(), 0);
    TaskType_SetHandler(gEnvSoundType, 0, EnvironmentSound_Create);
    TaskType_SetHandler2(gEnvSoundType, 1, EnvironmentSound_Run);
    TaskType_SetHandler2(gEnvSoundType, 2, TASK_NOTHING);
    TaskType_SetHandler(gEnvSoundType, Param_GetTableEvent(), EnvironmentSound_GetParams);
    TaskType_SetHandler(gEnvSoundType, Param_GetChangedEvent(), EnvironmentSound_Changed);
    gEnvSoundParams = ParamTable_New(5);
    ParamTable_Add(gEnvSoundParams, &e, &e.echo, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEnvSoundParams, &e, &e.surfaces, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEnvSoundParams, &e, &e.unk22, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEnvSoundParams, &e, &e.unk23, Param_Bool8(), 7, NULL);
    ParamTable_Add(gEnvSoundParams, &e, e.name, Param_String16(), 7, NULL);
}
HOOK(0x436B10, EnvironmentSound_Init);

void EnvironmentSound_Create(EnvSound *t)
{
    t->echo = 0;
    t->surfaces = 0;
    t->unk22 = 0;
    t->unk23 = 0;
    memcpy(t->name, "rain_2", 7);
    t->sound = NULL;
    EnvironmentSound_Reset(t);
}
HOOK(0x436C60, EnvironmentSound_Create);

void EnvironmentSound_Reset(EnvSound *t)
{
    int i;

    t->volume = 0.0f;
    if (t->sound) {
        Sound_Stop(t->sound);
        t->sound = NULL;
    }
    t->sound = Sound_Play(t, t->name);
    sub_4E6C60(t->sound, -1.0f, t->volume);
    for (i = 0; i < 4; i++) {
        t->left[i] = 0.0f;
        t->right[i] = 0.0f;
    }
}
HOOK(0x436CB0, EnvironmentSound_Reset);

void EnvironmentSound_UpdateAmbient(EnvSound *t);
void EnvironmentSound_UpdateSurfaces(EnvSound *t);
void EnvironmentSound_UpdateEcho(EnvSound *t);

void EnvironmentSound_Run(EnvSound *t)
{
    if (t->surfaces) {
        EnvironmentSound_UpdateAmbient(t);
        EnvironmentSound_UpdateSurfaces(t);
    }
    if (t->echo)
        EnvironmentSound_UpdateEcho(t);
    CALL_CHILDREN(&t->task, 1);
}
HOOK(0x436D10, EnvironmentSound_Run);

static float fade(long double v)
{
    return v <= kZeroF || v != v ? kZeroF : (float)v;
}

static float add_one(long double v)
{
    return v < kOneF || v != v ? (float)v : kOneF;
}

/* casts a line to each side of the listener and logs how near walls are */
void EnvironmentSound_UpdateEcho(EnvSound *t)
{
    struct {
        Vec3d *pos;
        void *player;
    } skip;
    Vec3f dirs[2];
    Vec3d ends[2], starts[2];
    Hit hits[2];
    Listener *l;
    void *world;
    uint8_t ev;
    float a, b;
    int i;

    for (i = 0; i < 4; i++) {
        t->left[i] = fade(t->left[i] - kEchoFade);
        t->right[i] = fade(t->right[i] - kEchoFade);
    }
    l = gEnvSoundListener;
    a = kZeroF - l->unk8;
    b = kZeroF - l->unk14;
    dirs[1].x = b;
    dirs[1].y = -a;
    dirs[1].z = kZeroF;
    dirs[0].x = -b;
    dirs[0].y = -dirs[1].y;
    dirs[0].z = -kZeroF;
    skip.pos = &l->pos;
    for (i = 0; i < 2; i++) {
        starts[i] = l->pos;
        ends[i].x = dirs[i].x;
        ends[i].y = dirs[i].y;
        ends[i].z = dirs[i].z;
        ends[i].x *= kEchoRange;
        ends[i].y *= kEchoRange;
        ends[i].z *= kEchoRange;
        ends[i].x += starts[i].x;
        ends[i].y = starts[i].y + ends[i].y;
        ends[i].z = starts[i].z + ends[i].z;
    }
    skip.player = Game_GetPlayer(0);
    ev = Game_GetEvent8();
    world = Game_GetWorld();
    Coll_Lines(world, hits, starts, ends, 2, ev, &skip, 0x20000000);
    for (i = 0; i < 2; i++) {
        if (hits[i].found && !(hits[i].dist >= kEchoRange)) {
            int n = round_int(hits[i].dist * kEchoUnit);
            int band = n <= 1 ? 0 : n <= 2 ? 1 : (n > 4) + 2;
            float *e = i & 1 ? t->left : t->right;

            e[band] = add_one(e[band] + kOneF);
        }
    }
    Debug_printf("Left ecco: %.2f %.2f %.2f %.2f\n", (double)t->left[0], (double)t->left[1], (double)t->left[2],
                 (double)t->left[3]);
    Debug_printf("Right ecco: %.2f %.2f %.2f %.2f\n", (double)t->right[0], (double)t->right[1],
                 (double)t->right[2], (double)t->right[3]);
}
HOOK(0x436DA0, EnvironmentSound_UpdateEcho);

/* the rain loop fades in while sub_4F80F0 is non zero at the listener */
void EnvironmentSound_UpdateAmbient(EnvSound *t)
{
    long double r = sub_4F80F0(&gEnvSoundListener->pos);
    float v;

    if (EQ_OR_NAN(r, kZeroF)) {
        t->volume = t->volume - kFade;
    } else {
        t->volume = t->volume + kFade;
        Debug_printf("Ambient rain\n");
    }
    if (t->volume < kZeroF || t->volume != t->volume)
        v = kZeroF;
    else if (t->volume <= kOneF)
        v = t->volume;
    else
        v = kOneF;
    t->volume = v;
    sub_4E6C60(t->sound, -1.0f, v);
    Sound_SetPos(t->sound, &gEnvSoundListener->pos);
}
HOOK(0x4370A0, EnvironmentSound_UpdateAmbient);

static void raise_volume(SoundTask *s, Vec3d *pos)
{
    long double v;

    Sound_SetPos(s, pos);
    v = s->volume + kRise;
    if (v < kZeroF)
        s->volume = kZeroF;
    else if (!(v <= kOneF))
        s->volume = kOneF;
    else
        s->volume = v;
}

/* one sound per surface material the listener's surroundings hit, played
   at the average of the hit points */
void EnvironmentSound_UpdateSurfaces(EnvSound *t)
{
    Hit *lists[8];
    int counts[8];
    int n, i, j, k;
    SoundTask *s;

    sub_4F8140((void **)lists, counts, &n);
    for (s = Task_FindChild(t, Sound_GetType()); s; s = Task_FindNext(s, Sound_GetType())) {
        if (s != t->sound)
            s->volume = fade(s->volume - kFade);
    }
    for (i = 0; i < n; i++) {
        char *mats[32];
        int hitsPer[32];
        Vec3d pos[32];
        int count = counts[i];
        int nm = 0;

        if (!count)
            continue;
        memset(mats, 0, sizeof(mats));
        memset(hitsPer, 0, sizeof(hitsPer));
        memset(pos, 0, sizeof(pos));
        for (j = 0; j < count; j++) {
            Hit *h = &lists[i][j];
            char *m;

            if (!h->found || !(m = h->material))
                continue;
            for (k = 0; k < nm; k++)
                if (mats[k] == m)
                    break;
            if (k == nm) {
                mats[nm] = m;
                pos[nm] = h->pos;
                hitsPer[nm]++;
                nm++;
            } else {
                pos[k].x = h->pos.x + pos[k].x;
                pos[k].y = h->pos.y + pos[k].y;
                pos[k].z = h->pos.z + pos[k].z;
                pos[k].x *= kHalf;
                pos[k].y *= kHalf;
                pos[k].z *= kHalf;
                hitsPer[nm]++;
            }
        }
        for (k = 0; k < nm; k++) {
            char *m = mats[k];

            for (s = Task_FindChild(t, Sound_GetType()); s; s = Task_FindNext(s, Sound_GetType()))
                if (!strcmp(m + 0x134, s->name))
                    break;
            if (!s)
                s = Sound_Play(t, mats[k] + 0x134);
            if (s)
                raise_volume(s, &pos[k]);
        }
    }
}
HOOK(0x437150, EnvironmentSound_UpdateSurfaces);

void EnvironmentSound_GetParams(void *t, void **out)
{
    (void)t;
    *out = gEnvSoundParams;
}
HOOK(0x4374C0, EnvironmentSound_GetParams);

void EnvironmentSound_Changed(EnvSound *t)
{
    EnvironmentSound_Reset(t);
}
HOOK(0x4374D0, EnvironmentSound_Changed);

void EnvironmentSound_Close(void)
{
    QTaskList_Free(gEnvSoundParams);
    TaskType_Free(gEnvSoundType);
}
HOOK(0x4374E0, EnvironmentSound_Close);

void EnvironmentSound_SetListener(Listener *l)
{
    gEnvSoundListener = l;
}
HOOK(0x437500, EnvironmentSound_SetListener);
