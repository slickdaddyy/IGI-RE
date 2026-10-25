#ifndef GRAPHTASK_H
#define GRAPHTASK_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "graphtask/graphtasktypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)(void *task, void *arg);

#define HANDLER(slot, type) gTaskHandlers[(uint8_t)(slot)][(uint16_t)(type)]

#define PHYS_CONTACTS 22

/* sent to a task the physics object ran into, result 1 ignores the hit */
typedef struct PhysHitMsg {
    PhysObj *obj;
    CollHit *hit;
    int result;
} PhysHitMsg;

/* the motion of a point carried by a physics object */
typedef struct PhysPointMsg {
    Vec3d *point;
    Vec3d move;
    Matrix turn;
} PhysPointMsg;

_Static_assert(offsetof(PhysPointMsg, turn) == 0x20, "PhysPointMsg");

/* x87 compare results: NaN counts as equal, as with fcomp + test ah,0x40 */
#define NONZERO(x) ((x) < 0 || (x) > 0)

#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }
#define F64(name, bits) static const union { uint64_t u; double d; } name = { bits }

#endif
