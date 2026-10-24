#include <string.h>

#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include "hook.h"
#include "win/win.h"

/* the exe's import thunks; our own DLL is dinput.dll */
ORIG_STD(0x4AE730, HRESULT, DirectInputCreateEx_, (HINSTANCE inst, DWORD version, REFIID iid, void **out,
                                                   IUnknown *outer));
ORIG_STD(0x4AF740, HRESULT, DirectInputCreateA_, (HINSTANCE inst, DWORD version, IDirectInputA **out,
                                                  IUnknown *outer));

extern Mouse gMouse;
extern int gMouseWheel;
extern int gMouseRight;
extern int gMouseLeft;
extern IDirectInput7A *gMouseDI;
extern int gMouseMiddle;
extern IDirectInputDevice7A *gMouseDev;

extern IDirectInputDeviceA *gKeyboardDev;
extern IDirectInputA *gKeyboardDI;
extern int gKeyboardAcquired;
extern char gKeyboardBlocked;
extern char gKeyboardPresent;
extern unsigned char gKeys[];
extern unsigned char gKeysPrev[];
extern unsigned char gWinKeys[];

extern const IID IID_IDirectInput7A_[];
extern const IID IID_IDirectInputDevice7A_[];
extern const GUID GUID_SysMouse_[];
extern const GUID GUID_SysKeyboard_[];
extern const DIDATAFORMAT c_dfDIMouse2_[];
extern const DIDATAFORMAT c_dfDIKeyboard_[];

const char *DInput_GetErrorString(HRESULT hr)
{
    switch ((unsigned)hr) {
    case 0x80004005: return "DIERR_GENERIC";
    case 0x80004002: return "DIERR_NOINTERFACE";
    case 0x80004001: return "DIERR_UNSUPPORTED";
    case 0x80040110: return "DIERR_NOAGGREGATION";
    case 0x80040154: return "DIERR_DEVICENOTREG";
    case 0x80040200: return "DIERR_INSUFFICIENTPRIVS";
    case 0x80040201: return "DIERR_DEVICEFULL";
    case 0x80040202: return "DIERR_MOREDATA";
    case 0x80040203: return "DIERR_NOTDOWNLOADED";
    case 0x80040204: return "DIERR_HASEFFECTS";
    case 0x80040205: return "DIERR_NOTEXCLUSIVEACQUIRED";
    case 0x80040206: return "DIERR_INCOMPLETEEFFECT";
    case 0x80040207: return "DIERR_NOTBUFFERED";
    case 0x80040208: return "DIERR_EFFECTPLAYING";
    case 0x80040209: return "DIERR_UNPLUGGED";
    case 0x8004020A: return "DIERR_REPORTFULL";
    case 0x80070002: return "DIERR_NOTFOUND";
    case 0x80070005: return "DIERR_OTHERAPPHASPRIO";
    case 0x8007000C: return "DIERR_NOTACQUIRED";
    case 0x8007000E: return "DIERR_OUTOFMEMORY";
    case 0x80070015: return "DIERR_NOTINITIALIZED";
    case 0x8007001E: return "DIERR_INPUTLOST";
    case 0x80070057: return "DIERR_INVALIDPARAM";
    case 0x80070077: return "DIERR_BADDRIVERVER";
    case 0x800700AA: return "DIERR_ACQUIRED";
    case 0x8007047E: return "DIERR_OLDDIRECTINPUTVERSION";
    case 0x80070481: return "DIERR_BETADIRECTINPUTVERSION";
    case 0x800704DF: return "DIERR_ALREADYINITIALIZED";
    }
    return "Unknown error";
}
HOOK(0x48FA80, DInput_GetErrorString);

