#ifndef ANIMCAM_ANIMCAM_H
#define ANIMCAM_ANIMCAM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)();

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

/* filled by the slot 7 (GetInfo) handlers */
typedef struct ObjInfo {
    Vec3d pos;
    Vec3f velocity;
    Matrix matrix;      /* 0x24 */
    float radius;       /* 0x4C */
    int unk50;
} ObjInfo;

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

_Static_assert(offsetof(ObjInfo, unk50) == 0x50, "ObjInfo");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]
#define gCurrentWorld VAR(void *, 0xA44344)
#define gKeys ((uint8_t *)0xC28C62)
#define gKeysPrev ((uint8_t *)0xC28D62)

#define TASK_NOTHING ((void *)0x4D3710)

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
