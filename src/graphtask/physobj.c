/* "PhysicsObj": the rigid body objects (vehicles, crates, grenades...) and
   their contact points with the world. */
#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "igi.h"
#include "math/x87.h"
#include "graphtask/graphtask.h"
#include "material/materialtypes.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x42DA70, void, Rotor_FreeDef, (void *task, void *arg));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B3C20, void, Matrix_SetScale, (float *m, float x, float y, float z));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (void *world, CollHit *hits, Vec3d *points, int count, double *dz,
                                          int event, void *msg, int flags));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, Vec3d *starts, Vec3d *ends, int count, int event,
                                  void *msg, int flags));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D09E0, void, Mesh3D_GetModelVertex, (float *out, void *obj, int index, const char *model));
ORIG(0x4EC630, void, Body_Free, (Body *b));
ORIG(0x4EC6A0, int, Body_Integrate, (Body *b, Matrix *m));
ORIG(0x4ECDB0, void, Body_Update, (Body *b));
ORIG(0x4ECEA0, void, Body_Init, (Body *b, float mass, float x, float y, float z, int maxForces));
ORIG(0x4ECED0, void, Body_SetBox, (Body *b, float mass, float x, float y, float z));
ORIG(0x4ECF50, void, Body_AddForce, (Body *b, const float *force));
ORIG(0x4ECF80, void, Body_GetPointVelocity, (float *out, Body *b, const float *p));
ORIG(0x4EE100, uint8_t, PhysicsObjType_GetFreeEvent, (void));
ORIG(0x4EE1B0, float *, PhysicsObjType_GetSize, (PhysType *t));
ORIG(0x4F4390, void, Vehicle_CollFilter, (void *task, void *arg));

extern uint16_t gPhysicsObjType;
extern int gPhysicsObjUnkA774B8;
extern uint8_t gPhysicsObjEvents[];

F32(kOne, 0x3F800000);
F32(kMinusOne, 0xBF800000);
F32(kQuarter, 0x3E800000);
F32(kMeter, 0x45800000);           /* 4096 */
F32(kInertiaScale, 0x3F8147AE);    /* 1.01 */
F32(kSlop, 0x4223D70A);            /* 40.96 */
F32(kFrictionMin, 0x3727C5AC);     /* 1e-5 */
F32(kDefMass, 0x447A0000);         /* 1000 */
F32(kDefX, 0x46133333);
F32(kDefY, 0x469CCCCD);
F32(kDefZ, 0x45C00000);
F64(kOneD, 0x3FF0000000000000ull);
F64(kDropDepth, 0x40D4000000000000ull); /* 20480 */

void PhysicsObj_CreateHandler(void);
void PhysicsObj_DeleteHandler(void);
void PhysicsObj_RunHandler(void);
void PhysicsObj_GetPointMotion(PhysObj *o, PhysPointMsg *msg);
uint8_t PhysicsObj_GetMotionEvent(void);

void PhysicsObj_Open(void)
{
    gPhysicsObjType = TaskType_New("PhysicsObj", sizeof(PhysObj), RigidDynCubeObj_GetType(), 0, 0);
    gPhysicsObjEvents[0] = TaskEvent_Alloc(0);
    gPhysicsObjEvents[1] = TaskEvent_Alloc(0);
    gPhysicsObjEvents[2] = TaskEvent_Alloc(0);
    TaskType_SetHandler(gPhysicsObjType, 0, PhysicsObj_CreateHandler);
    TaskType_SetHandler2(gPhysicsObjType, 2, PhysicsObj_DeleteHandler);
    TaskType_SetHandler2(gPhysicsObjType, 1, PhysicsObj_RunHandler);
    TaskType_SetHandler(gPhysicsObjType, 12, Vehicle_CollFilter);
    TaskType_SetHandler(gPhysicsObjType, PhysicsObj_GetMotionEvent(), PhysicsObj_GetPointMotion);
    TaskType_SetHandler(gPhysicsObjType, PhysicsObjType_GetFreeEvent(), Rotor_FreeDef);
    gPhysicsObjUnkA774B8 = 0;
}
HOOK(0x4FB1B0, PhysicsObj_Open);

