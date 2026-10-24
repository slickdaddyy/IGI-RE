#include "hook.h"
#include "igi.h"
#include "ammotype/ammotype.h"

FN_EXTERN(BulletHole_AddRigid);
FN_EXTERN(BulletHole_AddSpline);
FN_EXTERN(BulletHole_Delete);
FN_EXTERN(BulletHole_Draw);
FN_EXTERN(GraphNode_Run);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x4B0B00, void, Pool_Init, (void *pool, int size, int count, int align));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B1420, void, File_ScanDir, (const char *path, void *fn, void *user));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4D0D80, uint8_t, Mesh3D_GetEvent5, (void));
ORIG(0x4D97E0, uint8_t, QObj_GetEvent, (void));
ORIG(0x4E5350, uint16_t, SplineObjSegment_GetType, (void));
ORIG(0x4F1CD0, void, ParamEnum_Init, (void *e));
ORIG(0x4F1D80, void, ParamEnum_Free, (void *e));
ORIG(0x4F1DB0, void, ParamEnum_AddConstants, (void *e, const char *pattern));

#define BulletHole_Handler47D380 (FN(0x47D380, BulletHole_AddRigid))
#define BulletHole_Handler47E0C0 (FN(0x47E0C0, BulletHole_AddSpline))
#define BulletHole_Handler47D0B0 (FN(0x47D0B0, BulletHole_Draw))
#define BulletHole_Delete (FN(0x47D320, BulletHole_Delete))
#define BulletHole_Handler5188C0 (FN(0x5188C0, GraphNode_Run))

extern AmmoType gAmmoTypes[];
extern int gAmmoTypeCount;
extern uint8_t gAmmoListEvent;
extern char gAmmoIdEnum[];
extern uint8_t gBulletHoleEvent;
extern uint16_t gBulletHoleType;
extern char gBulletHolePool[];
extern char gBulletHolePool2[];
extern const char gScriptEmptyString[];

typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;

typedef struct DirEntry {
    char *name;
    int unk04;
    char isDir;
} DirEntry;

ScriptValue *DefineAmmoType(ScriptValue *out, void *data, void *args);
void AmmoType_LoadAll(void);
void BulletHole_Forward(void *t, void *arg);

/* runs ammo.qsc in every subfolder of LOCAL:ammo */
void AmmoType_Open(DirEntry *e)
{
    char path[256];
    void *script;

    if (!e->isDir)
        return;
    crt_sprintf(path, "%s/%s", e->name, "ammo.qsc");
    script = Script_Load(path);
    if (!script) {
        Error_Show("AmmoType_Open(): Couldn't load script: %s", "ammo.qsc");
        for (;;)
            ;
    }
    Script_Run(script);
    Script_Free(script);
}
HOOK(0x47CA40, AmmoType_Open);

void AmmoTypeOpen(void)
{
    Script_RegisterFunction("DefineAmmoType", DefineAmmoType, NULL, -1);
    AmmoType_LoadAll();
}
HOOK(0x47CAB0, AmmoTypeOpen);

ScriptValue *DefineAmmoType(ScriptValue *out, void *data, void *args)
{
    AmmoType *a = &gAmmoTypes[gAmmoTypeCount];
    char name[128];

    a->id = Script_GetArgInt(args, 0);
    Script_GetArgString(args, 1, name, sizeof(name));
    Script_RegisterConstant(name, a->id);
    Script_GetArgString(args, 2, a->name, sizeof(a->name));
    Script_GetArgString(args, 3, a->desc, sizeof(a->desc));
    a->caliber = Script_GetArgInt(args, 4);
    a->unk24 = Script_GetArgReal(args, 5);
    a->color0[0] = Script_GetArgInt(args, 6);
    a->color0[1] = Script_GetArgInt(args, 7);
    a->color0[2] = Script_GetArgInt(args, 8);
    a->color1[0] = Script_GetArgInt(args, 9);
    a->color1[1] = Script_GetArgInt(args, 10);
    a->color1[2] = Script_GetArgInt(args, 11);
    a->tracer = Script_GetArgInt(args, 12);
    Script_GetArgString(args, 13, a->unk148, sizeof(a->unk148));
    Script_GetArgString(args, 14, a->sound, sizeof(a->sound));
    gAmmoTypeCount++;
    out->i = 1;
    out->d = 1.0;
    out->s = gScriptEmptyString;
    return out;
}
HOOK(0x47CAD0, DefineAmmoType);

void AmmoType_Close(void)
{
    gAmmoTypeCount = 0;
}
HOOK(0x47CCE0, AmmoType_Close);

void AmmoType_LoadAll(void)
{
    gAmmoTypeCount = 0;
    File_ScanDir("LOCAL:ammo", AmmoType_Open, NULL);
}
HOOK(0x47CCF0, AmmoType_LoadAll);

AmmoType *AmmoType_Get(int i)
{
    return &gAmmoTypes[i];
}
HOOK(0x47CD10, AmmoType_Get);

int AmmoType_GetCount(void)
{
    return gAmmoTypeCount;
}
HOOK(0x47CD30, AmmoType_GetCount);

AmmoType *AmmoType_Find(int id)
{
    int count = AmmoType_GetCount();
    int i;

    for (i = 0; i < count; i++) {
        AmmoType *a = AmmoType_Get(i);
        if (a->id == id)
            return a;
    }
    return NULL;
}
HOOK(0x47CD40, AmmoType_Find);

