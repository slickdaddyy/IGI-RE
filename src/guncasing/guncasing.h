#ifndef GUNCASING_GUNCASING_H
#define GUNCASING_GUNCASING_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "coll/colltypes.h"
#include "magicobj/magicobjtypes.h"

typedef void (*TaskFn)();

typedef struct CollIgnore {
    int unk0;
    Task *task;
} CollIgnore;

/* M203 shells ignore their owner and one more task */
typedef struct CollFilter {
    int unk0;
    Task *task;
    Task *owner;
    Task *other;
    float unk10;
} CollFilter;

/* noise for HumanAI_Broadcast */
typedef struct Noise {
    int kind;
    int unk4;
    int unk8;
    float radius;
    Vec3d *pos;
    Task *source;
    int unk18;
} Noise;

/* the weapon type record of DefineWeaponType (0x6E8 bytes) */
typedef struct GrenadeConfig {
    float damage;
    float radius;       /* meters */
    float falloff;      /* meters */
} GrenadeConfig;

typedef struct WeaponType {
    char unk0[0x618];
    int category;       /* 0x618 */
    char key[0x10];     /* 0x61C, model and physics type name */
    char casing[0x10];  /* 0x62C, empty casing model */
    int ammo;           /* 0x63C */
    int pellets;        /* 0x640 */
    int rate;           /* 0x644, shots per minute */
    int rounds;         /* 0x648 */
    int burst;          /* 0x64C */
    char unk650[0x14];
    float spread[2];    /* 0x664, degrees: first shots, then the rest */
    char unk66C[0x44];
    char sound[0x10];   /* 0x6B0 */
    char sound2[0x10];  /* 0x6C0 */
    char unk6D0[8];
    uint16_t gunType;   /* 0x6D8 */
    char unk6DA[6];
    GrenadeConfig *grenade; /* 0x6E0 */
    char unk6E4[4];
} WeaponType;

/* Mine (0x250 bytes), a subtype of ExplodeObject (0x238 bytes) */
typedef struct Mine {
    RigidDynCubeObj o;
    Vec3d placePos;     /* 0xF0 */
    Matrix placeMatrix; /* 0x108 */
    char unk130[0x40];
    char sound[0x20];   /* 0x170 */
    char exploded;      /* 0x190 */
    char unk191[0x53];
    char explode;       /* 0x1E4 */
    char unk1E5[0x53];
    int armDelay;       /* 0x238, frames */
    int countdown;      /* 0x23C */
    float delay;        /* 0x240, seconds */
    float radius;       /* 0x244, meters */
    char triggered;     /* 0x248 */
    char remote;        /* 0x249 */
    char unk24A[6];
} Mine;

/* what the proximity handler gets */
typedef struct MineProbe {
    int unk0;
    int unk4;
    Vec3d pos;
    float radius;
} MineProbe;

/* what the computer map asks every object (0x2C bytes) */
typedef struct MapQuery {
    Vec3d *center;
    Matrix *view;
    float zoom;
    Vec3d *pos;         /* 0xC */
    Matrix *matrix;     /* 0x10 */
    float radius;       /* 0x14 */
    char *sprite;       /* 0x18 */
    char unk1C[0x10];
} MapQuery;

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

/* the physics part of a PhysicsObj (0x4ECDB0) */
typedef struct Body {
    char unk0[0x18];
    Vec3f vel;          /* 0x108 */
    char unk24[0xA0];
} Body;

typedef struct PhysType {
    char unk0[0x2D4];
    void *data;
} PhysType;

/* per type data read by Grenade_ParseArgs */
typedef struct GrenadeType {
    float fuse;         /* seconds */
    int explosion;      /* GRENADE_EXPLOSIONTYPE_* */
} GrenadeType;

/* Grenade (0x738 bytes), a subtype of PhysicsObj */
typedef struct Grenade {
    RigidDynCubeObj o;
    Body body;          /* 0xF0 */
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

typedef struct MoveMsg {
    Vec3d *pos;
    Matrix *matrix;
    Task *obj;
} MoveMsg;

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

/* hand weapon (0x1B0 bytes) */
typedef struct Gun {
    RigidDynCubeObj o;
    char inWorld;       /* 0xF0 */
    char unkF1;
    char unkF2[2];
    char unkF4;
    char unkF5[3];
    int unkF8;
    char remove;        /* 0xFC */
    char unkFD[3];
    int id;             /* 0x100 */
    char name[0x20];    /* 0x104 */
    int16_t unk124;
    char unk126[2];
    WeaponType *wt;     /* 0x128 */
    void *ammoType;     /* 0x12C */
    Task **shots;       /* 0x130 */
    char firing;        /* 0x134 */
    char unk135[3];
    int lastShot;       /* 0x138, frame */
    int cooldown;       /* 0x13C */
    int burst;          /* 0x140 */
    int rounds;         /* 0x144 */
    float spin;         /* 0x148 */
    char unk14C[4];
    Vec3d muzzle;       /* 0x150 */
    Vec3f dir;          /* 0x168 */
    char unk174[4];
    Vec3d point;        /* 0x178 */
    Task **unk190;
    Task **loopSound;   /* 0x194 */
    float recoil;       /* 0x198 */
    int unk19C;
    int unk1A0;
    int unk1A4;
    int unk1A8;
    int reload;         /* 0x1AC */
} Gun;

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
SCHECK(WeaponType, gunType, 0x6D8);
SCHECK(WeaponType, grenade, 0x6E0);
SCHECK(Mine, sound, 0x170);
SCHECK(Mine, explode, 0x1E4);
SCHECK(Mine, remote, 0x249);
SCHECK(Grenade, phys, 0x1B4);
SCHECK(Grenade, fuse, 0x720);
SCHECK(GunPartArgs, unk8C, 0x8C);
SCHECK(Gun, rounds, 0x144);
SCHECK(Gun, point, 0x178);
SCHECK(Gun, reload, 0x1AC);
_Static_assert(sizeof(Mine) == 0x250, "Mine");
_Static_assert(sizeof(M203) == 0x128, "M203");
_Static_assert(sizeof(Grenade) == 0x738, "Grenade");
_Static_assert(sizeof(GunPart) == 0x140, "GunPart");
_Static_assert(sizeof(Gun) == 0x1B0, "Gun");
_Static_assert(sizeof(GunCasing) == 0x128, "GunCasing");
_Static_assert(sizeof(WeaponType) == 0x6E8, "WeaponType");
#undef SCHECK

uint16_t Gun_GetType(void);
uint8_t Gun_GetEvent0(void);
uint8_t Gun_GetEvent1(void);
uint8_t Gun_GetEvent2(void);
uint8_t Gun_GetEvent3(void);
uint8_t Gun_GetEvent4(void);

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((TaskFn *)gTaskHandlers)[(slot) * 0x180 + (type)])

/* exe constants, read in place so the x87 code sees the same values */
extern float kZero;
extern float kOne;
extern float kMeter;       /* 4096 */
extern float kFps;         /* 30 */
extern double kZeroD;
extern double kOneD;
extern float kDegToRad;
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
