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
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x497800, Prim *, Frame_Alloc, (int size));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x499FF0, int, Float_Log2, (float f));
ORIG(0x4B3BA0, void, Matrix_SetRotY, (Matrix *m, float a));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1780, void, Debug_Log, (void *obj, int a, int b, int c, int d, int e, int f, float *color));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *o, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *o, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *o, Vec3d *pos, float radius, int category));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int *a, int *b));
ORIG(0x4DAA20, int, Light_GetActive, (int *a));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));

#define MAGICOBJ_DELETE (FN(0x51B940, EditorMagicObj_Delete))

extern uint16_t gAfterburnerType;
extern char gAfterburnerRandom[];
extern int gAfterburnerPrim;
extern float gViewDir[];
extern Vec3d gViewPos;

F32(kColor, 0x437F0000);        /* 255 */
F32(kGlowStart, 0x3F4F5C29);    /* 0.81 */
F32(kGlowScale, 0x4011745D);    /* 1 / 0.44 */
F32(kGlowKeep, 0x3F7AE148);     /* 0.98 */
F32(kGlowAdd, 0x3CA3D700);      /* 0.02 */
F32(kScaleMul, 0x3FB33333);     /* 1.4 */
F32(kFlicker, 0x3E4CCCCD);      /* 0.2 */
F32(kOne, 0x3F800000);
F32(kZero, 0x00000000);

static float part_radius(Part *o)
{
    float s = max_scale(o->obj.scale);

    return (float)(VirModel_GetRadius(o->obj.model) * s);
}

void Afterburner_Create(Part *o, MagicCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    o->offset = arg->offset;
    o->local = arg->local;
    RigidDynCubeObj_Init(arg->world, o, model);
    DynCube_Add(arg->world, o, &arg->pos, part_radius(o), 2);
    o->obj.matrix = arg->matrix;
    o->owner = arg->owner;
    Random_Init(gAfterburnerRandom);
}
HOOK(0x42BC30, Afterburner_Create);

void Afterburner_Draw(Part *o)
{
    Prim *p = Frame_Alloc(0x30);
    float depth;
    int queue;
    Frame f;

    p->type = gAfterburnerPrim;
    p->obj = o;
    p->color[0] = (float)(o->value * kColor.f);
    p->color[1] = (float)(o->value * kColor.f);
    p->color[2] = (float)(o->value * kColor.f);
    p->unk14 = Light_GetActive(&p->unk18);
    Haze_GetVisible(&p->unk20, &p->unk1C);
    depth = (float)((long double)gViewDir[2] * ((long double)o->obj.pos.z - gViewPos.z)
                    + (long double)gViewDir[1] * ((long double)o->obj.pos.y - gViewPos.y)
                    + (long double)gViewDir[0] * ((long double)o->obj.pos.x - gViewPos.x));
    if (Float_Log2(depth) < 1)
        queue = 1;
    else
        queue = Float_Log2(depth);
    RenderQueue_Add(queue, p);
    f.pos = o->obj.pos;
    f.matrix = o->obj.matrix;
    EACH_CHILD(&o->obj.task, RigidDynCubeObj_GetEvent2(), &f);
}
HOOK(0x42BD40, Afterburner_Draw);

void Afterburner_Run(Part *o);
void Afterburner_Move(Part *o, PartMoveArg *a);
void Afterburner_DrawPrim(Prim *p);

void Afterburner_Init(void)
{
    gAfterburnerType = TaskType_New("Afterburner", 0x140, MagicObj_GetType(), 0);
    gAfterburnerPrim = Primitive_Register("AfterburnerPrimitive");
    Primitive_SetHandler(gAfterburnerPrim, Afterburner_DrawPrim);
    TaskType_SetHandler(gAfterburnerType, 0, Afterburner_Create);
    TaskType_SetHandler2(gAfterburnerType, 1, Afterburner_Run);
    TaskType_SetHandler2(gAfterburnerType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler(gAfterburnerType, MagicObj_GetMoveEvent(), Afterburner_Move);
    TaskType_SetHandler(gAfterburnerType, 4, Afterburner_Draw);
    TaskType_SetHandler2(gAfterburnerType, Screen_GetDrawEvent(), NULL);
    TaskType_SetHandler(gAfterburnerType, RigidDynCubeObj_GetEvent2(), NULL);
    Script_RegisterConstant("TASKTYPE_AFTERBURNER", gAfterburnerType);
}
HOOK(0x42BEB0, Afterburner_Init);

void Afterburner_Run(Part *o)
{
    float throttle = o->owner->throttle;
    long double glow = throttle > kGlowStart.f ? ((long double)throttle - kGlowStart.f) * kGlowScale.f
                                               : (long double)kZero.f;
    long double s;
    float size;

    if (glow != o->value)
        o->value = (float)(glow * kGlowAdd.f + (long double)o->value * kGlowKeep.f);
    s = (long double)o->value * kScaleMul.f;
    size = s < kOne.f ? kOne.f : (float)s;
    o->obj.scale[1] = (float)(Random_Float(gAfterburnerRandom) * kFlicker.f + size);
    RUN_CHILDREN(&o->obj.task);
}
HOOK(0x42BFA0, Afterburner_Run);

void Afterburner_Move(Part *o, PartMoveArg *a)
{
    const float *m = a->matrix->m;
    long double x = (long double)m[2] * o->offset.z + (long double)m[1] * o->offset.y
                    + (long double)m[0] * o->offset.x;
    double y = (long double)m[5] * o->offset.z + (long double)m[4] * o->offset.y + (long double)m[3] * o->offset.x;
    double z = (long double)m[8] * o->offset.z + (long double)m[7] * o->offset.y + (long double)m[6] * o->offset.x;
    Matrix rot, t, f;
    Vec3d p;
    float radius;

    p.x = x + a->pos->x;
    p.y = y + a->pos->y;
    p.z = z + a->pos->z;
    radius = part_radius(o);
    DynCube_Move(Game_GetWorld(), o, &p, radius);
    Matrix_SetRotY(&rot, (float)Random_Float(gAfterburnerRandom));
    mat_mul(&t, &o->local, &rot, 1, 1, 1);
    mat_mul(&f, a->matrix, &t, 0, 1, 1);
    o->obj.matrix = f;
}
HOOK(0x42C0B0, Afterburner_Move);

void Afterburner_DrawPrim(Prim *p)
{
    RigidDynCubeObj *o = p->obj;

    Debug_Log(o, p->unk10, o->unk48, p->unk14, p->unk18, p->unk20, p->unk1C, p->color);
}
HOOK(0x42C4B0, Afterburner_DrawPrim);

void Afterburner_Close(void)
{
    TaskType_Free(gAfterburnerType);
    Primitive_Unregister(gAfterburnerPrim);
}
HOOK(0x42C4E0, Afterburner_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BCF8) uint16_t gAfterburnerType;
PAD(igb, 57BCF8, 57BCFA, 0x6);
PAD(igb, 57BCF8, 57BD00, 0x10);
ALIAS(gAfterburnerRandom, pad_57BD00, 0);
BSS(BC2364) int gAfterburnerPrim;
