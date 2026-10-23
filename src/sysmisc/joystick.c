#include <string.h>

#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include "hook.h"
#include "igi.h"

/* Up to 4 DirectInput joysticks, read into the input records the game
   polls. */

/* an axis of DIJOYSTATE; present is cleared when the device has none */
typedef struct JoyAxis {
    DWORD ofs;
    int present;
} JoyAxis;

typedef struct JoyInput {
    char present;
    char unk1;
    char unk2;
    char unk3;
    char axesAsButtons;     /* x and y also set button bits 0 - 3 */
    char waitRelease;       /* buttons read as 0 until all are let go */
    char unk6;
    char numButtons;
    float x;
    float y;
    float slider0;
    float slider1;
    float rzNeg;
    float rzPos;
    unsigned buttons;       /* button n is bit n + 4 */
    unsigned prevButtons;
} JoyInput;

#define JOY_MAX 4
#define JOY_AXES 8

ORIG_STD(0x4AF740, HRESULT, DirectInputCreateA_, (HINSTANCE inst, DWORD version, IDirectInputA **out,
                                                  IUnknown *outer));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x48F090, HINSTANCE, AppContext_GethInstance, (void));
ORIG(0x48F0A0, HWND, AppContext_GethWnd, (void));

#define gJoyDev ((IDirectInputDeviceA **)0xA7B2E0)
#define gJoyState ((DIJOYSTATE *)0xA7B2F0)
#define gJoyCaps ((DIDEVCAPS *)0xA7B430)
#define gJoyDev2 ((IDirectInputDevice2A **)0xA7B4E0)
#define gJoyDI VAR(IDirectInputA *, 0xA7B4F0)
#define gJoyPrevState ((DIJOYSTATE *)0xA7B4F8)
#define gJoyCount VAR(int, 0xA7B638)
#define gJoyDeadZone VAR(DWORD, 0xA7B63C)
#define gJoyAxes ((JoyAxis *)0x54E8A8)
#define gJoyInput ((JoyInput *)0xBCAA00)
#define gIID_IDirectInputDevice2A VAR(GUID, 0x5349C0)
#define gDfDIJoystick VAR(DIDATAFORMAT, 0x534C38)

static const double kAxisScale = 1.0 / 128.0;

BOOL CALLBACK Joystick_EnumDevice(const DIDEVICEINSTANCEA *inst, void *ref);
int Joystick_Acquire(int i);
void Joystick_InitInput(int i);

void Joystick_Init(void)
{
    int i;

    gJoyCount = 0;
    if (DirectInputCreateA_(AppContext_GethInstance(), 0x700, &gJoyDI, NULL)) {
        Error_Show("Could not create DirectInput joypad object.");
        for (;;)
            ;
    }
    gJoyDI->lpVtbl->EnumDevices(gJoyDI, DIDEVTYPE_JOYSTICK, Joystick_EnumDevice, NULL, DIEDFL_ATTACHEDONLY);
    for (i = 0; i < gJoyCount; i++) {
        IDirectInputDevice2A *dev;

        if (!Joystick_Acquire(i)) {
            Error_Show("Could not acquire one or more joystick devices.");
            for (;;)
                ;
        }
        dev = gJoyDev2[i];
        gJoyCaps[i].dwSize = sizeof(DIDEVCAPS);
        if (dev->lpVtbl->GetCapabilities(dev, &gJoyCaps[i])) {
            Error_Show("Could not get capabilities for one or more joysticks .");
            for (;;)
                ;
        }
        Joystick_InitInput(i);
    }
}
HOOK(0x509980, Joystick_Init);

/* Ranges and dead zones are set through the first interface, which was
   already released. A device that fails is dropped and its slot reused. */
