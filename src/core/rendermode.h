#ifndef RENDERMODE_H
#define RENDERMODE_H

#include "core/coretypes.h"

#define RENDERMODE_MAX 8
#define RENDERMODE_METHOD_MAX 128

typedef int (*RenderMethod)(void *arg);

typedef struct RenderMode {
    const char *name;
    RenderMethod methods[RENDERMODE_METHOD_MAX];
    char used;
} RenderMode;

typedef struct RenderModeMethod {
    const char *name;
    char used;
} RenderModeMethod;

void RenderMode_Init(void);
int RenderMode_Create(const char *name);
void RenderMode_Delete(int mode);
int RenderMode_CreateMethod(const char *name);
void RenderMode_DeleteMethod(int method);
void RenderMode_SetMethod(int mode, int method, RenderMethod fn);
void RenderMode_ForEach(void (*fn)(RenderModeInfo *info, void *user), void *user);
void RenderMode_Select(int mode);
void RenderMode_Deselect(void);
int RenderMode_GetCurrent(void);
int RenderMode_Pop(void);
void RenderMode_Push(int mode);
int RenderMode_Call(int mode, int method, void *arg);

#endif