void PhysicsObj_CreateHandler(void)
{
    Error_Show("PhysicsObj create handler must be overloaded");
    for (;;);
}
HOOK(0x4FB290, PhysicsObj_CreateHandler);

void PhysicsObj_DeleteHandler(void)
{
    Error_Show("PhysicsObj delete handler must be overloaded");
    for (;;);
}
HOOK(0x4FB2A0, PhysicsObj_DeleteHandler);

void PhysicsObj_RunHandler(void)
{
    Error_Show("PhysicsObj run handler must be overloaded");
    for (;;);
}
HOOK(0x4FB2B0, PhysicsObj_RunHandler);

/* how a point carried by the object moved since the last frame */
void PhysicsObj_GetPointMotion(PhysObj *o, PhysPointMsg *msg)
{
    const float *L = o->lastMatrix.m, *M = o->matrix.m;
    long double a = o->pos.x - o->lastPos.x;
    double b = o->pos.y - o->lastPos.y;
    double c = o->pos.z - o->lastPos.z;
    Vec3d *p = msg->point;
    long double d = p->x - o->lastPos.x;
    double e = p->y - o->lastPos.y;
    double f = p->z - o->lastPos.z;
    long double x = (L[3] * e + L[6] * f) + L[0] * d;
    long double y = (L[4] * e + L[7] * f) + L[1] * d;
    long double z = (L[5] * e + L[8] * f) + L[2] * d;
    double r0 = (M[1] * y + M[2] * z) + M[0] * x;
    double r1 = (M[4] * y + M[5] * z) + M[3] * x;
    double r2 = (M[7] * y + M[8] * z) + M[6] * x;
    Matrix t;
    int i, j;

    msg->move.x = (r0 + a) - d;
    msg->move.y = (r1 + b) - e;
    msg->move.z = (r2 + c) - f;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            t.m[i * 3 + j] = (M[i * 3 + 1] * L[j * 3 + 1] + M[i * 3 + 2] * L[j * 3 + 2]) + M[i * 3] * L[j * 3];
    t.count = o->matrix.count + 1 + o->lastMatrix.count;
    msg->turn = t;
}
HOOK(0x4FB2C0, PhysicsObj_GetPointMotion);

void PhysicsObj_Close(void)
{
    TaskEvent_Free(gPhysicsObjEvents[0]);
    TaskEvent_Free(gPhysicsObjEvents[1]);
    TaskEvent_Free(gPhysicsObjEvents[2]);
    TaskType_Free(gPhysicsObjType);
}
HOOK(0x4FB5A0, PhysicsObj_Close);

uint16_t PhysicsObj_GetType(void)
{
    return gPhysicsObjType;
}
HOOK(0x4FB5E0, PhysicsObj_GetType);

void PhysicsObj_Init(PhysObj *o)
{
    int i;

    o->startPos.x = 0;
    o->startPos.y = 0;
    o->startPos.z = 0;
    o->startVel.x = 0;
    o->startVel.y = 0;
    o->startVel.z = 0;
    for (i = 0; i < 9; i++)
        o->startMatrix.m[i] = i % 4 ? 0.0f : kOne.f;
    o->startMatrix.count = 0;
    o->phys = NULL;
    o->touching = 0;
    o->contactCount = 0;
    o->unk690 = 0;
    o->collide = 1;
    o->upright = 0;
    for (i = 0; i < PHYS_CONTACTS; i++)
        o->contacts[i].enabled = 0;
    Body_Init(&o->body, kDefMass.f, kDefX.f, kDefY.f, kDefZ.f, 32);
    o->lastPos = o->pos;
    o->lastMatrix = o->matrix;
}
HOOK(0x4FB5F0, PhysicsObj_Init);

/* sets the physics type: box inertia from its size and mass, contact points
   from its joints; NULL gives the default box without contacts */
