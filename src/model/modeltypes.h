#ifndef MODEL_MODELTYPES_H
#define MODEL_MODELTYPES_H

#include <stdint.h>

#include "types.h"

/* the MESH chunk of a .mef file */
typedef struct MeshHeader {
    float version;
    int time[7];
    int format;         /* 0 rigid, 1 bone, 3 lightmapped */
    void *data;         /* per format: MeshRender, LightmapMesh */
    int unk28;
    struct MeshColl *coll;
    Vec3f center;
    float sphere;       /* radius around center */
    char unk40[0x20];
    int complexity;
    char unk64[0x14];
    float radius;
    int16_t vertex_count;
    int16_t attach_count;
    char unk80[8];
    float (*verts)[4];  /* MVTX */
    char *attach;       /* ATTA, 0x44 bytes each */
} MeshHeader;
_Static_assert(sizeof(MeshHeader) == 0x90, "MeshHeader");

/* A mesh from LOCAL:models/<name>.mef */
typedef struct Mesh {
    Node node;
    void *res;
    MeshHeader *header; /* MESH chunk, version 0.1 */
    int anim_count;
    struct MeshAnim *anims;
} Mesh;

/* A named mesh instance with its textures. VirModel LODs point at these. */
typedef struct ModelLod {
    Node node;
    const char *name;
    Mesh *mesh;
    void **textures;
    void **texinfo;
    int count;
} ModelLod;
_Static_assert(sizeof(ModelLod) == 0x1C, "ModelLod");

/* Up to five LODs named <name>, then the last letter counted up */
typedef struct VirModel {
    ModelLod *lods[5];
    char name[16];
    int unk24;
    char unk28[0x14];
    uint16_t lod_count;
    uint16_t lod_dist[5];
} VirModel;
_Static_assert(sizeof(VirModel) == 0x48, "VirModel");

#endif
