#include "hook.h"
#include "igi.h"

/* typed keys for the menus: a ring of 32 entries, WM_CHAR codes or negative KEY_* values */
#define gKeys ((int *)0x5C8B20)
#define gKeysStart VAR(int, 0x5C8BA0)
#define gKeysCount VAR(int, 0x5C8BA4)
#define gKeysActive VAR(char, 0x5C8BA8)

enum {
    KEY_LEFT = -1,
    KEY_RIGHT = -2,
    KEY_UP = -3,
    KEY_DOWN = -4,
    KEY_BACKSPACE = -5,
    KEY_DELETE = -6,
    KEY_ENTER = -7,
    KEY_ESCAPE = -8,
    KEY_HOME = -9,
    KEY_END = -10,
    KEY_TAB = -11,
};

ORIG(0x48F490, void, AppMain_AddWinProcCallback, (void *fn));
ORIG(0x48F4F0, void, AppMain_RemoveWinProcCallback, (void *fn));

void Keys_Push(int key)
{
    if (gKeysCount < 31) {
        gKeys[(gKeysStart + gKeysCount) % 32] = key;
        gKeysCount++;
    }
}
HOOK(0x48A6D0, Keys_Push);

int Keys_WndProc(void *hwnd, unsigned msg, int key);

void Keys_Install(void)
{
    gKeysStart = 0;
    gKeysCount = 0;
    gKeysActive = 1;
    AppMain_AddWinProcCallback(Keys_WndProc);
}
HOOK(0x48A710, Keys_Install);

int Keys_WndProc(void *hwnd, unsigned msg, int key)
{
    if (msg == 0x100) { /* WM_KEYDOWN */
        switch (key) {
        case 0x08: Keys_Push(KEY_BACKSPACE); return 1;
        case 0x09: Keys_Push(KEY_TAB); return 1;
        case 0x0D: Keys_Push(KEY_ENTER); return 1;
        case 0x1B: Keys_Push(KEY_ESCAPE); return 1;
        case 0x23: Keys_Push(KEY_END); return 1;
        case 0x24: Keys_Push(KEY_HOME); return 1;
        case 0x25: Keys_Push(KEY_LEFT); return 1;
        case 0x26: Keys_Push(KEY_UP); return 1;
        case 0x27: Keys_Push(KEY_RIGHT); return 1;
        case 0x28: Keys_Push(KEY_DOWN); return 1;
        case 0x2E: Keys_Push(KEY_DELETE); return 1;
        }
    } else if (msg == 0x102) { /* WM_CHAR */
        if (key != 0x08 && key != 0x0D && key != 0x1B) {
            Keys_Push(key);
            return 1;
        }
    }
    return 1;
}
HOOK(0x48A730, Keys_WndProc);

void Keys_Uninstall(void)
{
    AppMain_RemoveWinProcCallback(Keys_WndProc);
    gKeysActive = 0;
}
HOOK(0x48A8A0, Keys_Uninstall);

void Keys_Clear(void)
{
    gKeysStart = 0;
    gKeysCount = 0;
}
HOOK(0x48A8C0, Keys_Clear);

int Keys_Pop(void)
{
    int key = 0;

    if (gKeysCount) {
        key = gKeys[gKeysStart];
        if (++gKeysStart == 32)
            gKeysStart = 0;
        gKeysCount--;
    }
    return key;
}
HOOK(0x48A8D0, Keys_Pop);

int Keys_GetCount(void)
{
    return gKeysCount != 0;
}
HOOK(0x48A910, Keys_GetCount);
