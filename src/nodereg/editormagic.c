/* EditorMagicObj: a MagicObj placed in the editor that spins about its
   z axis and follows its parent's matrix. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "nodereg/nodereg.h"

/* a script argument: a rate and the range the angle step is taken from */
typedef struct MagicSpin {
    float rate;
    int min;
    int max;
} MagicSpin;

/* "EditorMagicObj" (0x140), a MagicObj */
typedef struct EditorMagicObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0xD4 - 0xA4];
    int category;
    char unkD8[0x108 - 0xD8];
    Matrix local;
    int unk130;
    int unk134;
    MagicSpin *spin;
    int unk13C;
} EditorMagicObj;

_Static_assert(sizeof(EditorMagicObj) == 0x140, "EditorMagicObj");

/* what slot 0 gets when the object is made */
typedef struct EditorMagicInit {
    const char *model;
    void *world;
    Vec3d pos;
    char unk20[0x18];
    Matrix matrix;
    Matrix local;
    int unk88;
    MagicSpin *spin;
} EditorMagicInit;

/* what the parent passes down when it moves */
typedef struct MagicMove {
    char unk0[0x18];
    Matrix matrix;
} MagicMove;

typedef struct MagicParse {
    MagicSpin *out;
    void *args;
    int index;
} MagicParse;

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, int c));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model, int category));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));

extern uint16_t gEditorMagicObjType;

void EditorMagicObj_Create(EditorMagicObj *o, EditorMagicInit *in);
void EditorMagicObj_Delete(EditorMagicObj *o);
void EditorMagicObj_Parse(EditorMagicObj *o, MagicParse *p);
void EditorMagicObj_GetArgSize(EditorMagicObj *o, int *size);
void EditorMagicObj_Move(EditorMagicObj *o, MagicMove *mv);


void EditorMagicObj_Move(EditorMagicObj *o, MagicMove *mv)
{
    const float *w = o->local.m;
    const float *p = mv->matrix.m;
    MagicSpin *s = o->spin;
    int step = 0;
    int range = s->max - s->min;
    Matrix r, a, b;

    if (range)
        step = 0 % range + s->min;
    Matrix_SetRotZ(&r, step * s->rate);
    a.m[0] = r.m[6] * w[2] + r.m[0] * w[0] + r.m[3] * w[1];
    a.m[1] = r.m[7] * w[2] + r.m[1] * w[0] + r.m[4] * w[1];
    a.m[2] = r.m[8] * w[2] + r.m[2] * w[0] + r.m[5] * w[1];
    a.m[3] = r.m[6] * w[5] + r.m[0] * w[3] + r.m[3] * w[4];
    a.m[4] = r.m[7] * w[5] + r.m[1] * w[3] + r.m[4] * w[4];
    a.m[5] = r.m[8] * w[5] + r.m[2] * w[3] + r.m[5] * w[4];
    a.m[6] = r.m[6] * w[8] + r.m[0] * w[6] + r.m[3] * w[7];
    a.m[7] = r.m[7] * w[8] + r.m[1] * w[6] + r.m[4] * w[7];
    a.m[8] = r.m[8] * w[8] + r.m[2] * w[6] + r.m[5] * w[7];
    a.unk24 = o->local.unk24 + r.unk24 + 1;
    b.m[0] = a.m[6] * p[2] + a.m[0] * p[0] + a.m[3] * p[1];
    b.m[1] = a.m[7] * p[2] + a.m[1] * p[0] + a.m[4] * p[1];
    b.m[2] = a.m[8] * p[2] + a.m[2] * p[0] + a.m[5] * p[1];
    b.m[3] = a.m[3] * p[4] + a.m[6] * p[5] + a.m[0] * p[3];
    b.m[4] = a.m[4] * p[4] + a.m[7] * p[5] + a.m[1] * p[3];
    b.m[5] = a.m[5] * p[4] + a.m[8] * p[5] + a.m[2] * p[3];
    b.m[6] = a.m[6] * p[8] + a.m[0] * p[6] + a.m[3] * p[7];
    b.m[7] = a.m[7] * p[8] + a.m[1] * p[6] + a.m[4] * p[7];
    b.m[8] = a.m[8] * p[8] + a.m[2] * p[6] + a.m[5] * p[7];
    b.unk24 = mv->matrix.unk24 + a.unk24 + 1;
    o->matrix = b;
    ((void (*)(void *, void *))gTaskHandlers[4][RigidDynCubeObj_GetType()])(o, NULL);
}
HOOK(0x51B540, EditorMagicObj_Move);

void EditorMagicObj_Create(EditorMagicObj *o, EditorMagicInit *in)
{
    float m;

    RigidDynCubeObj_Init(in->world, o, VirModel_Find(in->model));
    m = !(o->scale[2] >= o->scale[1]) ? o->scale[1] : o->scale[2];
    if (o->scale[0] > m)
        m = o->scale[0];
    DynCube_Add(in->world, o, &in->pos, VirModel_GetRadius(o->model, o->category) * m, o->category);
    o->matrix = in->matrix;
    o->local = in->local;
    o->spin = in->spin;
    o->unk130 = in->unk88;
    RigidDynCubeObj_CreateMagicObjs(o, o, in->unk88);
}
HOOK(0x51B850, EditorMagicObj_Create);

void EditorMagicObj_Delete(EditorMagicObj *o)
{
    DynCube_Remove(o->world, o);
}
HOOK(0x51B940, EditorMagicObj_Delete);

void EditorMagicObj_Open(void)
{
    uint16_t t = TaskType_New("EditorMagicObj", sizeof(EditorMagicObj), MagicObj_GetType(), 0, 0);

    gEditorMagicObjType = t;
    TaskType_SetHandler(t, 0, EditorMagicObj_Create);
    TaskType_SetHandler2(gEditorMagicObjType, 2, EditorMagicObj_Delete);
    TaskType_SetHandler(gEditorMagicObjType, RigidDynCubeObj_GetEvent0(), EditorMagicObj_Parse);
    TaskType_SetHandler(gEditorMagicObjType, RigidDynCubeObj_GetEvent1(), EditorMagicObj_GetArgSize);
    TaskType_SetHandler(gEditorMagicObjType, RigidDynCubeObj_GetEvent2(), EditorMagicObj_Move);
    TaskType_SetHandler(gEditorMagicObjType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gEditorMagicObjType, 9, NULL);
    TaskType_SetHandler(gEditorMagicObjType, 10, NULL);
    TaskType_SetHandler(gEditorMagicObjType, 11, NULL);
    Script_RegisterConstant("TASKTYPE_EDITORMAGICOBJ", gEditorMagicObjType);
}
HOOK(0x51B960, EditorMagicObj_Open);

void EditorMagicObj_Parse(EditorMagicObj *o, MagicParse *p)
{
    MagicSpin *s = p->out;
    int i = p->index;

    s->rate = Script_GetArgReal(p->args, i);
    s->min = Script_GetArgInt(p->args, i + 1);
    s->max = Script_GetArgInt(p->args, i + 2);
}
HOOK(0x51BA60, EditorMagicObj_Parse);

void EditorMagicObj_GetArgSize(EditorMagicObj *o, int *size)
{
    *size = sizeof(MagicSpin);
}
HOOK(0x51BAA0, EditorMagicObj_GetArgSize);

void EditorMagicObj_Close(void)
{
    TaskType_Free(gEditorMagicObjType);
}
HOOK(0x51BAB0, EditorMagicObj_Close);