void PhysicsObj_SetPhysType(PhysObj *o, PhysType *phys)
{
    int i;

    if (!phys) {
        Body_SetBox(&o->body, kDefMass.f, kDefX.f, kDefY.f, kDefZ.f);
        o->contactCount = 0;
    } else {
        float x, y, z;

        o->phys = phys;
        z = PhysicsObjType_GetSize(phys)[2] * kMeter.f;
        y = PhysicsObjType_GetSize(o->phys)[1] * kMeter.f;
        x = PhysicsObjType_GetSize(o->phys)[0] * kMeter.f;
        Body_SetBox(&o->body, o->phys->mass, x, y, z);
        for (i = 0; i < 3; i++)
            o->body.invInertia[i] = o->body.invInertia[i] * kInertiaScale.f;
        o->contactCount = o->phys->jointCount;
        for (i = 0; i < PHYS_CONTACTS; i++)
            o->contacts[i].enabled = 0;
        for (i = 0; i < o->contactCount; i++) {
            Contact *c = &o->contacts[i];
            PhysJoint *j = &o->phys->joints[i];
            float p[3];

            c->unk4 = j->unk8;
            c->side = j->unk4;
            c->bounce = j->bounce;
            c->active = 1;
            c->enabled = 1;
            c->touching = 0;
            c->unk1C = j->unk10;
            c->friction = j->friction;
            c->height = 0;
            Mesh3D_GetModelVertex(p, o, o->phys->joints[i].point, phys->model);
            c->point.x = p[0];
            c->point.y = p[1];
            c->point.z = p[2];
        }
    }
    o->lastPos = o->pos;
    o->move.x = 0;
    o->lastMatrix = o->matrix;
    o->move.y = 0;
    o->move.z = 0;
    o->touching = 0;
    o->unk690 = 0;
    for (i = 0; i < 9; i++)
        o->turn.m[i] = i % 4 ? 0.0f : kOne.f;
    o->turn.count = 0;
}
HOOK(0x4FB700, PhysicsObj_SetPhysType);

void PhysicsObj_Free(PhysObj *o)
{
    o->phys = NULL;
    Body_Free(&o->body);
}
HOOK(0x4FB980, PhysicsObj_Free);

int Contact_Classify(const float *n, Body *b, const float *r, const Matrix *m, float *dot);
void PhysicsObj_ContactImpulse(PhysObj *o, const float *n, const float *r, const float *dot, float unused, int k,
                               int unused2);
void PhysicsObj_ContactFriction(PhysObj *o, void *world, const float *n, float push, int k, const float *r,
                                float damp);
uint8_t PhysicsObj_GetHitEvent(void);

/* traces each active contact point along its velocity; the first one that
   hits something solid gets an impulse and friction, then it returns 2 */
