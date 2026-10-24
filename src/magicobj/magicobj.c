#include <string.h>

#include "hook.h"
#include "igi.h"
#include "magicobj/magicobj.h"

FN_EXTERN(RigidDynCubeObj_CollLine);
FN_EXTERN(RigidDynCubeObj_EnterHandler);
FN_EXTERN(RigidDynCubeObj_LineHit);
FN_EXTERN(Task_Nothing);
FN_EXTERN(MagicObj_LoadDirEntry);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4017C0, char, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401F60, void, Task_SetGlobalFlags, (uint32_t flags));
ORIG(0x401F80, void, Task_ClearGlobalFlags, (uint32_t flags));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1420, void, File_ScanDir, (const char *path, void *fn, int unk));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *s));
ORIG(0x4B85B0, void, Script_Run, (void *s));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (RigidDynCubeObj *a, RigidDynCubeObj *b, RigidDynCubeObj *c));
ORIG(0x4C6940, void, DynCube_Move, (void *world, RigidDynCubeObj *o, void *arg, float radius));
ORIG(0x4C7140, void, DynCube_Add, (int unk, RigidDynCubeObj *o, double *pos, float radius, int unkD4));
ORIG(0x4C7760, void, DynCubeObj_ClearRenderData, (RigidDynCubeObj *o));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, float, VirModel_GetRadius, (void **model));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x4E55B0, uint8_t, CellMask_GetDrawEvent, (void));
ORIG(0x4E5610, void, CellMask_FillTriangle, (int target, float ax, float ay, float bx, float by, float cx, float cy));

#define RIGID_ENTER_HANDLER (FN(0x4A01B0, RigidDynCubeObj_EnterHandler))
#define RIGID_HANDLER_4C1840 (FN(0x4C1840, RigidDynCubeObj_LineHit))
#define RIGID_HANDLER_4D3710 (FN(0x4D3710, Task_Nothing))
#define RIGID_HANDLER_4C35F0 (FN(0x4C35F0, RigidDynCubeObj_CollLine))
#define DIR_CALLBACK_4C2F90 (FN(0x4C2F90, MagicObj_LoadDirEntry))

extern MagicHandler gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern char gRigidEvents[];
extern uint16_t gRigidType;
extern uint16_t gDynCubeObjType;
extern MagicObjDef gMagicObjDefs[];
extern int gMagicObjDefCount;
extern void *gCurrentWorld;
extern char gEmptyString[];

static float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

static float radius(const RigidDynCubeObj *o)
{
    float s = max_scale(o);

    return (float)(VirModel_GetRadius(o->model) * s);
}

void RigidDynCubeObj_CreateHandler(RigidDynCubeObj *o, RigidCreateArg *arg)
{
    void **model;

    if (!arg)
        return;
    model = VirModel_Find(arg->model);
    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    RigidDynCubeObj_Init(arg->unk4, o, model);
    DynCube_Add(arg->unk4, o, arg->pos, radius(o), o->unkD4);
    o->matrix = arg->matrix;
    RigidDynCubeObj_CreateMagicObjs(o, o, o);
    memset(o->unkC0, 0, sizeof(o->unkC0));
}
HOOK(0x4C3010, RigidDynCubeObj_CreateHandler);

void RigidDynCubeObj_GetInfo(RigidDynCubeObj *o, ObjInfo *info)
{
    memcpy(&info->pos, &o->pos, sizeof(info->pos));
    info->velocity = (Vec3f){0};
    info->matrix = o->matrix;
    info->radius = radius(o);
    if (info->name)
        memcpy(info->name, "Rigid Object", 13);
}
HOOK(0x4C3110, RigidDynCubeObj_GetInfo);

/* draws both collision meshes from above as 2D triangles */
void RigidDynCubeObj_DrawCellMask(RigidDynCubeObj *o, RigidDrawView *view)
{
    float pts[20000][2];
    int visible = 1;
    double px, py;
    MagicHandler h;
    MeshPart *part;
    int i, j;

    if (!o->model)
        return;
    h = gTaskHandlers[DynCubeObj_GetMaskVisibleEvent()][o->task.type];
    if (h)
        h(o, &visible);
    if (!visible)
        return;
    px = o->pos.x;
    py = o->pos.y;
    part = *(MeshPart **)(*(char **)(*(char **)((char *)o->model[o->lod] + 0xC) + 0xC) + 0x2C);
    for (j = 0; j < 2; j++, part++) {
        for (i = 0; i < part->vertCount; i++) {
            long double x = (long double)part->verts[i][0] * o->scale[0];
            long double y = (long double)part->verts[i][1] * o->scale[1];
            long double z = (long double)part->verts[i][2] * o->scale[2];
            const float *m = o->matrix.m;

            pts[i][0] = (float)(z * m[2] + x * m[0] + y * m[1]);
            pts[i][1] = (float)(z * m[5] + x * m[3] + y * m[4]);
        }
        for (i = 0; i < part->triCount; i++) {
            long double k = (long double)view->size * 0.5f / view->range;
            const float *a = pts[part->tris[i][0]];
            const float *b = pts[part->tris[i][1]];
            const float *c = pts[part->tris[i][2]];
            double *ctr = view->center;
            float ax = (float)((((long double)a[0] + px) - ctr[0] + view->range) * k);
            float ay = (float)((((long double)a[1] + py) - ctr[1] + view->range) * k);
            float bx = (float)((((long double)b[0] + px) - ctr[0] + view->range) * k);
            float by = (float)((((long double)b[1] + py) - ctr[1] + view->range) * k);
            float cx = (float)((((long double)c[0] + px) - ctr[0] + view->range) * k);
            float cy = (float)((((long double)c[1] + py) - ctr[1] + view->range) * k);

            CellMask_FillTriangle(view->target, ax, ay, bx, by, cx, cy);
        }
    }
}
HOOK(0x4C31F0, RigidDynCubeObj_DrawCellMask);

