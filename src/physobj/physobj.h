#ifndef PHYSOBJ_PHYSOBJ_H
#define PHYSOBJ_PHYSOBJ_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "script/scripttypes.h"
#include "physobj/physobjtypes.h"
#include "param/paramtypes.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)(void *task, void *arg);

extern TaskFn gTaskHandlers[][0x180];
#define HANDLER(slot, type) gTaskHandlers[(uint8_t)(slot)][(uint16_t)(type)]

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

extern char gEmptyString[];

/* a point of a physics object type, read from the script */
typedef struct PhysicsObjJoint {
    int point;          /* model point index */
    int unk4;
    int unk8;
    float unkC;
    float unk10;
    float unk14;
} PhysicsObjJoint;

/* one DefinePhysicsObjType() entry (0x2D8 bytes) */
typedef struct PhysicsObjType {
    uint16_t type;      /* task type that parses the extra arguments */
    int unk4;
    float mass;         /* 0x08 */
    float size[3];      /* 0x0C, 0 means "from the model points" */
    char unk18[0x20];
    char unk38[0x100];
    char model[0x10];   /* 0x138 */
    int jointCount;     /* 0x148 */
    PhysicsObjJoint joints[16]; /* 0x14C */
    float unk2CC;
    float unk2D0;
    void *data;         /* 0x2D4, filled by the task type */
} PhysicsObjType;

_Static_assert(sizeof(PhysicsObjType) == 0x2D8, "PhysicsObjType");
_Static_assert(offsetof(PhysicsObjType, model) == 0x138, "model");
_Static_assert(offsetof(PhysicsObjType, joints) == 0x14C, "joints");
_Static_assert(offsetof(PhysicsObjType, data) == 0x2D4, "data");

#endif
