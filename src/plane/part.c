#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "plane/plane.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));

float PlanePart_Radius(PlanePart *o)
{
    float s = max_scale(o->scale);

    return VirModel_GetRadius(o->model) * s;
}

void PlanePart_Init(PlanePart *o, MagicCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);
    float radius;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    o->offset = arg->offset;
    o->local = arg->local;
    RigidDynCubeObj_Init(arg->world, o, model);
    radius = PlanePart_Radius(o);
    DynCube_Add(arg->world, o, &arg->pos, radius, 2);
    o->matrix = arg->matrix;
    o->owner = arg->owner;
    o->def = arg->data;
}

void PlanePart_Place(PlanePart *o, PartMoveArg *a, int keep_x)
{
    const float *m = a->matrix->m;
    long double x = m[2] * o->offset.z + m[1] * o->offset.y + m[0] * o->offset.x;
    double y = m[5] * o->offset.z + m[4] * o->offset.y + m[3] * o->offset.x;
    double z = m[8] * o->offset.z + m[7] * o->offset.y + m[6] * o->offset.x;
    Vec3d p;
    float radius;

    if (!keep_x)
        x = (double)x;
    p.x = x + a->pos->x;
    p.y = y + a->pos->y;
    p.z = z + a->pos->z;
    radius = PlanePart_Radius(o);
    DynCube_Move(Game_GetWorld(), o, &p, radius);
}