int PhysicsObj_CollideContacts(PhysObj *o, float push, float damp, void *world, int event, void *msg)
{
    Vec3f r[PHYS_CONTACTS], vel[PHYS_CONTACTS];
    Vec3d from[PHYS_CONTACTS], to[PHYS_CONTACTS];
    CollHit hits[PHYS_CONTACTS];
    int idx[PHYS_CONTACTS];
    const float *M = o->matrix.m;
    int n = 0, i, j, ignored = 0;

    if (!o->contactCount)
        return 0;
    for (i = 0; i < PHYS_CONTACTS; i++) {
        Contact *c = &o->contacts[i];
        float x, y, z;

        hits[i].found = 0;
        if (!c->enabled || !c->active)
            continue;
        c->touching = 0;
        r[n] = c->point;
        x = (M[2] * r[n].z + M[1] * r[n].y) + M[0] * r[n].x;
        y = (M[5] * r[n].z + M[4] * r[n].y) + M[3] * r[n].x;
        z = (M[8] * r[n].z + M[7] * r[n].y) + M[6] * r[n].x;
        r[n].x = x;
        r[n].y = y;
        r[n].z = z;
        from[n].x = r[n].x;
        from[n].y = r[n].y;
        from[n].z = r[n].z;
        from[n].x = o->pos.x + from[n].x;
        from[n].y = o->pos.y + from[n].y;
        from[n].z = o->pos.z + from[n].z;
        Body_GetPointVelocity(&vel[n].x, &o->body, &r[n].x);
        to[n].x = vel[n].x + from[n].x;
        idx[n] = i;
        to[n].y = vel[n].y + from[n].y;
        to[n].z = vel[n].z + from[n].z;
        n++;
    }
    if (!n)
        return 0;
    o->touching = 0;
    Coll_Lines(world, hits, from, to, n, event, msg, -1);
    for (j = 0; j < n; j++) {
        CollHit *h = &hits[j];
        Contact *c;
        float dot;
        int hit = 0, k;

        if (!h->found)
            continue;
        if (h->back)
            hit = NONZERO(h->back->bounce);
        if (h->task) {
            PhysHitMsg m = {o, h, 0};
            TaskFn fn = HANDLER(PhysicsObj_GetHitEvent(), h->task->type);

            if (fn)
                fn(h->task, &m);
            ignored = m.result;
            if (m.result) {
                h->found = 0;
                continue;
            }
        }
        if (!hit) {
            Vec3f *v = &vel[j];

            hit = x87_sqrt(((long double)v->x * v->x + (long double)v->z * v->z) + (long double)v->y * v->y)
                + kSlop.f > h->dist;
        }
        h->found = hit;
        if (!hit)
            continue;
        k = idx[j];
        c = &o->contacts[k];
        c->dist = h->dist;
        if (NONZERO(h->normal.x) || NONZERO(h->normal.y) || NONZERO(h->normal.z)) {
            Vec3f *v = &h->normal;
            long double inv = kOneD.d / x87_sqrt(((long double)v->x * v->x + (long double)v->y * v->y)
                                                 + (long double)v->z * v->z);

            v->x = v->x * inv;
            v->y = v->y * inv;
            v->z = v->z * inv;
        }
        c->normal = h->normal;
        if (Contact_Classify(&h->normal.x, &o->body, &r[j].x, &o->matrix, &dot) != 2)
            continue;
        o->touching++;
        c->touching = 1;
        PhysicsObj_ContactImpulse(o, &h->normal.x, &r[j].x, &dot, push, k, 1);
        PhysicsObj_ContactFriction(o, world, &h->normal.x, push, k, &r[j].x, damp);
        if (ignored) {
            o->body.vel.x = o->body.vel.x * kQuarter.f;
            o->body.vel.y = o->body.vel.y * kQuarter.f;
            o->body.vel.z = o->body.vel.z * kQuarter.f;
            Body_Update(&o->body);
        }
        return 2;
    }
    return 0;
}
HOOK(0x4FB9A0, PhysicsObj_CollideContacts);

void Body_ApplyImpulse(Body *b, const float *M, const float *n, const float *r, float dot, float unused,
                       float bounce);
void Body_ApplyFriction(Body *b, const float *n, float push, float mu, float damp);

void PhysicsObj_ContactImpulse(PhysObj *o, const float *n, const float *r, const float *dot, float unused, int k,
                               int unused2)
{
    Body_ApplyImpulse(&o->body, o->matrix.m, n, r, *dot, unused, o->contacts[k].bounce);
}
HOOK(0x4FBF00, PhysicsObj_ContactImpulse);

void PhysicsObj_ContactFriction(PhysObj *o, void *world, const float *n, float push, int k, const float *r,
                                float damp)
{
    Body_ApplyFriction(&o->body, n, push, o->contacts[k].friction, damp);
}
HOOK(0x4FBF50, PhysicsObj_ContactFriction);

