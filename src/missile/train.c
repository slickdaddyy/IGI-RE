/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "missile/missile.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (Task *task, uint16_t type));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x414ED0, void, sub_414ED0, (Vec3f *out, void *obj, int i));
ORIG(0x416890, uint8_t, Game_GetEvent14, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4B3360, char, Vec3d_Equal, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B4010, void, Matrix_SetDirection, (Matrix *m, const Vec3f *dir));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6AB0, float *, SoundParams_New, (int n));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4ECDB0, void, Body_Update, (void *body));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F4600, void, Vehicle_ResetControls, (void *obj));
ORIG(0x4F4640, void, Vehicle_Free, (void *obj));
ORIG(0x4F4660, void, Vehicle_RunParts, (void *obj));
ORIG(0x4F4680, void, Vehicle_Init, (void *obj));
ORIG(0x4F4800, void, Vehicle_NewParts, (void *obj));
ORIG(0x4F4820, void, Vehicle_DeleteParts, (void *obj));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *obj, PhysType *phys));
ORIG(0x4FEC10, void, SplineObj_GetPoints, (int path, int unk, Vec3d *out, char *found, float dist, int count,
                                       float *offsets, int unk2));

#define gTrainType VAR(uint16_t, 0x57BCB8)
#define gTrainParams VAR(void *, 0x57BCBC)
#define gTrainOffsetRange ((float *)0x57BCE8)
#define gTrainChannels ((char *)0x53BCE8)

/* handlers of the Train type that live outside this file */
#define sub_4EFF20 ((void *)0x4EFF20)
#define sub_42B930 ((void *)0x42B930)
#define sub_42BB60 ((void *)0x42BB60)
#define sub_42BBA0 ((void *)0x42BBA0)

#define kKmhToFrame VAR(float, 0x5336C8)
#define kSoundMin VAR(float, 0x5336CC) /* 0.8 */
#define kSoundScale VAR(float, 0x5336D0) /* 5 */

static Train *head_of(Train *t)
{
    return t->v.task.parent->type == gTrainType ? (Train *)t->v.task.parent : t;
}

void Train_CreateHandler(Train *t)
{
    float scale, radius;

    if (t->v.task.parent->type == gTrainType)
        ((Train *)t->v.task.parent)->wagons++;
    t->v.pos.x = 0;
    t->v.pos.y = 0;
    t->v.pos.z = 0;
    t->start_dist = 0;
    t->dist = 0;
    t->velocity = 0;
    t->start_speed = 0;
    t->max_speed = 200.0f;
    t->accel = 1.0f;
    t->path = -1;
    t->wagons = 0;
    t->reverse = 0;
    t->offset[0] = 0;
    t->offset[1] = 0;
    memset(&t->v.matrix, 0, sizeof(Matrix));
    t->v.matrix.m[0] = 1.0f;
    t->v.matrix.m[4] = 1.0f;
    t->v.matrix.m[8] = 1.0f;
    t->model[0] = 0;
    RigidDynCubeObj_Init(Game_GetWorld(), t, NULL);
    scale = max_scale(&t->v);
    radius = VirModel_GetRadius(t->v.model) * scale;
    DynCube_Add(Game_GetWorld(), t, &t->v.pos, radius, 2);
    t->sound_params = SoundParams_New(2);
    Vehicle_Init(t);
    t->last_tick = -1;
    t->loop_sound = NULL;
    VarString_Init(&t->script);
}
HOOK(0x429F80, Train_CreateHandler);

void Train_DeleteHandler(Train *t)
{
    Vehicle_Free(t);
    if (t->v.task.parent->type == gTrainType)
        ((Train *)t->v.task.parent)->wagons--;
    VarString_Free(&t->script);
    DynCube_Remove(Game_GetWorld(), t);
    QTaskList_Free(t->sound_params);
}
HOOK(0x42A100, Train_DeleteHandler);

void Train_Place(Train *t, int quiet);

void Train_ParamsChanged(Train *t, char *arg)
{
    int changed = arg && *arg ? 1 : 0;
    Vec3f a, b;
    long double len, x;

    t->dist = t->start_dist;
    RigidDynCubeObj_SetModel(t, VirModel_Find(t->model));
    sub_414ED0(&a, t, 0);
    sub_414ED0(&b, t, 1);
    len = (long double)a.x - b.x;
    if (!(len >= kZero))
        len = -len;
    t->length = len;
    x = a.x;
    if (!(x >= kZero))
        x = -x;
    t->frac = x / len;
    PhysicsObj_SetPhysType(t, NULL);
    Vehicle_ResetControls(t);
    Vehicle_DeleteParts(t);
    Vehicle_NewParts(t);
    Train_Place(head_of(t), changed);
    BROADCAST(t->v.coll->head, Param_GetChangedEvent(), arg);
    VarString_Bind(&t->script, t);
}
HOOK(0x42A150, Train_ParamsChanged);

