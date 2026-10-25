#ifndef COMPUTER_COMPUTER_H
#define COMPUTER_COMPUTER_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "magicobj/magicobjtypes.h"
#include "qasm/qasmtypes.h"
#include "computer/computertypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)(void *task, void *arg);

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
