#ifndef SCAMERA_H
#define SCAMERA_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "door/doortypes.h"
#include "levelflow/levelflowtypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)();

/* "SectorInteriorObject" (0xF0 bytes), a RigidDynCubeObj placed in a sector */
/* "SCamera" (0x1C8): the camera body; its lens is a child task */
typedef struct SCamera {
    RigidDynCubeObj o;
    Vec3d place;        /* 0xF0 */
    float yaw;          /* 0x108 */
    char model[0x10];   /* 0x10C */
    char unk11C[4];
    float pitch;        /* 0x120, lens tilt while on */
    float yaw_offset;   /* 0x124 */
    char lens_model[0x10]; /* 0x128 */
    char broken_model[0x10]; /* 0x138 */
    int16_t yaw_min;    /* 0x148, degrees */
    int16_t yaw_max;    /* 0x14A */
    int16_t turn_speed; /* 0x14C, degrees per second */
    char unk14E[2];
    float wait;         /* 0x150, seconds at each end */
    int16_t fov_x;      /* 0x154, degrees */
    int16_t fov_y;      /* 0x156 */
    float range;        /* 0x158, meters */
    Task *lens;         /* 0x15C */
    int unk160;
    int alarm;          /* 0x164, the seen target is an alarm one */
    char on;            /* 0x168 */
    char detect;        /* 0x169, isDetection */
    char last_detect;   /* 0x16A, isLastDetection */
    char unk16B;
    int detect_time;    /* 0x16C, nDetectionTime */
    char exploded;      /* 0x170 */
    char registered;    /* 0x171 */
    char unk172[2];
    VarString script;   /* 0x174 */
    char unk1C4[4];
} SCamera;

/* the eye of a lens, passed to the AI detection code */
typedef struct SCameraEye {
    Vec3d pos;          /* 0x110 */
    Matrix matrix;      /* 0x128 */
    double half_x;      /* 0x150, radians */
    double half_y;      /* 0x158 */
    double range;       /* 0x160, world units */
    char unk168[0x18];
    char unk180;
    char unk181[7];
} SCameraEye;

typedef struct DetectItem {
    struct DetectItem *next;
    void *prev;
    Task **target;
    char unkC[0x1D];
    char visible;       /* 0x29 */
} DetectItem;

/* "SCameraLens" (0x25E8) */
typedef struct SCameraLens {
    RigidDynCubeObj o;
    float pitch;        /* 0xF0 */
    float yaw;          /* 0xF4 */
    float yaw_target;   /* 0xF8 */
    int wait;           /* 0xFC, frames */
    int dir;            /* 0x100 */
    float damage;       /* 0x104 */
    float max_damage;   /* 0x108 */
    char unk10C[4];
    SCameraEye eye;     /* 0x110 */
    char detect[0x18];  /* 0x188, AI detection state */
    DetectItem *seen;   /* 0x1A0, list of seen targets */
    char unk1A4[8];
    DetectItem *seen_alarm; /* 0x1AC, seen targets that raise the alarm */
    char unk1B0[0x25B8 - 0x1B0];
    Task *target;       /* 0x25B8 */
    int lost;           /* 0x25BC, frames since the target was lost */
    Task *move_sound;   /* 0x25C0 */
    Task **beep;        /* 0x25C4 */
    Task *beep_loop;    /* 0x25C8 */
    char explode;       /* 0x25CC */
    char unk25CD[3];
    SCamera *camera;    /* 0x25D0 */
    int blink_wait;     /* 0x25D4 */
    int blink;          /* 0x25D8 */
    int frame_wait;     /* 0x25DC */
    int frame;          /* 0x25E0 */
    char unk25E4[4];
} SCameraLens;

/* "Terminal" (0x2A0), an ExplodeObject (0x238 bytes) that can be used and hacked */
typedef struct Terminal {
    RigidDynCubeObj o;
    char unkF0[0xA0];
    char exploded;      /* 0x190 */
    char unk191[0x238 - 0x191];
    char in_use;        /* 0x238 */
    char last_in_use;   /* 0x239 */
    char unk23A[2];
    int use_tick;       /* 0x23C */
    char on;            /* 0x240 */
    char hacked;        /* 0x241 */
    char hacked_tick;   /* 0x242 */
    char unk243;
    int hack_tick;      /* 0x244 */
    char registered;    /* 0x248 */
    char unk249[3];
    float unk24C;
    VarString script;   /* 0x250 */
} Terminal;

/* "SCameraControl" (0x2F8), a Terminal */
typedef struct SCameraControl {
    Terminal t;
    char detect;        /* 0x2A0 */
    char registered;    /* 0x2A1 */
    char unk2A2[2];
    VarString script;   /* 0x2A4 */
} SCameraControl;

typedef struct DrawerArg {
    const char *model;
    void *world;
    Vec3d pos;          /* 0x08 */
    char unk20[0x18];
    Matrix matrix;      /* 0x38 */
    char unk60[0x28];
    Cabinet *cabinet;   /* 0x88 */
} DrawerArg;

/* "Drawer" (0x160), a RigidDynCubeObj that slides out of its cabinet */
typedef struct Drawer {
    RigidDynCubeObj o;
    char unkF0[0x40];
    Cabinet *cabinet;   /* 0x130 */
    char unk134[4];
    Vec3d base;         /* 0x138 */
    Vec3f offset;       /* 0x150 */
    char unk15C[4];
} Drawer;

_Static_assert(offsetof(SCamera, lens) == 0x15C, "SCamera");
_Static_assert(sizeof(SCamera) == 0x1C8, "SCamera");
_Static_assert(offsetof(SCameraLens, eye) == 0x110, "SCameraLens");
_Static_assert(offsetof(SCameraLens, eye.unk180) == 0x180, "SCameraLens");
_Static_assert(offsetof(SCameraLens, seen_alarm) == 0x1AC, "SCameraLens");
_Static_assert(offsetof(SCameraLens, camera) == 0x25D0, "SCameraLens");
_Static_assert(sizeof(SCameraLens) == 0x25E8, "SCameraLens");
_Static_assert(offsetof(Terminal, script) == 0x250, "Terminal");
_Static_assert(sizeof(Terminal) == 0x2A0, "Terminal");
_Static_assert(sizeof(SCameraControl) == 0x2F8, "SCameraControl");
_Static_assert(offsetof(DrawerArg, cabinet) == 0x88, "DrawerArg");
_Static_assert(sizeof(Drawer) == 0x160, "Drawer");

#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* Calls the slot handler of each child of t (not deeper), keeping the next
   child on gTaskStack so handlers may delete tasks. slot is evaluated after
   the push, like the original. */
#define EACH_CHILD(t, slot, ...)                                        \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(slot, c_->type);                              \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

/* the largest scale factor, compared like the original */
static inline float sobj_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

uint16_t Terminal_GetType(void);
void *Terminal_GetParams(void);
void *SCameraLens_GetList(void);
int SCamera_GetDetectMask(void);
void Terminal_MapHandler(RigidDynCubeObj *o, char *arg);

#endif