/* turns the matrix made from the track direction into the model's axes:
   row 0 = -row 2, row 2 = row 0, then transposed */
static void orient(Matrix *m)
{
    float r0 = m->m[0], r1 = m->m[1], r2 = m->m[2];
    float n[9];

    m->m[0] = -m->m[6];
    m->m[1] = -m->m[7];
    m->m[2] = -m->m[8];
    n[0] = m->m[0];
    n[1] = m->m[3];
    n[2] = r0;
    n[3] = m->m[1];
    n[4] = m->m[4];
    n[5] = r1;
    n[6] = m->m[2];
    n[7] = m->m[5];
    n[8] = r2;
    memcpy(m->m, n, sizeof(n));
}

static void identity(Matrix *m)
{
    m->m[0] = 1.0f;
    m->m[4] = 1.0f;
    m->m[8] = 1.0f;
    m->m[1] = 0;
    m->m[2] = 0;
    m->m[3] = 0;
    m->m[5] = 0;
    m->m[6] = 0;
    m->m[7] = 0;
    m->unk24 = 0;
}

void Train_PlaceWagons(Train *t, Vec3d *p, int quiet);

/* puts the train on its path at its distance, then the wagons behind it */
void Train_Place(Train *t, int quiet)
{
    Task *w = Task_FindChild(&t->v.task, gTrainType);
    float sx = t->offset[0] * kFixed;
    float sy = t->offset[1] * kFixed;
    float offsets[21], scale, radius, oy, oz;
    Vec3d pts[22], pos;
    long double ox, z;
    char found;
    int i = 0;

    offsets[0] = t->reverse ? t->length : -t->length;
    while (w) {
        offsets[++i] = t->reverse ? ((Train *)w)->length : -((Train *)w)->length;
        w = Task_FindNext(w, gTrainType);
    }
    SplineObj_GetPoints(t->path, 0, pts, &found, t->dist, t->wagons + 1, offsets, 0);
    if (!found)
        return;
    t->v.prev_pos = t->v.pos;
    t->v.prev_matrix = t->v.matrix;
    if (!Vec3d_Equal(&pts[0], &pts[1])) {
        Vec3f dir;

        dir.x = pts[1].x - pts[0].x;
        dir.y = pts[1].y - pts[0].y;
        dir.z = pts[1].z - pts[0].z;
        Matrix_SetDirection(&t->v.matrix, &dir);
        orient(&t->v.matrix);
        pos.x = (pts[1].x - pts[0].x) * t->frac + pts[0].x;
        pos.y = (pts[1].y - pts[0].y) * t->frac + pts[0].y;
        z = (pts[1].z - pts[0].z) * t->frac + pts[0].z;
    } else {
        identity(&t->v.matrix);
        pos = pts[0];
        z = pos.z;
    }
    ox = sx * t->v.matrix.m[1] + sy * t->v.matrix.m[2];
    oy = sy * t->v.matrix.m[5] + sx * t->v.matrix.m[4];
    oz = sx * t->v.matrix.m[7] + sy * t->v.matrix.m[8];
    pos.x = ox + pos.x;
    pos.y = oy + pos.y;
    pos.z = oz + z;
    scale = max_scale(&t->v);
    radius = VirModel_GetRadius(t->v.model) * scale;
    DynCube_Move(Game_GetWorld(), t, &pos, radius);
    if (!quiet) {
        struct {
            Vec3d *pos;
            Matrix *matrix;
        } msg = {&t->v.pos, &t->v.matrix};

        BROADCAST(t->v.task.head, RigidDynCubeObj_GetEvent4(), &msg);
    }
    if (t->v.coll) {
        struct {
            Vec3d *pos;
            Matrix *matrix;
            Train *t;
        } msg = {&t->v.pos, &t->v.matrix, t};

        BROADCAST(t->v.coll->head, MagicObj_GetMoveEvent(), &msg);
    }
    Train_PlaceWagons(t, &pts[1], quiet);
}
HOOK(0x42A2C0, Train_Place);