/* gravity (push * mass along the local z axis), one integration step, then damping */
int PhysicsObj_Integrate(PhysObj *o, float push, float damp, float spinDamp)
{
    float f[6];
    long double g = -(push * o->phys->mass);
    int turned;

    f[3] = 0;
    f[4] = 0;
    f[5] = 0;
    f[0] = g * o->matrix.m[6];
    f[1] = g * o->matrix.m[7];
    f[2] = g * o->matrix.m[8];
    Body_AddForce(&o->body, f);
    turned = Body_Integrate(&o->body, &o->matrix);
    o->body.angMom.x = o->body.angMom.x * spinDamp;
    o->body.angMom.y = o->body.angMom.y * spinDamp;
    o->body.angMom.z = o->body.angMom.z * spinDamp;
    o->body.vel.x = o->body.vel.x * damp;
    o->body.vel.y = o->body.vel.y * damp;
    o->body.vel.z = o->body.vel.z * damp;
    Body_Update(&o->body);
    return turned;
}
HOOK(0x4FBF90, PhysicsObj_Integrate);

int PhysicsObj_CheckContacts(PhysObj *o, void *world, int event, void *msg);

static float PhysicsObj_MaxScale(const PhysObj *o)
{
    float m = o->scale[2] < o->scale[1] ? o->scale[1] : o->scale[2];

    return o->scale[0] > m ? o->scale[0] : m;
}

/* one physics step: move, collide the contacts, move in the dyncube tree
   (back to the last place when a contact ends up inside something) and
   remember the motion of this frame */
void PhysicsObj_Collide(PhysObj *o, float push, float damp, float spinDamp, void *world, int event, void *msg)
{
    /* order[i] lists the k of turn[i] = sum M[row][k] * T[k][col] in adding order */
    static const char order[9][3] = {{2, 0, 1}, {2, 0, 1}, {1, 2, 0}, {0, 2, 1}, {2, 1, 0},
                                     {2, 1, 0}, {1, 0, 2}, {2, 1, 0}, {0, 2, 1}};
    Matrix old = o->matrix, t;
    Vec3d pos = o->pos;
    const float *M = o->matrix.m, *T = o->turn.m;
    float s;
    int i;

    PhysicsObj_Integrate(o, push, damp, spinDamp);
    if (o->collide)
        PhysicsObj_CollideContacts(o, push, spinDamp, world, event, msg);
    if (o->upright) {
        o->body.vel.z = 0;
        Body_Update(&o->body);
    }
    if (o->body.speed > kSlop.f) {
        pos.x = o->body.vel.x + pos.x;
        pos.y = o->body.vel.y + pos.y;
        pos.z = o->body.vel.z + pos.z;
    }
    if (o->upright) {
        float *m = o->matrix.m;
        const float *z = &old.m[6];

        m[0] = z[2] * m[4] - z[1] * m[5];
        m[1] = -(z[2] * m[3] - z[0] * m[5]);
        m[2] = z[1] * m[3] - z[0] * m[4];
        m[3] = z[1] * m[2] - z[2] * m[1];
        m[4] = -(z[0] * m[2] - z[2] * m[0]);
        m[5] = z[0] * m[1] - z[1] * m[0];
        m[6] = z[0];
        m[7] = z[1];
        m[8] = z[2];
    }
    s = PhysicsObj_MaxScale(o);
    DynCube_Move(world, o, &pos, VirModel_GetRadius(o->model) * s);
    if (!PhysicsObj_CheckContacts(o, world, event, msg)) {
        s = PhysicsObj_MaxScale(o);
        DynCube_Move(world, o, &o->lastPos, VirModel_GetRadius(o->model) * s);
        o->matrix = o->lastMatrix;
    }
    o->move.x = o->pos.x - o->lastPos.x;
    o->move.y = o->pos.y - o->lastPos.y;
    o->move.z = o->pos.z - o->lastPos.z;
    for (i = 0; i < 9; i++)
        o->turn.m[i] = o->lastMatrix.m[i % 3 * 3 + i / 3];
    o->turn.count = o->lastMatrix.count;
    for (i = 0; i < 9; i++) {
        int row = i / 3 * 3, col = i % 3;
        int a = order[i][0], b = order[i][1], c = order[i][2];

        t.m[i] = ((long double)M[row + a] * T[a * 3 + col] + (long double)M[row + b] * T[b * 3 + col])
            + (long double)M[row + c] * T[c * 3 + col];
    }
    t.count = o->turn.count + 1 + o->matrix.count;
    o->turn = t;
    o->lastPos = o->pos;
    o->lastMatrix = o->matrix;
}
HOOK(0x4FC0E0, PhysicsObj_Collide);

