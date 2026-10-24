#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "glass/glass.h"
#include "hook.h"

FN_EXTERN(ParamReal32x9_Size);

ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, void, Script_GetArgString, (void *args, int i, char *out, int size));
ORIG(0x4E6AB0, float *, SoundParams_New, (int count));

/* the DefineMagicObj data of a radar dish */
typedef struct RadarDishDef {
    char sound[0x20];
    float max_speed;    /* 0x20, degrees per second */
    float accel;        /* 0x24 */
} RadarDishDef;

/* "RadarDish" (0x180), a turning magic object */
typedef struct RadarDish {
    Obj o;
    MagicPart m;        /* 0xF0 */
    float target_speed; /* 0x13C */
    float speed;        /* 0x140 */
    float angle;        /* 0x144, degrees */
    Matrix base;        /* 0x148 */
    char unk170[4];
    Task **sound;       /* 0x174 */
    float *sound_params; /* 0x178 */
    char unk17C[4];
} RadarDish;

_Static_assert(offsetof(RadarDish, sound) == 0x174, "RadarDish");
_Static_assert(sizeof(RadarDish) == 0x180, "RadarDish");

/* {data, args, first arg index} of the DefineMagicObj data event */
typedef struct DefArg {
    RadarDishDef *def;
    void *args;
    int index;
} DefArg;

extern uint16_t gRadarDishType;

extern float k360;

#define sub_4EFCD0 (FN(0x4EFCD0, ParamReal32x9_Size))

void RadarDish_Create(RadarDish *r, MagicArg *arg);
void RadarDish_Run(RadarDish *r);
void RadarDish_Delete(RadarDish *r);
void RadarDish_ParamsChanged(RadarDish *r, char *arg);
void RadarDish_ParseDef(Task *t, DefArg *arg);
void RadarDish_Stop(RadarDish *r);
void RadarDish_GetSoundSource(RadarDish *r, void **out);

void RadarDish_Create(RadarDish *r, MagicArg *arg)
{
    void **model = VirModel_Find(arg->model);
    RadarDishDef *def;
    float scale, radius;
    Task *s;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    memcpy(r->m.offset, &arg->offset, sizeof(r->m.offset));
    r->m.local = arg->local;
    RigidDynCubeObj_Init(arg->world, r, model);
    scale = obj_scale(&r->o);
    radius = VirModel_GetRadius(r->o.model) * scale;
    DynCube_Add(arg->world, r, &arg->pos, radius, 2);
    r->o.matrix = arg->matrix;
    r->base = arg->matrix;
    def = arg->data;
    r->m.data = def;
    r->target_speed = def->max_speed;
    r->speed = ((RadarDishDef *)r->m.data)->max_speed;
    r->angle = 0;
    r->sound = NULL;
    r->m.owner = arg->owner;
    RigidDynCubeObj_CreateMagicObjs(r, r, arg->owner);
    r->sound_params = SoundParams_New(1);
    s = Sound_Play(r, r->m.data);
    if (s)
        r->sound = TaskRef_Add(s);
}
HOOK(0x448B70, RadarDish_Create);

void RadarDish_Delete(RadarDish *r)
{
    DynCube_Remove(r->o.world, r);
    if (r->sound)
        TaskRef_Remove(r->sound);
    if (r->sound_params)
        QTaskList_Free(r->sound_params);
}
HOOK(0x448CF0, RadarDish_Delete);

