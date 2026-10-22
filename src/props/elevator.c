/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "props/props.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B7FC0, void, Script_RegisterReal, (const char *name, double *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7560, void, DynCube_Remove, (void *room, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4FEC10, void, Graph_GetPoint, (int graph, Matrix *m, Vec3d *pos, char *found, float dist, int unk5,
                                      int unk6, float *offset));
ORIG(0x5020C0, long double, Graph_GetLength, (int id));
ORIG(0x502130, int, Graph_GetFloorCount, (int id));
ORIG(0x502200, void, Graph_GetFloorPos, (Vec3d *out, int id, int floor));

/* "Elevator" (0x648 bytes): a platform moving along a graph between floors.
   Field names follow src/elevator. */
typedef struct Elevator {
    RigidObj obj;
    Vec3d place;            /* 0xF0, point on the graph */
    Vec3d lastPos;          /* 0x108 */
    Matrix placeMatrix;     /* 0x120 */
    Matrix lastMatrix;      /* 0x148 */
    char model[0x10];       /* 0x170 */
    int graph;              /* 0x180 */
    float height;           /* 0x184, distance along the graph */
    float unk188;           /* start distance */
    char unk18C;            /* straight line from the first to the last floor */
    char unk18D;            /* keep the matrix */
    char pad18E[2];
    float speed;            /* 0x190 */
    float maxSpeed;
    float accel;
    int wanted;             /* 0x19C */
    double floorPos;        /* 0x1A0 */
    double lastFloorPos;    /* 0x1A8 */
    char request[10];       /* 0x1B0 */
    char pad1BA[2];
    int restTicks;          /* 0x1BC */
    char registered;        /* 0x1C0 */
    char pad1C1[3];
    VarString wantedFloor[10]; /* 0x1C4 */
    VarString up;           /* 0x4E4 */
    VarString down;         /* 0x534 */
    VarString move;         /* 0x584 */
    char lastUp;            /* 0x5D4 */
    char lastDown;
    char isMoving;          /* 0x5D6 */
    char isStarting;
    char isStopping;
    char sounds[0x60];
    char pad639[3];
    Task *moving;           /* 0x63C, sound */
    char lastMove;          /* 0x640 */
    char pad641[3];
    int lastTick;
} Elevator;

_Static_assert(offsetof(Elevator, wantedFloor) == 0x1C4, "wantedFloor");
_Static_assert(offsetof(Elevator, isMoving) == 0x5D6, "isMoving");
_Static_assert(sizeof(Elevator) == 0x648, "Elevator");

#define gElevatorParams VAR(void *, 0x57C0E0)
#define gElevatorType VAR(uint16_t, 0x57C108)
#define gElevatorLength VAR(float, 0x57C0EC)
#define gElevatorMove VAR(Vec3d, 0x57C0F0)
#define gElevatorTurn VAR(Matrix, 0x57C110)

#define kTenth VAR(float, 0x5333C8)

/* puts the elevator at its distance along the graph */
void Elevator_UpdatePlace(Elevator *e)
{
    char found = 1;
    float len, dist, scale, radius;

    if (e->graph == -1)
        return;
    len = Graph_GetLength(e->graph);
    dist = e->height;
    if (dist >= len) {
        dist = len - kTenth;
        MOV32(e->height, len);
    }
    if (!(dist >= kZero)) {
        dist = 0;
        e->height = 0;
    }
    if (e->unk18C) {
        int n = Graph_GetFloorCount(e->graph);
        Vec3d a, b;
        long double t;

        Graph_GetFloorPos(&a, e->graph, 0);
        Graph_GetFloorPos(&b, e->graph, n - 1);
        t = e->height / (long double)len;
        e->place.x = (b.x - a.x) * t + a.x;
        e->place.y = (b.y - a.y) * t + a.y;
        e->place.z = (b.z - a.z) * t + a.z;
    } else {
        Matrix m;
        float offset[3] = {0, 0, 0};

        Graph_GetPoint(e->graph, &m, &e->place, &found, dist, 0, 0, offset);
        if (!found)
            return;
        if (!e->unk18D)
            mat_mul(&e->obj.matrix, &e->placeMatrix, &m, 4);
    }
    scale = max_scale(&e->obj);
    radius = VirModel_GetRadius(e->obj.model) * scale;
    DynCube_Move(Game_GetWorld(), e, &e->place, radius);
}
HOOK(0x43D960, Elevator_UpdatePlace);

void Elevator_DeleteHandler(Elevator *e)
{
    int i;

    if (e->moving) {
        Sound_Stop(e->moving);
        e->moving = NULL;
    }
    if (Task_GetId(e) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), e->obj.task.type);

        if (fn)
            fn(e);
    }
    for (i = 0; i < 10; i++)
        VarString_Free(&e->wantedFloor[i]);
    VarString_Free(&e->down);
    VarString_Free(&e->up);
    VarString_Free(&e->move);
    DynCube_Remove(e->obj.room, e);
}
HOOK(0x43DCF0, Elevator_DeleteHandler);

void Elevator_GetParams(void *task, void **out)
{
    *out = gElevatorParams;
}
HOOK(0x43DDA0, Elevator_GetParams);

