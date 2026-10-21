#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "math/vecmath.h"
#include "math/x87.h"

/* out = a * b. The original inlines this everywhere, always summing k = 0, 2, 1. */
static void Mul(Matrix *out, const Matrix *a, const Matrix *b)
{
    Matrix t;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            const float *r = &a->m[i * 3];

            t.m[i * 3 + j] = (float)((long double)r[0] * b->m[j] + (long double)r[2] * b->m[6 + j] +
                                     (long double)r[1] * b->m[3 + j]);
        }
    }
    t.unk24 = a->unk24 + b->unk24 + 1;
    *out = t;
}

static long double Atan2(long double y, long double x)
{
    if (x == 0.0) {
        if (y == 0.0) {
            Error_Show("Atan2 undefined in origo");
            for (;;)
                ;
        }
        return (y < 0.0f ? -1.0 : 1.0) * HALF_PI;
    }
    return x87_atan2(y, x);
}

static void Vec3f_Normalize(Vec3f *v)
{
    long double inv;

    if (v->x == 0.0f && v->y == 0.0f && v->z == 0.0f)
        return;
    inv = 1.0 / x87_sqrt((long double)v->x * v->x + (long double)v->y * v->y + (long double)v->z * v->z);
    v->x = (float)(v->x * inv);
    v->y = (float)(v->y * inv);
    v->z = (float)(v->z * inv);
}

static void Vec3f_Cross(Vec3f *out, const Vec3f *a, const Vec3f *b)
{
    out->x = (float)((long double)b->z * a->y - (long double)a->z * b->y);
    out->y = (float)-((long double)b->z * a->x - (long double)a->z * b->x);
    out->z = (float)((long double)a->x * b->y - (long double)b->x * a->y);
}

void Matrix_RotateEuler(Matrix *out, const Matrix *m, float ax, float ay, float az)
{
    Matrix r;

    Matrix_SetEuler(&r, ax, ay, az);
    Mul(out, &r, m);
}
HOOK(0x4B33A0, Matrix_RotateEuler);

void Matrix_RotateX(Matrix *out, const Matrix *m, float a)
{
    Matrix r;

    Matrix_SetRotX(&r, a);
    Mul(out, m, &r);
}
HOOK(0x4B34F0, Matrix_RotateX);

void Matrix_RotateY(Matrix *out, const Matrix *m, float a)
{
    Matrix r;

    Matrix_SetRotY(&r, a);
    Mul(out, m, &r);
}
HOOK(0x4B3640, Matrix_RotateY);

void Matrix_RotateZ(Matrix *out, const Matrix *m, float a)
{
    Matrix r;

    Matrix_SetRotZ(&r, a);
    Mul(out, m, &r);
}
HOOK(0x4B3790, Matrix_RotateZ);

void Matrix_SetEuler(Matrix *m, float ax, float ay, float az)
{
    Matrix r;

    Matrix_SetRotZ(m, az);
    Matrix_SetRotY(&r, ay);
    Mul(m, m, &r);
    Matrix_SetRotX(&r, ax);
    Mul(m, m, &r);
    m->unk24 = 0;
}
HOOK(0x4B38E0, Matrix_SetEuler);

void Matrix_SetRotX(Matrix *m, float a)
{
    long double s = x87_sin(a);
    float c = (float)x87_cos(a);

    m->m[0] = 1.0f;
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[6] = 0.0f;
    m->unk24 = 0;
    m->m[4] = c;
    m->m[5] = (float)-s;
    m->m[8] = c;
    m->m[7] = (float)s;
}
HOOK(0x4B3B60, Matrix_SetRotX);

void Matrix_SetRotY(Matrix *m, float a)
{
    long double s = x87_sin(a);
    long double c = x87_cos(a);

    m->m[1] = 0.0f;
    m->m[3] = 0.0f;
    m->m[4] = 1.0f;
    m->m[5] = 0.0f;
    m->m[7] = 0.0f;
    m->unk24 = 0;
    m->m[0] = (float)c;
    m->m[2] = (float)s;
    m->m[6] = (float)-s;
    m->m[8] = (float)c;
}
HOOK(0x4B3BA0, Matrix_SetRotY);

void Matrix_SetRotZ(Matrix *m, float a)
{
    long double s = x87_sin(a);
    float c = (float)x87_cos(a);

    m->m[2] = 0.0f;
    m->m[5] = 0.0f;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->m[8] = 1.0f;
    m->unk24 = 0;
    m->m[0] = c;
    m->m[1] = (float)-s;
    m->m[4] = c;
    m->m[3] = (float)s;
}
HOOK(0x4B3BE0, Matrix_SetRotZ);

void Matrix_SetScale(Matrix *m, float sx, float sy, float sz)
{
    m->m[4] = sy;
    m->m[0] = sx;
    m->m[8] = sz;
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[5] = 0.0f;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->unk24 = 0;
}
HOOK(0x4B3C20, Matrix_SetScale);