void RadarDish_Open(void)
{
    gRadarDishType = TaskType_New("RadarDish", sizeof(RadarDish), MagicObj_GetType(), 0);
    TaskType_SetHandler(gRadarDishType, 0, RadarDish_Create);
    TaskType_SetHandler2(gRadarDishType, 1, RadarDish_Run);
    TaskType_SetHandler2(gRadarDishType, 2, RadarDish_Delete);
    TaskType_SetHandler(gRadarDishType, Param_GetChangedEvent(), RadarDish_ParamsChanged);
    TaskType_SetHandler(gRadarDishType, RigidDynCubeObj_GetEvent0(), RadarDish_ParseDef);
    TaskType_SetHandler(gRadarDishType, RigidDynCubeObj_GetEvent1(), sub_4EFCD0);
    TaskType_SetHandler(gRadarDishType, Game_GetEvent9(), RadarDish_Stop);
    TaskType_SetHandler(gRadarDishType, Sound_GetInfoEvent(), RadarDish_GetSoundSource);
    Script_RegisterConstant("TASKTYPE_RADARDISH", gRadarDishType);
}
HOOK(0x448D30, RadarDish_Open);

/* m = base * rot, with the summation order of the original per row */
static void turn(Matrix *out, const Matrix *b, const Matrix *rot)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            long double p0 = (long double)rot->m[j] * b->m[i * 3];
            long double p1 = (long double)rot->m[3 + j] * b->m[i * 3 + 1];
            long double p2 = (long double)rot->m[6 + j] * b->m[i * 3 + 2];

            out->m[i * 3 + j] = i == 1 ? (p2 + p0) + p1 : (p1 + p2) + p0;
        }
    }
    out->unk24 = b->unk24 + rot->unk24 + 1;
}

/* turns the dish towards its target speed and spins it */
void RadarDish_Run(RadarDish *r)
{
    Matrix rot, n;
    long double v;
    float *p;

    EACH_CHILD(&r->o.task, 1);
    if (r->speed < r->target_speed) {
        v = ((RadarDishDef *)r->m.data)->accel * kFrame + r->speed;
        r->speed = v;
        if (v > r->target_speed)
            r->speed = r->target_speed;
    } else if (r->speed > r->target_speed) {
        v = r->speed - ((RadarDishDef *)r->m.data)->accel * kFrame;
        r->speed = v;
        if (!(v >= r->target_speed))
            r->speed = r->target_speed;
    }
    v = r->speed * kFrame + r->angle;
    r->angle = v;
    while (v >= k360) {
        v = r->angle - k360;
        r->angle = v;
    }
    Matrix_SetRotZ(&rot, r->angle * kDegToRad);
    turn(&n, &r->base, &rot);
    r->o.matrix = n;
    p = r->sound_params;
    if (p) {
        p[1] = r->speed / ((RadarDishDef *)r->m.data)->max_speed;
        p = r->sound_params;
        if (!(p[1] >= kZero))
            p[1] = 0;
        else if (p[1] > kOne)
            p[1] = 1.0f;
    }
}
HOOK(0x448E30, RadarDish_Run);

/* keeps the placed orientation as the base the dish turns from */
void RadarDish_ParamsChanged(RadarDish *r, char *arg)
{
    r->base = r->o.matrix;
    if (!arg || !*arg)
        EACH_CHILD(&r->o.task, Param_GetChangedEvent(), NULL);
}
HOOK(0x449180, RadarDish_ParamsChanged);

/* DefineMagicObj(name, model, type, sound, speed, accel) */
void RadarDish_ParseDef(Task *t, DefArg *arg)
{
    int i = arg->index;
    RadarDishDef *d = arg->def;

    Script_GetArgString(arg->args, i, d->sound, 0x20);
    d->max_speed = Script_GetArgReal(arg->args, i + 1);
    d->accel = Script_GetArgReal(arg->args, i + 2);
}
HOOK(0x449220, RadarDish_ParseDef);

void RadarDish_Stop(RadarDish *r)
{
    r->target_speed = 0;
}
HOOK(0x449260, RadarDish_Stop);

void RadarDish_GetSoundSource(RadarDish *r, void **out)
{
    out[0] = &r->o.pos;
    out[1] = &r->o.matrix;
    out[2] = r->sound_params;
}
HOOK(0x449270, RadarDish_GetSoundSource);

void RadarDish_Close(void)
{
    TaskType_Free(gRadarDishType);
}
HOOK(0x449290, RadarDish_Close);