void Elevator_ParamsChanged(Elevator *e, char *arg)
{
    int i;
    float len;

    RigidDynCubeObj_SetModel(e, VirModel_Find(e->model));
    e->obj.matrix = e->placeMatrix;
    memcpy(&e->lastFloorPos, &e->floorPos, 8);
    e->restTicks = 0;
    e->speed = 0;
    for (i = 0; i < 10; i++)
        VarString_Bind(&e->wantedFloor[i], e);
    VarString_Bind(&e->down, e);
    VarString_Bind(&e->up, e);
    VarString_Bind(&e->move, e);
    len = Graph_GetLength(e->graph);
    MOV32(e->height, e->unk188);
    if (!(e->unk188 >= kZero))
        e->height = 0;
    if (e->unk188 > len)
        MOV32(e->height, len);
    Elevator_UpdatePlace(e);
    (void)arg; /* the original copies the same thing once more when arg is set */
    e->lastPos = e->obj.pos;
    e->lastMatrix = e->obj.matrix;
    MOV32(gElevatorLength, len);
    e->isStarting = 0;
    e->isStopping = 0;
    e->lastMove = 1;
}
HOOK(0x43DDB0, Elevator_ParamsChanged);

void Elevator_Activate(Elevator *e)
{
    char name[64], buf[128];
    int i;

    if (e->registered)
        return;
    e->registered = 1;
    Task_GetName(name, e);
    crt_sprintf(buf, "%s.vFloor", name);
    Script_RegisterReal(buf, &e->floorPos);
    crt_sprintf(buf, "%s.vLastFloor", name);
    Script_RegisterReal(buf, &e->lastFloorPos);
    for (i = 0; i < 10; i++) {
        crt_sprintf(buf, "%s.isWantedFloor%d", name, i);
        Script_RegisterBool8(buf, &e->request[i]);
    }
    crt_sprintf(buf, "%s.nInactive", name);
    Script_RegisterInt(buf, &e->restTicks);
    crt_sprintf(buf, "%s.nWantedFloor", name);
    Script_RegisterInt(buf, &e->wanted);
    crt_sprintf(buf, "%s.isMoving", name);
    Script_RegisterBool8(buf, &e->isMoving);
    crt_sprintf(buf, "%s.isStart", name);
    Script_RegisterBool8(buf, &e->isStarting);
    crt_sprintf(buf, "%s.isStop", name);
    Script_RegisterBool8(buf, &e->isStopping);
}
HOOK(0x43DF20, Elevator_Activate);

void Elevator_Deactivate(Elevator *e)
{
    char name[64], buf[128];
    int i;

    if (!e->registered)
        return;
    Task_GetName(name, e);
    e->registered = 0;
    crt_sprintf(buf, "%s.vFloor", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.vLastFloor", name);
    Script_RemoveSymbol(buf);
    for (i = 0; i < 10; i++) {
        crt_sprintf(buf, "%s.isWantedFloor%d", name, i);
        Script_RemoveSymbol(buf);
    }
    crt_sprintf(buf, "%s.nInactive", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nWantedFloor", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isMoving", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isStart", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isStop", name);
    Script_RemoveSymbol(buf);
    e->registered = 0;
}
HOOK(0x43E0A0, Elevator_Deactivate);

/* RigidDynCubeObj event 4: follow the graph, then move the children along */
void Elevator_MoveHandler(Elevator *e)
{
    MoveMsg msg;

    Elevator_UpdatePlace(e);
    msg.pos = &e->place;
    msg.matrix = &e->obj.matrix;
    BROADCAST(&e->obj.task, RigidDynCubeObj_GetEvent4(), &msg);
}
HOOK(0x43E1F0, Elevator_MoveHandler);

/* Game event 14: runs the elevator and reports how far it moved and turned
   since the last parameter change, for whoever stands on it */
void Elevator_GetMotion(Elevator *e, void **out)
{
    Matrix t;

    HANDLER(1, e->obj.task.type)(e);
    gElevatorMove.x = e->obj.pos.x - e->lastPos.x;
    gElevatorMove.y = e->obj.pos.y - e->lastPos.y;
    gElevatorMove.z = e->obj.pos.z - e->lastPos.z;
    t.m[0] = e->lastMatrix.m[0];
    t.m[1] = e->lastMatrix.m[3];
    t.m[2] = e->lastMatrix.m[6];
    t.m[3] = e->lastMatrix.m[1];
    t.m[4] = e->lastMatrix.m[4];
    t.m[5] = e->lastMatrix.m[7];
    t.m[6] = e->lastMatrix.m[2];
    t.m[7] = e->lastMatrix.m[5];
    t.m[8] = e->lastMatrix.m[8];
    t.unk24 = e->lastMatrix.unk24;
    mat_mul(&gElevatorTurn, &e->obj.matrix, &t, 5);
    out[0] = &e->obj.pos;
    out[2] = &gElevatorMove;
    out[1] = &gElevatorTurn;
}
HOOK(0x43E290, Elevator_GetMotion);

void Elevator_Close(void)
{
    QTaskList_Free(gElevatorParams);
    TaskType_Free(gElevatorType);
}
HOOK(0x43E4C0, Elevator_Close);

uint16_t Elevator_GetType(void)
{
    return gElevatorType;
}
HOOK(0x43E4E0, Elevator_GetType);
