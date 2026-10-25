#ifndef GUNCASING_GUNCASING_H
#define GUNCASING_GUNCASING_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "graphtask/graphtasktypes.h"
#include "humanai/humanaitypes.h"
#include "computer/computertypes.h"
#include "guncasing/guncasingtypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)();

/* the weapon type record of DefineWeaponType (0x6E8 bytes) */
typedef struct GrenadeConfig {
    float damage;
    float radius;       /* meters */
    float falloff;      /* meters */
} GrenadeConfig;

/* what the proximity handler gets */
typedef struct MineProbe {
    int unk0;
    int unk4;
    Vec3d pos;
    float radius;
} MineProbe;

/* M203 grenade (0x128 bytes) */
typedef struct M203 {
    RigidDynCubeObj o;
    Vec3d pos;          /* 0xF0 */
    Vec3f vel;          /* 0x108 */
    Task **owner;       /* 0x114 */
    Task **other;       /* 0x118 */
    void *smoke;        /* 0x11C */
    float damage;       /* 0x120 */
    float power;        /* 0x124 */
} M203;

/* per type data read by Grenade_ParseArgs */
typedef struct GrenadeType {
    float fuse;         /* seconds */
    int explosion;      /* GRENADE_EXPLOSIONTYPE_* */
} GrenadeType;

/* Grenade (0x738 bytes), a subtype of PhysicsObj */
typedef struct Grenade {
    RigidDynCubeObj o;
    Body body;          /* 0xF0 */
    char unk154[0x60];
    PhysType *phys;     /* 0x1B4 */
    int hit;            /* 0x1B8 */
    char unk1BC[0x564];
    int fuse;           /* 0x720, frames */
    int bounceTick;     /* 0x724 */
    char bounced;       /* 0x728 */
    char unk729[3];
    Task **owner;       /* 0x72C */
    WeaponType *weapon; /* 0x730 */
    char unk734[4];
} Grenade;

/* GunFlame, GunClip and GunMovingPart (0x140 bytes) */
typedef struct GunPart {
    RigidDynCubeObj o;
    Vec3d offset;       /* 0xF0 */
    Matrix local;       /* 0x108 */
    int unk130;
    int unk134;
    int unk138;
    int flashStart;     /* 0x13C */
} GunPart;

typedef struct GunPartArgs {
    const char *model;
    void *world;
    char unk8[0x18];
    Vec3d offset;       /* 0x20 */
    Matrix matrix;      /* 0x38 */
    Matrix local;       /* 0x60 */
    int unk88;
    int unk8C;
} GunPartArgs;

/* fire message of the Gun (event 0x4F1FF0) */
typedef struct GunFire {
    char trigger;
    char fired;
    char unk2[2];
    Vec3d *muzzle;
    Matrix *aim;
    Vec3f *vel;
    char unk10[0x18];
    int unk28;
} GunFire;

/* the gun's collision task (0x4E0930); the gun parts are its children */
typedef struct GunColl {
    Task task;
    Gun *gun;
} GunColl;

/* empty cartridge thrown out of a gun (0x128 bytes) */
typedef struct GunCasing {
    RigidDynCubeObj o;
    int life;           /* 0xF0, frames */
    Matrix spin;        /* 0xF4 */
    Vec3f vel;          /* 0x11C */
} GunCasing;

#define SCHECK(t, f, off) _Static_assert(offsetof(t, f) == (off), #f)
SCHECK(RigidDynCubeObj, attached, 0xE8);
SCHECK(WeaponType, spread, 0x664);
SCHECK(WeaponType, sound2, 0x6C0);
SCHECK(WeaponType, taskType2, 0x6D8);
SCHECK(WeaponType, data, 0x6E0);
SCHECK(Mine, model, 0x170);
SCHECK(Mine, explode, 0x1E4);
SCHECK(Mine, remote, 0x249);
SCHECK(Grenade, phys, 0x1B4);
SCHECK(Grenade, fuse, 0x720);
SCHECK(GunPartArgs, unk8C, 0x8C);
SCHECK(Gun, rounds, 0x144);
SCHECK(Gun, point, 0x178);
SCHECK(Gun, reload, 0x1AC);
_Static_assert(sizeof(M203) == 0x128, "M203");
_Static_assert(sizeof(Grenade) == 0x738, "Grenade");
_Static_assert(sizeof(GunPart) == 0x140, "GunPart");
_Static_assert(sizeof(GunCasing) == 0x128, "GunCasing");
#undef SCHECK

uint16_t Gun_GetType(void);
uint8_t Gun_GetEvent0(void);
uint8_t Gun_GetEvent1(void);
uint8_t Gun_GetEvent2(void);
uint8_t Gun_GetEvent3(void);
uint8_t Gun_GetEvent4(void);

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define TASK_HANDLER(slot, type) (((TaskFn *)gTaskHandlers)[(slot) * 0x180 + (type)])

/* exe constants, read in place so the x87 code sees the same values */
extern const float kZero;
extern const float kOne;
extern float kMeter;       /* 4096 */
extern float kFps;         /* 30 */
extern double kZeroD;
extern const double kOneD;
extern const float kDegToRad;
extern float kDrag;        /* 0.985 */
extern double kMinute;     /* 60 */
extern double kFpsD;       /* 30 */

static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the slot handler of each task in a child list, keeping the next one
   on gTaskStack so handlers may delete tasks; the slot is read after the push */
#define BROADCAST(first, SLOT, ...)                                     \
    do {                                                                \
        Task *c_ = (first);                                             \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = TASK_HANDLER(SLOT, c_->type);                         \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* the largest scale, compared the way the original does */
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = !(o->scale[2] >= o->scale[1]) ? o->scale[1] : o->scale[2];

    return o->scale[0] > m ? o->scale[0] : m;
}

/* fistp with the current rounding mode */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

#endif