/* drops every active contact point down onto the ground; returns 0 when one
   of them is inside something solid */
int PhysicsObj_CheckContacts(PhysObj *o, void *world, int event, void *msg)
{
    Vec3d pts[PHYS_CONTACTS];
    double dz[PHYS_CONTACTS];
    int idx[PHYS_CONTACTS];
    CollHit hits[PHYS_CONTACTS];
    const float *M = o->matrix.m;
    int n = 0, i;

    for (i = 0; i < PHYS_CONTACTS; i++) {
        Contact *c = &o->contacts[i];
        const Vec3f *p = &c->point;
        long double x;
        float y, z;

        if (!c->enabled || !c->active)
            continue;
        x = (M[2] * p->z + M[1] * p->y) + M[0] * p->x;
        idx[n] = i;
        dz[n] = kDropDepth.d;
        y = (M[5] * p->z + M[4] * p->y) + M[3] * p->x;
        z = (M[8] * p->z + M[7] * p->y) + M[6] * p->x;
        pts[n].x = x;
        pts[n].y = y;
        pts[n].z = z;
        pts[n].x = o->pos.x + pts[n].x;
        pts[n].y = o->pos.y + pts[n].y;
        pts[n].z = o->pos.z + pts[n].z;
        n++;
    }
    if (n) {
        Coll_DropPointsMat(world, hits, pts, n, dz, event, msg, -1);
        for (i = 0; i < n; i++) {
            if (hits[i].found && hits[i].back) {
                o->contacts[idx[i]].touching = 1;
                o->touching++;
                return 0;
            }
        }
    }
    return 1;
}
HOOK(0x4FC620, PhysicsObj_CheckContacts);

/* lying still: touching the ground, no forces pending and slow enough */
int PhysicsObj_IsResting(PhysObj *o)
{
    char moved = o->unk690;
    const Vec3f *w = &o->body.angVel;

    o->unk690 = 0;
    if (o->touching > 0 && !moved && !o->body.forceCount && !(o->body.speed >= o->phys->restSpeed)
        && !(((long double)w->x * w->x + (long double)w->y * w->y) + (long double)w->z * w->z
             >= o->phys->restSpin))
        return 1;
    return 0;
}
HOOK(0x4FC7B0, PhysicsObj_IsResting);

uint8_t PhysicsObj_GetEvent0(void)
{
    return gPhysicsObjEvents[0];
}
HOOK(0x4FC840, PhysicsObj_GetEvent0);

uint8_t PhysicsObj_GetMotionEvent(void)
{
    return gPhysicsObjEvents[1];
}
HOOK(0x4FC850, PhysicsObj_GetMotionEvent);

uint8_t PhysicsObj_GetHitEvent(void)
{
    return gPhysicsObjEvents[2];
}
HOOK(0x4FC860, PhysicsObj_GetHitEvent);

/* speed of the contact point along the normal: 0 moving away (over 1),
   1 resting, 2 moving into the surface (-1 or less) */
int Contact_Classify(const float *n, Body *b, const float *r, const Matrix *m, float *dot)
{
    float v[3];
    long double d;

    Body_GetPointVelocity(v, b, r);
    d = ((long double)v[2] * n[2] + (long double)v[1] * n[1]) + (long double)v[0] * n[0];
    *dot = d;
    if (d > kOne.f)
        return 0;
    return d > kMinusOne.f ? 1 : 2;
}
HOOK(0x4FC870, Contact_Classify);

