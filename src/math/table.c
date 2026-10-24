#include "hook.h"
#include "igi.h"
#include "math/vecmath.h"
#include "math/x87.h"

ORIG(0x4B27D0, long double, Math_ASinF, (float x));

extern float gSinTable[];
extern float gCosTable[];
extern float gAsinTable[];

/* sin and cos over a full turn in 4096 steps, asin over [-1, 1] in 8192 steps */
void Math_InitTables(void)
{
    int i;

    for (i = 0; i < 4096; i++) {
        long double a = i * (long double)0.0015339808f;

        gSinTable[i] = (float)x87_sin(a);
        gCosTable[i] = (float)x87_cos(a);
    }
    for (i = 0; i <= 8192; i++)
        gAsinTable[i] = (float)Math_ASinF((float)(i * (long double)(1.0f / 4096) - 1.0f));
}
HOOK(0x4B46A0, Math_InitTables);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, A84A60, A84A60, 0x4000);
ALIAS(gCosTable, pad_A84A60, 0);
PAD(igb, A84A60, A88A60, 0x4000);
ALIAS(gSinTable, pad_A88A60, 0);
PAD(igb, A84A60, A8CA60, 0x8004);
ALIAS(gAsinTable, pad_A8CA60, 0);
