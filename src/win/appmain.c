#include <string.h>

#include "hook.h"
#include "win/win.h"

ORIG(0x4A5A0C, char *, crt_strtok, (char *s, const char *delim));
ORIG(0x4A5AD5, void, crt_exit, (int code));
ORIG(0x401000, char, Game_Start, (void));
ORIG(0x401050, void, Game_Update, (void));
ORIG(0x4010B0, void, Game_Shutdown, (void));
ORIG(0x498460, void, FPU_SetTruncate, (void));
ORIG(0x498480, void, DirectX_GetVersion, (DWORD *dx, DWORD *platform));

#define WINPROC_MAX 16

typedef struct WinProcEntry {
    WinNode node;
    WinProcCallback fn;
} WinProcEntry;

extern WinProcEntry gWinProcs[];
extern WinList gWinProcList;
extern char gRunning;
extern int gAltDown;
extern const char *gClassName;

static void Main_ParseWindowCB(const char *rest)
{
    AppContext_SetFullscreen(0);
}
HOOK(0x48F350, Main_ParseWindowCB);

void AppMain_ParseCmdLineArgs(const char *name, void (*fn)(const char *rest))
{
    char buf[512];
    char *tok;

    strcpy(buf, AppContext_GetCmdLine());
    for (tok = crt_strtok(buf, " \t"); tok; tok = crt_strtok(NULL, " \t")) {
        int i, match = 1;

        if (strlen(tok) < strlen(name))
            continue;
        for (i = 0; i < (int)strlen(name) && match; i++) {
            char a = tok[i], b = name[i];

            if (a >= 'a' && a <= 'z')
                a -= 0x20;
            if (b >= 'a' && b <= 'z')
                b -= 0x20;
            if (a != b)
                match = 0;
        }
        if (match)
            fn(tok + strlen(name));
    }
}
HOOK(0x48F360, AppMain_ParseCmdLineArgs);

void AppMain_SetRunning(char on)
{
    gRunning = on;
}
HOOK(0x48F480, AppMain_SetRunning);

void AppMain_AddWinProcCallback(WinProcCallback fn)
{
    int i;

    for (i = 0; i < WINPROC_MAX; i++) {
        if (!gWinProcs[i].fn) {
            gWinProcs[i].fn = fn;
            gWinProcs[i].node.prev = NULL;
            gWinProcs[i].node.next = NULL;
            List_AddTail(&gWinProcList, &gWinProcs[i].node);
            return;
        }
    }
    Error_Show("Failed to register WinProc callback.");
    for (;;)
        ;
}
HOOK(0x48F490, AppMain_AddWinProcCallback);

void AppMain_RemoveWinProcCallback(WinProcCallback fn)
{
    WinProcEntry *e;

    for (e = (WinProcEntry *)gWinProcList.head; e->node.next; e = (WinProcEntry *)e->node.next) {
        if (e->fn == fn) {
            List_Remove(&e->node);
            e->fn = NULL;
            return;
        }
    }
    Error_Show("Unable to remove WinProc callback");
    for (;;)
        ;
}
HOOK(0x48F4F0, AppMain_RemoveWinProcCallback);

static void Main_ParseWinInputCB(const char *rest)
{
    AppContext_SetWinInput(1);
}

static void Main_ParseGermanyCB(const char *rest)
{
    AppContext_SetCountry("Germany");
    AppContext_SetGerman(1);
}

static void Main_ParseUSACB(const char *rest)
{
    AppContext_SetCountry("USA");
}

static void Main_ParseHardcoreGermanCB(const char *rest)
{
    AppContext_SetGerman(0);
}

LRESULT WINAPI AppMain_WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

/* -1.75 must become -1: the game needs the x87 in truncating mode */
static int FPU_IsTruncating(void)
{
    float f = -1.75f;
    long long v;

    __asm__ volatile("fistpll %0" : "=m"(v) : "t"(f) : "st");
    return (int)v == -1;
}

