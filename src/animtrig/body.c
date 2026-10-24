#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "animtrig/animtrig.h"
#include "graphtask/graphtasktypes.h"

ORIG(0x4B2A10, long double, Vec3_FastLength, (const float *v));
ORIG(0x4B3C20, void, Matrix_SetScale, (float *m, float x, float y, float z));

/* a lifting surface: area, normal, position */
typedef struct AeroPart {
    float area;
    float normal[3];
    float pos[3];
} AeroPart;

_Static_assert(sizeof(AeroPart) == 0x1C, "AeroPart");

static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kHalf = 0.5f;
static const float kTwelfth = 0x1.555556p-4f;
static const double kOneD = 1.0;

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

/* v scaled to length 1, unless it is 0 */
static void normalize(float *out, float x, float y, float z)
{
    long double s;

    out[0] = x;
    out[1] = y;
    out[2] = z;
    if (x != kZero || y != kZero || z != kZero) {
        s = kOneD / x87_sqrt((long double)x * x + (long double)y * y + (long double)z * z);
        out[0] = x * s;
        out[1] = y * s;
        out[2] = z * s;
    }
}

void Body_AddForce(Body *b, BodyForce *f);

/* each part pushes along its normal by speed^2 * k * area * cos(angle) */
void Body_AddAeroForces(AeroPart *parts, int count, float k, Body *body, const float *m, const float *v)
{
    float lv[3], dir[3];
    float q;

    lv[0] = ((long double)m[6] * v[2] + (long double)m[3] * v[1]) + (long double)m[0] * v[0];
    lv[1] = ((long double)m[7] * v[2] + (long double)m[1] * v[0]) + (long double)m[4] * v[1];
    lv[2] = ((long double)m[8] * v[2] + (long double)m[2] * v[0]) + (long double)m[5] * v[1];
    {
        long double len = Vec3_FastLength(lv);

        q = len * len * k;
    }
    if (!(q > kZero))
        return;
    normalize(dir, lv[0], lv[1], lv[2]);
    for (; count > 0; count--, parts++) {
        BodyForce f;
        long double s;

        if (!(parts->area > kZero))
            continue;
        s = ((long double)dir[1] * parts->normal[1] + (long double)dir[2] * parts->normal[2]) +
            (long double)dir[0] * parts->normal[0];
        s = s * parts->area * q;
        f.force[0] = parts->normal[0] * s;
        f.force[1] = parts->normal[1] * s;
        f.force[2] = parts->normal[2] * s;
        memcpy(f.point, parts->pos, sizeof(f.point));
        Body_AddForce(body, &f);
    }
}
HOOK(0x4EC2C0, Body_AddAeroForces);

void AeroPart_Init(AeroPart *p, float area, float x, float y, float z, float nx, float ny, float nz)
{
    normalize(p->normal, nx, ny, nz);
    p->pos[0] = x;
    p->pos[1] = y;
    p->pos[2] = z;
    p->area = area;
}
HOOK(0x4EC4D0, AeroPart_Init);

void Body_AllocForces(Body *b, int count)
{
    b->maxForces = count;
    if (count > 0) {
        b->forces = Mem_Alloc(count * sizeof(BodyForce), 4);
        return;
    }
    b->forces = NULL;
}
HOOK(0x4EC5B0, Body_AllocForces);

void Body_Reset(Body *b);

void Body_SetMass(Body *b, float mass, float ix, float iy, float iz)
{
    b->mass = mass;
    b->invInertia[0] = kOne / ix;
    b->invInertia[1] = kOne / iy;
    b->invInertia[2] = kOne / iz;
    b->invMass = kOne / mass;
    Body_Reset(b);
}
HOOK(0x4EC5E0, Body_SetMass);

void Body_Free(Body *b)
{
    if (b->forces) {
        Mem_Free(b->forces);
        b->forces = NULL;
        memset(b, 0, sizeof(*b));
    }
}
HOOK(0x4EC630, Body_Free);

void Body_Reset(Body *b)
{
    memset(&b->vel, 0, sizeof(b->vel));
    memset(&b->dir, 0, sizeof(b->dir));
    memset(&b->angMom, 0, sizeof(b->angMom));
    memset(&b->force, 0, sizeof(b->force));
    memset(&b->angVel, 0, sizeof(b->angVel));
    b->speed = 0;
    b->forceCount = 0;
    b->lastForceCount = 0;
}
HOOK(0x4EC660, Body_Reset);

void Body_Update(Body *b);

/* Applies the collected forces and turns the matrix by the angular velocity
   through a quaternion. Returns 1 when the body rotates. */
