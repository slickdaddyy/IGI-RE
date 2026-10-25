#ifndef EXPLOSION_EXPLOSION_H
#define EXPLOSION_EXPLOSION_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "magicobj/magicobjtypes.h"
#include "qasm/qasmtypes.h"
#include "explosion/explosiontypes.h"
#include "task/taskvars.h"

FN_EXTERN(Task_FalseHandler);
FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)(void *task, void *arg);

/* the start of the 0x70 byte collision hit sent with the bullet hole event */
typedef struct HoleHit {
    Task *task;
    int unk4;
    Vec3d pos;
    Vec3f point;  /* 0x20 */
    Vec3f normal; /* 0x2C, the hole faces this way */
    Vec3f dir;    /* 0x38, the corners are tested along it */
} HoleHit;

/* argument of the bullet hole event */
typedef struct HoleMsg {
    HoleHit *hit;
    int unk4;
    void *texture;
    float size;
} HoleMsg;

/* a polygon of a hole that was cut to fit (0x40 bytes, from the pool at 0x5C1120) */
typedef struct HolePoly {
    Vec3f v[3];
    float uv[6];
    struct HolePoly *next;
} HolePoly;

/* one bullet hole: a square of four corners facing the normal */
typedef struct Hole {
    Vec3f normal;
    Vec3f pos;
    Vec3f corner[4];
    int state; /* -1 when not on a surface */
    void *texture;
    float size;
    HolePoly *polys;
} Hole;

/* a triangle of the hole mesh */
typedef struct HoleTri {
    Vec3f *v[3];
    const Vec3f *color[3];
    float uv[6];
    char unk30[0xC];
    Vec3f normal;
    void *texture;
    int unk4C;
} HoleTri;

/* "BulletHole" (0xE148 bytes), the holes in one object, a child of it */
typedef struct BulletHole {
    Task task;
    Hole holes[200];
    int last;  /* 0x44E0, the newest hole */
    int next;  /* 0x44E4, the hole to reuse when full */
    int count; /* 0x44E8 */
    HoleTri tris[500];
    void *mesh; /* 0xE12C */
    Task *obj;
    void **poolItem;
    char dirty;
    char unkE139[0xE144 - 0xE139];
    unsigned int time;
} BulletHole;

/* "FlashBang" (0x48 bytes) */
typedef struct FlashBang {
    Task task;
    Vec3d pos;
    void *ps;
    float falloff;
    float radius;
    int unk44;
} FlashBang;

/* "ExplosionSmoke" (0x48 bytes) */
typedef struct ExplosionSmoke {
    Task task;
    int timer;
    int frames;
    Vec3d pos;
    float size;
    void *ps2; /* the second, slow emitter */
} ExplosionSmoke;

/* "FireBall" (0x118 bytes), an animated sprite model that faces the camera */
typedef struct FireBall {
    RigidDynCubeObj obj;
    int frame;  /* 0xF0, -1 when not animated */
    int life;   /* 0xF4 */
    int frames; /* 0xF8 */
    int delay;  /* 0xFC */
    int unk100;
    int unk104;
    int animFrames; /* 0x108 */
    float size;     /* 0x10C */
    float angle;    /* 0x110 */
    float spin;     /* 0x114 */
} FireBall;

/* "Fragment" (0x118 bytes), a piece thrown by an explosion */
typedef struct Fragment {
    Task task;
    Vec3d pos;
    char unk38[0x60];
    float scale[3]; /* 0x98 */
    char unkA4[0xF0 - 0xA4];
    Vec3f vel;  /* 0xF0 */
    int life;   /* 0xFC */
    int start;  /* 0x100 */
    int kind;   /* 0x104, 0 none, 1 smoke trail, 2 fire and smoke */
    void *ps;   /* 0x108 */
    void *ps2;  /* 0x10C */
    float size; /* 0x110 */
    int unk114;
} Fragment;

/* broadcast to the AI when a flashbang goes off */
typedef struct FlashNoise {
    int type;
    int unk4;
    int unk8;
    float range;
    Vec3d *pos;
    Task *source;
    int unk18;
} FlashNoise;

/* Game event 11, an explosion */
typedef struct BlastMsg {
    Task *fireball;
    Vec3d *pos;
    float blast;
    float size;
    float unk58;
    int unk14;
    int unk84;
} BlastMsg;

/* sent to the children of a fireball when it is drawn */
typedef struct DrawMsg {
    Vec3d pos;
    Matrix matrix;
} DrawMsg;

/* render function argument */
typedef struct RenderArg {
    void *obj;
    int unk4;
    int unk8;
    int unkC;
    int unk10;
} RenderArg;

_Static_assert(offsetof(HoleHit, dir) == 0x38, "HoleHit");
_Static_assert(sizeof(HolePoly) == 0x40, "HolePoly");
_Static_assert(sizeof(Hole) == 0x58, "Hole");
_Static_assert(sizeof(HoleTri) == 0x50, "HoleTri");
_Static_assert(offsetof(BulletHole, last) == 0x44E0, "BulletHole");
_Static_assert(offsetof(BulletHole, mesh) == 0xE12C, "BulletHole");
_Static_assert(sizeof(BulletHole) == 0xE148, "BulletHole");
_Static_assert(sizeof(FlashBang) == 0x48, "FlashBang");
_Static_assert(sizeof(ExplosionSmoke) == 0x48, "ExplosionSmoke");
_Static_assert(sizeof(FireBall) == 0x118, "FireBall");
_Static_assert(sizeof(Fragment) == 0x118, "Fragment");
_Static_assert(sizeof(FlashNoise) == 0x1C, "FlashNoise");

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]
#define TASK_NOTHING ((TaskFn)FN(0x4D3710, Task_Nothing))
#define TASK_FALSE ((TaskFn)FN(0x45F920, Task_FalseHandler))

/* exe constants, read in place */

extern Pool gHolePool;
extern Pool gHolePolyPool;
extern uint8_t gFlashEvent;
extern uint16_t gFlashType;
extern char *gFlashTexture;
extern char *gFlashSprite;

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

#endif