void Train_PlaceWagons(Train *t, Vec3d *p, int quiet)
{
    Train *w = (Train *)Task_FindChild(&t->v.task, gTrainType);

    for (; w; p++) {
        Vec3d *q = p + 1;
        float sx = w->offset[0] * kFixed;
        float sy = w->offset[1] * kFixed;
        float scale, radius, oy, oz;
        long double ox;
        Vec3d pos;

        w->v.prev_pos = w->v.pos;
        w->v.prev_matrix = w->v.matrix;
        if (!Vec3d_Equal(p, q)) {
            Vec3f dir;

            dir.x = q->x - p->x;
            dir.y = q->y - p->y;
            dir.z = q->z - p->z;
            Matrix_SetDirection(&w->v.matrix, &dir);
            orient(&w->v.matrix);
            pos.x = (q->x - p->x) * w->frac + p->x;
            pos.y = (q->y - p->y) * w->frac + p->y;
            pos.z = (q->z - p->z) * w->frac + p->z;
        } else {
            identity(&w->v.matrix);
            pos = *p;
        }
        ox = sy * w->v.matrix.m[2] + sx * w->v.matrix.m[1];
        oy = sx * w->v.matrix.m[4] + sy * w->v.matrix.m[5];
        oz = sy * w->v.matrix.m[8] + sx * w->v.matrix.m[7];
        pos.x = ox + pos.x;
        pos.y = oy + pos.y;
        pos.z = oz + pos.z;
        scale = max_scale(&w->v);
        radius = VirModel_GetRadius(w->v.model) * scale;
        DynCube_Move(Game_GetWorld(), w, &pos, radius);
        if (!quiet) {
            struct {
                Vec3d *pos;
                Matrix *matrix;
            } msg = {&w->v.pos, &w->v.matrix};

            BROADCAST(w->v.task.head, RigidDynCubeObj_GetEvent4(), &msg);
        }
        if (w->v.coll) {
            struct {
                Vec3d *pos;
                Matrix *matrix;
                Train *t;
            } msg = {&w->v.pos, &w->v.matrix, w};

            BROADCAST(w->v.coll->head, MagicObj_GetMoveEvent(), &msg);
        }
        w = (Train *)Task_FindNext(&w->v.task, gTrainType);
    }
}
HOOK(0x42A7C0, Train_PlaceWagons);

void Train_MoveHandler(Train *t)
{
    struct {
        Vec3d *pos;
        Matrix *matrix;
    } msg = {&t->v.pos, &t->v.matrix};

    BROADCAST(t->v.task.head, RigidDynCubeObj_GetEvent4(), &msg);
}
HOOK(0x42AC20, Train_MoveHandler);

void Train_GetParams(Task *t, void **out);
void Train_InputHandler(Train *t, void **msg);
void Train_GetSoundSource(Train *t, void **msg);
void Train_SaveState(Train *t, void **msg);
void Train_LoadState(Train *t, void **msg);
void Train_InputChannelHandler(Task *t, int *msg);
void Train_ReadInputHandler(Task *t, float **msg);
void Train_GetInfo(Train *t, char *out);
void Train_Run(Train *t);

#define FIELD(name) ((void *)offsetof(Train, name))