void Mouse_Update(void)
{
    DIMOUSESTATE2 st;
    HRESULT hr;
    int b = 0, i;

    if (gMouseDev->lpVtbl->GetDeviceState(gMouseDev, sizeof(st), &st)) {
        if (gMouseDev->lpVtbl->Acquire(gMouseDev))
            return;
        hr = gMouseDev->lpVtbl->GetDeviceState(gMouseDev, sizeof(st), &st);
        if (hr) {
            Error_Show("Failed get mouse state: %s", DInput_GetErrorString(hr));
            for (;;)
                ;
        }
    }
    gMouse.dx = st.lX * (1.0 / 255.0);
    gMouse.dy = st.lY * (1.0 / 255.0);
    if (AppContext_GetWinInput()) {
        if (gMouseLeft)
            b = 1;
        if (gMouseRight)
            b |= 2;
        if (gMouseMiddle)
            b |= 4;
        if (gMouseWheel > 0) {
            b |= 0x100;
            gMouseWheel--;
        }
        if (gMouseWheel < 0) {
            b |= 0x200;
            gMouseWheel++;
        }
    } else {
        for (i = 0; i < 8; i++) {
            if (st.rgbButtons[i])
                b |= 1 << i;
        }
        if (st.lZ > 0)
            b |= 0x100;
        else if (st.lZ < 0)
            b |= 0x200;
    }
    gMouse.buttons = b;
}
HOOK(0x48FC20, Mouse_Update);

void Mouse_GetCaps(void)
{
    DIDEVCAPS caps;

    caps.dwSize = sizeof(caps);
    if (gMouseDev->lpVtbl->GetCapabilities(gMouseDev, &caps)) {
        Error_Show("Could not get mouse caps!");
        for (;;)
            ;
    }
    gMouse.present = 1;
    gMouse.numButtons = caps.dwButtons;
}
HOOK(0x48FD90, Mouse_GetCaps);

int Mouse_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void Mouse_Open(void)
{
    gMouseDI = NULL;
    gMouseDev = NULL;
    if (DirectInputCreateEx_(AppContext_GethInstance(), 0x700, IID_IDirectInput7A_, (void **)&gMouseDI, NULL)) {
        Error_Show("Could not create DirectInput mouse object!");
        for (;;)
            ;
    }
    if (gMouseDI->lpVtbl->CreateDeviceEx(gMouseDI, GUID_SysMouse_, IID_IDirectInputDevice7A_, (void **)&gMouseDev,
                                         NULL)) {
        Error_Show("Could not create DirectInput device mouse!");
        for (;;)
            ;
    }
    if (gMouseDev->lpVtbl->SetDataFormat(gMouseDev, c_dfDIMouse2_)) {
        Error_Show("Could not set data format for mouse!");
        for (;;)
            ;
    }
    if (gMouseDev->lpVtbl->SetCooperativeLevel(gMouseDev, AppContext_GethWnd(), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)) {
        Error_Show("Could not set cooperative level for keyboard!");
        for (;;)
            ;
    }
    Mouse_GetCaps();
    gMouseLeft = 0;
    gMouseRight = 0;
    gMouseWheel = 0;
    AppMain_AddWinProcCallback(Mouse_WinProcCB);
}
HOOK(0x48FDE0, Mouse_Open);

int Mouse_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
        gMouseLeft = 1;
        break;
    case WM_LBUTTONUP:
        gMouseLeft = 0;
        break;
    case WM_RBUTTONDOWN:
        gMouseRight = 1;
        break;
    case WM_RBUTTONUP:
        gMouseRight = 0;
        break;
    case WM_MBUTTONDOWN:
        gMouseMiddle = 1;
        break;
    case WM_MBUTTONUP:
        gMouseMiddle = 0;
        break;
    case WM_MOUSEWHEEL:
        gMouseWheel += (short)(wparam >> 16) / 120;
        break;
    }
    return 1;
}
HOOK(0x48FED0, Mouse_WinProcCB);

void Mouse_Close(void)
{
    if (gMouseDI) {
        if (gMouseDev) {
            gMouseDev->lpVtbl->Unacquire(gMouseDev);
            gMouseDev->lpVtbl->Release(gMouseDev);
            gMouseDev = NULL;
        }
        gMouseDI->lpVtbl->Release(gMouseDI);
        gMouseDI = NULL;
    }
    AppMain_RemoveWinProcCallback(Mouse_WinProcCB);
}
HOOK(0x48FFA0, Mouse_Close);

