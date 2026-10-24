#ifndef TAGITEM_TAGITEM_H
#define TAGITEM_TAGITEM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "editobj/editobjtypes.h"

typedef void (*TaskFn)(void *task, void *arg);

/* TagItem (0x124), TagItemInt32 / Real32 (0x128), String16 (0x134), String256 (0x224) */
typedef struct TagItem {
    Task task;
    char name[0x100];
    void *paramType;
    union {
        int i;
        float f;
        char s[0x100];
    } v;
} TagItem;

/* "SplineObjSegment" (0x5B8), the bent model between two waypoints; see notes/sound.md */
typedef struct SplineObjSegment {
    Task task;
    Vec3d pos;
    char unk38[0xA8 - 0x38];
    Vec3f unkA8;
    Vec3f unkB4;
    char unkC0[0xF8 - 0xC0];
    float length;
    float start;
    float end;
    float segPos[21][3];
    Matrix segRot[21];          /* [20] overlaps the spare word before segLength */
    float segLength[20];
    int unk598;
    int segments;
} SplineObjSegment;

/* "SplineObj" (0x68), the owner of a chain of waypoints */
typedef struct SplineObj {
    Task task;
    int unk20;
    int maxSegments;
    float length;
    char unk2C[0x4C - 0x2C];
    char snap;
    char collide;
    char rigid;
    char autoOrient;
    Vec3f unk50;
    Vec3f unk5C;
} SplineObj;

_Static_assert(sizeof(TagItem) == 0x224, "TagItem");
_Static_assert(offsetof(SplineObjSegment, segRot) == 0x200, "segRot");
_Static_assert(offsetof(SplineObjSegment, segments) == 0x59C, "segments");
_Static_assert(sizeof(SplineObj) == 0x68, "SplineObj");

#endif
