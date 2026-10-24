#ifndef BINOCULAR_BINOCULARTYPES_H
#define BINOCULAR_BINOCULARTYPES_H

#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "res/restypes.h"

/* "BinocularOverlayHolder" (0x78 bytes): shows an overlay while "active" is true */
typedef struct BinocularHolder {
    Task task;
    Task **overlay;         /* 0x20 */
    VarString active;       /* 0x24 */
    char isActive;          /* 0x74 */
    char pad75[3];
} BinocularHolder;

typedef struct VUMeter {
    float level;
    float unk4;
    float decay;
    int timer;
    int period;
} VUMeter;

/* a human marked in the binocular view (0x2C bytes) */
typedef struct Target {
    char *human;
    int unk4;           /* the human's 0xCF5 flag */
    int unk8;           /* the human's 0xFC field is not 1 */
    int occluded;
    int seen;           /* seen this frame */
    float radius;
    float depth;
    int time;           /* last seen */
    int firstTime;
    float sx;
    float sy;
} Target;

typedef struct BinocularOverlay {
    Task task;
    Text *text;             /* 0x20, the range */
    Text *letters[4];       /* 0x24, W N E S */
    Text *unk34;
    Matrix matrix;          /* 0x38 */
    Vec3d pos;              /* 0x60 */
    float fov;              /* 0x78 */
    Vec3f dir;              /* 0x7C */
    char ctx[0xA8];         /* 0x88, the view used for drawing */
    int visible;            /* 0x130, humans inside the view */
    int shown;              /* 0x134, humans marked */
    char rng[0xC];          /* 0x138 */
    int unk144;
    int unk148;
    char grid[0x2A04];      /* 0x14C */
    int unk2B50;
    VUMeter meters[12];     /* 0x2B54 */
    Target targets[10];     /* 0x2C44 */
    int unk2DFC;
} BinocularOverlay;
_Static_assert(sizeof(BinocularOverlay) == 0x2E00, "BinocularOverlay");

typedef struct DetectQuery {
    char result;
    char pad[7];
    Task *a;
    Task *b;
} DetectQuery;

#endif