void Keyboard_SetPresent(void);
void Keyboard_SetBlocked(char on);
int Keyboard_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void Keyboard_Open(void)
{
    GUID guid = *GUID_SysKeyboard_;

    gKeyboardDI = NULL;
    gKeyboardDev = NULL;
    if (DirectInputCreateA_(AppContext_GethInstance(), 0x700, &gKeyboardDI, NULL)) {
        Error_Show("Could not create DirectInput keyboard object!");
        for (;;)
            ;
    }
    if (gKeyboardDI->lpVtbl->CreateDevice(gKeyboardDI, &guid, &gKeyboardDev, NULL)) {
        Error_Show("Could not create DirectInput device keyboard!");
        for (;;)
            ;
    }
    if (gKeyboardDev->lpVtbl->SetDataFormat(gKeyboardDev, c_dfDIKeyboard_)) {
        Error_Show("Could not set data format for keyboard!");
        for (;;)
            ;
    }
    if (gKeyboardDev->lpVtbl->SetCooperativeLevel(gKeyboardDev, AppContext_GethWnd(),
                                                  DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)) {
        Error_Show("Could not set cooperative level for keyboard!");
        for (;;)
            ;
    }
    if (!gKeyboardDev->lpVtbl->Acquire(gKeyboardDev))
        gKeyboardAcquired = 1;
    Keyboard_SetPresent();
    Keyboard_SetBlocked(0);
    memset(gWinKeys, 0, 256);
    AppMain_AddWinProcCallback(Keyboard_WinProcCB);
}
HOOK(0x48FFF0, Keyboard_Open);

int Keyboard_VKToDIK(WPARAM vk, LPARAM lparam);

int Keyboard_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        gWinKeys[Keyboard_VKToDIK(wparam, lparam)] = 0x80;
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        gWinKeys[Keyboard_VKToDIK(wparam, lparam)] = 0;
        break;
    }
    return 1;
}
HOOK(0x490120, Keyboard_WinProcCB);

/* Windows virtual key to DirectInput scan code */
int Keyboard_VKToDIK(WPARAM vk, LPARAM lparam)
{
    switch (vk) {
    case VK_CONTROL:
    case VK_MENU:
        /* the extended key flag becomes bit 7 */
        return ((lparam >> 1 & 0x800000) | (lparam & 0x7F0000)) >> 16;
    case VK_PRIOR:
        return DIK_PRIOR;
    case VK_NEXT:
        return DIK_NEXT;
    case VK_LEFT:
        return DIK_LEFT;
    case VK_UP:
        return DIK_UP;
    case VK_RIGHT:
        return DIK_RIGHT;
    case VK_DOWN:
        return DIK_DOWN;
    }
    return lparam >> 16 & 0x7F;
}
HOOK(0x490190, Keyboard_VKToDIK);

void Keyboard_Update(void)
{
    if (!gKeyboardAcquired) {
        if (gKeyboardDev->lpVtbl->Acquire(gKeyboardDev))
            return;
        gKeyboardAcquired = 1;
    }
    memcpy(gKeysPrev, gKeys, 256);
    if (!gKeyboardBlocked) {
        if (AppContext_GetWinInput()) {
            memcpy(gKeys, gWinKeys, 256);
            return;
        }
        if (!gKeyboardDev->lpVtbl->GetDeviceState(gKeyboardDev, 256, gKeys))
            return;
        if (!gKeyboardDev->lpVtbl->Acquire(gKeyboardDev)
            && !gKeyboardDev->lpVtbl->GetDeviceState(gKeyboardDev, 256, gKeys))
            return;
    }
    memset(gKeys, 0, 256);
}
HOOK(0x490230, Keyboard_Update);

void Keyboard_Close(void)
{
    if (gKeyboardDI) {
        if (gKeyboardDev) {
            gKeyboardDev->lpVtbl->Unacquire(gKeyboardDev);
            gKeyboardDev->lpVtbl->Release(gKeyboardDev);
            gKeyboardDev = NULL;
        }
        gKeyboardDI->lpVtbl->Release(gKeyboardDI);
        gKeyboardDI = NULL;
    }
    AppMain_RemoveWinProcCallback(Keyboard_WinProcCB);
}
HOOK(0x4902F0, Keyboard_Close);