/* collision impulse at point r (world offset) along the normal n */
void Body_ApplyImpulse(Body *b, const float *M, const float *n, const float *r, float dot, float unused,
                       float bounce)
{
    float I[10], J[3];
    float im = b->invMass;
    long double c0, c1, c2, w2, u2, v2, k1, k2, num, j, d0, d1, d2, e2, L2, jx;
    float w0, w1, u0, u1, v0, v1, k0, e0, e1, L0, L1, q0, q1, q2, p0, p1, p2, jy, jz;

    Matrix_SetScale(I, b->invInertia[0], b->invInertia[1], b->invInertia[2]);
    c0 = (long double)r[1] * n[2] - (long double)n[1] * r[2];
    c1 = -((long double)r[0] * n[2] - (long double)n[0] * r[2]);
    c2 = (long double)n[1] * r[0] - (long double)n[0] * r[1];
    w0 = (c0 * M[0] + c2 * M[6]) + c1 * M[3];
    w1 = (c0 * M[1] + c2 * M[7]) + c1 * M[4];
    w2 = (c0 * M[2] + c2 * M[8]) + c1 * M[5];
    u0 = (I[2] * w2 + (long double)I[1] * w1) + (long double)I[0] * w0;
    u1 = (I[5] * w2 + (long double)I[4] * w1) + (long double)I[3] * w0;
    u2 = (w2 * I[8] + (long double)I[7] * w1) + (long double)I[6] * w0;
    v0 = ((long double)u1 * M[1] + (long double)u0 * M[0]) + u2 * M[2];
    v1 = ((long double)u1 * M[4] + (long double)u0 * M[3]) + u2 * M[5];
    v2 = ((long double)u1 * M[7] + (long double)u0 * M[6]) + u2 * M[8];
    k0 = (long double)v1 * r[2] - v2 * r[1];
    k1 = -((long double)v0 * r[2] - v2 * r[0]);
    k2 = (long double)v0 * r[1] - (long double)v1 * r[0];
    num = -(((long double)bounce + kOne.f) * dot);
    j = num / (((k1 * n[1] + (long double)k0 * n[0]) + k2 * n[2]) + im);
    J[0] = n[0] * j;
    J[1] = n[1] * j;
    J[2] = n[2] * j;
    d0 = (long double)J[2] * r[1] - (long double)J[1] * r[2];
    d1 = -((long double)J[2] * r[0] - (long double)J[0] * r[2]);
    d2 = (long double)J[1] * r[0] - (long double)J[0] * r[1];
    e0 = (d2 * M[6] + d1 * M[3]) + d0 * M[0];
    e1 = (d2 * M[7] + d1 * M[4]) + d0 * M[1];
    e2 = (d0 * M[2] + d2 * M[8]) + d1 * M[5];
    L0 = e0 + b->angMom.x;
    b->angMom.x = L0;
    L1 = e1 + b->angMom.y;
    b->angMom.y = L1;
    L2 = e2 + b->angMom.z;
    b->angMom.z = L2;
    q0 = (L2 * I[2] + (long double)L1 * I[1]) + (long double)L0 * I[0];
    q1 = (L2 * I[5] + (long double)L1 * I[4]) + (long double)L0 * I[3];
    q2 = (L2 * I[8] + (long double)L1 * I[7]) + (long double)L0 * I[6];
    p0 = ((long double)M[1] * q1 + (long double)M[0] * q0) + (long double)M[2] * q2;
    p1 = ((long double)M[4] * q1 + (long double)q2 * M[5]) + (long double)q0 * M[3];
    p2 = ((long double)M[7] * q1 + (long double)q2 * M[8]) + (long double)q0 * M[6];
    b->angVel.x = p0;
    b->angVel.y = p1;
    b->angVel.z = p2;
    jx = (long double)J[0] * b->invMass;
    jy = J[1] * b->invMass;
    jz = J[2] * b->invMass;
    b->vel.x = b->vel.x + jx;
    b->vel.y = b->vel.y + jy;
    b->vel.z = b->vel.z + jz;
    Body_Update(b);
}
HOOK(0x4FC8E0, Body_ApplyImpulse);

/* friction across the normal: the velocity along the sliding direction is
   reduced by at most |push * n.z| * mu; the spin is damped */
