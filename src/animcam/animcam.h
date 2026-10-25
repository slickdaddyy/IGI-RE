#ifndef ANIMCAM_ANIMCAM_H
#define ANIMCAM_ANIMCAM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "magicobj/magicobjtypes.h"
#include "task/taskvars.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)();

/* {name, default, min, max} of one animation input channel */
typedef struct AnimInput {
    const char *name;
    float def;
    float min;
    float max;
} AnimInput;

/* the animation control event: mode 0 resets, > 0 runs one step */
typedef struct AnimControlMsg {
    int mode;
    float *keys;
} AnimControlMsg;

typedef struct AnimInputMsg {
    int index;
    const AnimInput *out;
} AnimInputMsg;

#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* keys never touched by the key handlers */
#define ANIM_NO_KEY -999.0f

/* DirectInput key codes */
enum {
    KEY_1 = 0x02,
    KEY_9 = 0x0A,
    KEY_0 = 0x0B,
    KEY_E = 0x12,
    KEY_R = 0x13,
    KEY_LCONTROL = 0x1D,
    KEY_A = 0x1E,
    KEY_F = 0x21,
    KEY_LSHIFT = 0x2A,
    KEY_C = 0x2E,
    KEY_V = 0x2F,
    KEY_B = 0x30,
    KEY_N = 0x31,
    KEY_RSHIFT = 0x36,
    KEY_SPACE = 0x39,
    KEY_RCONTROL = 0x9D,
};

#define KEY_DOWN(k) (gKeys[k] & 0x80)
#define KEY_PRESSED(k) ((gKeys[k] & 0x80) && !(gKeysPrev[k] & 0x80))

/* fistp with the current rounding mode */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* float copy without going through the x87 */
static inline void fcopy(float *dst, const float *src)
{
    memcpy(dst, src, sizeof(float));
}

#endif