double *DynCubeObj_GetPosition(RigidDynCubeObj *o)
{
    return &o->pos.x;
}
HOOK(0x4C3440, DynCubeObj_GetPosition);

void RigidDynCubeObj_Open(void)
{
    gRigidType = TaskType_New("RigidDynCubeObj", sizeof(RigidDynCubeObj), gDynCubeObjType, 0);
    gRigidEvents[0] = TaskEvent_Alloc(0);
    gRigidEvents[1] = TaskEvent_Alloc(0);
    gRigidEvents[2] = TaskEvent_Alloc(0);
    gRigidEvents[3] = TaskEvent_Alloc(0);
    gRigidEvents[4] = TaskEvent_Alloc(0);
    TaskType_SetHandler(gRigidType, 4, RIGID_ENTER_HANDLER);
    TaskType_SetHandler(gRigidType, 7, RigidDynCubeObj_GetInfo);
    TaskType_SetHandler(gRigidType, 10, RIGID_HANDLER_4C1840);
    TaskType_SetHandler(gRigidType, 11, RIGID_HANDLER_4D3710);
    TaskType_SetHandler(gRigidType, 9, RIGID_HANDLER_4C1840);
    TaskType_SetHandler(gRigidType, 8, RIGID_HANDLER_4C35F0);
    TaskType_SetHandler(gRigidType, 12, RigidDynCubeObj_CollFilter);
    TaskType_SetHandler(gRigidType, 0, RigidDynCubeObj_CreateHandler);
    TaskType_SetHandler(gRigidType, CellMask_GetDrawEvent(), RigidDynCubeObj_DrawCellMask);
    TaskType_SetHandler(gRigidType, World_GetEvent0(), RigidDynCubeObj_ActivateHandler);
    TaskType_SetHandler(gRigidType, 6, RigidDynCubeObj_MoveHandler);
    Script_RegisterFunction("DefineMagicObj", DefineMagicObj, NULL, -1);
    Task_SetGlobalFlags(0x8000000);
    TaskType_SetFlags(gRigidType, 0x8000000);
}
HOOK(0x4C3450, RigidDynCubeObj_Open);

void RigidDynCubeObj_CollFilter(RigidDynCubeObj *o, char *arg)
{
    *arg = o != *(RigidDynCubeObj **)(arg + 4);
}
HOOK(0x4C35D0, RigidDynCubeObj_CollFilter);

/* DefineMagicObj(name, model, type) */
MagicScriptValue *DefineMagicObj(MagicScriptValue *ret, void *data, void *args)
{
    MagicObjDef *d;
    MagicHandler h;
    void *size;

    (void)data;
    gMagicObjDefs[gMagicObjDefCount].data = NULL;
    Script_GetArgString(args, 0, gMagicObjDefs[gMagicObjDefCount].name, 16);
    Script_GetArgString(args, 1, gMagicObjDefs[gMagicObjDefCount].model, 16);
    d = &gMagicObjDefs[gMagicObjDefCount];
    d->type = Script_GetArgInt(args, 2);
    d->data = NULL;
    h = gTaskHandlers[(uint8_t)gRigidEvents[1]][d->type];
    if (h) {
        struct { void *data; void *args; int unk8; } init;

        size = args;
        h(NULL, &size);
        init.data = Mem_Alloc((size_t)size, 4);
        init.args = args;
        init.unk8 = 3;
        gMagicObjDefs[gMagicObjDefCount].data = init.data;
        gTaskHandlers[(uint8_t)gRigidEvents[0]][gMagicObjDefs[gMagicObjDefCount].type](NULL, &init);
    }
    gMagicObjDefCount++;
    ret->i = 1;
    ret->d = 1.0;
    ret->type = gEmptyString;
    return ret;
}
HOOK(0x4C4520, DefineMagicObj);

