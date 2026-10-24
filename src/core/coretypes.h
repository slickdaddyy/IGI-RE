#ifndef CORE_CORETYPES_H
#define CORE_CORETYPES_H

#define RENDERMODE_MAX 8
#define RENDERMODE_METHOD_MAX 128

typedef int (*RenderMethod)(void *arg);

typedef struct RenderMode {
    const char *name;
    RenderMethod methods[RENDERMODE_METHOD_MAX];
    char used;
} RenderMode;

typedef struct RenderModeInfo {
    int index;
    const char *name;
} RenderModeInfo;

#endif
