#ifndef AMMOTYPE_AMMOTYPE_H
#define AMMOTYPE_AMMOTYPE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

#define AMMO_TYPE_MAX 32
#define AMMO_LIST_MAX 32

typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    struct Task *tail;
    struct Task *tailpred;
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

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define HANDLER(slot, type) (gTaskHandlers[(uint8_t)(slot)][(uint16_t)(type)])
extern Task *gTaskStack[];
extern int gTaskStackTop;

/* One DefineAmmoType() call from ammo/<name>/ammo.qsc, 0x168 bytes. */
typedef struct AmmoType {
    int id;
    int caliber; /* 556, 762, ... matched against the weapon type's +0x63C */
    int color0[3]; /* tracer colour at the start and end, 0..255 */
    int color1[3];
    int tracer;
    float unk24;
    char name[0x20];
    char desc[0x100];
    char unk148[0x10];
    char sound[0x10];
} AmmoType;

typedef struct AmmoEntry {
    int id;
    int count;
    int limit; /* -1: none */
} AmmoEntry;

/* The ammo a human carries (Human + 0x340). */
typedef struct AmmoList {
    int count;
    AmmoEntry e[AMMO_LIST_MAX];
} AmmoList;

/* round to float / double, even when the compiler keeps extended precision */
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

_Static_assert(sizeof(AmmoType) == 0x168, "AmmoType");

uint8_t AmmoList_GetEvent(void);
AmmoEntry *AmmoList_Find(AmmoList *list, int id);
AmmoType *AmmoType_FindByCaliber(int caliber);

#endif
