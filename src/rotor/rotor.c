#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "rotor/rotor.h"

FN_EXTERN(EditorMagicObj_Delete);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x47E6F0, uint8_t, BulletHole_GetEvent, (void));
ORIG(0x497800, Prim *, Frame_Alloc, (int size));
ORIG(0x497860, void, Primitive_Draw, (void *prim));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4CFC30, int, Mesh3D_GetDrawRigidMethod, (void));
ORIG(0x4B2B30, long double, Vec3_Angle, (const float *a, const float *b));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (int unk, void *o, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *o, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (int unk, void *o, Vec3d *pos, float radius, int category));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int *a, int *b));
ORIG(0x4DAA20, int, Light_GetActive, (int *a));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4ECF50, void, Body_AddForce, (float *body, float *force));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4EE100, uint8_t, PhysicsObjType_GetFreeEvent, (void));
ORIG(0x4EE140, MagicDef *, PhysicsObjType_Find, (const char *name));

#define MAGICOBJ_DELETE (FN(0x51B940, EditorMagicObj_Delete))

extern void (*gRenderFuncs[])(void *);
extern uint16_t gRotorType;
extern int gRotorPrim;

/* "Rotor" MagicObj arguments (0x20 bytes) */
typedef struct RotorDef {
    char lift;
    char tilt;
    char pad[2];
    float angle;
    float speed;
    int dir;
    char name[0x10];
} RotorDef;

_Static_assert(sizeof(RotorDef) == 0x20, "RotorDef");

F32(kLift, 0x4232674F);         /* 44.600887 */
F32(kTilt, 0x40200000);         /* 2.5 */
F32(kZero, 0x00000000);
F32(kTiltY, 0x46000000);        /* 8192 */
F32(kOne, 0x3F800000);
F32(kMinPitch, 0xBDCCCCCD);     /* -0.1 */
F32(kYawTilt, 0x3CD80000);      /* 0.0263671875 */
F32(kMassTilt, 0x3ECCCCCD);     /* 0.4 */
F32(kSpin, 0x41AAE714);         /* 21.362831 */
static const double kDegToRad = 0x1.1df46aaaaaaabp-6;

static float part_radius(Part *o)
{
    float s = max_scale(o->obj.scale);

    return (float)(VirModel_GetRadius(o->obj.model) * s);
}

void Rotor_Create(Part *o, PartCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    o->def = PhysicsObjType_Find(arg->model);
    o->offset = arg->offset;
    o->local = arg->local;
    RigidDynCubeObj_Init(arg->unk4, o, model);
    DynCube_Add(arg->unk4, o, &arg->pos, part_radius(o), 2);
    o->obj.matrix = arg->matrix;
    o->value = 0;
    o->owner = arg->owner;
    RigidDynCubeObj_CreateMagicObjs(o, o, arg->owner);
}
HOOK(0x42C500, Rotor_Create);

/* draws each blade, tilted by the throttle and the turn rate of the owner */
void Rotor_DrawPrim(Prim *p)
{
    Part *src = p->obj;
    RigidDynCubeObj copy = src->obj;
    Matrix base = copy.matrix;
    RotorDef *d = src->def->data;
    Car *car = src->owner;
    CarData *cd = car->def->data;
    long double rate, k;
    float step, maxTilt, tilt, a;
    int i;
    struct {
        RigidDynCubeObj *obj;
        int unk14;
        int unk18;
        int unk20;
        int unk1C;
    } arg;

    if (!copy.model)
        return;
    step = (float)((long double)d->speed * car->throttle);
    maxTilt = d->angle;
    rate = (long double)car->matrix.m[8] * car->spin[2] + (long double)car->matrix.m[5] * car->spin[1]
           + (long double)car->matrix.m[2] * car->spin[0];
    k = (long double)kOne.f - car->throttle;
    if (kMinPitch.f > k)
        k = kMinPitch.f;
    else if (kOne.f < k)
        k = kOne.f;
    tilt = (float)(-(k * maxTilt) - rate * kYawTilt.f * maxTilt / ((long double)cd->unk0 * kMassTilt.f));
    if (-maxTilt > tilt)
        tilt = -maxTilt;
    else if (maxTilt < tilt)
        tilt = maxTilt;
    a = (float)(src->value * kSpin.f);
    for (i = 0; i < d->dir; i++) {
        Matrix rz, t;

        Matrix_SetRotZ(&rz, tilt);
        Matrix_SetRotX(&copy.matrix, a);
        mat_mul(&t, &base, &copy.matrix, 0, 0, 0);
        copy.matrix = t;
        mat_mul(&t, &copy.matrix, &rz, 0, 0, 0);
        copy.matrix = t;
        /* the original also re-orthonormalizes both products when unk24
           passes 1000, but only into locals that are never read */
        arg.obj = &copy;
        arg.unk14 = p->unk14;
        arg.unk18 = p->unk18;
        arg.unk20 = p->unk20;
        arg.unk1C = p->unk1C;
        gRenderFuncs[Mesh3D_GetDrawRigidMethod()](&arg);
        a = a + step;
    }
}
HOOK(0x42C630, Rotor_DrawPrim);

