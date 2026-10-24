#include "hook.h"
#include "igi.h"
#include "math/vecmath.h"
#include "math/x87.h"

ORIG(0x4B27D0, long double, Math_ASinF, (float x));

#define gSinTable ((float *)0xA88A60)
#define gCosTable ((float *)0xA84A60)
#define gAsinTable ((float *)0xA8CA60)

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