BOOL CALLBACK Joystick_EnumDevice(const DIDEVICEINSTANCEA *inst, void *ref)
{
    GUID guid = inst->guidInstance;
    DIPROPRANGE range;
    DIPROPDWORD dw;
    IDirectInputDeviceA *dev;
    IDirectInputDevice2A *dev2;
    JoyAxis *a;
    HRESULT hr;

    if (gJoyDI->lpVtbl->CreateDevice(gJoyDI, &guid, &gJoyDev[gJoyCount], NULL) < 0) {
        Error_Show("Failed to create device.");
        for (;;)
            ;
    }
    dev = gJoyDev[gJoyCount];
    hr = dev->lpVtbl->QueryInterface(dev, &gIID_IDirectInputDevice2A, (void **)&gJoyDev2[gJoyCount]);
    dev = gJoyDev[gJoyCount];
    dev->lpVtbl->Release(dev);
    if (hr < 0) {
        Error_Show("Failed to obtain interface.");
        for (;;)
            ;
    }
    dev2 = gJoyDev2[gJoyCount];
    if (dev2->lpVtbl->SetDataFormat(dev2, &gDfDIJoystick) < 0)
        goto fail;
    dev2 = gJoyDev2[gJoyCount];
    if (dev2->lpVtbl->SetCooperativeLevel(dev2, AppContext_GethWnd(), DISCL_EXCLUSIVE | DISCL_FOREGROUND) < 0)
        goto fail;
    range.diph.dwSize = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwHow = DIPH_BYOFFSET;
    range.lMin = -128;
    range.lMax = 128;
    for (a = gJoyAxes; a < gJoyAxes + JOY_AXES; a++) {
        range.diph.dwObj = a->ofs;
        dev = gJoyDev[gJoyCount];
        hr = dev->lpVtbl->SetProperty(dev, DIPROP_RANGE, &range.diph);
        if (hr) {
            if (hr != DIERR_OBJECTNOTFOUND)
                goto fail;
            a->present = 0;
        }
    }
    dw.diph.dwSize = sizeof(DIPROPDWORD);
    dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dw.diph.dwObj = 0;
    dw.diph.dwHow = DIPH_DEVICE;
    dw.dwData = DIPROPAUTOCENTER_ON;
    dev = gJoyDev[gJoyCount];
    dev->lpVtbl->SetProperty(dev, DIPROP_AUTOCENTER, &dw.diph);
    dw.diph.dwHow = DIPH_DEVICE;
    dw.dwData = 10000;
    dev = gJoyDev[gJoyCount];
    dev->lpVtbl->SetProperty(dev, DIPROP_SATURATION, &dw.diph);
    dw.diph.dwHow = DIPH_BYOFFSET;
    dw.dwData = gJoyDeadZone;
    for (a = gJoyAxes; a < gJoyAxes + JOY_AXES; a++) {
        if (!a->present)
            continue;
        dw.diph.dwObj = a->ofs;
        dev = gJoyDev[gJoyCount];
        if (dev->lpVtbl->SetProperty(dev, DIPROP_DEADZONE, &dw.diph))
            goto fail;
    }
    gJoyCount++;
    return DIENUM_CONTINUE;
fail:
    dev2 = gJoyDev2[gJoyCount];
    dev2->lpVtbl->Release(dev2);
    gJoyDev2[gJoyCount] = NULL;
    return DIENUM_CONTINUE;
}
HOOK(0x509A40, Joystick_EnumDevice);

int Joystick_Acquire(int i)
{
    if (!gJoyDev2[i])
        return 0;
    return gJoyDev[i]->lpVtbl->Acquire(gJoyDev[i]) >= 0;
}
HOOK(0x509CC0, Joystick_Acquire);

void Joystick_Poll(void)
{
    int i, b, n;

    for (i = 0; i < gJoyCount; i++) {
        IDirectInputDevice2A *dev = gJoyDev2[i];
        HRESULT hr;

        if (!dev)
            continue;
        gJoyPrevState[i] = gJoyState[i];
        dev->lpVtbl->Poll(dev);
        dev = gJoyDev2[i];
        hr = dev->lpVtbl->GetDeviceState(dev, sizeof(DIJOYSTATE), &gJoyState[i]);
        if (hr < 0) {
            if (hr == DIERR_INPUTLOST)
                Joystick_Acquire(i);
            memset(&gJoyState[i], 0, sizeof(DIJOYSTATE));
        }
    }
    n = gJoyCount;
    for (i = 0; i < n; i++) {
        JoyInput *in = &gJoyInput[i];
        DIJOYSTATE *st = &gJoyState[i];
        unsigned bits;

        in->prevButtons = in->buttons;
        in->buttons = 0;
        bits = 0;
        /* the shift wraps, so buttons 28 - 31 land on bits 0 - 3 */
        for (b = 0; b < 32; b++)
            bits |= ((st->rgbButtons[b] & 0x80) == 0x80) << ((b + 4) & 31);
        in->buttons = bits;
        if (in->axesAsButtons) {
            if (st->lX < 0)
                in->buttons |= 1;
            else if (st->lX > 0)
                in->buttons |= 2;
            if (st->lY < 0)
                in->buttons |= 4;
            else if (st->lY > 0)
                in->buttons |= 8;
        }
        if (in->waitRelease) {
            if (!in->buttons)
                in->waitRelease = 0;
            in->buttons = 0;
        }
        in->x = st->lX * kAxisScale;
        in->y = st->lY * kAxisScale;
        if (st->lRz > 0) {
            in->rzNeg = 0.0f;
            in->rzPos = st->lRz * kAxisScale;
        } else {
            in->rzNeg = -(st->lRz * kAxisScale);
            in->rzPos = 0.0f;
        }
        in->slider0 = st->rglSlider[0];
        in->slider1 = st->rglSlider[1];
    }
}
HOOK(0x509CF0, Joystick_Poll);

void Joystick_Shutdown(void)
{
    int i;

    if (!gJoyDI) {
        gJoyCount = 0;
        return;
    }
    for (i = 0; i < gJoyCount; i++) {
        IDirectInputDevice2A *dev = gJoyDev2[i];

        if (!dev)
            continue;
        dev->lpVtbl->Unacquire(dev);
        dev = gJoyDev2[i];
        dev->lpVtbl->Release(dev);
        gJoyDev2[i] = NULL;
    }
    gJoyDI->lpVtbl->Release(gJoyDI);
    gJoyDI = NULL;
    gJoyCount = 0;
}
HOOK(0x509E90, Joystick_Shutdown);

void Joystick_InitInput(int i)
{
    JoyInput *in = &gJoyInput[i];

    in->present = gJoyDev2[i] != NULL;
    in->unk1 = 1;
    in->unk3 = 1;
    in->axesAsButtons = 1;
    in->numButtons = (char)gJoyCaps[i].dwButtons;
}
HOOK(0x509F00, Joystick_InitInput);
