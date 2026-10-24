#include "hook.h"
#include "igi.h"

/*
 * The original keeps intermediate results on the x87 stack, so they are
 * long double here. Comparisons are written so that NaN takes the same
 * branch as in the original.
 */

ORIG(0x516824, double, crt_acos, (double x));
ORIG(0x5168F4, double, crt_asin, (double x));

#define EPS 1e-5f
#define ONE_EPS 1.00001f

static inline long double fsqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

static inline long double fchs(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

static inline long double fpatan(long double y, long double x)
{
    long double r;

    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

double Math_ACosF(float x)
{
    double d = x;

    if (!(d >= -1.0))
        return crt_acos(-1.0);
    if (d > 1.0)
        d = 1.0;
    return crt_acos(d);
}
HOOK(0x4B2760, Math_ACosF);

double Math_ASinF(float x)
{
    double d = x;

    if (!(d >= -1.0))
        return crt_asin(-1.0);
    if (d > 1.0)
        d = 1.0;
    return crt_asin(d);
}
HOOK(0x4B27D0, Math_ASinF);

double Math_ASin(double x)
{
    if (!(x >= -1.0))
        return crt_asin(-1.0);
    if (x > 1.0)
        x = 1.0;
    return crt_asin(x);
}
HOOK(0x4B2840, Math_ASin);

double Math_ACos(double x)
{
    if (!(x >= -1.0))
        return crt_acos(-1.0);
    if (x > 1.0)
        x = 1.0;
    return crt_acos(x);
}
HOOK(0x4B28A0, Math_ACos);

long double Vec2_Length(const float *v);

void Vec2_Normalize(float *v)
{
    long double s = 1.0L / Vec2_Length(v);

    v[0] = v[0] * s;
    v[1] = v[1] * s;
}
HOOK(0x4B2900, Vec2_Normalize);

long double Vec2_Length(const float *v)
{
    long double x = v[0], y = v[1];

    return fsqrt(x * x + y * y);
}
HOOK(0x4B2930, Vec2_Length);

long double Vec2_Distance(const float *a, const float *b)
{
    long double dx = (long double)a[0] - b[0];
    long double dy = (long double)a[1] - b[1];

    return fsqrt(dy * dy + dx * dx);
}
HOOK(0x4B2950, Vec2_Distance);

long double Vec2_Dot(const float *a, const float *b)
{
    return (long double)a[1] * b[1] + (long double)a[0] * b[0];
}
HOOK(0x4B2980, Vec2_Dot);

long double Vec2d_Length(const double *v);

void Vec2d_Normalize(double *v)
{
    long double s = 1.0L / Vec2d_Length(v);

    v[0] = s * v[0];
    v[1] = s * v[1];
}
HOOK(0x4B29A0, Vec2d_Normalize);

long double Vec2d_Length(const double *v)
{
    long double x = v[0], y = v[1];

    return fsqrt(x * x + y * y);
}
HOOK(0x4B29D0, Vec2d_Length);

long double Vec2d_Dot(const double *a, const double *b)
{
    return (long double)a[1] * b[1] + (long double)a[0] * b[0];
}
HOOK(0x4B29F0, Vec2d_Dot);

/* Length estimate: max + 0.414 * mid + 0.318 * min. */
long double Vec3_FastLength(const float *v)
{
    float x = v[0], y = v[1], z = v[2];
    float max, mid, min;

    if (!(x >= 0.0f))
        x = -x;
    if (!(y >= 0.0f))
        y = -y;
    if (!(z >= 0.0f))
        z = -z;
    if (x > y && x > z) {
        max = x;
        if (y > z) {
            mid = y;
            min = z;
        } else {
            mid = z;
            min = y;
        }
    } else if (y > z) {
        max = y;
        if (x > z) {
            mid = x;
            min = z;
        } else {
            mid = z;
            min = x;
        }
    } else {
        max = z;
        if (x > y) {
            mid = x;
            min = y;
        } else {
            mid = y;
            min = x;
        }
    }
    return (long double)min * 0.31783724f + (long double)mid * 0.41421357f + max;
}
HOOK(0x4B2A10, Vec3_FastLength);

/* True for 0 and NaN, like the original's equality tests. */
static inline int IsZero(long double x)
{
    return !(x < 0 || x > 0);
}

double Vec3_Angle(const float *a, const float *b)
{
    long double dot, la, lb;
    volatile double c;

    if (IsZero(a[0]) && IsZero(a[1]) && IsZero(a[2]))
        return 0.0f;
    if (IsZero(b[0]) && IsZero(b[1]) && IsZero(b[2]))
        return 0.0f;
    dot = (long double)a[2] * b[2] + (long double)a[1] * b[1] + (long double)a[0] * b[0];
    la = fsqrt((long double)a[0] * a[0] + (long double)a[1] * a[1] + (long double)a[2] * a[2]);
    lb = fsqrt((long double)b[0] * b[0] + (long double)b[1] * b[1] + (long double)b[2] * b[2]);
    c = dot / (la * lb);
    if (c >= 1.0)
        return 0.0f;
    if (!(c > -1.0))
        return 3.14159265f;
    return crt_acos(c);
}
HOOK(0x4B2B30, Vec3_Angle);

/* Moves p along dir onto the plane through the origin with the given normal. */
int Vec3_RayPlane(float *out, const float *p, const float *dir, const float *normal)
{
    long double d = (long double)dir[1] * normal[1] + (long double)dir[0] * normal[0]
                  + (long double)normal[2] * dir[2];
    long double t;

    if (d <= EPS && d >= -EPS) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
        return 0;
    }
    t = ((long double)p[2] * normal[2] + (long double)p[0] * normal[0]
         + (long double)p[1] * normal[1]) / d;
    out[0] = p[0] - t * dir[0];
    out[1] = p[1] - t * dir[1];
    out[2] = p[2] - t * dir[2];
    return 1;
}
HOOK(0x4B2C70, Vec3_RayPlane);

void Vec3_ToAngles(const float *v, float *pitch, float *yaw)
{
    long double len;

    if (IsZero(v[0]))
        *yaw = 0.0f;
    else
        *yaw = fpatan(v[1], v[0]);
    len = fsqrt((long double)v[0] * v[0] + (long double)v[1] * v[1] + (long double)v[2] * v[2]);
    if (IsZero(len))
        *pitch = 0.0f;
    else
        *pitch = fchs(Math_ASinF(v[2] / len));
}
HOOK(0x4B2D00, Vec3_ToAngles);

/*
 * Solves p = s * e1 + t * e2 in the plane of the two axes that are not the
 * largest axis of normal. Returns 1 when p is inside the triangle
 * (s >= 0, t >= 0, s + t <= 1, with a small tolerance).
 */
char Vec3_TriangleCoords(float *s_out, float *t_out, const float *p, const float *e1,
                         const float *e2, const float *normal)
{
    float ax = normal[0], ay = normal[1], az = normal[2];
    long double d, s, t, au, av;
    char inside = 1;
    int u, v;

    if (!(ax >= 0.0f))
        ax = -ax;
    if (!(ay >= 0.0f))
        ay = -ay;
    if (!(az >= 0.0f))
        az = -az;
    if (ay > ax && ay > az) {
        u = 0;
        v = 2;
    } else if (az > ax) {
        u = 0;
        v = 1;
    } else {
        u = 1;
        v = 2;
    }
    d = (long double)e1[u] * e2[v] - (long double)e2[u] * e1[v];
    if (IsZero(d))
        inside = 0;
    t = ((long double)e1[u] * p[v] - (long double)p[u] * e1[v]) / d;
    if (!(t >= -EPS) || t > ONE_EPS)
        inside = 0;
    au = e1[u];
    if (!(au >= 0))
        au = -au;
    av = e1[v];
    if (!(av >= 0))
        av = -av;
    if (au > av) {
        s = (p[u] - t * e2[u]) / e1[u];
    } else {
        if (IsZero(e1[v]))
            inside = 0;
        s = (p[v] - t * e2[v]) / e1[v];
    }
    if (!(s >= -EPS) || s + t > ONE_EPS)
        inside = 0;
    if (s_out)
        *s_out = s;
    if (t_out)
        *t_out = t;
    return inside;
}
HOOK(0x4B2D90, Vec3_TriangleCoords);
