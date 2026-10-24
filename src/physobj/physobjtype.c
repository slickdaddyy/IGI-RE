/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <float.h>
#include <string.h>

#include "hook.h"
#include "orig.h"
#include "physobj/physobj.h"

ORIG(0x401D80, void, Task_Broadcast, (void *task, uint8_t slot, void *arg));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x48F340, int, AppContext_GetQvmOnly, (void));
ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B1420, void, File_ScanDir, (const char *path, void *fn, void *user));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0970, void, Mesh3D_GetVertex, (float *out, void *obj, int point));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));

/* the task that ran LOCAL:lod.qsc */
extern void *gModelLODSettingsContainer;

extern uint8_t gPhysicsObjTypeEvents[];
extern int gPhysicsObjTypeCount;
extern PhysicsObjType gPhysicsObjTypes[];

/* what File_ScanDir passes to its callback */
typedef struct DirEntry {
    char *path;
    int unk4;
    char isDir;
} DirEntry;

/* the parts of an object Mesh3D_GetVertex reads */
typedef struct PointQuery {
    char unk0[0x6C];
    void **model;
    char unk70[0x28];
    float scale[3];
} PointQuery;

void Lod_SendChanged(void)
{
    if (gModelLODSettingsContainer)
        Task_Broadcast(gModelLODSettingsContainer, Param_GetChangedEvent(), NULL);
}
HOOK(0x4EDCC0, Lod_SendChanged);

ScriptValue *DefinePhysicsObjType(ScriptValue *ret, void *data, void *args);

void PhysicsObjType_Open(void)
{
    gPhysicsObjTypeEvents[0] = TaskEvent_Alloc(0);
    gPhysicsObjTypeEvents[1] = TaskEvent_Alloc(0);
    gPhysicsObjTypeEvents[2] = TaskEvent_Alloc(0);
    Script_RegisterFunction("DefinePhysicsObjType", DefinePhysicsObjType, NULL, -1);
}
HOOK(0x4EDCE0, PhysicsObjType_Open);

/* DefinePhysicsObjType(type, unk, mass, sx, sy, sz, s32, s256, model, n,
   n * (point, i, i, r, r, r), r, r, args for the task type...) */
ScriptValue *DefinePhysicsObjType(ScriptValue *ret, void *data, void *args)
{
    PhysicsObjType *t = &gPhysicsObjTypes[gPhysicsObjTypeCount];
    PhysicsObjJoint *j;
    int i, arg;
    TaskFn fn;

    (void)data;
    t->data = NULL;
    t->type = Script_GetArgInt(args, 0);
    t->unk4 = Script_GetArgInt(args, 1);
    t->mass = Script_GetArgReal(args, 2);
    t->size[0] = Script_GetArgReal(args, 3);
    t->size[1] = Script_GetArgReal(args, 4);
    t->size[2] = Script_GetArgReal(args, 5);
    Script_GetArgString(args, 6, t->unk18, sizeof(t->unk18));
    Script_GetArgString(args, 7, t->unk38, sizeof(t->unk38));
    Script_GetArgString(args, 8, t->model, sizeof(t->model));
    for (i = 0; i < gPhysicsObjTypeCount; i++) {
        if (!strcmp(t->model, gPhysicsObjTypes[i].model)) {
            Error_Show("Physics obj model %s is defined multiple times.", t->model);
            for (;;)
                ;
        }
    }
    arg = 10;
    t->jointCount = Script_GetArgInt(args, 9);
    j = t->joints;
    for (i = 0; i < t->jointCount; i++, j++) {
        j->point = Script_GetArgInt(args, arg++);
        j->unk4 = Script_GetArgInt(args, arg++);
        j->unk8 = Script_GetArgInt(args, arg++);
        j->unkC = Script_GetArgReal(args, arg++);
        j->unk10 = Script_GetArgReal(args, arg++);
        j->unk14 = Script_GetArgReal(args, arg++);
    }
    t->unk2CC = Script_GetArgReal(args, arg);
    t->unk2D0 = Script_GetArgReal(args, arg + 1);
    t->data = NULL;
    fn = HANDLER(gPhysicsObjTypeEvents[1], t->type);
    if (fn) {
        struct { void *data; void *args; int index; } init;

        /* the size handler writes over the joint counter */
        fn(NULL, &i);
        init.data = t->data = Mem_Alloc(i, 4);
        init.args = args;
        init.index = arg + 2;
        HANDLER(gPhysicsObjTypeEvents[0], t->type)(NULL, &init);
    }
    gPhysicsObjTypeCount++;
    ret->i = 1;
    ret->d = 1.0;
    ret->type = gScriptNumberType;
    return ret;
}
HOOK(0x4EDD20, DefinePhysicsObjType);

void PhysicsObjType_Close(void)
{
    TaskEvent_Free(gPhysicsObjTypeEvents[0]);
    TaskEvent_Free(gPhysicsObjTypeEvents[1]);
    TaskEvent_Free(gPhysicsObjTypeEvents[2]);
}
HOOK(0x4EDFB0, PhysicsObjType_Close);

void PhysicsObjType_ParseConfigFile(const char *path)
{
    void *s;

    if (crt_strstr(path, "vssver"))
        return;
    s = Script_Load(path);
    if (!s) {
        Error_Show("PhysicsObjType_ParseConfigFile(): Couldn't load script: %s", path);
        for (;;)
            ;
    }
    Script_Run(s);
    Script_Free(s);
}
HOOK(0x4EDFE0, PhysicsObjType_ParseConfigFile);

void PhysicsObjLoad_Visit(DirEntry *e);

