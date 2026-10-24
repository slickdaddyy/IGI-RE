#ifndef COMPUTER_COMPUTER_H
#define COMPUTER_COMPUTER_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"

typedef void (*TaskFn)(void *task, void *arg);

/* a script string parameter that may hold an expression */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* a task list: count, then the tasks from +8 */
typedef struct TaskArray {
    int count;
    int unk04;
    Task *items[1];
} TaskArray;

/* RigidDynCubeObj, the fields used here */
typedef struct Obj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    int world;
    void **model;
    Matrix matrix;
    float scale[3];
} Obj;

/* passed to every object drawn on the map computer (ComputerObject event 0) */
typedef struct MapQuery {
    Vec3d *center;
    float *view;
    float zoom;
    Vec3d *pos;
    Matrix *matrix;
    float radius;
    void *sprite;
    int marker;
    int color;
    char *text;
    char *text2;
} MapQuery;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(offsetof(Obj, scale) == 0x98, "Obj");
_Static_assert(sizeof(MapQuery) == 0x2C, "MapQuery");

extern TaskFn gTaskHandlers[][0x180];
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

uint8_t ComputerObject_GetEvent0(void);
uint8_t ComputerObject_GetEvent1(void);
uint8_t ComputerObject_GetEvent2(void);
TaskArray *ComputerHilight_GetList(void);
void ComputerObject_ProjectDir(float *out, float *dir, MapQuery *q);
void ComputerObject_DrawSprite(MapQuery *q);
void ComputerObject_DrawMarker(MapQuery *q);
void ComputerObject_DrawText(MapQuery *q);

/* fistp with the current rounding mode, like the original */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* the largest of the three scale factors */
static inline float max_scale(const Obj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
