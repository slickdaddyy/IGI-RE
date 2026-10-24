#ifndef MAGICOBJ_H
#define MAGICOBJ_H

#include <stdint.h>

#include "igi.h"

typedef struct MagicNode {
    struct MagicNode *next;
    struct MagicNode *prev;
} MagicNode;

/* Task header shared by every task instance */
typedef struct MagicTask {
    MagicNode node;
    MagicNode *head;    /* child list */
    MagicNode *tail;
    MagicNode *tailpred;
    struct MagicTask *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} MagicTask;

typedef void (*MagicHandler)(void *task, void *arg);

typedef struct MagicMatrix {
    float m[9];
    int unk24;
} MagicMatrix;

/* "RigidDynCubeObj", a DynCubeObj with a model, orientation and scale */
typedef struct RigidDynCubeObj {
    MagicTask task;
    double pos[3];
    char unk38[0x68 - 0x38];
    int unk68;
    void **model;       /* VirModel, one entry per LOD */
    MagicMatrix matrix;
    float scale[3];
    int lod;
    float unkA8[3];
    int unkB4[3];
    int unkC0[5];
    int unkD4;
    int unkD8;
    int unkDC;
    int unkE0;
    int unkE4;
    int unkE8;
    int unkEC;
} RigidDynCubeObj;

typedef struct RigidCreateArg {
    const char *model;
    int unk4;
    double pos[3];
    char unk20[0x18];
    MagicMatrix matrix;
} RigidCreateArg;

typedef struct RigidInfo {
    double pos[3];
    int unk18[3];
    MagicMatrix matrix;
    float radius;
    char *name;
} RigidInfo;

typedef struct RigidDrawView {
    int target;
    int size;
    double *center;
    float range;
} RigidDrawView;

/* one DefineMagicObj() entry */
typedef struct MagicObjDef {
    char name[16];
    char model[16];
    uint16_t type;
    uint16_t unk22;
    void *data;
} MagicObjDef;

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

typedef struct Lightmap {
    int size;
    int *data;
    char texture[0x24];
} Lightmap;

void RigidDynCubeObj_CreateHandler(RigidDynCubeObj *o, RigidCreateArg *arg);
void RigidDynCubeObj_GetInfo(RigidDynCubeObj *o, RigidInfo *info);
void sub_4C31F0(RigidDynCubeObj *o, RigidDrawView *view);
double *DynCubeObj_GetPosition(RigidDynCubeObj *o);
void RigidDynCubeObj_Open(void);
void sub_4C35D0(RigidDynCubeObj *o, char *arg);
MagicScriptValue *DefineMagicObj(MagicScriptValue *ret, void *data, void *args);
void sub_4C4660(RigidDynCubeObj *o, int *arg);
void sub_4C4780(RigidDynCubeObj *o, char *arg);
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
void sub_4C5900(void);
void sub_4C5A10(MagicTask *t, void **arg);
void sub_4C5AA0(void);
World *World_New(int rooms);
void DynCubeNode_Init(World *w, WorldRoom *r, int unk04, int unk00, int index);
void World_Free(World *w);
void sub_4C5F00(World *w, const char *name);

#endif