void Body_ApplyFriction(Body *b, const float *n, float push, float mu, float damp)
{
    long double x = (long double)push * n[2], lim, d, ad, s, tz, az;
    float t0, t1, t2, c0, c1, c2, len, k0, k1, k2, tx, ty, df, ax, ay;

    if (!(x >= 0))
        x = -x;
    if (!NONZERO(x))
        return;
    lim = x * mu;
    t0 = b->dir.x;
    t1 = b->dir.y;
    t2 = b->dir.z;
    c0 = (long double)t1 * n[2] - (long double)t2 * n[1];
    c1 = -((long double)t0 * n[2] - (long double)t2 * n[0]);
    c2 = (long double)t0 * n[1] - (long double)t1 * n[0];
    len = x87_sqrt(((long double)c1 * c1 + (long double)c2 * c2) + (long double)c0 * c0);
    if (!(len >= kFrictionMin.f)) {
        k0 = 0;
        k1 = 0;
        k2 = 0;
    } else {
        if (NONZERO(c0) || NONZERO(c1) || NONZERO(c2)) {
            long double inv = kOneD.d / len;

            tx = c0 * inv;
            ty = c1 * inv;
            tz = inv * c2;
        } else {
            tx = c0;
            ty = c1;
            tz = c2;
        }
        k0 = (long double)ty * n[2] - tz * n[1];
        k1 = -((long double)tx * n[2] - tz * n[0]);
        k2 = (long double)tx * n[1] - (long double)ty * n[0];
    }
    d = ((long double)k0 * b->vel.x + (long double)k2 * b->vel.z) + (long double)k1 * b->vel.y;
    df = d;
    ad = !(d >= 0) ? -d : d;
    if (!(lim > ad))
        s = lim;
    else
        s = !(df >= 0) ? -df : df;
    ax = k0 * s;
    ay = k1 * s;
    az = s * k2;
    b->vel.x = ax + b->vel.x;
    b->vel.y = ay + b->vel.y;
    b->vel.z = az + b->vel.z;
    Body_Update(b);
    b->angMom.x = b->angMom.x * damp;
    b->angMom.y = b->angMom.y * damp;
    b->angMom.z = b->angMom.z * damp;
}
HOOK(0x4FCCA0, Body_ApplyFriction);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 54E4CC, 54E4CC, 0x9C) INIT({0xFF, 0xFF, 0xFF, 0x00, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4F, 0x62, 0x6A, 0x00, 0x00, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4F, 0x62, 0x6A, 0x20, 0x63, 0x72, 0x65, 0x61, 0x74, 0x65, 0x20, 0x68, 0x61, 0x6E, 0x64, 0x6C, 0x65, 0x72, 0x20, 0x6D, 0x75, 0x73, 0x74, 0x20, 0x62, 0x65, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x6C, 0x6F, 0x61, 0x64, 0x65, 0x64, 0x00, 0x00, 0x00, 0x00, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4F, 0x62, 0x6A, 0x20, 0x64, 0x65, 0x6C, 0x65, 0x74, 0x65, 0x20, 0x68, 0x61, 0x6E, 0x64, 0x6C, 0x65, 0x72, 0x20, 0x6D, 0x75, 0x73, 0x74, 0x20, 0x62, 0x65, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x6C, 0x6F, 0x61, 0x64, 0x65, 0x64, 0x00, 0x00, 0x00, 0x00, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4F, 0x62, 0x6A, 0x20, 0x72, 0x75, 0x6E, 0x20, 0x68, 0x61, 0x6E, 0x64, 0x6C, 0x65, 0x72, 0x20, 0x6D, 0x75, 0x73, 0x74, 0x20, 0x62, 0x65, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x6C, 0x6F, 0x61, 0x64, 0x65, 0x64});
ALIAS(gPhysicsObjEvents, pad_54E4CC, 0);
BSS(A774B0) uint16_t gPhysicsObjType;
PAD(igb, A774B0, A774B2, 0x6);
BSS(A774B0) int gPhysicsObjUnkA774B8;