void Matrix_GetEuler(const Matrix *m, float *ax, float *ay, float *az)
{
    const float *e = m->m;
    long double cy = x87_sqrt((long double)e[0] * e[0] + (long double)e[3] * e[3]);
    long double x, y;
    double rx, rz;

    if (cy > 0.0001) {
        rx = (double)Atan2(e[7], e[8]);
        y = Atan2(-e[6], cy);
        rz = (double)Atan2(e[3], e[0]);
    } else {
        rx = (double)Atan2(-e[5], e[4]);
        y = Atan2(-e[6], cy);
        rz = 0.0;
    }
    x = rx;
    if (x < 0.0)
        x += TWO_PI;
    if (y < 0.0)
        y += TWO_PI;
    if (rz < 0.0)
        rz = (double)(rz + (long double)TWO_PI);
    *ax = (float)x;
    *ay = (float)y;
    *az = (float)rz;
}
HOOK(0x4B3C50, Matrix_GetEuler);

void Matrix_SetAxisAngle(Matrix *m, const Vec3f *axis, float angle)
{
    long double h = angle * (long double)0.5f;
    long double s = x87_sin(h), w;
    float x = (float)(s * axis->x);
    float y = (float)(s * axis->y);
    float z = (float)(s * axis->z);
    float xx, yx, zx, wx, yy, zy, wy, zz;
    long double wz, t;

    w = x87_cos(h);
    xx = (float)((long double)x * x * 2);
    yx = (float)((long double)y * x * 2);
    zx = (float)((long double)z * x * 2);
    wx = (float)(w * x * 2);
    yy = (float)((long double)y * y * 2);
    zy = (float)((long double)z * y * 2);
    wy = (float)(w * y * 2);
    zz = (float)((long double)z * z * 2);
    wz = w * z * 2;
    m->m[0] = (float)((long double)1.0f - yy - zz);
    m->m[1] = (float)(yx - wz);
    m->m[2] = (float)((long double)wy + zx);
    m->m[3] = (float)(wz + yx);
    t = (long double)1.0f - xx;
    m->m[4] = (float)(t - zz);
    m->m[5] = (float)((long double)zy - wx);
    m->m[6] = (float)((long double)zx - wy);
    m->unk24 = 0;
    m->m[7] = (float)((long double)zy + wx);
    m->m[8] = (float)(t - yy);
}
HOOK(0x4B3EF0, Matrix_SetAxisAngle);

void Matrix_SetDirection(Matrix *m, const Vec3f *dir)
{
    Vec3f d = *dir, up, side, s, t;
    Matrix r;

    if (d.x == 0.0f && d.y == 0.0f) {
        up.x = 0.0f;
        up.y = 1.0f;
    } else {
        up.x = d.y;
        up.y = -d.x;
        up.z = 0.0f;
        Vec3f_Normalize(&up);
    }
    up.z = 0.0f;
    Vec3f_Cross(&side, &up, &d);
    r.m[0] = side.x;
    r.m[1] = side.y;
    r.m[2] = side.z;
    Vec3f_Normalize((Vec3f *)&r.m[0]);
    Vec3f_Cross(&s, &d, &side);
    Vec3f_Cross(&t, (Vec3f *)&r.m[0], &s);
    r.m[3] = s.x;
    r.m[4] = s.y;
    r.m[5] = s.z;
    r.m[6] = t.x;
    r.m[7] = t.y;
    r.m[8] = t.z;
    Vec3f_Normalize((Vec3f *)&r.m[3]);
    Vec3f_Normalize((Vec3f *)&r.m[6]);
    r.unk24 = 0;
    *m = r;
}
HOOK(0x4B4010, Matrix_SetDirection);

void Matrix_Invert(Matrix *out, const Matrix *m)
{
    const float *e = m->m;
    long double a = (long double)e[3] * e[7];
    long double b = (long double)e[6] * e[5];
    long double c = (long double)e[8] * e[4];
    long double d = (long double)e[5] * e[7];
    float f = (float)((long double)e[8] * e[3]);
    float g = (float)((long double)e[6] * e[4]);
    float inv;
    Matrix t;

    inv = (float)(1.0f / (c * e[0] + a * e[2] + b * e[1] - d * e[0] - (long double)f * e[1] - (long double)g * e[2]));
    t.m[0] = (float)((c - d) * inv);
    t.m[1] = (float)-((f - b) * inv);
    t.m[2] = (float)((a - g) * inv);
    t.m[3] = (float)-(((long double)e[8] * e[1] - (long double)e[2] * e[7]) * inv);
    t.m[4] = (float)(((long double)e[8] * e[0] - (long double)e[6] * e[2]) * inv);
    t.m[5] = (float)-(((long double)e[0] * e[7] - (long double)e[6] * e[1]) * inv);
    t.m[6] = (float)(((long double)e[5] * e[1] - (long double)e[2] * e[4]) * inv);
    t.m[7] = (float)-(((long double)e[5] * e[0] - (long double)e[3] * e[2]) * inv);
    t.m[8] = (float)(((long double)e[4] * e[0] - (long double)e[3] * e[1]) * inv);
    t.unk24 = m->unk24 + 1;
    *out = t;
}
HOOK(0x4B4390, Matrix_Invert);
