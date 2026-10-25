#ifndef MAGICOBJ_H
#define MAGICOBJ_H

#include <stdint.h>

#include "igi.h"
#include "magicobj/magicobjtypes.h"
#include "litmap/litmaptypes.h"
#include "terrain/terraintypes.h"

typedef void (*MagicHandler)(void *task, void *arg);

typedef struct RigidCreateArg {
    const char *model;
    int unk4;
    double pos[3];
    char unk20[0x18];
    Matrix matrix;
} RigidCreateArg;

typedef struct RigidDrawView {
    int target;
    int size;
    double *center;
    float range;
} RigidDrawView;

typedef struct MagicScriptValue {
    int i;
    int unk4;
    double d;
    const void *type;
    int unk14;
} MagicScriptValue;

void RigidDynCubeObj_CreateHandler(RigidDynCubeObj *o, RigidCreateArg *arg);
void RigidDynCubeObj_GetInfo(RigidDynCubeObj *o, ObjInfo *info);
void RigidDynCubeObj_DrawCellMask(RigidDynCubeObj *o, RigidDrawView *view);
double *DynCubeObj_GetPosition(RigidDynCubeObj *o);
void RigidDynCubeObj_Open(void);
void RigidDynCubeObj_CollFilter(RigidDynCubeObj *o, char *arg);
MagicScriptValue *DefineMagicObj(MagicScriptValue *ret, void *data, void *args);
void RigidDynCubeObj_ActivateHandler(RigidDynCubeObj *o, int *arg);
void RigidDynCubeObj_MoveHandler(RigidDynCubeObj *o, char *arg);
void RigidDynCubeObj_Close(void);
uint16_t RigidDynCubeObj_GetType(void);
void RigidDynCubeObj_SetModel(RigidDynCubeObj *o, void **model);
void RigidDynCubeObj_ParseConfigFile(const char *path);
void MagicObjLoad(const char *path);
char RigidDynCubeObj_GetEvent0(void);
char RigidDynCubeObj_GetEvent1(void);
char RigidDynCubeObj_GetEvent2(void);
char RigidDynCubeObj_GetEvent3(void);
void RigidDynCubeObj_Init(int unk68, RigidDynCubeObj *o, void **model);
char RigidDynCubeObj_GetEvent4(void);

void Coll_ResetMods(void);
void Coll_SaveMods(void);
void Coll_RestoreMods(void);
int DynCubeTree_GetChildCount(int kind);
void World_Init(void);
void DynCubeObj_DeactivateHandler(Task *t, void **arg);
void World_Shutdown(void);
Terrain *World_New(int rooms);
void DynCubeNode_Init(Terrain *w, DynCubeNode *r, int unk04, int unk00, int index);
void World_Free(Terrain *w);
void World_LoadTextures(Terrain *w, const char *name);

#endif
