#ifndef HUMANBASE_H
#define HUMANBASE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "humanai/humanaitypes.h"
#include "camera/cameratypes.h"
#include "viewcam/viewcamtypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

typedef struct HumanText {
    void *grid;
    float x, y, z;
    char unk10[0x14];
    unsigned int flags;
} HumanText;

#define HUMAN_ON_GROUND 0x4
#define HUMAN_ROOF      0x10
#define HUMAN_DEAD      0x20000

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
