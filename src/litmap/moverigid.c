#include "hook.h"
#include "litmap/litmap.h"

ORIG(0x4B3360, char, Vec3d_Equal, (const Vec3d *a, const Vec3d *b));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x51FBA0, void, MoveRigidObj_Apply, (void *obj, void *obj2, char force));
ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));

/* the parts of a MoveRigidObj used here: a model moved between two keys */
typedef struct MoveRigidObj {
    Task task;
    char unk20[0xC8];
    Task *path;
    int unkEC;
    Vec3d key[2];
    Vec3d savedKey[2];
    float angle[3][2];
    float savedAngle[3][2];
    char model[0x20];
    double time;
    double unk1A8;
    int unk1B0;
    VarString expr;
} MoveRigidObj;

_Static_assert(offsetof(MoveRigidObj, key) == 0xF0, "key");
_Static_assert(offsetof(MoveRigidObj, angle) == 0x150, "angle");
_Static_assert(offsetof(MoveRigidObj, expr) == 0x1B4, "expr");

#define gMoveRigidObjType VAR(uint16_t, 0xA837CC)
#define gMoveRigidObjParams VAR(void *, 0xA837D0)

/* jumps to the last key that was edited */
void MoveRigidObj_Changed(MoveRigidObj *m, const char *arg)
{
    int i;

    for (i = 1; i >= 0; i--) {
        if (!Vec3d_Equal(&m->savedKey[i], &m->key[i]) || fne(m->angle[0][i], m->savedAngle[0][i]) ||
            fne(m->angle[1][i], m->savedAngle[1][i]) || fne(m->angle[2][i], m->savedAngle[2][i])) {
            m->savedKey[i] = m->key[i];
            m->savedAngle[0][i] = m->angle[0][i];
            m->savedAngle[1][i] = m->angle[1][i];
            m->savedAngle[2][i] = m->angle[2][i];
            m->time = i;
        }
    }
    RigidDynCubeObj_SetModel(m, VirModel_Find(m->model));
    MoveRigidObj_Apply(m, m, 1);
    Task_DeleteChildren(m->path);
    RigidDynCubeObj_CreateMagicObjs(m->path, m, m);
    if (!arg || !*arg)
        BROADCAST(m->task.head, Param_GetChangedEvent(), 0);
    VarString_Bind(&m->expr, m);
}
HOOK(0x51FD60, MoveRigidObj_Changed);

void MoveRigidObj_Shutdown(void)
{
    QTaskList_Free(gMoveRigidObjParams);
    TaskType_Free(gMoveRigidObjType);
}
HOOK(0x51FED0, MoveRigidObj_Shutdown);
