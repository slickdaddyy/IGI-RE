#ifndef MAGICOBJ_H
#define MAGICOBJ_H

#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "magicobj/magicobjtypes.h"
#include "litmap/litmaptypes.h"

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
    void *type;
    int unk14;
} MagicScriptValue;

typedef struct MagicPool {
    char *data;
    int *slots;
    int used;
    int capacity;
    int stride;
    int index_offset;
} MagicPool;

typedef struct WorldRoom {
    int unk00;
    int unk04;
    int unk08[10];
    char unk30;
    char unk31[3];
    int index;
    uint16_t unk38;
    uint16_t unk3A;
    void *unk3C;
} WorldRoom;

typedef struct World {
    int unk00;
    void *unk04;
    void *unk08;
    void *unk0C;
    WorldRoom *rooms[6];
    char unk28[0xE8 - 0x28];
    int unkE8[6];
    int unk100[6];
    int unk118[6][0x7F];
    MagicPool pool;
    int unkD18;
    int unkD1C[8];
    char unkD3C[0xDBC - 0xD3C];
    void *texbank;
    void *unkDC0;
    void *unkDC4;
    int unkDC8;
    void **unkDCC;      /* 256 entries of 3 words */
    void **unkDD0;
    char unkDD4[0xDE0 - 0xDD4];
    int unkDE0;
    int unkDE4;
    int unkDE8;
    int unkDEC;
    int unkDF0;
    void *palette;
    int *lightmaps;
    int unkDFC;
    char lightmapPath[0x100];
} World;

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
World *World_New(int rooms);
void DynCubeNode_Init(World *w, WorldRoom *r, int unk04, int unk00, int index);
void World_Free(World *w);
void World_LoadTextures(World *w, const char *name);

#endif
