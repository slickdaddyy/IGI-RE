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

typedef struct Mouse {
    char present;
    char numButtons;
    float dx;
    float dy;
    int buttons;
} Mouse;

#define gMouse VAR(Mouse, 0xC28F80)
#define gMouseWheel VAR(int, 0x5C8E08)
#define gMouseRight VAR(int, 0x5C8E0C)
#define gMouseLeft VAR(int, 0x5C8E10)
#define gMouseDI VAR(IDirectInput7A *, 0x5C8E14)
#define gMouseMiddle VAR(int, 0x5C8E18)
#define gMouseDev VAR(IDirectInputDevice7A *, 0x5C8E1C)

#define gKeyboardDev VAR(IDirectInputDeviceA *, 0x5C8E24)
#define gKeyboardDI VAR(IDirectInputA *, 0x5C8E28)
#define gKeyboardAcquired VAR(int, 0x5C8E30)
#define gKeyboardBlocked VAR(char, 0xC28C60)
#define gKeyboardPresent VAR(char, 0xC28C61)
#define gKeys ((unsigned char *)0xC28C62)
#define gKeysPrev ((unsigned char *)0xC28D62)
#define gWinKeys ((unsigned char *)0xC28E80)

#define IID_IDirectInput7A_ ((const IID *)0x5349D0)
#define IID_IDirectInputDevice7A_ ((const IID *)0x5349B0)
#define GUID_SysMouse_ ((const GUID *)0x534910)
#define GUID_SysKeyboard_ ((const GUID *)0x534900)
#define c_dfDIMouse2_ ((const DIDATAFORMAT *)0x5348B0)
#define c_dfDIKeyboard_ ((const DIDATAFORMAT *)0x5348C8)

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