void RigidDynCubeObj_ActivateHandler(RigidDynCubeObj *o, int *arg)
{
    Task *n;
    MagicHandler h;

    if (arg[0] == 0)
        DynCube_Add(arg[1], o, &o->pos.x, radius(o), arg[2]);
    n = o->task.head;
    if (!n->next)
        return;
    /* same walk as Task_BroadcastChildren */
    while (n) {
        gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
        h = gTaskHandlers[World_GetEvent0()][((Task *)n)->type];
        if (h)
            h(n, arg);
        n = gTaskStack[--gTaskStackTop];
    }
}
HOOK(0x4C4660, RigidDynCubeObj_ActivateHandler);

void RigidDynCubeObj_MoveHandler(RigidDynCubeObj *o, char *arg)
{
    DynCube_Move(gCurrentWorld, o, arg, radius(o));
    o->matrix = *(Matrix *)(arg + 0x18);
}
HOOK(0x4C4780, RigidDynCubeObj_MoveHandler);

void RigidDynCubeObj_Close(void)
{
    int i;

    Task_ClearGlobalFlags(0x8000000);
    TaskType_Free(gRigidType);
    gRigidType = 0x181;
    for (i = 0; i < gMagicObjDefCount; i++) {
        if (gMagicObjDefs[i].data) {
            Mem_Free(gMagicObjDefs[i].data);
            gMagicObjDefs[i].data = NULL;
        }
    }
    TaskEvent_Free(gRigidEvents[0]);
    TaskEvent_Free(gRigidEvents[1]);
    TaskEvent_Free(gRigidEvents[2]);
    TaskEvent_Free(gRigidEvents[3]);
    TaskEvent_Free(gRigidEvents[4]);
}
HOOK(0x4C4820, RigidDynCubeObj_Close);

uint16_t RigidDynCubeObj_GetType(void)
{
    return gRigidType;
}
HOOK(0x4C48C0, RigidDynCubeObj_GetType);

void RigidDynCubeObj_SetModel(RigidDynCubeObj *o, void **model)
{
    o->model = model;
}
HOOK(0x4C48D0, RigidDynCubeObj_SetModel);

/* runs one script from the magicobj folder; the scripts call DefineMagicObj */
void RigidDynCubeObj_ParseConfigFile(const char *path)
{
    void *s;

    if (crt_strstr(path, "vssver"))
        return;
    s = Script_Load(path);
    if (!s) {
        Error_Show("RigidDynCubeObj_ParseConfigFile(): Couldn't load script: %s", path);
        for (;;)
            ;
    }
    Script_Run(s);
    Script_Free(s);
}
HOOK(0x4C48E0, RigidDynCubeObj_ParseConfigFile);

void MagicObjLoad(const char *path)
{
    File_ScanDir(path, DIR_CALLBACK_4C2F90, 0);
}
HOOK(0x4C4930, MagicObjLoad);

char RigidDynCubeObj_GetEvent0(void)
{
    return gRigidEvents[0];
}
HOOK(0x4C56D0, RigidDynCubeObj_GetEvent0);

char RigidDynCubeObj_GetEvent1(void)
{
    return gRigidEvents[1];
}
HOOK(0x4C56E0, RigidDynCubeObj_GetEvent1);

char RigidDynCubeObj_GetEvent2(void)
{
    return gRigidEvents[2];
}
HOOK(0x4C56F0, RigidDynCubeObj_GetEvent2);

char RigidDynCubeObj_GetEvent3(void)
{
    return gRigidEvents[3];
}
HOOK(0x4C5700, RigidDynCubeObj_GetEvent3);

void RigidDynCubeObj_Init(int unk68, RigidDynCubeObj *o, void **model)
{
    DynCubeObj_ClearRenderData(o);
    o->world = (void *)unk68;
    o->model = model;
    memset(&o->matrix, 0, sizeof(o->matrix));
    o->matrix.m[0] = 1.0f;
    o->matrix.m[4] = 1.0f;
    o->matrix.m[8] = 1.0f;
    o->scale[0] = 1.0f;
    o->scale[1] = 1.0f;
    o->scale[2] = 1.0f;
    o->unkE4 = 0;
    o->lod = 0;
    o->unkD4 = 2;
    o->rgb[0].x = 1.0f;
    o->rgb[0].y = 1.0f;
    o->rgb[0].z = 1.0f;
    o->rgb[1] = (Vec3f){0};
    memset(o->unkC0, 0, sizeof(o->unkC0));
    o->unkD8 = 1;
    o->lightmapRefs = 0;
    o->unkE0 = 1;
    o->attached = 0;
    o->hidden = 1;
}
HOOK(0x4C5710, RigidDynCubeObj_Init);

char RigidDynCubeObj_GetEvent4(void)
{
    return gRigidEvents[4];
}
HOOK(0x4C5800, RigidDynCubeObj_GetEvent4);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 547F3C, 1) char store_547F3C[6] INIT({-1, -1, -1, -1, -1});
ALIAS(gRigidEvents, store_547F3C, 0);
DATA(547F3C) uint16_t gRigidType INIT(385);
BSS(A4433C) int gMagicObjDefCount;
STORE(igb, B97E00, 4) __typeof__(gMagicObjDefs[0]) store_B97E00[1024];
ALIAS(gMagicObjDefs, store_B97E00, 0);