AmmoType *AmmoType_FindByCaliber(int caliber)
{
    int count = AmmoType_GetCount();
    int i;

    for (i = 0; i < count; i++) {
        AmmoType *a = AmmoType_Get(i);
        if (a->caliber == caliber)
            return a;
    }
    return NULL;
}
HOOK(0x47CD70, AmmoType_FindByCaliber);

int Script_EvalConst(const char *expr)
{
    int v;

    Mem_PushMode(1);
    v = Script_EvalExpr(expr);
    Mem_PopMode();
    return v;
}
HOOK(0x47CDA0, Script_EvalConst);

uint8_t AmmoList_GetEvent(void)
{
    return gAmmoListEvent;
}
HOOK(0x47CDC0, AmmoList_GetEvent);

void AmmoList_Init(AmmoList *list)
{
    int i;

    list->count = 0;
    for (i = 0; i < AMMO_LIST_MAX; i++) {
        list->e[i].id = -1;
        list->e[i].count = 0;
        list->e[i].limit = -1;
    }
}
HOOK(0x47CDD0, AmmoList_Init);

static AmmoEntry *find_entry(AmmoList *list, int id)
{
    int i;

    for (i = 0; i < list->count; i++)
        if (list->e[i].id == id)
            return &list->e[i];
    return NULL;
}

/* adds n rounds (n may be negative), kept within 0..limit when there is one */
void AmmoList_Add(AmmoList *list, int id, int n)
{
    AmmoEntry *a = find_entry(list, id);

    if (a) {
        a->count += n;
    } else {
        a = &list->e[list->count];
        a->id = id;
        a->count = n;
        list->count++;
    }
    if (a->limit != -1) {
        if (a->count < 0)
            a->count = 0;
        else if (a->count > a->limit)
            a->count = a->limit;
    }
}
HOOK(0x47CE00, AmmoList_Add);

void AmmoList_SetLimit(AmmoList *list, int id, int limit)
{
    AmmoEntry *a = find_entry(list, id);

    if (a) {
        a->limit = limit;
    } else {
        a = &list->e[list->count];
        a->id = id;
        a->limit = limit;
        list->count++;
    }
    if (a->limit != -1) {
        if (a->count < 0)
            a->count = 0;
        else if (a->count > a->limit)
            a->count = a->limit;
    }
}
HOOK(0x47CE70, AmmoList_SetLimit);

AmmoEntry *AmmoList_Find(AmmoList *list, int id)
{
    return find_entry(list, id);
}
HOOK(0x47CEE0, AmmoList_Find);

void *AmmoType_GetEnum(void)
{
    return gAmmoIdEnum;
}
HOOK(0x47CF10, AmmoType_GetEnum);

void Ammo_Init(void)
{
    ParamEnum_Init(gAmmoIdEnum);
    ParamEnum_AddConstants(gAmmoIdEnum, "AMMO_ID_.*");
    gAmmoListEvent = TaskEvent_Alloc(0);
}
HOOK(0x47CF20, Ammo_Init);

void Ammo_Close(void)
{
    ParamEnum_Free(gAmmoIdEnum);
    TaskEvent_Free(gAmmoListEvent);
}
HOOK(0x47CF50, Ammo_Close);

void BulletHole_Init(void)
{
    gBulletHoleEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(RigidDynCubeObj_GetType(), gBulletHoleEvent, BulletHole_Handler47D380);
    TaskType_SetHandler(SplineObjSegment_GetType(), gBulletHoleEvent, BulletHole_Handler47E0C0);
    gBulletHoleType = TaskType_New("BulletHole", 0xE148, Task_GetRootType(), 0);
    TaskType_SetHandler(gBulletHoleType, RigidDynCubeObj_GetEvent2(), BulletHole_Handler47D0B0);
    TaskType_SetHandler2(gBulletHoleType, 2, BulletHole_Delete);
    TaskType_SetHandler2(gBulletHoleType, Mesh3D_GetEvent5(), BulletHole_Handler5188C0);
    TaskType_SetHandler(gBulletHoleType, QObj_GetEvent(), BulletHole_Forward);
    Pool_Init(gBulletHolePool, 4, 100, 4);
    Pool_Init(gBulletHolePool2, 0x40, 100, 4);
}
HOOK(0x47CF70, BulletHole_Init);

/* passes the event on to the task at +0xE130 */
void BulletHole_Forward(void *t, void *arg)
{
    Task *c = *(Task **)((char *)t + 0xE130);
    TaskFn fn;

    if (!c)
        return;
    fn = HANDLER(QObj_GetEvent(), c->type);
    if (fn)
        fn(c, arg);
}
HOOK(0x47D060, BulletHole_Forward);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, 5BE3E0, 5BE3E0, 0x2D04);
ALIAS(gAmmoTypes, pad_5BE3E0, 0);
BSS(5BE3E0) int gAmmoTypeCount;
BSS(5BE3E0) uint8_t gAmmoListEvent;
PAD(igb, 5BE3E0, 5C10E9, 0x7);
STORE(igb, 5BE3E0, 1) __typeof__(gAmmoIdEnum[0]) store_5C10F0[24];
ALIAS(gAmmoIdEnum, store_5C10F0, 0);
