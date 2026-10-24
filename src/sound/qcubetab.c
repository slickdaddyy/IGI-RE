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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD_CONST(igd, 548790, 548790, 0x18) INIT({0x06, 0x04, 0x02, 0x00, 0x05, 0x07, 0x01, 0x03, 0x04, 0x05, 0x00, 0x01, 0x07, 0x06, 0x03, 0x02, 0x00, 0x01, 0x02, 0x03, 0x06, 0x07, 0x04, 0x05});
ALIAS(gQCubeFaceCorners, pad_548790, 0);
PAD_CONST(igd, 548790, 5487A8, 0x30) INIT({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x03, 0x02, 0x00, 0x01, 0x04, 0x05, 0x01, 0x00, 0x03, 0x02, 0x04, 0x05, 0x02, 0x03, 0x01, 0x00, 0x04, 0x05, 0x01, 0x00, 0x02, 0x03, 0x04, 0x05, 0x03, 0x02, 0x01, 0x00, 0x04, 0x05, 0x00, 0x01, 0x03, 0x02, 0x04, 0x05, 0x02, 0x03, 0x00, 0x01, 0x04, 0x05});
ALIAS(gQCubePermA, pad_5487A8, 0);
PAD_CONST(igd, 548790, 5487D8, 0x30) INIT({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x01, 0x00, 0x04, 0x05, 0x01, 0x00, 0x03, 0x02, 0x04, 0x05, 0x03, 0x02, 0x00, 0x01, 0x04, 0x05, 0x01, 0x00, 0x02, 0x03, 0x04, 0x05, 0x03, 0x02, 0x01, 0x00, 0x04, 0x05, 0x00, 0x01, 0x03, 0x02, 0x04, 0x05, 0x02, 0x03, 0x00, 0x01, 0x04, 0x05});
ALIAS(gQCubePermB, pad_5487D8, 0);
BSS(A5E410) double gQCubeUnkA5E410;
BSS(A5E410) double gQCubeUnkA5E418;
PAD(igb, A5E410, A5E420, 0x4);
PAD(igb, A5E410, A5E424, 0x40);
ALIAS(gQCubeUnkA5E424, pad_A5E424, 0);
PAD(igb, A5E410, A5E464, 0x184);
ALIAS(gQCubeUnkA5E464, pad_A5E464, 0);
BSS(B015B8) double gQCubeScaleX;
BSS(B015B8) double gQCubeScaleY;
BSS(B015B8) uint8_t gQCubeEvent;
PAD(igb, B015B8, B015C9, 0x17);
PAD(igb, B015B8, B015E0, 0x200);
ALIAS(gQCubeUnkB015E0, pad_B015E0, 0);
PAD(igb, B015B8, B017E0, 0x200);
ALIAS(gQCubeUnkB017E0, pad_B017E0, 0);
PAD(igb, B04280, B04280, 0xC0);
ALIAS(gQCubeCorners, pad_B04280, 0);
