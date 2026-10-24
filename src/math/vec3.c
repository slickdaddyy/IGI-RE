#include "hook.h"
#include "igi.h"
#include "math/vecmath.h"
#include "math/x87.h"

/* Calls the C runtime acos helper, which takes and returns st(0). */
static long double crt_CIacos(long double x)
{
    __asm__ volatile("call *%1"
                     : "+t"(x)
                     : "r"(0x516810)
                     : "eax", "ecx", "edx", "memory", "cc",
                       "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
    return x;
}

void Vec3d_SetLength(Vec3d *v, double len)
{
    long double inv = 1.0 / Vec3d_Length(v);

    v->x = (double)(inv * v->x * len);
    v->y = (double)(inv * v->y * len);
    v->z = (double)(inv * v->z * len);
}
HOOK(0x4B30C0, Vec3d_SetLength);

void Vec3d_Normalize(Vec3d *v)
{
    long double inv = 1.0 / Vec3d_Length(v);

    v->x = (double)(inv * v->x);
    v->y = (double)(inv * v->y);
    v->z = (double)(inv * v->z);
}
HOOK(0x4B3100, Vec3d_Normalize);

long double Vec3d_Length(const Vec3d *v)
{
    long double x = v->x, y = v->y, z = v->z;

    return x87_sqrt(x * x + y * y + z * z);
}
HOOK(0x4B3130, Vec3d_Length);

long double Vec3d_LengthSq(const Vec3d *v)
{
    long double x = v->x, y = v->y, z = v->z;

    return x * x + y * y + z * z;
}
HOOK(0x4B3160, Vec3d_LengthSq);

void Vec3d_Cross(Vec3d *out, const Vec3d *a, const Vec3d *b)
{
    Vec3d t;

    t.x = (double)((long double)b->z * a->y - (long double)a->z * b->y);
    t.y = (double)x87_neg((long double)b->z * a->x - (long double)a->z * b->x);
    t.z = (double)((long double)a->x * b->y - (long double)b->x * a->y);
    *out = t;
}
HOOK(0x4B3190, Vec3d_Cross);

long double Vec3d_Angle(const Vec3d *a, const Vec3d *b)
{
    double dot, la;
    long double c;

    if (Vec3d_IsZero(a))
        return 0.0;
    if (Vec3d_IsZero(b))
        return 0.0;
    dot = (double)((long double)a->z * b->z + (long double)a->y * b->y + (long double)a->x * b->x);
    la = (double)Vec3d_Length(a);
    c = dot / (Vec3d_Length(b) * la);
    if (c >= 1.0)
        return 0.0;
    if (c > -1.0)
        return crt_CIacos(c);
    return PI;
}
HOOK(0x4B31F0, Vec3d_Angle);

long double Vec3d_Distance(const Vec3d *a, const Vec3d *b)
{
    long double dx = (long double)a->x - b->x;
    long double dy = (long double)a->y - b->y;
    long double dz = (long double)a->z - b->z;

    return x87_sqrt(dz * dz + dy * dy + dx * dx);
}
HOOK(0x4B32B0, Vec3d_Distance);

long double Vec3d_DistanceSq(const Vec3d *a, const Vec3d *b)
{
    long double dx = (long double)a->x - b->x;
    long double dy = (long double)a->y - b->y;
    long double dz = (long double)a->z - b->z;

    return dz * dz + dy * dy + dx * dx;
}
HOOK(0x4B32F0, Vec3d_DistanceSq);

int Vec3d_IsZero(const Vec3d *v)
{
    return v->x == 0.0 && v->y == 0.0 && v->z == 0.0;
}
HOOK(0x4B3320, Vec3d_IsZero);

int Vec3d_Equal(const Vec3d *a, const Vec3d *b)
{
    return a->x == b->x && a->y == b->y && a->z == b->z;
}
HOOK(0x4B3360, Vec3d_Equal);