int Body_Integrate(Body *b, float *m)
{
    static const int next[3] = {1, 2, 0};
    float q[4];
    int turned = 0;
    int n;

    {
        long double tr = ((long double)m[4] + m[0]) + m[8];

        if (tr > kZero) {
            long double s = x87_sqrt(tr + kOne);

            q[3] = s * kHalf;
            s = kHalf / s;
            q[0] = -(((long double)m[5] - m[7]) * s);
            q[1] = -(((long double)m[6] - m[2]) * s);
            q[2] = -(((long double)m[1] - m[3]) * s);
        } else {
            int i = 0, j, k;
            long double s;

            if (m[4] > m[0])
                i = 1;
            if (m[8] > m[i * 4])
                i = 2;
            j = next[i];
            k = next[j];
            s = x87_sqrt((((long double)m[i * 4] - m[j * 4]) - m[k * 4]) + kOne);
            q[i] = s * kHalf;
            if (s != kZero)
                s = kHalf / s;
            q[j] = ((long double)m[j * 3 + i] + m[i * 3 + j]) * s;
            q[k] = ((long double)m[k * 3 + i] + m[i * 3 + k]) * s;
            q[3] = -(((long double)m[j * 3 + k] - m[k * 3 + j]) * s);
        }
    }

    n = b->forceCount;
    b->lastForceCount = n;
    if (n > 0) {
        BodyForce *f = b->forces;
        long double fx = 0, fy = 0, tx = 0, ty = 0, tz = 0;
        float fz = 0, ax, ay, dvx, dvy, sty, stz;
        long double az, dvz;

        for (; n; n--, f++) {
            const float *r = f->force, *p = f->point;

            fx += r[0];
            fy += r[1];
            fz = fz + r[2];
            if ((p[0] != kZero || p[1] != kZero || p[2] != kZero) &&
                (r[0] != kZero || r[1] != kZero || r[2] != kZero)) {
                float t1, t2;

                t1 = -((long double)r[2] * p[0] - (long double)r[0] * p[2]);
                t2 = (long double)r[1] * p[0] - (long double)r[0] * p[1];
                tx += (long double)r[2] * p[1] - (long double)p[2] * r[1];
                ty += t1;
                tz += t2;
            }
        }
        b->force.x = fx;
        b->force.y = fy;
        b->force.z = fz;
        sty = ty;
        stz = tz;
        ax = (long double)b->invMass * b->force.x;
        ay = (long double)b->invMass * b->force.y;
        az = (long double)b->invMass * fz;
        dvx = (az * m[2] + (long double)ay * m[1]) + (long double)ax * m[0];
        dvy = (az * m[5] + (long double)ax * m[3]) + (long double)ay * m[4];
        dvz = ((long double)ay * m[7] + (long double)ax * m[6]) + az * m[8];
        b->forceCount = 0;
        b->vel.x = dvx + b->vel.x;
        b->vel.y = dvy + b->vel.y;
        b->vel.z = dvz + b->vel.z;
        b->angMom.x = tx + b->angMom.x;
        b->angMom.y = sty + b->angMom.y;
        b->angMom.z = stz + b->angMom.z;
    }

    if (b->angMom.x != kZero || b->angMom.y != kZero || b->angMom.z != kZero) {
        float s[10], w[3];
        const float *L = &b->angMom.x;
        long double A, B, C, E, F, J, K, W, U, V, inv;
        float cf, t24, t28, t30, t34, t38, t4c, t50, t54, t58, t5c, t6c;

        Matrix_SetScale(s, b->invInertia[0], b->invInertia[1], b->invInertia[2]);
        b->angVel.x = ((long double)s[2] * L[2] + (long double)s[1] * L[1]) + (long double)s[0] * L[0];
        b->angVel.y = ((long double)s[5] * L[2] + (long double)s[4] * L[1]) + (long double)s[3] * L[0];
        b->angVel.z = ((long double)s[8] * L[2] + (long double)s[7] * L[1]) + (long double)s[6] * L[0];
        memcpy(w, &b->angVel, sizeof(w));
        b->angVel.x = ((long double)w[1] * m[1] + (long double)m[0] * w[0]) + (long double)w[2] * m[2];
        b->angVel.y = ((long double)m[3] * w[0] + (long double)w[2] * m[5]) + (long double)w[1] * m[4];
        b->angVel.z = ((long double)m[7] * w[1] + (long double)w[2] * m[8]) + (long double)w[0] * m[6];

        /* q += 0.5 * (0, w) * q, then normalized twice */
        A = (long double)b->angVel.x * kHalf;
        B = (long double)b->angVel.y * kHalf;
        C = (long double)b->angVel.z * kHalf;
        cf = C;
        t58 = (C - B) * ((long double)q[1] - q[2]);
        t28 = ((long double)q[2] + q[1]) * -A;
        t5c = (cf + B) * ((long double)q[3] - q[0]);
        t54 = (cf + A) * ((long double)q[1] + q[0]);
        t50 = ((long double)q[3] - q[2]) * B;
        E = ((long double)q[2] + q[3]) * -B;
        t4c = E;
        F = (E + t50) + t54;
        t24 = ((cf - A) * ((long double)q[0] - q[1]) + F) * kHalf;
        J = (((long double)q[0] + q[3]) * A + t24) - F;
        t30 = ((long double)t24 + t28) - t4c;
        t34 = ((long double)t24 + t5c) - t50;
        t38 = ((long double)t24 + t58) - t54;
        q[0] = J + q[0];
        q[1] = (long double)t30 + q[1];
        q[2] = (long double)t34 + q[2];
        K = (long double)t38 + q[3];

        inv = kOneD / x87_sqrt((((long double)q[2] * q[2] + (long double)q[1] * q[1]) + (long double)q[0] * q[0]) +
                               K * K);
        q[0] = q[0] * inv;
        q[1] = q[1] * inv;
        q[2] = q[2] * inv;
        K = K * inv;
        inv = kOneD / x87_sqrt((((long double)q[2] * q[2] + (long double)q[1] * q[1]) + (long double)q[0] * q[0]) +
                               K * K);
        q[0] = q[0] * inv;
        q[1] = q[1] * inv;
        q[2] = q[2] * inv;
        W = K * inv;

        t6c = 2 * ((long double)q[0] * q[0]);
        t54 = 2 * ((long double)q[1] * q[0]);
        t4c = 2 * ((long double)q[2] * q[0]);
        t5c = 2 * (q[0] * W);
        t28 = 2 * ((long double)q[1] * q[1]);
        t58 = 2 * ((long double)q[2] * q[1]);
        t24 = 2 * (q[1] * W);
        t50 = 2 * ((long double)q[2] * q[2]);
        U = 2 * (W * q[2]);
        m[9] = 0;
        turned = 1;
        m[0] = ((long double)kOne - t28) - t50;
        m[1] = t54 - U;
        m[2] = (long double)t24 + t4c;
        m[3] = U + t54;
        V = (long double)kOne - t6c;
        m[4] = V - t50;
        m[5] = (long double)t58 - t5c;
        m[6] = (long double)t4c - t24;
        m[7] = (long double)t58 + t5c;
        m[8] = V - t28;
    }
    Body_Update(b);
    return turned;
}
HOOK(0x4EC6A0, Body_Integrate);

