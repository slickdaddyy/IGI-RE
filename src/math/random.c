#include "hook.h"
#include "math/vecmath.h"

void Random_Init(Random *r)
{
    r->a = 0xC8FB1A07;
    r->b = 0x39EA7756;
    r->c = 0x7FF13CCD;
}
HOOK(0x4B4720, Random_Init);

unsigned int Random_Next(Random *r)
{
    r->c ^= r->a >> 7;
    r->b ^= r->c;
    r->a += r->b;
    return r->a;
}
HOOK(0x4B4740, Random_Next);

/* in [0, 1) */
long double Random_Float(Random *r)
{
    return (long double)(Random_Next(r) & 0xFFFF) * (1.0 / 65536);
}
HOOK(0x4B4770, Random_Float);

unsigned int Random_Int(Random *r, unsigned int n)
{
    return Random_Next(r) % n;
}
HOOK(0x4B47C0, Random_Int);

void Random_Seed(Random *r, unsigned int seed)
{
    Random_Init(r);
    r->a ^= seed;
    r->b += seed << 15;
    r->c -= seed << 9;
}
HOOK(0x4B47E0, Random_Seed);
