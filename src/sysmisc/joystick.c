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

extern IDirectInputDeviceA *gJoyDev[];
extern DIJOYSTATE gJoyState[];
extern DIDEVCAPS gJoyCaps[];
extern IDirectInputDevice2A *gJoyDev2[];
extern IDirectInputA *gJoyDI;
extern DIJOYSTATE gJoyPrevState[];
extern int gJoyCount;
extern DWORD gJoyDeadZone;
extern JoyAxis gJoyAxes[];
extern JoyInput gJoyInput[];
extern GUID gIID_IDirectInputDevice2A;
extern DIDATAFORMAT gDfDIJoystick;

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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(igi_dijoystick);
DATA(5349C0) GUID gIID_IDirectInputDevice2A INIT({.Data1 = 1497687682, .Data2 = 51502, .Data3 = 4559, .Data4 = {191, 199, 68, 69, 83, 84}});
DATA(534C38) DIDATAFORMAT gDfDIJoystick INIT({.dwSize = 24, .dwObjSize = 16, .dwFlags = 1, .dwDataSize = 80, .dwNumObjs = 44, .rgodf = (__typeof__(gDfDIJoystick.rgodf))ref_igi_dijoystick});
STORE(igd, 54E8A8, 4) __typeof__(gJoyAxes[0]) store_54E8A8[115] INIT({{.ofs = 12, .present = 1}, {.ofs = 16, .present = 1}, {.ofs = 20, .present = 1}, {.present = 1}, {.ofs = 4, .present = 1}, {.ofs = 8, .present = 1}, {.ofs = 24, .present = 1}, {.ofs = 28, .present = 1}, {.ofs = 1819635523, .present = 1869488228}, {.ofs = 1701257332, .present = 1633886324}, {.ofs = 1768055152, .present = 1769236844}, {.ofs = 1713402725, .present = 1864397423}, {.ofs = 1864394094, .present = 1869422706}, {.ofs = 1780508018, .present = 1953724783}, {.ofs = 1936417641, .present = 11808}, {.ofs = 1819635523, .present = 1869488228}, {.ofs = 1667309684, .present = 1919513969}, {.ofs = 1852776549, .present = 1919885413}, {.ofs = 1919905056, .present = 1869226085}, {.ofs = 1769239417, .present = 1679846243}, {.ofs = 1667855973, .present = 3044197}, {.ofs = 1819635523, .present = 1869488228}, {.ofs = 1919098996, .present = 1702125925}, {.ofs = 1919501344, .present = 1232364389}, {.ofs = 1953853550, .present = 2037344800}, {.ofs = 543449456, .present = 1701470831}, {.ofs = 3044451, .present = 1818845510}, {.ofs = 1948279909, .present = 1919098991}, {.ofs = 1702125925, .present = 1986356256}, {.ofs = 778396521}, {.ofs = 1818845510, .present = 1948279909}, {.ofs = 1651449967, .present = 1852399988}, {.ofs = 1953392928, .present = 1634103909}, {.ofs = 3040611, .present = 2764330}, {.ofs = 47, .present = 25207}, {.ofs = 6433655, .present = 1701604425}, {.ofs = 543973735, .present = 1852141679}, {.ofs = 1701080941, .present = 1478830112}, {.ofs = 1868963881, .present = 1768300658}, {.ofs = 622880108, .present = 11891}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1952802592, .present = 1953784096}, {.ofs = 1701340001, .present = 1970479204}, {.ofs = 1667327602, .present = 101}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1952802592, .present = 1920299808}, {.ofs = 1701011814, .present = 1936024608}, {.ofs = 1885958755, .present = 1852795252}, {.present = 1970499157}, {.ofs = 1919905904, .present = 543450484}, {.ofs = 1954047348, .present = 543519349}, {.ofs = 1836216166, .present = 29793}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1952802592, .present = 1885957408}, {.ofs = 7364973, .present = 1769566804}, {.ofs = 1948280686, .present = 1868832879}, {.ofs = 1869377143, .present = 1830839393}, {.ofs = 1634562153, .present = 1702109296}, {.ofs = 1920300152, .present = 1752440933}, {.ofs = 1763734625, .present = 1869488243}, {.ofs = 1869619316, .present = 544367991}, {.ofs = 840984175, .present = 544106784}, {.ofs = 1952737655, .present = 1919885416}, {.ofs = 1768253472, .present = 7628903}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1768710688, .present = 1970479220}, {.ofs = 1667327602, .present = 101}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1819178272, .present = 543908719}, {.ofs = 1718777203, .present = 6644577}, {.ofs = 1635151433, .present = 543451500}, {.ofs = 1954047348, .present = 543519349}, {.ofs = 1836216166, .present = 29793}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1668246560, .present = 1970479211}, {.ofs = 1667327602, .present = 101}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1701995296, .present = 543519841}, {.ofs = 1718777203, .present = 6644577}, {.ofs = 1819635523, .present = 1948741220}, {.ofs = 1952802592, .present = 1920299808}, {.ofs = 1701011814, .present = 1668506980}, {.present = 1769108563}, {.ofs = 1701585008, .present = 1752459118}, {.ofs = 1953459744, .present = 1886745376}, {.ofs = 1953656688, .present = 3040357}, {.ofs = 1936090703, .present = 1701147235}, {.ofs = 1718960750, .present = 1400005990}, {.ofs = 1953067632, .present = 1699897189}, {.ofs = 1919247470, .present = 1769107539}, {.ofs = 690513268, .present = 1411398176}, {.ofs = 1970567269, .present = 1948280178}, {.ofs = 543518841, .present = 544501614}, {.ofs = 1886418291, .present = 1702130287}, {.ofs = 11876}, {.present = 1074266112}, {.ofs = 1920099668, .present = 1299081569}, {.ofs = 1600680805, .present = 1684956498}, {.ofs = 1816228453, .present = 108}, {.ofs = 2003790919, .present = 1953721929}, {.ofs = 1701015137, .present = 1835627088}, {.ofs = 1986622569, .present = 101}, {.ofs = 2003790919, .present = 1684104530}, {.ofs = 1717990722, .present = 1917874789}, {.ofs = 1953066345, .present = 6649449}, {.ofs = 1852534357, .present = 544110447}, {.ofs = 1852140642, .present = 1685024100}, {.ofs = 101, .present = 544503119}, {.ofs = 1948280431, .present = 1634890341}, {.ofs = 1914728041, .present = 1701080677}, {.ofs = 1969775474, .present = 7564142}, {.ofs = 1936679777}, {0}, {.ofs = 1852404577}});
ALIAS(gJoyAxes, store_54E8A8, 0);
STORE(igb, A7B2E0, 4) __typeof__(gJoyDev[0]) store_A7B2E0[4];
ALIAS(gJoyDev, store_A7B2E0, 0);
STORE(igb, A7B2E0, 4) __typeof__(gJoyState[0]) store_A7B2F0[4];
ALIAS(gJoyState, store_A7B2F0, 0);
STORE(igb, A7B2E0, 4) __typeof__(gJoyCaps[0]) store_A7B430[4];
ALIAS(gJoyCaps, store_A7B430, 0);
STORE(igb, A7B2E0, 4) __typeof__(gJoyDev2[0]) store_A7B4E0[4];
ALIAS(gJoyDev2, store_A7B4E0, 0);
BSS(A7B2E0) IDirectInputA *gJoyDI;
PAD(igb, A7B2E0, A7B4F4, 0x4);
STORE(igb, A7B2E0, 4) __typeof__(gJoyPrevState[0]) store_A7B4F8[4];
ALIAS(gJoyPrevState, store_A7B4F8, 0);
BSS(A7B2E0) int gJoyCount;
BSS(A7B2E0) DWORD gJoyDeadZone;
PAD(igb, BCAA00, BCAA00, 0x8);
ALIAS(gJoyInput, pad_BCAA00, 0);