/* speed and direction from the velocity */
void Body_Update(Body *b)
{
    float v[3];
    long double len;

    memcpy(v, &b->vel, sizeof(v));
    len = Vec3_FastLength(v);
    b->speed = len;
    if (len > kZero) {
        normalize(&b->dir.x, v[0], v[1], v[2]);
    } else {
        b->dir.x = 0;
        b->dir.y = 0;
        b->dir.z = 0;
    }
}
HOOK(0x4ECDB0, Body_Update);

void Body_SetBox(Body *b, float mass, float x, float y, float z);

void Body_Init(Body *b, float mass, float x, float y, float z, int maxForces)
{
    Body_SetBox(b, mass, x, y, z);
    Body_AllocForces(b, maxForces);
}
HOOK(0x4ECEA0, Body_Init);

/* inertia of a solid box with sides x, y, z */
void Body_SetBox(Body *b, float mass, float x, float y, float z)
{
    long double xx = (long double)x * x;
    float yy = (long double)y * y;
    float zz = (long double)z * z;
    float ix = ((long double)zz + yy) * mass * kTwelfth;
    float iy = (zz + xx) * mass * kTwelfth;
    float iz = (yy + xx) * mass * kTwelfth;

    Body_SetMass(b, mass, ix, iy, iz);
}
HOOK(0x4ECED0, Body_SetBox);

void Body_AddForce(Body *b, BodyForce *f)
{
    b->forces[b->forceCount] = *f;
    b->forceCount++;
}
HOOK(0x4ECF50, Body_AddForce);

/* velocity of a point (object space) of the body */
void Body_GetPointVelocity(float *out, Body *b, const float *p)
{
    float r[3];

    r[0] = (long double)p[2] * b->angVel.y - (long double)b->angVel.z * p[1];
    r[1] = -((long double)p[2] * b->angVel.x - (long double)b->angVel.z * p[0]);
    r[2] = (long double)b->angVel.x * p[1] - (long double)p[0] * b->angVel.y;
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
    out[0] = (long double)b->vel.x + out[0];
    out[1] = (long double)b->vel.y + out[1];
    out[2] = (long double)b->vel.z + out[2];
}
HOOK(0x4ECF80, Body_GetPointVelocity);
