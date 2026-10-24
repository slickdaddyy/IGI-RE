#ifndef STATGUN_STATGUN_H
#define STATGUN_STATGUN_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "win/wintypes.h"
#include "statgun/statguntypes.h"
#include "guncasing/guncasingtypes.h"
#include "ammotype/ammotypetypes.h"
#include "viewcam/viewcamtypes.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)();

/* the weapon config message: parameters of the weapon's config task */
typedef struct WeaponConfigMsg {
    void *config;
    WeaponType *info;
} WeaponConfigMsg;

/* a value of a config task (0x4FE650 / 0x4FE730) */
typedef struct ConfigVar {
    char unk0[0x124];
    union {
        float f;
        char s[0x20];
    } v;
} ConfigVar;

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
