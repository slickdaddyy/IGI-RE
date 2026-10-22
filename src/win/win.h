#ifndef WIN_H
#define WIN_H

#include <windows.h>

#include "igi.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));

typedef struct WinNode {
    struct WinNode *next;
    struct WinNode *prev;
} WinNode;

typedef struct WinList {
    WinNode *head;
    WinNode *tail;
    WinNode *tailpred;
} WinList;

ORIG(0x4AF8F0, void, List_AddTail, (WinList *list, WinNode *node));
ORIG(0x4AF960, void, List_Remove, (WinNode *node));

typedef int (*WinProcCallback)(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

typedef struct AppContext {
    HINSTANCE hInstance;
    HWND hWnd;
    const char *cmdLine;
    const char *fileSysCfg;
    int unk10;
    const char *name;
    int unk18;
    int german;
    int winInput;
    int lightmaps;
    int terrainLightmaps;
    int compileScripts;
    int qvmOnly;
    int debugText;
    int debugged;
    int active;
    int fullscreen;
    int unk44;
    int playIntro;
    int unk4C;
    int unk50;
    int unk54;
    void (*activeFn)(int active, int data);
    int activeData;
    char country[0x100];
} AppContext;

#define gApp VAR(AppContext, 0x5C8BC0)

/* A screen mode. The active one is at 0xC28B40. */
typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
    int unkC;
    int depth;
    char unk14;
    char unk15;
    char unk16;
    char unk17;
    int renderMode;
    char device[0x80];
    char desc[0x80];
} DisplayMode;

void AppContext_Open(void);
void AppContext_Close(void);
const char *AppContext_GetCmdLine(void);
HINSTANCE AppContext_GethInstance(void);
HWND AppContext_GethWnd(void);
const char *AppContext_GetName(void);
void AppContext_SetCmdLine(const char *cmdLine);
void AppContext_SethInstance(HINSTANCE hInstance);
void AppContext_SethWnd(HWND hWnd);
void AppContext_SetFileSysCfg(const char *path);
void AppContext_SetName(const char *name);
void AppContext_SetFullscreen(char on);
int AppContext_IsFullscreen(void);
void AppContext_SetActive(int active);
void AppContext_SetActiveCallback(void (*fn)(int active, int data), int data);
int AppContext_IsActive(void);
void AppContext_SetDebugged(char on);
int AppContext_IsDebugged(void);
void AppContext_SetCompileScripts(char on);
int AppContext_GetCompileScripts(void);
void AppContext_SetDebugText(char on);
int AppContext_GetDebugText(void);
void AppContext_SetUnk44(int v);
void AppContext_SetPlayIntro(int on);
int AppContext_GetPlayIntro(void);
int AppContext_GetLightmapsUsed(void);
void AppContext_SetLightmapsUsed(char on);
int AppContext_GetTerrainLightmapsUsed(void);
void AppContext_SetTerrainLightmapsUsed(char on);
int AppContext_GetWinInput(void);
void AppContext_SetWinInput(int on);
void AppContext_SetUnk4C(int v);
void AppContext_SetUnk50(int v);
void AppContext_SetGerman(int on);
int AppContext_IsGerman(void);
int AppContext_IsUSA(void);
char *AppContext_GetCountry(void);
void AppContext_SetCountry(const char *country);
void AppContext_SetQvmOnly(int on);
int AppContext_GetQvmOnly(void);
void AppMain_ParseCmdLineArgs(const char *name, void (*fn)(const char *rest));
void AppMain_AddWinProcCallback(WinProcCallback fn);
void AppMain_RemoveWinProcCallback(WinProcCallback fn);
int Timer_GetMs(void);

#endif