void PhysicsObjLoad(const char *path)
{
    File_ScanDir(path, PhysicsObjLoad_Visit, NULL);
}
HOOK(0x4EE030, PhysicsObjLoad);

/* loads every .qsc (or .qvm) script below the folder */
void PhysicsObjLoad_Visit(DirEntry *e)
{
    const char *ext;
    size_t n, m;

    if (e->isDir) {
        PhysicsObjLoad(e->path);
        return;
    }
    ext = AppContext_GetQvmOnly() ? "qvm" : "qsc";
    n = strlen(e->path);
    m = strlen(ext);
    if (n > m && !crt_stricmp(e->path + n - m, ext))
        PhysicsObjType_ParseConfigFile(e->path);
}
HOOK(0x4EE050, PhysicsObjLoad_Visit);

uint8_t PhysicsObjType_GetParseEvent(void)
{
    return gPhysicsObjTypeEvents[0];
}
HOOK(0x4EE0E0, PhysicsObjType_GetParseEvent);

uint8_t PhysicsObjType_GetSizeEvent(void)
{
    return gPhysicsObjTypeEvents[1];
}
HOOK(0x4EE0F0, PhysicsObjType_GetSizeEvent);

uint8_t PhysicsObjType_GetFreeEvent(void)
{
    return gPhysicsObjTypeEvents[2];
}
HOOK(0x4EE100, PhysicsObjType_GetFreeEvent);

int PhysicsObjType_GetCount(void)
{
    return gPhysicsObjTypeCount;
}
HOOK(0x4EE110, PhysicsObjType_GetCount);

PhysicsObjType *PhysicsObjType_Get(int i)
{
    return &gPhysicsObjTypes[i];
}
HOOK(0x4EE120, PhysicsObjType_Get);

PhysicsObjType *PhysicsObjType_Find(const char *model)
{
    int i;

    for (i = 0; i < PhysicsObjType_GetCount(); i++) {
        PhysicsObjType *t = PhysicsObjType_Get(i);

        if (!strcmp(t->model, model))
            return t;
    }
    return NULL;
}
HOOK(0x4EE140, PhysicsObjType_Find);

void PhysicsObjType_CalcSize(PhysicsObjType *t);

/* zero (or NaN) */
static int unset(float v)
{
    return !(v < 0.0f || v > 0.0f);
}

float *PhysicsObjType_GetSize(PhysicsObjType *t)
{
    if (unset(t->size[0]) || unset(t->size[1]) || unset(t->size[2]))
        PhysicsObjType_CalcSize(t);
    return t->size;
}
HOOK(0x4EE1B0, PhysicsObjType_GetSize);

/* the bounding box of the joint points */
void PhysicsObjType_CalcSize(PhysicsObjType *t)
{
    double lo[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
    double hi[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
    PointQuery q;   /* the scale is never set, as in the original */
    float p[3];
    int i, k;

    q.model = VirModel_Find(t->model);
    for (i = 0; i < t->jointCount; i++) {
        Mesh3D_GetVertex(p, &q, t->joints[i].point);
        for (k = 0; k < 3; k++)
            if (!(p[k] >= lo[k]))
                lo[k] = p[k];
        for (k = 0; k < 3; k++)
            if (p[k] > hi[k])
                hi[k] = p[k];
    }
    for (k = 0; k < 3; k++) {
        long double d = (long double)hi[k] - lo[k];

        if (!(d >= 0))
            d = -d;
        t->size[k] = d;
    }
}
HOOK(0x4EE200, PhysicsObjType_CalcSize);

void PhysicsObjType_FreeAll(void)
{
    int n = PhysicsObjType_GetCount();
    int i;

    for (i = 0; i < n; i++) {
        PhysicsObjType *t = PhysicsObjType_Get(i);

        HANDLER(gPhysicsObjTypeEvents[2], t->type)(NULL, &t);
    }
    gPhysicsObjTypeCount = 0;
}
HOOK(0x4EE3B0, PhysicsObjType_FreeAll);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 54DC30, 54DC30, 0x88) INIT({0xFF, 0xFF, 0xFF, 0x00, 0x44, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4F, 0x62, 0x6A, 0x54, 0x79, 0x70, 0x65, 0x00, 0x00, 0x00, 0x00, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x20, 0x6F, 0x62, 0x6A, 0x20, 0x6D, 0x6F, 0x64, 0x65, 0x6C, 0x20, 0x25, 0x73, 0x20, 0x69, 0x73, 0x20, 0x64, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x64, 0x20, 0x6D, 0x75, 0x6C, 0x74, 0x69, 0x70, 0x6C, 0x65, 0x20, 0x74, 0x69, 0x6D, 0x65, 0x73, 0x2E, 0x00, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4F, 0x62, 0x6A, 0x54, 0x79, 0x70, 0x65, 0x5F, 0x50, 0x61, 0x72, 0x73, 0x65, 0x43, 0x6F, 0x6E, 0x66, 0x69, 0x67, 0x46, 0x69, 0x6C, 0x65, 0x28, 0x29, 0x3A, 0x20, 0x43, 0x6F, 0x75, 0x6C, 0x64, 0x6E, 0x27, 0x74, 0x20, 0x6C, 0x6F, 0x61, 0x64, 0x20, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x3A, 0x20, 0x25, 0x73});
ALIAS(gPhysicsObjTypeEvents, pad_54DC30, 0);
BSS(A5EFD4) void *gModelLODSettingsContainer;
BSS(A5EFD4) int gPhysicsObjTypeCount;
PAD(igb, A5EFD4, A5EFDC, 0x4);
PAD(igb, A5EFD4, A5EFE0, 0x11C68);
ALIAS(gPhysicsObjTypes, pad_A5EFE0, 0);