void Rotor_Draw(Part *o)
{
    Prim *p = Frame_Alloc(0x24);
    Frame f;

    p->type = gRotorPrim;
    p->obj = o;
    p->unk14 = Light_GetActive(&p->unk18);
    Haze_GetVisible(&p->unk20, &p->unk1C);
    Primitive_Draw(p);
    f.pos = o->obj.pos;
    f.matrix = o->obj.matrix;
    EACH_CHILD(&o->obj.task, RigidDynCubeObj_GetEvent2(), &f);
}
HOOK(0x42D1D0, Rotor_Draw);

void Rotor_Run(Part *o);
void Rotor_Move(Part *o, PartMoveArg *a);
void Rotor_ForwardEvent9(Part *o, void *arg);
void Rotor_ParseDef(void *unused, DefParseArg *a);
void Rotor_GetDefSize(void *unused, int *size);
void Rotor_FreeDef(void *unused, MagicDef **def);

void Rotor_Init(void)
{
    gRotorType = TaskType_New("Rotor", 0x140, MagicObj_GetType(), 0);
    gRotorPrim = Primitive_Register("RotorPrimitive");
    Primitive_SetHandler(gRotorPrim, Rotor_DrawPrim);
    TaskType_SetHandler(gRotorType, 0, Rotor_Create);
    TaskType_SetHandler2(gRotorType, 1, Rotor_Run);
    TaskType_SetHandler2(gRotorType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler(gRotorType, MagicObj_GetMoveEvent(), Rotor_Move);
    TaskType_SetHandler(gRotorType, Game_GetEvent9(), Rotor_ForwardEvent9);
    TaskType_SetHandler(gRotorType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gRotorType, PhysicsObjType_GetParseEvent(), Rotor_ParseDef);
    TaskType_SetHandler(gRotorType, PhysicsObjType_GetSizeEvent(), Rotor_GetDefSize);
    TaskType_SetHandler(gRotorType, 4, Rotor_Draw);
    TaskType_SetHandler2(gRotorType, Screen_GetDrawEvent(), NULL);
    TaskType_SetHandler(gRotorType, RigidDynCubeObj_GetEvent2(), NULL);
    TaskType_SetHandler(gRotorType, PhysicsObjType_GetFreeEvent(), Rotor_FreeDef);
    TaskType_SetHandler(gRotorType, BulletHole_GetEvent(), NULL);
    Script_RegisterConstant("TASKTYPE_ROTOR", gRotorType);
}
HOOK(0x42D2B0, Rotor_Init);

void Rotor_Run(Part *o)
{
    Car *car = o->owner;
    RotorDef *d = o->def->data;

    if (d->lift) {
        long double lift = (long double)car->def->power * car->throttle * kLift.f;
        float f = (float)lift;

        if (lift != 0) {
            long double a = kZero.f;
            float up[3], fwd[3], force[6];

            up[0] = car->matrix.m[1];
            up[1] = car->matrix.m[4];
            up[2] = car->matrix.m[7];
            fwd[0] = car->matrix.m[2];
            fwd[1] = car->matrix.m[5];
            fwd[2] = car->matrix.m[8];
            if (up[0] != 0 || up[1] != 0) {
                up[2] = 0;
                a = Vec3_Angle(up, fwd);
            }
            force[0] = 0;
            force[1] = (float)(x87_cos(a) * f);
            force[2] = f;
            force[3] = 0;
            force[4] = 0;
            force[5] = 0;
            Body_AddForce(car->body, force);
        }
    }
    if (car->throttle != 0 && (d->tilt || d->lift) && !car->unk1B8) {
        float force[6];

        force[3] = 0;
        force[4] = kTiltY.f;
        force[5] = 0;
        force[0] = 0;
        force[1] = 0;
        force[2] = 0;
        if (d->tilt)
            force[0] = -(float)((long double)car->def->power * car->throttle * kTilt.f);
        else if (d->lift)
            force[0] = (float)((long double)car->def->power * car->throttle * kTilt.f);
        Body_AddForce(car->body, force);
    }
    o->value = (float)((long double)d->dir * d->speed * car->throttle + o->value);
    RUN_CHILDREN(&o->obj.task);
}
HOOK(0x42D440, Rotor_Run);

void Rotor_Move(Part *o, PartMoveArg *a)
{
    const float *m = a->matrix->m;
    double x = (long double)m[2] * o->offset.z + (long double)m[1] * o->offset.y + (long double)m[0] * o->offset.x;
    double y = (long double)m[5] * o->offset.z + (long double)m[4] * o->offset.y + (long double)m[3] * o->offset.x;
    double z = (long double)m[8] * o->offset.z + (long double)m[7] * o->offset.y + (long double)m[6] * o->offset.x;
    Matrix f;
    Vec3d p;
    float radius;

    p.x = x + a->pos->x;
    p.y = y + a->pos->y;
    p.z = z + a->pos->z;
    radius = part_radius(o);
    DynCube_Move(Game_GetWorld(), o, &p, radius);
    mat_mul(&f, a->matrix, &o->local, 1, 0, 0);
    o->obj.matrix = f;
    if (o->obj.task.head->next) {
        PartMoveArg arg;

        arg.pos = &o->obj.pos;
        arg.matrix = &o->obj.matrix;
        EACH_CHILD(&o->obj.task, MagicObj_GetMoveEvent(), &arg);
    }
}
HOOK(0x42D680, Rotor_Move);

void Rotor_ForwardEvent9(Part *o, void *arg)
{
    Car *car = o->owner;
    TaskFn fn = HANDLER(Game_GetEvent9(), car->task.type);

    if (fn)
        fn(car, arg);
}
HOOK(0x42D9B0, Rotor_ForwardEvent9);

void Rotor_ParseDef(void *unused, DefParseArg *a)
{
    RotorDef *d = a->out;
    void *args = a->args;
    int i = a->index;

    (void)unused;
    d->lift = Script_GetArgInt(args, i);
    d->tilt = Script_GetArgInt(args, i + 1);
    d->dir = Script_GetArgInt(args, i + 2);
    d->angle = (float)(Script_GetArgReal(args, i + 3) * kDegToRad);
    d->speed = (float)Script_GetArgReal(args, i + 4);
    Script_GetArgString(args, i + 5, d->name, 0x10);
}
HOOK(0x42D9F0, Rotor_ParseDef);

void Rotor_GetDefSize(void *unused, int *size)
{
    (void)unused;
    *size = sizeof(RotorDef);
}
HOOK(0x42DA60, Rotor_GetDefSize);

void Rotor_FreeDef(void *unused, MagicDef **def)
{
    (void)unused;
    if ((*def)->data) {
        Mem_Free((*def)->data);
        (*def)->data = NULL;
    }
}
HOOK(0x42DA70, Rotor_FreeDef);

void Rotor_Close(void)
{
    TaskType_Free(gRotorType);
    Primitive_Unregister(gRotorPrim);
}
HOOK(0x42DAA0, Rotor_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BD10) uint16_t gRotorType;
PAD(igb, 57BD10, 57BD12, 0x2);
BSS(57BD10) int gRotorPrim;
