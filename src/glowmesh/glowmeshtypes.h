#ifndef GLOWMESH_GLOWMESHTYPES_H
#define GLOWMESH_GLOWMESHTYPES_H

typedef struct MeshDrawArg {
    void *obj;          /* the object drawn */
    int *lights;
    int lightCount;
    int *haze;
    int hazeCount;
} MeshDrawArg;

#endif
