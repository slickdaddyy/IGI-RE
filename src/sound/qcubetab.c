#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "sound/sound.h"
#include "transctx/transctxtypes.h"

extern TransContext gTransContext;
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
extern const uint8_t gQCubeFaceCorners[][4];

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

    gQCubeScaleX = kOne / (gTransContext.fov * k1_2) * k32;
    memset(gQCubeUnkA5E464, 0, 0x60 * 4);
    gQCubeUnkA5E410 = gQCubeCorners[0].x;
    gQCubeScaleY = kOne / (gTransContext.fovY * k1_2) * k24;
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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 548790, 1) const uint8_t store_548790[6][4] INIT({{6, 4, 2}, {5, 7, 1, 3}, {4, 5, 0, 1}, {7, 6, 3, 2}, {0, 1, 2, 3}, {6, 7, 4, 5}});
ALIAS(gQCubeFaceCorners, store_548790, 0);
STORE(igd, 548790, 1) const uint8_t store_5487A8[48] INIT({0, 1, 2, 3, 4, 5, 3, 2, 0, 1, 4, 5, 1, 0, 3, 2, 4, 5, 2, 3, 1, 0, 4, 5, 1, 0, 2, 3, 4, 5, 3, 2, 1, 0, 4, 5, 0, 1, 3, 2, 4, 5, 2, 3, 0, 1, 4, 5});
ALIAS(gQCubePermA, store_5487A8, 0);
STORE(igd, 548790, 1) const uint8_t store_5487D8[48] INIT({0, 1, 2, 3, 4, 5, 2, 3, 1, 0, 4, 5, 1, 0, 3, 2, 4, 5, 3, 2, 0, 1, 4, 5, 1, 0, 2, 3, 4, 5, 3, 2, 1, 0, 4, 5, 0, 1, 3, 2, 4, 5, 2, 3, 0, 1, 4, 5});
ALIAS(gQCubePermB, store_5487D8, 0);
BSS(A5E410) double gQCubeUnkA5E410;
BSS(A5E410) double gQCubeUnkA5E418;
PAD(igb, A5E410, A5E420, 0x4);
STORE(igb, A5E410, 1) uint8_t store_A5E424[64];
ALIAS(gQCubeUnkA5E424, store_A5E424, 0);
STORE(igb, A5E410, 4) int store_A5E464[97];
ALIAS(gQCubeUnkA5E464, store_A5E464, 0);
BSS(B015B8) double gQCubeScaleX;
BSS(B015B8) double gQCubeScaleY;
BSS(B015B8) uint8_t gQCubeEvent;
PAD(igb, B015B8, B015C9, 0x17);
STORE(igb, B015B8, 1) uint8_t store_B015E0[512];
ALIAS(gQCubeUnkB015E0, store_B015E0, 0);
STORE(igb, B015B8, 1) uint8_t store_B017E0[512];
ALIAS(gQCubeUnkB017E0, store_B017E0, 0);
STORE(igb, B04280, 8) __typeof__(gQCubeCorners[0]) store_B04280[8];
ALIAS(gQCubeCorners, store_B04280, 0);
