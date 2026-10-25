#ifndef HAZE_HAZE_H
#define HAZE_HAZE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "math/vecmath.h"
#include "magicobj/magicobjtypes.h"
#include "qasm/qasmtypes.h"
#include "haze/hazetypes.h"

typedef void (*HzHandler)(void *task, void *arg);

/* --- animation data, shared with the bone code before 0x4D6D40 --- */

#define ANIM_LOOPS 0x80000000u

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

_Static_assert(sizeof(QObj) == 0x50, "QObj");
_Static_assert(sizeof(MagicObj) == 0x138, "MagicObj");
_Static_assert(sizeof(HazeLayer) == 0x90, "HazeLayer");

extern Task *gTaskStack[];
extern int gTaskStackTop;

uint8_t Camera_GetChangedEvent(void);
QTaskList *Camera_GetListeners(void);
uint16_t MagicObj_GetType(void);
uint8_t MagicObj_GetMoveEvent(void);

#endif
