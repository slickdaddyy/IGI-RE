#ifndef COMPMAP_COMPMAP_H
#define COMPMAP_COMPMAP_H

#include <stdint.h>

#include "igi.h"
#include "ui/ui.h"
#include "qasm/qasmtypes.h"
#include "coll/colltypes.h"
#include "comprun/compruntypes.h"
#include "win/wintypes.h"
#include "levelflow/levelflowtypes.h"
#include "levelflow/levelflowvars.h"

/* the storage of a loaded sprite */
typedef struct SpriteSlot {
    int data[6];
} SpriteSlot;

/* everything the computer screens load at level start (0x5BDDB0) */
typedef struct ComputerRes {
    void *computerRes;   /* COMPUTER:computer.res */
    void *languageRes;   /* LANGUAGE:computer.res */
    void *objectivesRes; /* LANGUAGE:objectives.res */
    Sprite *font1, *font3, *font4;
    SpriteSlot pointer;  /* 0x18 */
    SpriteSlot arrow[4][3];
    SpriteSlot arrowUp[3];   /* 0x150 */
    SpriteSlot arrowDown[3]; /* 0x198 */
    SpriteSlot plus[3];      /* 0x1E0 */
    SpriteSlot minus[3];     /* 0x228 */
    SpriteSlot sphere[3];    /* 0x270 */
    SpriteSlot objective[2]; /* 0x2B8 */
    SpriteSlot icon[19];     /* 0x2E8, map icons */
    const char *iconName[19];
} ComputerRes;

/* base of the clickable items on a computer screen */
typedef struct ComputerItem {
    Task task;
    char unk20[0x18];
    int id;              /* 0x38 */
    int unk3C[4];
    char unk4C, unk4D;
    char hover;          /* 0x4E */
    char unk4F;
    void *handler;       /* 0x50, fn(item, &id) */
    void *node[2];       /* 0x54 */
    float x, y, w, h;    /* 0x5C hit rectangle */
    int unk6C, unk70;
    void *self;          /* 0x74 */
} ComputerItem;

typedef struct ComputerButtonItem {
    ComputerItem base;
    float x, y;          /* 0x78 */
    char pressed;        /* 0x80 */
    char wasPressed;
    char unk82[2];
    Text *text;          /* 0x84 */
    Sprite *sprite[3];   /* 0x88 normal, hover, pressed */
} ComputerButtonItem;

typedef struct ComputerTextItem {
    ComputerItem base;
    Text *text;          /* 0x78 */
    int color;
    float x, y, w, h;    /* 0x80 */
} ComputerTextItem;

/* "ComputerMenu": the pointer and the current screen */
typedef struct ComputerMenu {
    Task task;
    Task *panel;         /* 0x20 */
    float x, y;          /* 0x24 pointer */
    char dragging;       /* 0x2C, pointer hidden while the map is dragged */
    char unk2D[3];
    unsigned int buttons; /* 0x30 */
    unsigned int pressed; /* 0x34 */
    int nextScreen;      /* 0x38 */
} ComputerMenu;

typedef struct ComputerLog {
    Task task;
    int scroll;
} ComputerLog;

typedef struct ComputerObjectives {
    Task task;
    ComputerButtonItem *button[6];
} ComputerObjectives;

typedef struct ComputerMap {
    Task task;
    Matrix view;         /* 0x20 */
    Matrix tilt;         /* 0x48 */
    float fov;           /* 0x70, tan of half the view angle */
    void *noise;         /* 0x74 */
    float noiseLevel;    /* 0x78 */
    int noiseTime;       /* 0x7C */
    int clickTime;       /* 0x80, double click window */
    int rclickTime;      /* 0x84 */
    float clickX, clickY;
    Task **target;       /* 0x90, task reference */
    int unk94, unk98;
    char *startup;       /* 0x9C, the "INITIALIZING" box */
    char *object;        /* 0xA0, map object of the camera */
    Task *camera;        /* 0xA4 */
    unsigned int lastButtons; /* 0xA8 */
} ComputerMap;

/* what the map camera is looking at */
typedef struct MapCamera {
    float m[9];
    int unk24;
    Vec3d pos;           /* 0x28 */
    float fov;           /* 0x40 */
} MapCamera;