void Keyboard_SetPresent(void)
{
    gKeyboardPresent = 1;
}
HOOK(0x490340, Keyboard_SetPresent);

void Keyboard_SetBlocked(char on)
{
    gKeyboardBlocked = on;
}
HOOK(0x490350, Keyboard_SetBlocked);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(igi_dikeyboard);
REF_EXTERN(igi_dimouse);
STORE(igd, 5348B0, 4) __typeof__(c_dfDIMouse2_[0]) store_5348B0[1] INIT({{.dwSize = 24, .dwObjSize = 16, .dwFlags = 2, .dwDataSize = 20, .dwNumObjs = 11, .rgodf = (__typeof__(store_5348B0[0].rgodf))ref_igi_dimouse}});
ALIAS(c_dfDIMouse2_, store_5348B0, 0);
STORE(igd, 5348B0, 4) __typeof__(c_dfDIKeyboard_[0]) store_5348C8[1] INIT({{.dwSize = 24, .dwObjSize = 16, .dwFlags = 2, .dwDataSize = 256, .dwNumObjs = 256, .rgodf = (__typeof__(store_5348C8[0].rgodf))ref_igi_dikeyboard}});
ALIAS(c_dfDIKeyboard_, store_5348C8, 0);
STORE(igd, 534900, 4) __typeof__(GUID_SysKeyboard_[0]) store_534900[1] INIT({{.Data1 = 1864182625, .Data2 = 54688, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}});
ALIAS(GUID_SysKeyboard_, store_534900, 0);
STORE(igd, 534900, 4) __typeof__(GUID_SysMouse_[0]) store_534910[10] INIT({{.Data1 = 1864182624, .Data2 = 54688, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830386u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 1433567776, .Data2 = 54076, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830372u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830371u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830389u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830388u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830370u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830369u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}, {.Data1 = 2741830368u, .Data2 = 51699, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}}});
ALIAS(GUID_SysMouse_, store_534910, 0);
STORE(igd, 534900, 4) __typeof__(IID_IDirectInputDevice7A_[0]) store_5349B0[1] INIT({{.Data1 = 1473758908, .Data2 = 9046, .Data3 = 4563, .Data4 = {142, 157, 0, 192, 79, 104, 68, 174}}});
ALIAS(IID_IDirectInputDevice7A_, store_5349B0, 0);
STORE(igd, 5349D0, 4) __typeof__(IID_IDirectInput7A_[0]) store_5349D0[1] INIT({{.Data1 = 2588718724u, .Data2 = 9069, .Data3 = 4563, .Data4 = {142, 157, 0, 192, 79, 104, 68, 174}}});
ALIAS(IID_IDirectInput7A_, store_5349D0, 0);
BSS(5C8E08) int gMouseWheel;
BSS(5C8E08) int gMouseRight;
BSS(5C8E08) int gMouseLeft;
BSS(5C8E08) IDirectInput7A *gMouseDI;
BSS(5C8E08) int gMouseMiddle;
BSS(5C8E08) IDirectInputDevice7A *gMouseDev;
PAD(igb, 5C8E08, 5C8E20, 0x4);
BSS(5C8E08) IDirectInputDeviceA *gKeyboardDev;
BSS(5C8E08) IDirectInputA *gKeyboardDI;
PAD(igb, 5C8E08, 5C8E2C, 0x4);
BSS(5C8E08) int gKeyboardAcquired;
BSS(C28C60) char gKeyboardBlocked;
BSS(C28C60) char gKeyboardPresent;
STORE(igb, C28C60, 1) uint8_t store_C28C62[256];
ALIAS(gKeys, store_C28C62, 0);
STORE(igb, C28C60, 1) unsigned char store_C28D62[286];
ALIAS(gKeysPrev, store_C28D62, 0);
STORE(igb, C28C60, 1) unsigned char store_C28E80[256];
ALIAS(gWinKeys, store_C28E80, 0);
BSS(C28C60) Mouse gMouse;
