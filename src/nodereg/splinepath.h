#ifndef NODEREG_SPLINEPATH_H
#define NODEREG_SPLINEPATH_H

#include "nodereg/nodereg.h"

#define SPLINE_NODES 100
#define SPLINE_SUBSEGS 10

typedef struct Quat {
    float x, y, z, w;
    int unk;
} Quat;

/* "SplinePathNodeQTask" (0xC8) */
typedef struct SplinePathNode {
    Task task;
    Vec3d pos;
    Matrix matrix;
    Quat quat;
    int unk74;
    Vec3d tangent;
    float speed;
    float time;         /* at this node, from the speeds and lengths */
    float length;       /* of the segment to the next node */
    float segLength[SPLINE_SUBSEGS];
    char autoTangent;
    char unkC5[3];
} SplinePathNode;

/* "SplinePathDynCubeObj" (0x200) */
typedef struct SplinePath {
    Task task;
    char unk20[0x48];
    SplinePathNode *nodes[SPLINE_NODES];
    int count;
    int unk1FC;
} SplinePath;

/* filled by the slot 7 (GetInfo) handlers */
typedef struct ObjInfo {
    Vec3d pos;
    int unk18[3];
    Matrix matrix;
    float radius;
    char *name;
} ObjInfo;

_Static_assert(sizeof(SplinePathNode) == 0xC8, "SplinePathNode");
_Static_assert(offsetof(SplinePathNode, tangent) == 0x78, "SplinePathNode.tangent");
_Static_assert(offsetof(SplinePathNode, autoTangent) == 0xC4, "SplinePathNode.autoTangent");
_Static_assert(sizeof(SplinePath) == 0x200, "SplinePath");
_Static_assert(offsetof(ObjInfo, radius) == 0x4C, "ObjInfo");

void SplinePath_CollectNodes(SplinePath *p);
void SplinePath_RemoveNode(SplinePath *p, SplinePathNode *n);
void SplinePath_UpdateLengths(SplinePath *p);
void SplinePath_UpdateRotations(SplinePath *p);
long double SplinePathNode_GetStepScale(SplinePathNode *n, float u);
Vec3d *SplinePath_GetPos(Vec3d *out, SplinePath *p, int i, float u);
void SplinePath_GetMatrix(SplinePath *p, float *out, int i, float u);

#endif