_Static_assert(sizeof(ComputerRes) == 0x4FC, "ComputerRes");
_Static_assert(sizeof(ComputerButtonItem) == 0x94, "ComputerButtonItem");
_Static_assert(sizeof(ComputerTextItem) == 0x90, "ComputerTextItem");
_Static_assert(sizeof(ComputerMenu) == 0x3C, "ComputerMenu");
_Static_assert(sizeof(ComputerObjectives) == 0x38, "ComputerObjectives");
_Static_assert(sizeof(ComputerMap) == 0xAC, "ComputerMap");

extern const char *gLogLines[];
extern ComputerLog *gLog;
extern int gLogCount;
extern uint16_t gObjectivesType;
extern Vec3d gMapPos;
extern uint16_t gMapType;
extern ComputerMap *gMap;
extern Task *gMapStartTask;
extern int16_t gMapZoom;
extern uint16_t gMenuType;
extern ComputerRes gComputerRes;
extern ComputerMenu *gMenu;
extern uint16_t gButtonItemType;
extern uint16_t gTextItemType;
extern uint8_t gMapSelectEvent;
extern const float gMapZoomAngle[];
extern const float gItemColor[][3];

#define MENU_MANAGER() (gLevelFlow->menu)
#define RUN_SLOT() 1

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4B5F00, const char *, Resource_Load, (const char *name, int flags));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B4910, void, Draw_RectZ, (float x, float y, float w, float h, float r, float g, float b, unsigned int flags,
                                  int unk9, float z));
ORIG(0x4B50B0, void, QVertex_DrawFrame, (Sprite *s, float x, float y, float r, float g, float b, float alpha, float z,
                                         int frame, unsigned int flags, int unk20));
ORIG(0x4B6CE0, int, Sprite_Width, (Sprite *s));
ORIG(0x4B6D00, int, Sprite_Height, (Sprite *s));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x46C060, Computer *, Computer_Get, (void));
ORIG(0x46C070, void *, Computer_GetView, (void));
ORIG(0x46C090, void *, Computer_GetCamera, (void));
ORIG(0x46C0B0, int, Computer_IsMapEnabled, (void));
ORIG(0x406220, char *, Profile_GetActive, (void));
ORIG(0x505600, Task *, MenuItemTask_New, (void *parent));
ORIG(0x4E7200, void, Sound_PlayDefined, (const char *name, int flags, const float *params));
ORIG(0x4682E0, void, ComputerObject_SetFlags, (char *obj, int flags));
ORIG(0x4663B0, void, ComputerStartup_Start, (char *startup));

/* x87 tan of a float angle */
static inline long double ftan(float a)
{
    long double r;

    __asm__("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"((long double)a));
    return r;
}

ComputerRes *ComputerRes_Get(void);
ComputerMenu *ComputerMenu_Get2(void);
void ComputerMenu_SetPanel(Task *panel);
Task *ComputerMenu_GetPanel(void);
void ComputerMenu_SetScreen(int screen);
unsigned int ComputerMenu_GetButtons(void);
unsigned int ComputerMenu_GetPressed(void);
void ComputerMenu_OpenScreen(int screen);
uint8_t ComputerMap_GetSelectEvent(void);
void ComputerMap_SetStartTask(Task *t);
Task *ComputerMap_New(void *parent);
Task *ComputerObjectives_New(void *parent);
Task *ComputerLog_New(void *parent);
void ComputerMap_LookAt(ComputerMap *map, const Vec3d *p);
void ComputerMap_SetNoise(ComputerMap *map, float level, int time);
void ComputerMap_ClampCamera(ComputerMap *map);
void ComputerMap_DropCamera(ComputerMap *map);
char ComputerMap_InBounds(ComputerMap *map, const Vec3d *p);
void ComputerMap_SetTarget(ComputerMap *map, Task *t);
int Computer_DrawWrapped(int x, int y, float z, int width, int unused, const char *text, Sprite *font, int color);
ComputerButtonItem *ComputerButtonItem_New(Task *parent, int id, int a, int b, int c, int d, char e, float x,
                                           float y, char pressed, Sprite *font, const char *text, Sprite *normal,
                                           Sprite *hover, Sprite *down, void *handler);
void ComputerButtonItem_SetPos(ComputerButtonItem *item, float x, float y);
ComputerTextItem *ComputerTextItem_New(Task *parent, int id, int a, int b, int c, int d, char e, float x, float y,
                                       Sprite *font, const char *text, int color, void *handler);

#endif
