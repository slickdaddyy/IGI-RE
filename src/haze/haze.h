#ifndef HAZE_HAZE_H
#define HAZE_HAZE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "math/vecmath.h"
#include "task/tasktypes.h"
#include "magicobj/magicobjtypes.h"
#include "qasm/qasmtypes.h"
#include "boneobj/boneobjtypes.h"

typedef void (*HzHandler)(void *task, void *arg);

/* --- animation data, shared with the bone code before 0x4D6D40 --- */

typedef struct Bone {
    int index;
    unsigned int flags;
    Vec3f offset;
    float unk14;
    Vec3f pos;
    Matrix matrix;
    Quat world;
    Quat local;
    struct Bone *parent;
    struct Bone *child;
    struct Bone *next;
    void *blend;
} Bone;

typedef struct BoneObj {
    int skel;
    int unk4;
    Bone *bones;
} BoneObj;

#define ANIM_LOOPS 0x80000000u

extern Skeleton gSkeletons[];
extern AnimSet gAnimSets[];
extern int gBoneSmoothAnims;

/* --- task types --- */

/* "QObj", 0x50 bytes: anything the qcube renderer can draw */
typedef struct QObj {
    Task task;
    char unk20[0x28];
    void *renderData;   /* argument of handler slots 4 and 5 */
    char unk4C[4];
} QObj;

/* "BoneDynCubeObj", 0xD8 bytes */
typedef struct BoneDynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    BoneObj bones;
    void **model;
    Matrix matrix;
    float radius;
    float scale[3];
    float unkB0;
    int unkB4;
    int unkB8[5];
    int unkCC;
    int unkD0;
    int unkD4;
} BoneDynCubeObj;

/* "MagicObj", 0x138 bytes: a RigidDynCubeObj placed relative to its parent */
typedef struct MagicObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0x30];
    int unkD4;
    char unkD8[0x18];
    Vec3d offset;
    Matrix localMatrix;
    int unk130;
    int unk134;
} MagicObj;

typedef struct MagicMoveArg {
    Vec3d *pos;
    Matrix *matrix;
} MagicMoveArg;

typedef struct Camera {
    Matrix axes;
    Vec3d pos;
    float fov;
    float aspect;
    float priority;
    int frame;
    int count;
    void *owner;
    int unk58;
    int unk5C;
} Camera;

/* "Hazinglayers", 0x90 bytes: a fog box */
typedef struct HazeLayer {
    Task task;
    char unk20[0x48];
    void *world;
    float density;
    double bottom;
    double top;
    float color[3];
    int visible;
} HazeLayer;

_Static_assert(sizeof(Bone) == 0x84, "Bone");
_Static_assert(sizeof(QObj) == 0x50, "QObj");
_Static_assert(sizeof(BoneDynCubeObj) == 0xD8, "BoneDynCubeObj");
_Static_assert(sizeof(MagicObj) == 0x138, "MagicObj");
_Static_assert(sizeof(Camera) == 0x60, "Camera");
_Static_assert(sizeof(HazeLayer) == 0x90, "HazeLayer");

extern HzHandler gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gWorld;

uint8_t Camera_GetChangedEvent(void);
QTaskList *Camera_GetListeners(void);
uint16_t MagicObj_GetType(void);
uint8_t MagicObj_GetMoveEvent(void);

#endif
