#ifndef STATGUN_STATGUN_H
#define STATGUN_STATGUN_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "win/wintypes.h"
#include "magicobj/magicobjtypes.h"
#include "weapon/weapontypes.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)();

/* the parts of a WeaponType (see weapon/weapon.h) used here */
typedef struct WeaponInfo {
    char unk0[0x61C];
    char key[0x10];     /* 0x61C */
    char unk62C[0x1C];
    int clipSize;       /* 0x648 */
    char unk64C[4];
    float range;        /* 0x650 */
    char unk654[0x10];
    float unk664;
    float unk668;
    char unk66C[0x78];
    void *data;         /* 0x6E4 */
} WeaponInfo;

/* the parts of a Human used here */
typedef struct HumanView {
    char unk0[0xE0];
    Vec3d unkE0;
    Matrix unkF8;
    char unk120[0xC4];
    float fov;          /* 0x1E4 */
    float unk1E8;
} HumanView;

typedef struct Human {
    Task task;
    char unk20[0xF0];
    float prevInput[40];    /* 0x110 */
    float input[40];        /* 0x1B0 */
    char unk250[0x1E4];
    char unk434[0xA0];
    float unk4D4;
    char unk4D8[0x14];
    HumanView *view;        /* 0x4EC */
} Human;

/* what a weapon task gets each frame from its user (event 0x460BF0) */
typedef struct WeaponUseMsg {
    char fire;
    char shot;
    char unk2[2];
    double *eye;
    Matrix *view;
    float *vel;
    int unk10;
    int active;
    int alt;
    int zoomed;
    float recoilPitch;
    float recoilYaw;
    int unk28;
    Human *h;
} WeaponUseMsg;

/* the weapon config message: parameters of the weapon's config task */
typedef struct WeaponConfigMsg {
    void *config;
    WeaponInfo *info;
} WeaponConfigMsg;

/* an entry of a human's ammo table */
typedef struct AmmoEntry {
    int id;
    int count;
} AmmoEntry;

/* a value of a config task (0x4FE650 / 0x4FE730) */
typedef struct ConfigVar {
    char unk0[0x124];
    union {
        float f;
        char s[0x20];
    } v;
} ConfigVar;

/* base of the hand weapons (type 0x477BF0, 0x1B0 bytes) */
typedef struct Gun {
    RigidDynCubeObj obj;
    char unkF0;
    char unkF1[0x13];
    char id[0x24];      /* 0x104 */
    WeaponInfo *info;   /* 0x128 */
    int *ammoId;        /* 0x12C */
    char unk130[4];
    char unk134;
    char unk135[0xF];
    int rounds;         /* 0x144 */
    char unk148[8];
    Vec3d muzzle;       /* 0x150 */
    Vec3f dir;          /* 0x168 */
    char unk174[0x38];
    int reload;         /* 0x1AC */
} Gun;

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* copies 4 bytes without going through the FPU, like the original */
#define MOV32(dst, src) memcpy(&(dst), &(src), 4)

/* calls the slot handler of each child of t, keeping the next child on
   gTaskStack so handlers may delete tasks; the slot is read after the push */
#define BROADCAST(t, SLOT, ...)                                         \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next)                                                  \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(SLOT, c_->type);                              \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

static inline long double x87_tan(long double x)
{
    long double one;

    __asm__("fptan" : "=t"(one), "=u"(x) : "0"(x));
    return x;
}

/* (a, b) row i times column j, the terms k other than last added first */
static inline float mat_elem(const float *a, int i, const float *b, int j, int last)
{
    int k0 = last == 0 ? 1 : 0, k1 = last == 2 ? 1 : 2;

    return a[i * 3 + k0] * b[k0 * 3 + j] + a[i * 3 + k1] * b[k1 * 3 + j] + a[i * 3 + last] * b[last * 3 + j];
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* the largest of the three scale factors, compared like the original */
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] < o->scale[1] ? o->scale[1] : o->scale[2];

    return o->scale[0] > m ? o->scale[0] : m;
}

_Static_assert(offsetof(WeaponInfo, clipSize) == 0x648, "WeaponInfo");
_Static_assert(offsetof(WeaponInfo, unk664) == 0x664, "WeaponInfo");
_Static_assert(offsetof(WeaponInfo, data) == 0x6E4, "WeaponInfo");
_Static_assert(offsetof(HumanView, fov) == 0x1E4, "HumanView");
_Static_assert(offsetof(Human, input) == 0x1B0, "Human");
_Static_assert(offsetof(Human, view) == 0x4EC, "Human");
_Static_assert(offsetof(WeaponUseMsg, h) == 0x2C, "WeaponUseMsg");
_Static_assert(offsetof(Gun, id) == 0x104, "Gun");
_Static_assert(offsetof(Gun, rounds) == 0x144, "Gun");
_Static_assert(offsetof(Gun, dir) == 0x168, "Gun");
_Static_assert(sizeof(Gun) == 0x1B0, "Gun");

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CC0, Task *, Task_FindAncestor, (void *task, uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4FE650, ConfigVar *, TagItem_AddReal32, (void *config, const char *name, float value));
ORIG(0x4FE6B0, ConfigVar *, TagItem_AddString16, (void *config, const char *name, const char *value));
ORIG(0x4FE730, ConfigVar *, TagItem_Find, (void *config, const char *name));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x477BF0, uint16_t, Gun_GetType, (void));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x460C10, uint8_t, Human_GetEvent1, (void));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));
ORIG(0x413B30, uint8_t, WeaponType_GetSizeEvent2, (void));
ORIG(0x413B40, uint8_t, WeaponType_GetInitEvent2, (void));
ORIG(0x4071D0, uint8_t, WeaponConfig_GetEvent, (void));
ORIG(0x47CDC0, uint8_t, AmmoList_GetEvent, (void));
ORIG(0x47CEE0, AmmoEntry *, AmmoList_Find, (void *list, int id));
ORIG(0x427D70, void, RocketPod_GetArgSize, (void *task, int *size));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));

ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));

/* a zoomable gun: the private data of its weapon type is the tangent of the
   narrowest view angle */
typedef struct ZoomGun {
    Gun gun;
    int zoomed;         /* 0x1B0 */
    char pad1B4[4];
} ZoomGun;

_Static_assert(sizeof(ZoomGun) == 0x1B8, "ZoomGun");

void Gun_Delete(Gun *g);
void Gun_ReadZoomArg(void *task, WeaponInitArgs *a);

#endif
