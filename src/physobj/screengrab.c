#include <string.h>

#include "hook.h"
#include "physobj/physobj.h"

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x491CF0, int *, Display_Get, (void));
ORIG(0x491D80, void, Display_ReadPixels, (void *rgb));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B1700, void, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B17C0, void, File_Flush, (void *f));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* "ScreenGrab" (0x28 bytes): saves the screen as numbered .tga files */
typedef struct ScreenGrab {
    Task task;
    char *dir;          /* 0x20 */
    char on;            /* 0x24, records while set */
    char wasOn;         /* 0x25 */
} ScreenGrab;

typedef struct __attribute__((packed)) TgaHeader {
    uint8_t idLength;
    uint8_t colorMapType;
    uint8_t imageType;
    uint8_t colorMap[5];
    uint16_t x, y;
    uint16_t width, height;
    uint8_t bits;
    uint8_t descriptor;
} TgaHeader;

_Static_assert(sizeof(TgaHeader) == 0x12, "TgaHeader");

#define STEP_RESET 100000

#define gScreenGrabType VAR(uint16_t, 0x54DCB8)
#define gScreenGrabStep VAR(int, 0x54DCBC)
#define gScreenGrabHeader VAR(TgaHeader, 0xA70C48)
#define gScreenGrabSingle VAR(char, 0xA70C5A)
#define gScreenGrabRecording VAR(char, 0xA70C5B)
#define gScreenGrabParams VAR(void *, 0xA70C60)
#define gScreenGrabFrame VAR(int, 0xA70C64)

/* DirectInput key tables, now and last frame */
#define gKeys ((uint8_t *)0xC28C62)
#define gPrevKeys ((uint8_t *)0xC28D62)
#define DIK_LCONTROL 0x1D
#define DIK_LSHIFT 0x2A
#define DIK_LMENU 0x38
#define DIK_SCROLL 0x46
#define DIK_DIVIDE 0xB5

void ScreenGrab_Create(ScreenGrab *g);
void ScreenGrab_Run(void);
void ScreenGrab_Draw(ScreenGrab *g);
void ScreenGrab_ParamsChanged(ScreenGrab *g);
void ScreenGrab_GetParams(void *task, void **out);
void ScreenGrab_Save(char *dir);
int ScreenGrab_IsActive(void);

void ScreenGrab_Open(void)
{
    ScreenGrab g;

    gScreenGrabType = TaskType_New("ScreenGrab", sizeof(ScreenGrab), Task_GetRootType(), 0, 0);
    TaskType_SetHandler(gScreenGrabType, 0, ScreenGrab_Create);
    TaskType_SetHandler2(gScreenGrabType, 1, ScreenGrab_Run);
    TaskType_SetHandler2(gScreenGrabType, Screen_GetDrawEvent(), ScreenGrab_Draw);
    TaskType_SetHandler2(gScreenGrabType, 2, TASK_NOTHING);
    TaskType_SetHandler(gScreenGrabType, Param_GetChangedEvent(), ScreenGrab_ParamsChanged);
    TaskType_SetHandler(gScreenGrabType, Param_GetTableEvent(), ScreenGrab_GetParams);
    gScreenGrabParams = ParamTable_New(2);
    ParamTable_Add(gScreenGrabParams, &g, &g.dir, Param_VarString(), 7, NULL);
    ParamTable_Add(gScreenGrabParams, &g, &g.on, Param_Bool8(), 7, NULL);
    gScreenGrabFrame = 0;
    gScreenGrabRecording = 0;
    gScreenGrabSingle = 0;
}
HOOK(0x4EE410, ScreenGrab_Open);

void ScreenGrab_Create(ScreenGrab *g)
{
    g->dir = NULL;
    g->on = 0;
    g->wasOn = 0;
}
HOOK(0x4EE530, ScreenGrab_Create);

#define DOWN(k) (gKeys[k] & 0x80)
#define PRESSED(k) (DOWN(k) && !(gPrevKeys[k] & 0x80))

/* Ctrl+Num/ or Scroll Lock grabs one frame, Alt+Num/ starts recording,
   Alt+Shift+Num/ stops it */
void ScreenGrab_Run(void)
{
    if ((DOWN(DIK_LCONTROL) && PRESSED(DIK_DIVIDE)) || PRESSED(DIK_SCROLL)) {
        gScreenGrabStep = STEP_RESET;
        gScreenGrabSingle = 1;
    }
    if (DOWN(DIK_LMENU) && PRESSED(DIK_DIVIDE)) {
        if (!gScreenGrabRecording)
            gScreenGrabStep = STEP_RESET;
        gScreenGrabRecording = 1;
    }
    if (DOWN(DIK_LMENU) && DOWN(DIK_LSHIFT) && PRESSED(DIK_DIVIDE)) {
        gScreenGrabStep = STEP_RESET;
        gScreenGrabRecording = 0;
    }
}
HOOK(0x4EE540, ScreenGrab_Run);