int WINAPI AppMain_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int nShowCmd)
{
    WNDCLASSA wc;
    MSG msg;
    char text[1024];
    HANDLE mutex;
    HWND prev, hwnd;
    DWORD dxVersion, platform;
    int result = 0, x, y;

    mutex = CreateMutexA(NULL, TRUE, "IGIMUTEX");
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_INVALID_HANDLE) {
        MessageBoxA(NULL, "IGI is already running.", "Error", MB_ICONERROR);
        return 0;
    }
    CoInitialize(NULL);
    prev = GetForegroundWindow();
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AppMain_WinProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconA(hInstance, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)7;
    wc.lpszMenuName = gClassName;
    wc.lpszClassName = gClassName;
    RegisterClassA(&wc);

    AppContext_SetCmdLine(cmdLine);
    AppContext_SethInstance(hInstance);
    AppContext_SetFileSysCfg("filesys.cfg");
    AppContext_SetName("Innerloop");
    AppContext_SetActive(1);
    AppContext_SetFullscreen(1);
    AppContext_SetDebugged(0);
    AppContext_SetCompileScripts(1);
    AppContext_SetDebugText(0);
    AppContext_SetLightmapsUsed(1);
    AppContext_SetTerrainLightmapsUsed(1);
    AppContext_SetWinInput(0);
    AppContext_SetUnk4C(0);
    AppContext_SetUnk50(0);
    AppContext_SetPlayIntro(1);
    AppContext_SetGerman(0);
    AppContext_SetCountry("");
    AppContext_SetUnk44(0);

    memset(gWinProcs, 0, sizeof(WinProcEntry) * WINPROC_MAX);
    gWinProcList.head = (WinNode *)&gWinProcList.tail;
    gWinProcList.tailpred = (WinNode *)&gWinProcList.head;
    gWinProcList.tail = NULL;

    AppMain_ParseCmdLineArgs("Window", Main_ParseWindowCB);
    AppMain_ParseCmdLineArgs("WinInput", Main_ParseWinInputCB);
    AppMain_ParseCmdLineArgs("Germany", Main_ParseGermanyCB);
    AppMain_ParseCmdLineArgs("USA", Main_ParseUSACB);

    DirectX_GetVersion(&dxVersion, &platform);
    if (dxVersion < 0x700) {
        /* the format has two %d but only 7 is passed, the second reads a zero */
        crt_sprintf(text, "This application requires DirectX version %d.%d or greater to run.", 7, 0);
        MessageBoxA(NULL, text, "Error", MB_ICONERROR);
        crt_exit(1);
    }

    y = (GetSystemMetrics(SM_CYSCREEN) - 480) / 2;
    x = (GetSystemMetrics(SM_CXSCREEN) - 640) / 2;
    hwnd = CreateWindowExA(0, gClassName, gClassName, AppContext_IsFullscreen() ? 0x96000000 : 0x37C80000,
                           x, y, 640, 480, NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        MessageBoxA(NULL, "Couldn't open window", "Innerloop", 0);
        return 0;
    }
    ShowCursor(FALSE);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    AppContext_SethWnd(hwnd);
    FPU_SetTruncate();
    gRunning = Game_Start();
    AppMain_ParseCmdLineArgs("HardcoreGerman", Main_ParseHardcoreGermanCB);

    while (gRunning) {
        if (!FPU_IsTruncating()) {
            Error_Show("Invalid FPU rounding mode. May be caused by running a debug session with ftol "
                       "optimize turned on.");
            for (;;)
                ;
        }
        FPU_SetTruncate();
        if (AppContext_IsActive())
            Game_Update();
        else
            Sleep(500);
        if (!gRunning)
            DestroyWindow(hwnd);
        while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, NULL, 0, 0)) {
                gRunning = 0;
                result = msg.wParam;
                goto done;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
done:
    SetForegroundWindow(prev);
    UnregisterClassA(gClassName, hInstance);
    CoUninitialize();
    ReleaseMutex(mutex);
    return result;
}
HOOK(0x48F530, AppMain_WinMain);

HOOK(0x48F920, Main_ParseWinInputCB);
HOOK(0x48F930, Main_ParseGermanyCB);
HOOK(0x48F950, Main_ParseUSACB);
HOOK(0x48F960, Main_ParseHardcoreGermanCB);

LRESULT WINAPI AppMain_WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (gRunning) {
        WinProcEntry *e;

        for (e = (WinProcEntry *)gWinProcList.head; e->node.next; e = (WinProcEntry *)e->node.next) {
            if (!e->fn(hwnd, msg, wparam, lparam))
                return 0;
        }
    }
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_DESTROY:
        gRunning = 0;
        ShowCursor(TRUE);
        Game_Shutdown();
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATEAPP:
        FPU_SetTruncate();
        return 0;
    case WM_SYSKEYDOWN:
        if (wparam == VK_F4) {
            if (gAltDown)
                PostMessageA(AppContext_GethWnd(), WM_DESTROY, 0, 0);
        } else if (wparam == VK_MENU) {
            gAltDown = 1;
        }
        return 0;
    case WM_SYSKEYUP:
        if (wparam == VK_MENU)
            gAltDown = 0;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}
HOOK(0x48F970, AppMain_WinProc);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_53B534);
PAD(igd, 535648, 535648, 0x1) INIT({0x41});
DATA(535648) char str_535649[1] INIT("");
PAD(igd, 535648, 53564A, 0x2) INIT({0x66});
DATA(541D68) const char *gClassName INIT((__typeof__(gClassName))ref_str_53B534);
PAD(igd, 54534C, 54534C, 0x3);
DATA(54534C) char str_54534F[1] INIT("");
PAD(igb, 5C8D28, 5C8D28, 0xC0);
ALIAS(gWinProcs, pad_5C8D28, 0);
BSS(5C8D28) char gRunning;
PAD(igb, 5C8D28, 5C8DE9, 0x7);
BSS(5C8D28) WinList gWinProcList;
BSS(5C8E04) int gAltDown;
