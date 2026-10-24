#ifndef MAGICOBJ_MAGICOBJTYPES_H
#define MAGICOBJ_MAGICOBJTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"

/* "RigidDynCubeObj", a DynCubeObj with a model, orientation and scale */
typedef struct RigidDynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0x10];
    int unk48;
    char unk4C[0x14];
    int inSector;
    int unk64;
    void *world;
    void **model;       /* VirModel, one entry per LOD */
    Matrix matrix;
    float scale[3];
    int lod;            /* -1: none */
    Vec3f rgb[2];
    int unkC0[5];
    int unkD4;
    int unkD8;
    void *lightmapRefs;
    int unkE0;
    int unkE4;
    Task *attached;     /* a MagicObjContainer for the objects on it */
    int hidden;
} RigidDynCubeObj;
_Static_assert(offsetof(RigidDynCubeObj, scale) == 0x98, "RigidDynCubeObj");
_Static_assert(sizeof(RigidDynCubeObj) == 0xF0, "RigidDynCubeObj");

/* filled by the RigidDynCubeObj slot 7 handler */
typedef struct ObjInfo {
    Vec3d pos;
    Vec3f velocity;
    Matrix matrix;
    float radius;
    char *name;
} ObjInfo;
_Static_assert(offsetof(ObjInfo, name) == 0x50, "ObjInfo");

/* sent to children with the RigidDynCubeObj event 4 */
typedef struct MoveArg {
    Vec3d *pos;
    Matrix *matrix;
} MoveArg;

/* a MoveArg with the object the parts belong to */
typedef struct PartMoveArg {
    Vec3d *pos;
    Matrix *matrix;
    void *owner;
} PartMoveArg;

/* a position and rotation, sent to children with the RigidDynCubeObj event 2 */
typedef struct Frame {
    Vec3d pos;
    Matrix matrix;
} Frame;

/* one DefineMagicObj() entry */
typedef struct MagicObjDef {
    char name[16];
    char model[16];
    uint16_t type;
    uint16_t unk22;
    void *data;
} MagicObjDef;

/* creates a magic object, or a part of a vehicle or object, at owner's place */
typedef struct MagicCreateArg {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;       /* from the owner */
    Matrix matrix;
    Matrix local;
    void *owner;
    void *data;         /* the DefineMagicObj data block */
} MagicCreateArg;
_Static_assert(sizeof(MagicCreateArg) == 0x90, "MagicCreateArg");

typedef struct RigidCollNode {
    float center[3];
    float radius;
    uint16_t firstTri;
    uint16_t triCount;
    uint16_t child;
    uint16_t next;
} RigidCollNode;
_Static_assert(sizeof(RigidCollNode) == 0x18, "RigidCollNode");

#endif