void ScreenGrab_Draw(ScreenGrab *g)
{
    if ((char)ScreenGrab_IsActive())
        ScreenGrab_Save(g->dir);
}
HOOK(0x4EE5E0, ScreenGrab_Draw);

void ScreenGrab_ParamsChanged(ScreenGrab *g)
{
    if (!g->dir) {
        g->dir = Mem_Alloc(11, 4);
        memcpy(g->dir, "LOCAL:grab", 11);
    }
    if (g->on && !g->wasOn) {
        gScreenGrabStep = STEP_RESET;
        gScreenGrabRecording = 1;
    }
    if (!g->on)
        gScreenGrabRecording = 0;
    g->wasOn = g->on;
}
HOOK(0x4EE600, ScreenGrab_ParamsChanged);

void ScreenGrab_GetParams(void *task, void **out)
{
    *out = gScreenGrabParams;
}
HOOK(0x4EE680, ScreenGrab_GetParams);

void ScreenGrab_Close(void)
{
    QTaskList_Free(gScreenGrabParams);
    TaskType_Free(gScreenGrabType);
}
HOOK(0x4EE690, ScreenGrab_Close);

void Tga_WriteHeader(void *f, TgaHeader *h, uint16_t width, uint16_t height);

/* writes the screen to the first free <dir>/A%07d.tga; the folder must
   be writable (checked with empty.dat) */
void ScreenGrab_Save(char *dir)
{
    char base[1024], path[1024];
    void *f;
    char mode;
    void *rgb;
    int *d1, *d2;

    if (!(char)ScreenGrab_IsActive())
        return;
    if (gScreenGrabSingle)
        gScreenGrabSingle = 0;
    if (gScreenGrabStep == STEP_RESET)
        gScreenGrabFrame = gScreenGrabFrame / STEP_RESET * STEP_RESET;
    if (!dir) {
        dir = "LOCAL:grab";
    } else {
        /* meant to drop a trailing slash, but looks at the terminator */
        char *end = dir + strlen(dir);

        if (*end == '\\' || *end == '/')
            *end = 0;
    }
    crt_sprintf(base, "%s", dir);
    crt_sprintf(path, "%s/empty.dat", base);
    f = File_Open(path, "w");
    if (!f) {
        Warning_Show("%s DOESN'T EXIST", base);
        return;
    }
    File_Close(f);
    do {
        crt_sprintf(path, "%s/A%07d.tga", base, gScreenGrabFrame);
        f = File_Open(path, "r");
        if (f)
            File_Close(f);
        else
            gScreenGrabStep = 1;
        gScreenGrabFrame += gScreenGrabStep;
    } while (f);
    mode = Mem_GetMode();
    if (!mode)
        Mem_SetDefaultMode();
    d1 = Display_Get();
    d2 = Display_Get();
    rgb = Mem_Alloc(d1[2] * d2[1] * 3, 4);
    if (!mode)
        Mem_ClearMode();
    if (!rgb)
        return;
    f = File_Open(path, "w");
    if (f) {
        uint16_t w, h;

        Display_ReadPixels(rgb);
        h = Display_Get()[2];
        w = Display_Get()[1];
        Tga_WriteHeader(f, &gScreenGrabHeader, w, h);
        d1 = Display_Get();
        d2 = Display_Get();
        File_Write(f, rgb, d1[2] * d2[1] * 3);
        File_Flush(f);
        File_Close(f);
    }
    Mem_Free(rgb);
}
HOOK(0x4EE6B0, ScreenGrab_Save);

/* uncompressed 24 bit, top-left origin */
void Tga_WriteHeader(void *f, TgaHeader *h, uint16_t width, uint16_t height)
{
    h->width = width;
    h->height = height;
    h->idLength = 0;
    h->colorMapType = 0;
    h->imageType = 2;
    memset(h->colorMap, 0, sizeof(h->colorMap));
    h->x = 0;
    h->y = 0;
    h->bits = 24;
    h->descriptor = 0x20;
    if (f)
        File_Write(f, h, sizeof(*h));
}
HOOK(0x4EE8E0, Tga_WriteHeader);

int ScreenGrab_IsActive(void)
{
    return gScreenGrabRecording || gScreenGrabSingle;
}
HOOK(0x4EE940, ScreenGrab_IsActive);

/* bucket of the task reference hash table */
unsigned TaskRef_Hash(unsigned key)
{
    return key % 251;
}
HOOK(0x4EE960, TaskRef_Hash);
