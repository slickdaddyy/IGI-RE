#include "hook.h"
#include "igi.h"
#include "math/vecmath.h"
#include "math/x87.h"

ORIG(0x4B2760, long double, Math_ACosF, (float x));

void Quat_SlerpPrepare(char *flip, float *angle, float *inv_sin, const Quat *a, const Quat *b)
{
    long double dot, ang;
    float d;

    *flip = 0;
    dot = Quat_Dot(a, b);
    d = (float)dot;
    if (dot < 0.0f) {
        if (d < -1e-8f) {
            d = -d;
            *flip = 1;
        } else {
            d = 0.0f;
        }
    }
    ang = Math_ACosF(d);
    *angle = (float)ang;
    if (ang != 0.0f)
        *inv_sin = (float)(1.0f / x87_sin(ang));
    else
        *inv_sin = 1.0f;
}
HOOK(0x4B44D0, Quat_SlerpPrepare);

long double Quat_Dot(const Quat *a, const Quat *b)
{
    return (long double)a->w * b->w + (long double)a->z * b->z + (long double)a->y * b->y +
           (long double)a->x * b->x;
}
HOOK(0x4B4570, Quat_Dot);

void Quat_SetEuler(Quat *q, float ax, float ay, float az)
{
    long double ha = ax * (long double)0.5f;
    long double hb = ay * (long double)0.5f;
    long double hc = az * (long double)0.5f;
    float sa, sb, sc, cc, t1, t2;
    long double ca, cb, cbca, sbsa;

    sa = (float)x87_sin(ha);
    ca = x87_cos(ha);
    q->unk10 = 0;
    sb = (float)x87_sin(hb);
    cb = x87_cos(hb);
    sc = (float)x87_sin(hc);
    cc = (float)x87_cos(hc);
    t1 = (float)(cb * sa);
    t2 = (float)(sb * ca);
    cbca = cb * ca;
    sbsa = (long double)sb * sa;
    q->x = (float)((long double)t1 * cc - (long double)t2 * sc);
    q->y = (float)((long double)t2 * cc + (long double)t1 * sc);
    q->z = (float)(cbca * sc - sbsa * cc);
    q->w = (float)(cbca * cc + sbsa * sc);
}
HOOK(0x4B45A0, Quat_SetEuler);

void Quat_SetAxisAngle(Quat *q, const Vec3f *axis, float angle)
{
    long double h = angle * (long double)0.5f;
    long double s = x87_sin(h);

    q->x = (float)(s * axis->x);
    q->y = (float)(s * axis->y);
    q->z = (float)(s * axis->z);
    q->unk10 = 0;
    q->w = (float)x87_cos(h);
}
HOOK(0x4B4660, Quat_SetAxisAngle);
