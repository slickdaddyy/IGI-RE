#ifndef COMPUTER_COMPUTER_H
#define COMPUTER_COMPUTER_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "magicobj/magicobjtypes.h"
#include "qasm/qasmtypes.h"

typedef void (*TaskFn)(void *task, void *arg);

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

_Static_assert(sizeof(MapQuery) == 0x2C, "MapQuery");

extern TaskFn gTaskHandlers[][0x180];
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

uint8_t ComputerObject_GetEvent0(void);
uint8_t ComputerObject_GetEvent1(void);
uint8_t ComputerObject_GetEvent2(void);
QTaskList *ComputerHilight_GetList(void);
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
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
