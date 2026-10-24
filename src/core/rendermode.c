#include <string.h>

#include "hook.h"
#include "igi.h"
#include "core/error.h"
#include "core/rendermode.h"

extern RenderMode gRenderModes[];
extern RenderModeMethod gRenderMethods[];
extern RenderMode gRenderModeCurrent;
extern int gRenderModeIndex;
extern int gRenderModeStack[];
extern int gRenderModeStackTop;

void RenderMode_Init(void)
{
    memset(gRenderModes, 0, sizeof(RenderMode) * RENDERMODE_MAX);
    memset(&gRenderModeCurrent, 0, sizeof(RenderMode));
    memset(gRenderMethods, 0, sizeof(RenderModeMethod) * RENDERMODE_METHOD_MAX);
    gRenderModeStackTop = 8;
    gRenderModeIndex = -1;
}
HOOK(0x4B0DC0, RenderMode_Init);

int RenderMode_Create(const char *name)
{
    int i;

    for (i = 0; i < RENDERMODE_MAX; i++) {
        if (!gRenderModes[i].used) {
            gRenderModes[i].name = name;
            gRenderModes[i].used = 1;
            return i;
        }
    }
    Error_Show("RenderMode: Failed to create new rendermode '%s'", name);
    for (;;)
        ;
}
HOOK(0x4B0E00, RenderMode_Create);

void RenderMode_Delete(int mode)
{
    gRenderModes[mode].used = 0;
}
HOOK(0x4B0E50, RenderMode_Delete);

int RenderMode_CreateMethod(const char *name)
{
    int i;

    for (i = 0; i < RENDERMODE_METHOD_MAX; i++) {
        if (!gRenderMethods[i].used) {
            gRenderMethods[i].name = name;
            gRenderMethods[i].used = 1;
            return i;
        }
    }
    Error_Show("RenderMode: Failed to create new method '%s'", name);
    for (;;)
        ;
}
HOOK(0x4B0E70, RenderMode_CreateMethod);

void RenderMode_DeleteMethod(int method)
{
    gRenderMethods[method].used = 0;
}
HOOK(0x4B0EB0, RenderMode_DeleteMethod);

void RenderMode_SetMethod(int mode, int method, RenderMethod fn)
{
    gRenderModes[mode].methods[method] = fn;
}
HOOK(0x4B0EC0, RenderMode_SetMethod);

void RenderMode_ForEach(void (*fn)(RenderModeInfo *info, void *user), void *user)
{
    int i;

    for (i = 0; i < RENDERMODE_MAX; i++) {
        if (gRenderModes[i].used) {
            RenderModeInfo info;

            info.index = i;
            info.name = gRenderModes[i].name;
            fn(&info, user);
        }
    }
}
HOOK(0x4B0EE0, RenderMode_ForEach);

void RenderMode_Select(int mode)
{
    if (mode != gRenderModeIndex) {
        gRenderModeIndex = mode;
        memcpy(&gRenderModeCurrent, &gRenderModes[mode], sizeof(RenderMode));
    }
}
HOOK(0x4B0F30, RenderMode_Select);

void RenderMode_Deselect(void)
{
    gRenderModeIndex = -1;
}
HOOK(0x4B0F70, RenderMode_Deselect);

int RenderMode_GetCurrent(void)
{
    return gRenderModeIndex;
}
HOOK(0x4B0F80, RenderMode_GetCurrent);

int RenderMode_Pop(void)
{
    int mode = gRenderModeStack[gRenderModeStackTop];

    if (mode == -1) {
        gRenderModeIndex = -1;
        return -1;
    }
    gRenderModeStackTop++;
    RenderMode_Select(mode);
    return mode;
}
HOOK(0x4B0F90, RenderMode_Pop);

void RenderMode_Push(int mode)
{
    gRenderModeStack[--gRenderModeStackTop] = gRenderModeIndex;
    RenderMode_Select(mode);
}
HOOK(0x4B0FD0, RenderMode_Push);

int RenderMode_Call(int mode, int method, void *arg)
{
    return gRenderModes[mode].methods[method](arg);
}
HOOK(0x4B1000, RenderMode_Call);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(942304) int gRenderModeStackTop;
STORE(igb, 942304, 4) int store_942308[10];
ALIAS(gRenderModeStack, store_942308, 0);
BSS(A94A64) int gRenderModeIndex;
PAD(igb, A94A64, A94A68, 0x18);
STORE(igb, A94A64, 4) __typeof__(gRenderMethods[0]) store_A94A80[128];
ALIAS(gRenderMethods, store_A94A80, 0);
STORE(igb, A950A0, 4) __typeof__(gRenderModes[0]) store_A950A0[8];
ALIAS(gRenderModes, store_A950A0, 0);