void Train_Open(void)
{
    gTrainType = TaskType_New("Train", sizeof(Train), Vehicle_GetType(), 0);
    TaskType_SetHandler(gTrainType, 0, Train_CreateHandler);
    TaskType_SetHandler2(gTrainType, 2, Train_DeleteHandler);
    TaskType_SetHandler(gTrainType, Param_GetChangedEvent(), Train_ParamsChanged);
    TaskType_SetHandler(gTrainType, Param_GetTableEvent(), Train_GetParams);
    TaskType_SetHandler(gTrainType, 7, Train_GetInfo);
    TaskType_SetHandler2(gTrainType, 1, Train_Run);
    TaskType_SetHandler(gTrainType, Sound_GetInfoEvent(), Train_GetSoundSource);
    TaskType_SetHandler(gTrainType, Vehicle_GetControlEvent(), Train_InputHandler);
    TaskType_SetHandler(gTrainType, Vehicle_GetInputInfoEvent(), Train_InputChannelHandler);
    TaskType_SetHandler(gTrainType, Vehicle_GetStateSizeEvent(), sub_4EFF20);
    TaskType_SetHandler(gTrainType, Vehicle_GetSaveEvent(), Train_SaveState);
    TaskType_SetHandler(gTrainType, Vehicle_GetLoadEvent(), Train_LoadState);
    TaskType_SetHandler(gTrainType, Vehicle_GetKeysEvent(), Train_ReadInputHandler);
    TaskType_SetHandler(gTrainType, Game_GetEvent14(), sub_42B930);
    TaskType_SetHandler(gTrainType, RigidDynCubeObj_GetEvent4(), Train_MoveHandler);
    TaskType_SetHandler(gTrainType, World_GetEvent1(), sub_42BBA0);
    TaskType_SetHandler(gTrainType, World_GetEvent0(), sub_42BB60);
    gTrainParams = ParamTable_New(10);
    gTrainOffsetRange[0] = -5.0f;
    gTrainOffsetRange[1] = 5.0f;
    /* the original passes a dummy Train on its stack, only the difference
       of the two pointers is kept */
    ParamTable_Add(gTrainParams, NULL, FIELD(start_dist), Param_Real32(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(start_speed), Param_Real32(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(path), Param_Int32(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(model), Param_String256(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(accel), Param_Real32(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(max_speed), Param_Real32(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(reverse), Param_Bool8(), 7, NULL);
    ParamTable_Add(gTrainParams, NULL, FIELD(offset[0]), Param_Real32(), 7, gTrainOffsetRange);
    ParamTable_Add(gTrainParams, NULL, FIELD(offset[1]), Param_Real32(), 7, gTrainOffsetRange);
    ParamTable_Add(gTrainParams, NULL, FIELD(script), Param_VarString(), 7, NULL);
}
HOOK(0x42ACC0, Train_Open);

void Train_GetParams(Task *t, void **out)
{
    *out = gTrainParams;
}
HOOK(0x42B030, Train_GetParams);

/* msg is {reset, controls}: controls[0] is the wanted speed (-1 keeps it);
   a reset puts the train back to its start */
void Train_InputHandler(Train *t, void **msg)
{
    float *in = msg[1];
    long double s;
    float v;

    if (*(uint32_t *)in != 0xBF800000)
        memcpy(&t->v.target_speed, in, 4);
    if (!msg[0]) {
        ((float *)msg[1])[1] = -2.0f;
        t->dist = t->start_dist;
        PhysicsObj_SetPhysType(t, NULL);
        Vehicle_ResetControls(t);
        Vehicle_DeleteParts(t);
        Vehicle_NewParts(t);
        Train_Place(t, 1);
        v = t->max_speed * t->start_speed * kKmhToFrame;
        t->velocity = v;
        t->v.body.velocity.x = v;
        t->v.body.velocity.y = 0;
        t->v.body.velocity.z = 0;
        Body_Update(&t->v.body);
        t->v.target_speed = t->start_speed;
        t->v.speed = t->start_speed;
        return;
    }
    if (!(t->v.speed >= t->v.target_speed)) {
        s = t->accel + t->v.speed;
        t->v.speed = s;
        if (s > t->v.target_speed)
            memcpy(&t->v.speed, &t->v.target_speed, 4);
    } else if (t->v.speed > t->v.target_speed) {
        s = t->v.speed - t->accel;
        t->v.speed = s;
        if (!(s >= t->v.target_speed))
            memcpy(&t->v.speed, &t->v.target_speed, 4);
    }
    v = t->max_speed * t->v.speed * kKmh * kPerFrameHour;
    t->velocity = v;
    t->v.body.velocity.x = v;
    t->v.body.velocity.y = 0;
    t->v.body.velocity.z = 0;
    Body_Update(&t->v.body);
    t->dist = (t->reverse ? -t->velocity : t->velocity) + t->dist;
    Vehicle_RunParts(t);
    Train_Place(t, 0);
}
HOOK(0x42B040, Train_InputHandler);

void Train_GetSoundSource(Train *t, void **msg)
{
    msg[1] = NULL;
    msg[0] = &t->v.pos;
    msg[2] = t->sound_params;
}
HOOK(0x42B220, Train_GetSoundSource);

/* {state[4], unused, pos} */
void Train_SaveState(Train *t, void **msg)
{
    float *s = msg[0];

    memcpy(msg[2], &t->v.pos, sizeof(Vec3d));
    memcpy(&s[0], &t->velocity, 4);
    memcpy(&s[1], &t->dist, 4);
    memcpy(&s[2], &t->v.speed, 4);
    memcpy(&s[3], &t->v.target_speed, 4);
}
HOOK(0x42B240, Train_SaveState);

void Train_LoadState(Train *t, void **msg)
{
    float *s = msg[0];

    memcpy(&t->velocity, &s[0], 4);
    memcpy(&t->dist, &s[1], 4);
    memcpy(&t->v.speed, &s[2], 4);
    memcpy(&t->v.target_speed, &s[3], 4);
    Train_Place(t, 0);
}
HOOK(0x42B280, Train_LoadState);

/* {index, out}: the 16 byte input channel record ("Thrust", ...) */
void Train_InputChannelHandler(Task *t, int *msg)
{
    if (msg[0] >= 0 && msg[0] < 2) {
        msg[1] = (int)(gTrainChannels + msg[0] * 16);
        return;
    }
    msg[1] = 0;
}
HOOK(0x42B2C0, Train_InputChannelHandler);

/* debug keys: ` and 1 - 0 set the speed (0, 0.1 .. 1.0); with ctrl a new press
   of ` or 1 - 0 sets the second control (-1, 1 .. 9, 0) */
void Train_ReadInputHandler(Task *t, float **msg)
{
    static const float speeds[10] = {
        0x1.99999ap-4f, 0x1.99999ap-3f, 0x1.333334p-2f, 0x1.99999ap-2f, 0.5f,
        0x1.333334p-1f, 0x1.666666p-1f, 0x1.99999ap-1f, 0x1.ccccccp-1f, 1.0f,
    };
    int ctrl, i;

    msg[0][0] = -1.0f;
    if (!(gKeys[0x1D] & 0x80) && !(gKeys[0x9D] & 0x80)) {
        if (gKeys[0x29] & 0x80)
            msg[0][0] = 0;
        for (i = 0; i < 10; i++) {
            if (gKeys[2 + i] & 0x80)
                msg[0][0] = speeds[i];
        }
    }
    msg[0][1] = -2.0f;
    ctrl = (gKeys[0x1D] & 0x80) || (gKeys[0x9D] & 0x80);
    if (ctrl) {
        if ((gKeys[0x29] & 0x80) && !(gKeysPrev[0x29] & 0x80))
            msg[0][1] = -1.0f;
        for (i = 0; i < 10; i++) {
            if ((gKeys[2 + i] & 0x80) && !(gKeysPrev[2 + i] & 0x80))
                msg[0][1] = i < 9 ? i + 1 : 0;
        }
    }
}
HOOK(0x42B2F0, Train_ReadInputHandler);

/* slot 7: position, matrix turned -90 degrees about z, radius and the type name */
void Train_GetInfo(Train *t, char *out)
{
    Matrix r;
    float scale;

    memcpy(out, &t->v.pos, sizeof(Vec3d));
    memset(out + 0x18, 0, 12);
    memcpy(out + 0x24, &t->v.matrix, sizeof(Matrix));
    Matrix_SetRotZ(&r, -0x1.921fb6p+0f);
    mat_mul((Matrix *)(out + 0x24), &r, 3);
    scale = max_scale(&t->v);
    *(float *)(out + 0x4C) = VirModel_GetRadius(t->v.model) * scale;
    if (*(char **)(out + 0x50))
        memcpy(*(char **)(out + 0x50), "Train", 6);
}
HOOK(0x42B500, Train_GetInfo);

void Train_Run(Train *t)
{
    if (t->last_tick == Flow_GetFrames())
        return;
    t->last_tick = Flow_GetFrames();
    if (VarString_HasExpr(&t->script) && !round_int(VarString_EvalReal(&t->script, t))) {
        if (t->loop_sound) {
            Task_Delete(t->loop_sound);
            t->loop_sound = NULL;
        }
    } else if (!t->loop_sound) {
        Task *s = Sound_Play(t, t->wagons ? "train_loop_1" : "train_loop_2");

        if (s)
            t->loop_sound = s;
    }
    if (t->sound_params) {
        float speed = head_of(t)->v.speed;
        long double v = speed * kSoundScale;

        if (v < kSoundMin)
            v = kSoundMin;
        else if (!(v <= kOne))
            v = kOne;
        t->sound_params[1] = v;
        t->sound_params[2] = speed > kMilli ? 1 : 0;
    }
    BROADCAST(t->v.task.head, 1);
    if (t->v.coll && !t->wagons)
        BROADCAST(t->v.coll->head, 1);
}
HOOK(0x42B700, Train_Run);
