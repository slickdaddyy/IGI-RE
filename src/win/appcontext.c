#include <string.h>

#include "hook.h"
#include "win/win.h"

ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));

void AppContext_Open(void)
{
    if (!gApp.country[0]) {
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SENGCOUNTRY, gApp.country, 0x100);
        if (crt_stricmp(gApp.country, "Germany") == 0)
            AppContext_SetGerman(1);
    }
    AppContext_SetActiveCallback(NULL, 0);
}
HOOK(0x48F020, AppContext_Open);

void AppContext_Close(void)
{
    AppContext_SetActiveCallback(NULL, 0);
}
HOOK(0x48F070, AppContext_Close);

const char *AppContext_GetCmdLine(void)
{
    return gApp.cmdLine;
}
HOOK(0x48F080, AppContext_GetCmdLine);

HINSTANCE AppContext_GethInstance(void)
{
    return gApp.hInstance;
}
HOOK(0x48F090, AppContext_GethInstance);

HWND AppContext_GethWnd(void)
{
    return gApp.hWnd;
}
HOOK(0x48F0A0, AppContext_GethWnd);

const char *AppContext_GetName(void)
{
    return gApp.name;
}
HOOK(0x48F0B0, AppContext_GetName);

void AppContext_SetCmdLine(const char *cmdLine)
{
    gApp.cmdLine = cmdLine;
}
HOOK(0x48F0C0, AppContext_SetCmdLine);

void AppContext_SethInstance(HINSTANCE hInstance)
{
    gApp.hInstance = hInstance;
}
HOOK(0x48F0D0, AppContext_SethInstance);

void AppContext_SethWnd(HWND hWnd)
{
    gApp.hWnd = hWnd;
}
HOOK(0x48F0E0, AppContext_SethWnd);

void AppContext_SetFileSysCfg(const char *path)
{
    gApp.fileSysCfg = path;
}
HOOK(0x48F0F0, AppContext_SetFileSysCfg);

void AppContext_SetName(const char *name)
{
    gApp.name = name;
    if (gApp.hWnd)
        SetWindowTextA(gApp.hWnd, name);
}
HOOK(0x48F100, AppContext_SetName);

void AppContext_SetFullscreen(char on)
{
    gApp.fullscreen = on;
}
HOOK(0x48F120, AppContext_SetFullscreen);

int AppContext_IsFullscreen(void)
{
    return gApp.fullscreen;
}
HOOK(0x48F130, AppContext_IsFullscreen);

void AppContext_SetActive(int active)
{
    gApp.active = (char)active;
    if (gApp.activeFn)
        gApp.activeFn(active, gApp.activeData);
}
HOOK(0x48F140, AppContext_SetActive);

int AppContext_IsActive(void)
{
    return gApp.active;
}
HOOK(0x48F170, AppContext_IsActive);

void AppContext_SetActiveCallback(void (*fn)(int active, int data), int data)
{
    gApp.activeFn = fn;
    gApp.activeData = data;
}
HOOK(0x48F180, AppContext_SetActiveCallback);

void AppContext_SetDebugged(char on)
{
    gApp.debugged = on;
}
HOOK(0x48F1A0, AppContext_SetDebugged);

int AppContext_IsDebugged(void)
{
    return gApp.debugged;
}
HOOK(0x48F1B0, AppContext_IsDebugged);

void AppContext_SetCompileScripts(char on)
{
    gApp.compileScripts = on;
}
HOOK(0x48F1C0, AppContext_SetCompileScripts);

int AppContext_GetCompileScripts(void)
{
    return gApp.compileScripts;
}
HOOK(0x48F1D0, AppContext_GetCompileScripts);

void AppContext_SetDebugText(char on)
{
    gApp.debugText = on;
}
HOOK(0x48F1E0, AppContext_SetDebugText);

int AppContext_GetDebugText(void)
{
    return gApp.debugText;
}
HOOK(0x48F1F0, AppContext_GetDebugText);

void AppContext_SetUnk44(int v)
{
    gApp.unk44 = v;
}
HOOK(0x48F200, AppContext_SetUnk44);

void AppContext_SetPlayIntro(int on)
{
    gApp.playIntro = on;
}
HOOK(0x48F210, AppContext_SetPlayIntro);

int AppContext_GetPlayIntro(void)
{
    return gApp.playIntro;
}
HOOK(0x48F220, AppContext_GetPlayIntro);

int AppContext_GetLightmapsUsed(void)
{
    return gApp.lightmaps;
}
HOOK(0x48F230, AppContext_GetLightmapsUsed);

void AppContext_SetLightmapsUsed(char on)
{
    gApp.lightmaps = on;
}
HOOK(0x48F240, AppContext_SetLightmapsUsed);

int AppContext_GetTerrainLightmapsUsed(void)
{
    return gApp.terrainLightmaps;
}
HOOK(0x48F250, AppContext_GetTerrainLightmapsUsed);

void AppContext_SetTerrainLightmapsUsed(char on)
{
    gApp.terrainLightmaps = on;
}
HOOK(0x48F260, AppContext_SetTerrainLightmapsUsed);

int AppContext_GetWinInput(void)
{
    return gApp.winInput;
}
HOOK(0x48F270, AppContext_GetWinInput);

void AppContext_SetWinInput(int on)
{
    gApp.winInput = on;
}
HOOK(0x48F280, AppContext_SetWinInput);

void AppContext_SetUnk4C(int v)
{
    gApp.unk4C = v;
}
HOOK(0x48F290, AppContext_SetUnk4C);

void AppContext_SetUnk50(int v)
{
    gApp.unk50 = v;
}
HOOK(0x48F2A0, AppContext_SetUnk50);

void AppContext_SetGerman(int on)
{
    gApp.german = on;
}
HOOK(0x48F2B0, AppContext_SetGerman);

int AppContext_IsGerman(void)
{
    return gApp.german;
}
HOOK(0x48F2C0, AppContext_IsGerman);

int AppContext_IsUSA(void)
{
    return crt_stricmp(gApp.country, "USA") == 0;
}
HOOK(0x48F2D0, AppContext_IsUSA);

char *AppContext_GetCountry(void)
{
    return gApp.country;
}
HOOK(0x48F2F0, AppContext_GetCountry);

void AppContext_SetCountry(const char *country)
{
    strcpy(gApp.country, country);
}
HOOK(0x48F300, AppContext_SetCountry);

void AppContext_SetQvmOnly(int on)
{
    gApp.qvmOnly = on;
}
HOOK(0x48F330, AppContext_SetQvmOnly);

int AppContext_GetQvmOnly(void)
{
    return gApp.qvmOnly;
}
HOOK(0x48F340, AppContext_GetQvmOnly);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5C8BC0) AppContext gApp;
