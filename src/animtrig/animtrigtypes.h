#ifndef ANIMTRIG_ANIMTRIGTYPES_H
#define ANIMTRIG_ANIMTRIGTYPES_H

#include <stdint.h>

/* the action bits are set while the key of that device is down */
typedef struct InputBinding {
    uint32_t bits;
    int device;
    uint32_t key;
} InputBinding;

/* what an input port hands its controller */
typedef struct InputState {
    int unk0;
    float x;
    float y;
    float throttle;
    float rudder;
    int unk14[2];
    uint32_t bits;
} InputState;

/* One of four players' controls. Keys move x and y towards +-limit by
   accel per update and back to 0 by ret; the analog device is added. */
typedef struct InputPort {
    int active;
    int count;
    InputBinding *bindings;
    int analog;
    float axis[4];
    float limit[4];
    float accel[4];
    float ret[4];
    InputState state;
    char unk70[0x40];
} InputPort;
_Static_assert(sizeof(InputPort) == 0xB0, "InputPort");

#endif
