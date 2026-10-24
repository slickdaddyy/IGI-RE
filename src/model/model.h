#ifndef MODEL_MODEL_H
#define MODEL_MODEL_H

#include <stdint.h>

#include "igi.h"
#include "core/list.h"
#include "model/modeltypes.h"

#define VIRMODEL_MAX 600
#define VIRMODEL_LODS 5

/* Big endian chunk ids of the .mtp and collision .mef files */
#define MTP_VNAM 0x564E414D
#define MTP_INST 0x494E5354
#define MTP_GTT 0x47545420
#define MTP_MODS 0x4D4F4453
#define MTP_SVOL 0x53564F4C
#define MTP_TEXF 0x54455846
#define MTP_SMES 0x534D4553
#define MTP_SFAC 0x53464143
#define MTP_SVTX 0x53565458
#define MTP_EDGE 0x45444745

/* Chunk ids of the ILFF .mef mesh files */
#define MEF_MESH 0x4D455348
#define MEF_ATTA 0x41545441
#define MEF_MVTX 0x4D565458
#define MEF_PORT 0x504F5254
#define MEF_PTFC 0x50544643
#define MEF_PTVX 0x50545658
#define MEF_NAXT 0x4E415854

typedef struct MeshAnim {
    int unk0;
    int unk4;
    int unk8;
    int unkC;
    void *data;
} MeshAnim;

/* A collision mesh from <dir>models/<name>.mef */
typedef struct CollMeshRes {
    Node node;
    int count;
    int *parts;       /* 0x1C bytes each, the first three words become pointers */
    void *res;
} CollMeshRes;

/* One loaded <dir><name>.mtp file */
typedef struct ModelFile {
    Node node;
    void *mtp;
    void *res;
    List meshes;      /* Mesh */
    List coll;        /* CollMeshRes */
    List lods;        /* ModelLod */
    char *textures;   /* 0x24 bytes each */
    char *texinfo;    /* 0xC bytes each */
    char *unk3C;
    int texture_count;
    int unk44;
    int unk48;
} ModelFile;

int Model_GetInitMethod(void);
int Model_GetDeinitMethod(void);
int Model_GetLoadMethod(void);
int Model_GetUnloadMethod(void);
void *Mtp_FindChunk(const void *file, unsigned int id, unsigned int *size);
VirModel *VirModel_Add(const char *name);
VirModel *VirModel_Find(const char *name);
VirModel *VirModel_Get(const char *name);
void VirModel_InitLods(VirModel *m);
ModelLod *ModelLod_Find(const char *name);
void VirModel_RestoreCount(void);
void Model_Init(void);
void VirModel_SaveCount(void);
void Model_Shutdown(void);
ModelFile *ModelFile_Load(const char *name);
void VirModel_InitAllLods(void);
void ModelFile_Setup(ModelFile *f, const char *name, int textures);
Node *List_GetNth(List *list, int n);
void ModelFile_LoadMeshes(ModelFile *f);
Mesh *Mesh_Load(const char *name);
void ModelFile_LoadCollMeshes(ModelFile *f);
CollMeshRes *CollMesh_Load(const char *name);
void ModelFile_LoadTextures(ModelFile *f);

#endif
