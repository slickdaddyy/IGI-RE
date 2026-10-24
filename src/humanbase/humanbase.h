#ifndef HUMANBASE_H
#define HUMANBASE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "coll/colltypes.h"
#include "humanai/humanaitypes.h"
#include "camera/cameratypes.h"
#include "loadscreen/loadscreentypes.h"
#include "humanbase/humanbasetypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

/* gTaskHandlers[slot][type] */
extern TaskHandler gTaskHandlers[][0x180];

typedef struct HumanText {
    void *grid;
    float x, y, z;
    char unk10[0x14];
    unsigned int flags;
} HumanText;

typedef struct HumanView {
    char unk0[0x20];
    double pos[3];
    char unk38[0x38];
    char *model;
    char unk74[0x65];
    char firstPerson;
    char unkDA[0x5A];
    float unk134;
    char unk138[2];
    char camOverride;
    char unk13B[5];
    double camPos[3];
    float camMatrix[10];
    char unk180[0x64];
    float fov;
    float unk1E8;
} HumanView;

#define HUMAN_ON_GROUND 0x4
#define HUMAN_ROOF      0x10
#define HUMAN_DEAD      0x20000

_Static_assert(offsetof(HumanView, firstPerson) == 0xD9, "firstPerson");
_Static_assert(offsetof(HumanView, unk134) == 0x134, "unk134");
_Static_assert(offsetof(HumanView, camOverride) == 0x13A, "camOverride");
_Static_assert(offsetof(HumanView, camPos) == 0x140, "camPos");
_Static_assert(offsetof(HumanView, camMatrix) == 0x158, "camMatrix");
_Static_assert(offsetof(HumanView, fov) == 0x1E4, "fov");
_Static_assert(offsetof(HumanText, flags) == 0x24, "flags");

/* where a weapon sits: hand bone of the body, or of the view model */
typedef struct HumanBoneMsg {
    int firstPerson;
    int unk4;
    double pos[3];
    float matrix[10];
} HumanBoneMsg;

/* slot 9: segments against everything; filter is an event asked first */
typedef struct CollLinesMsg {
    double *starts;
    double *ends;
    int count;
    uint8_t filter;
    char unkD[3];
    char *result;
    int unk14;
    int unk18;
    int magic;
    CollHit *hits;
} CollLinesMsg;

/* slot 8: one segment (line of sight) */
typedef struct CollLineMsg {
    double *start;
    double *end;
    uint8_t filter;
    char unk9[3];
    char *result;
} CollLineMsg;

void Human_WakeIfVisible(Human *h);
void Human_Wake(Human *h);
int Human_LinesHitBounds(Human *h, const double *starts, const double *ends, int count);
uint16_t Human_GetType(void);
void *Human_GetList(void);
void Human_printf(Human *h, const char *fmt, ...);

#endif
