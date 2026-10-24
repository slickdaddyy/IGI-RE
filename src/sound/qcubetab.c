#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "sound/sound.h"

extern float gViewFov;
extern float gViewFovY;
extern double gQCubeUnkA5E410;
extern double gQCubeUnkA5E418;
extern uint8_t gQCubeUnkA5E424[];
extern int gQCubeUnkA5E464[];
extern double gQCubeScaleX;
extern double gQCubeScaleY;
extern uint8_t gQCubeEvent;
extern uint8_t gQCubeUnkB015E0[];
extern uint8_t gQCubeUnkB017E0[];
extern Vec3d gQCubeCorners[];
extern const uint8_t gQCubePermA[];
extern const uint8_t gQCubePermB[];
#define gQCubeFaceCorners ((const uint8_t (*)[4])0x548790)

static const double kOne = 1.0;
static const double k1_2 = 1.2;
static const double k32 = 32.0;
static const double k24 = 24.0;

/* per frame: the screen scale of the view and the largest x and y of the
   eight corners at 0xB04280 in those units */
void QCube_UpdateViewScale(void)
{
    double mx, my;
    int i;

    gQCubeScaleX = kOne / (gViewFov * k1_2) * k32;
    memset(gQCubeUnkA5E464, 0, 0x60 * 4);
    gQCubeUnkA5E410 = gQCubeCorners[0].x;
    gQCubeScaleY = kOne / (gViewFovY * k1_2) * k24;
    my = gQCubeCorners[0].y;
    for (i = 0; i < 8; i++) {
        if (gQCubeCorners[i].x > gQCubeUnkA5E410)
            gQCubeUnkA5E410 = gQCubeCorners[i].x;
        if (my < gQCubeCorners[i].y)
            my = gQCubeCorners[i].y;
    }
    mx = gQCubeUnkA5E410;
    gQCubeUnkA5E410 = mx * gQCubeScaleX * k1_2;
    gQCubeUnkA5E418 = my * gQCubeScaleY * k1_2;
}
HOOK(0x4E5CB0, QCube_UpdateViewScale);

/* bit permutation tables: for each 6 bit code, its bits moved to the
   positions given by eight rows of six */
void QCube_InitTables(void)
{
    int g, i, j;

    gQCubeEvent = TaskEvent_Alloc(0);
    memset(gQCubeUnkA5E464, 0, 0x60 * 4);
    for (g = 0; g < 8; g++) {
        for (i = 0; i < 64; i++) {
            uint8_t *a = &gQCubeUnkB017E0[g * 64 + i];
            uint8_t *b = &gQCubeUnkB015E0[g * 64 + i];

            *a = 0;
            for (j = 0; j < 6; j++)
                if (i & 1 << j)
                    *a |= 1 << gQCubePermA[g * 6 + j];
            *b = 0;
            for (j = 0; j < 6; j++)
                if (i & 1 << j)
                    *b |= 1 << gQCubePermB[g * 6 + j];
        }
    }
    for (i = 0; i < 64; i++) {
        gQCubeUnkA5E424[i] = 0;
        for (j = 0; j < 6; j++) {
            const uint8_t *f = gQCubeFaceCorners[j];

            if (i & 1 << j)
                gQCubeUnkA5E424[i] |= 1 << f[3] | 1 << f[0] | 1 << f[2] | 1 << f[1];
        }
    }
}
HOOK(0x4E5D90, QCube_InitTables);

void QCube_FreeTables(void)
{
    TaskEvent_Free(gQCubeEvent);
    gQCubeEvent = 0xFF;
}
HOOK(0x4E5EA0, QCube_FreeTables);
