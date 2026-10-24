#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "igi.h"
#include "animtrig/animtrig.h"
#include "win/wintypes.h"
#include "animtrig/animtrigtypes.h"

ORIG(0x490230, void, Keyboard_Update, (void));
ORIG(0x48FC20, void, Mouse_Update, (void));
ORIG(0x509CF0, void, Joystick_Poll, (void));
ORIG(0x507E90, void, Controller_Setup, (void *ctrl, void *handler));
ORIG(0x507EC0, void, Controller_Update, (void *ctrl, void *task));

enum {
    INPUTPORT_DEVICE_KEYBOARD = 1,
    INPUTPORT_DEVICE_JOYPAD,
    INPUTPORT_DEVICE_MOUSE,
};

typedef struct Joypad {
    float x;
    float y;
    int unk8[2];
    float throttle;
    float rudder;
    uint32_t buttons;
    int unk1C[3];
} Joypad;

_Static_assert(sizeof(Joypad) == 0x28, "Joypad");

extern InputPort gInputPorts[];
extern int gInputPortIndex;
extern uint16_t gInputPortType;
extern int gInputPortUnused;
extern Joypad gJoypads[];
extern uint8_t gKeys[];
extern Mouse gMouse;

static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kMinusOne = -1.0f;

void InputPort_ResetUnused(void)
{
    gInputPortUnused = 0;
}
HOOK(0x4ED000, InputPort_ResetUnused);

/* key driven axis: up while plus is held, down while minus is held, back
   to 0 otherwise */
static void move_axis(float *v, int minus, int plus, float limit, float accel, float ret)
{
    long double n;

    if (minus) {
        float lo = -limit;

        if (*v < -(long double)limit) {
            n = (long double)accel + *v;
            *v = n;
            if (n > lo)
                *v = lo;
        } else if (-(long double)limit < *v) {
            n = (long double)*v - accel;
            *v = n;
            if (n < lo)
                *v = lo;
        }
    } else if (plus) {
        if (*v < limit) {
            n = (long double)accel + *v;
            *v = n;
            if (n > limit)
                *v = limit;
        } else if (*v > limit) {
            n = (long double)*v - accel;
            *v = n;
            if (n < limit)
                *v = limit;
        }
    } else {
        if (*v < kZero) {
            n = (long double)ret + *v;
            *v = n;
            if (n > kZero)
                *v = 0;
        } else if (*v > kZero) {
            n = (long double)*v - ret;
            *v = n;
            if (n < kZero)
                *v = 0;
        }
    }
}

void InputPort_InputHandler(InputState *out)
{
    InputPort *p = &gInputPorts[gInputPortIndex];
    InputBinding *b = p->bindings;
    uint32_t bits = 0;
    float x, y = 0, throttle = 0, rudder = 0;
    long double t;
    int n;

    for (n = p->count; n > 0; n--, b++) {
        switch (b->device) {
        case INPUTPORT_DEVICE_KEYBOARD:
            if (gKeys[b->key] & 0x80)
                bits |= b->bits;
            break;
        case INPUTPORT_DEVICE_JOYPAD:
            if (b->key & gJoypads[gInputPortIndex].buttons)
                bits |= b->bits;
            break;
        case INPUTPORT_DEVICE_MOUSE:
            if (b->key & gMouse.buttons)
                bits |= b->bits;
            break;
        }
    }
    x = kZero;
    if (p->analog == INPUTPORT_DEVICE_JOYPAD) {
        Joypad *j = &gJoypads[gInputPortIndex];

        y = j->y;
        throttle = j->throttle;
        rudder = j->rudder;
        x = j->x;
    } else if (p->analog == INPUTPORT_DEVICE_MOUSE) {
        y = gMouse.dy;
        x = gMouse.dx;
    }
    move_axis(&p->axis[0], bits & 0x10, bits & 0x20, p->limit[0], p->accel[0], p->ret[0]);
    move_axis(&p->axis[1], bits & 0x40, bits & 0x80, p->limit[0], p->accel[1], p->ret[1]);
    x = x + p->axis[0];
    y = y + p->axis[1];
    if (kMinusOne > x)
        x = -1.0f;
    else if (kOne < x)
        x = 1.0f;
    if (kMinusOne > y)
        y = -1.0f;
    else if (kOne < y)
        y = 1.0f;
    t = (long double)throttle + p->axis[2];
    rudder = rudder + p->axis[3];
    if (kZero > t)
        t = kZero;
    else if (kOne < t)
        t = kOne;
    out->x = x;
    out->bits = bits;
    out->throttle = t;
    out->y = y;
    out->rudder = rudder;
}
HOOK(0x4ED010, InputPort_InputHandler);

void InputPort_Run(Task *t);

void InputPort_Init(void)
{
    gInputPortType = TaskType_New("InputPort", 0x20, Task_GetRootType(), 0);
    TaskType_SetHandler2(gInputPortType, 1, InputPort_Run);
    gInputPorts[0].active = 0;
    gInputPorts[1].active = 0;
    gInputPorts[2].active = 0;
    gInputPorts[3].active = 0;
}
HOOK(0x4ED510, InputPort_Init);

void InputPort_Run(Task *t)
{
    int i;

    Keyboard_Update();
    Mouse_Update();
    Joystick_Poll();
    for (i = 0; i < 4; i++) {
        if (gInputPorts[i].active) {
            gInputPortIndex = i;
            Controller_Update(&gInputPorts[i].state, t);
        }
    }
}
HOOK(0x4ED560, InputPort_Run);

void InputPort_Shutdown(void)
{
    TaskType_Free(gInputPortType);
}
HOOK(0x4ED5B0, InputPort_Shutdown);

Task *InputPort_New(Task *parent)
{
    return Task_New(parent, gInputPortType, NULL);
}
HOOK(0x4ED5C0, InputPort_New);

/* (limit, accel, return) for each of the four axes; y uses x's limit */
void InputPort_Setup(int port, int count, InputBinding *bindings, int analog, float limitX, float accelX, float retX,
                     float limitY, float accelY, float retY, float limitZ, float accelZ, float retZ, float limitW,
                     float accelW, float retW)
{
    InputPort *p = &gInputPorts[port];

    (void)limitY;
    p->count = count;
    p->bindings = bindings;
    p->analog = analog;
    p->axis[0] = 0;
    p->axis[1] = 0;
    p->axis[2] = 0;
    p->limit[0] = limitX;
    p->limit[1] = limitX;
    p->limit[2] = limitZ;
    p->limit[3] = limitW;
    p->accel[0] = accelX;
    p->accel[1] = accelY;
    p->accel[2] = accelZ;
    p->accel[3] = accelW;
    p->ret[0] = retX;
    p->ret[1] = retY;
    p->ret[2] = retZ;
    p->ret[3] = retW;
    Controller_Setup(&p->state, InputPort_InputHandler);
}
HOOK(0x4ED5E0, InputPort_Setup);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A5EF98) int gInputPortUnused;
BSS(A5EF98) int gInputPortIndex;
BSS(A5EF98) uint16_t gInputPortType;
PAD(igb, A5EF98, A5EFA2, 0x2);
PAD(igb, BC20A0, BC20A0, 0x54);
ALIAS(gInputPorts, pad_BC20A0, 0);
PAD(igb, BCAA08, BCAA08, 0x98);
ALIAS(gJoypads, pad_BCAA08, 0);
