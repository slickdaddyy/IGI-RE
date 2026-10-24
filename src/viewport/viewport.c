#include <string.h>

#include "hook.h"
#include "viewport/viewport.h"

/* "ViewportQTask": a rectangle of the screen drawn through a camera. Its
   draw handler sets up a scene and a TransContext, then sends the draw
   event to its children. */

typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
} DisplayMode;

typedef void (*ViewportCallback)(Camera *camera);

extern int gFogEnable;
extern uint16_t gViewportType;
extern ViewportCallback gViewportCallbacks[];
extern Viewport *gViewportDrawn[];
extern int gViewportCount;
extern char gViewportReady;
extern Viewport *gViewportCurrent;
extern int gViewportDrawnCount;

#define VIEWPORT_CALLBACKS 15

ORIG(0x4916C0, void, Display_AddFrameCallback, (void (*fn)(void)));
ORIG(0x4916E0, void, Display_RemoveFrameCallback, (void (*fn)(void)));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x497570, void, Scene_Init, (Scene *s, int queues));
ORIG(0x4975F0, void, Scene_Free, (Scene *s));
ORIG(0x497620, void, Scene_Select, (Scene *s, int add));
ORIG(0x497680, void, Scene_SetRect, (Scene *s, const float *rect));
ORIG(0x497700, void, Scene_DrawAll, (void));
ORIG(0x497D70, void, TransContext_Create, (TransContext *ctx, const float *rot, const double *pos, float fov));
ORIG(0x497E60, void, sub_497E60, (TransContext *ctx, float v));
ORIG(0x497E70, void, TransContext_Select, (TransContext *ctx));
ORIG(0x497EE0, void, TransContext_Queue, (const TransContext *ctx));
ORIG(0x4B7330, void *, SortRender_NewList, (int capacity));
ORIG(0x4B7360, void, SortRender_Submit, (void *list));
ORIG(0x4B73A0, void, SortRender_Begin, (void *list));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x5181F0, void, ViewSlot_Alloc, (int *slot));
ORIG(0x518230, void, ViewSlot_SetCurrent, (int slot));
ORIG(0x518250, void, ViewSlot_Free, (int slot));

static const float kHalf = 0.5f;

void Viewport_Delete(Viewport *v);
void Viewport_ResetFrame(void);
void ViewportQTask_DrawHandler(Viewport *v);

void Viewport_Init(void)
{
    gViewportType = TaskType_New("ViewportQTask", sizeof(Viewport), Task_GetRootType(), 0);
    TaskType_SetHandler2(gViewportType, 1, SUBLEVEL_RUN);
    TaskType_SetHandler2(gViewportType, 2, Viewport_Delete);
    TaskType_SetHandler2(gViewportType, Screen_GetDrawEvent(), ViewportQTask_DrawHandler);
    gViewportReady = 1;
    Display_AddFrameCallback(Viewport_ResetFrame);
    Viewport_ResetFrame();
    memset(gViewportCallbacks, 0, VIEWPORT_CALLBACKS * sizeof(ViewportCallback));
}
HOOK(0x4E7D90, Viewport_Init);

void Viewport_ResetFrame(void)
{
    gViewportDrawnCount = 0;
}
HOOK(0x4E7E20, Viewport_ResetFrame);

void ViewportQTask_DrawHandler(Viewport *v)
{
    float dw = (float)Display_GetMode()->width;
    float dh = (float)Display_GetMode()->height;
    float x = v->x, y = v->y, w = v->w, h = v->h;
    long double mid, bottom;
    volatile float left, top; /* rounded to float like the original */
    float rot[10];
    double pos[3];
    TransContext ctx;
    Camera *cam;
    int i;

    gViewportCurrent = v;
    ViewSlot_SetCurrent(v->slot);
    mid = (h + y) * kHalf;
    left = x * dw;
    top = ((y - mid) * v->camera->squeeze + mid) * dh;
    bottom = ((h - mid) * v->camera->squeeze + mid) * dh;
    v->rect[0] = (float)x87_fistp(left);
    v->rect[1] = (float)x87_fistp(top);
    v->rect[2] = (float)x87_fistp(w * dw - left);
    v->rect[3] = (float)x87_fistp(bottom - top);
    Scene_SetRect(&v->scene, v->rect);
    Scene_Select(&v->scene, 1);

    cam = v->camera;
    memcpy(rot, cam, sizeof(rot));
    memcpy(pos, cam->pos, sizeof(pos));
    TransContext_Create(&ctx, rot, pos, cam->fov * v->fovScale);
    sub_497E60(&ctx, 1.0f);
    TransContext_Select(&ctx);
    gFogEnable = v->fog;
    for (i = 0; i < VIEWPORT_CALLBACKS; i++) {
        if (gViewportCallbacks[i])
            gViewportCallbacks[i](v->camera);
    }
    SortRender_Begin(v->sortList);
    CALL_CHILDREN(&v->task, Screen_GetDrawEvent(), 1);
    SortRender_Submit(v->sortList);
    TransContext_Queue(&ctx);
    gViewportCurrent = NULL;
    gViewportDrawn[gViewportDrawnCount++] = v;
    Debug_Log();
    Scene_DrawAll();
}
HOOK(0x4E7E30, ViewportQTask_DrawHandler);

void Viewport_Delete(Viewport *v)
{
    QTaskList_Free(v->sortList);
    if (--gViewportCount == 0)
        Debug_Log();
    Scene_Free(&v->scene);
    ViewSlot_Free(v->slot);
}
HOOK(0x4E8090, Viewport_Delete);

void Viewport_Shutdown(void)
{
    Display_RemoveFrameCallback(Viewport_ResetFrame);
    TaskType_Free(gViewportType);
    gViewportReady = 0;
}
HOOK(0x4E80D0, Viewport_Shutdown);

uint16_t Viewport_GetType(void)
{
    return gViewportType;
}
HOOK(0x4E8100, Viewport_GetType);

/* x, y, w, h are fractions of the display; unused is never read */
Viewport *Viewport_New(void *parent, float x, float y, float w, float h, void *world, Camera *camera, int unused,
                       int unkA0, int unkA4, int unkA8, float fovScale)
{
    Viewport *v = (Viewport *)Task_New(parent, Viewport_GetType(), NULL);

    (void)unused;
    v->world3d = 1;
    v->fog = 1;
    memset(v->lists, 1, sizeof(v->lists));
    ViewSlot_Alloc(&v->slot);
    if (gViewportCount == 0)
        Debug_Log();
    gViewportCount++;
    v->y = y;
    v->x = x;
    v->unkA0 = unkA0;
    v->w = w;
    v->h = h;
    v->fovScale = fovScale;
    v->unkA4 = unkA4;
    v->unkA8 = unkA8;
    v->world = world;
    v->camera = camera;
    Scene_Init(&v->scene, 0x100);
    v->sortList = SortRender_NewList(5000);
    return v;
}
HOOK(0x4E8110, Viewport_New);

Camera *Viewport_GetCurrentCamera(void)
{
    return gViewportCurrent->camera;
}
HOOK(0x4E81F0, Viewport_GetCurrentCamera);

float *Viewport_GetRect(Viewport *v)
{
    return v->rect;
}
HOOK(0x4E8200, Viewport_GetRect);

Camera *Viewport_GetCamera(Viewport *v)
{
    return v->camera;
}
HOOK(0x4E8210, Viewport_GetCamera);

void Viewport_SetList(Viewport *v, int i, char on)
{
    v->lists[1 + i] = on;
}
HOOK(0x4E8220, Viewport_SetList);

uint8_t *Viewport_GetLists(Viewport *v)
{
    return v->lists;
}
HOOK(0x4E8240, Viewport_GetLists);

void Viewport_SetDrawWorld(Viewport *v, char on)
{
    v->world3d = on;
}
HOOK(0x4E8250, Viewport_SetDrawWorld);

int Viewport_GetDrawWorld(Viewport *v)
{
    return v->world3d;
}
HOOK(0x4E8260, Viewport_GetDrawWorld);

void Viewport_SetFog(Viewport *v, int on)
{
    v->fog = on;
}
HOOK(0x4E8270, Viewport_SetFog);

Viewport *Viewport_GetCurrent(void)
{
    return gViewportCurrent;
}
HOOK(0x4E8280, Viewport_GetCurrent);

/* fn(camera) is called by every viewport before it draws; ignored when all 15 slots are taken */
void Viewport_AddCallback(ViewportCallback fn)
{
    int i;

    for (i = 0; i < VIEWPORT_CALLBACKS; i++) {
        if (!gViewportCallbacks[i]) {
            gViewportCallbacks[i] = fn;
            return;
        }
    }
}
HOOK(0x4E8290, Viewport_AddCallback);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54D98C) uint16_t gViewportType INIT(385);
PAD(igd, 54D98C, 54D98E, 0x2);
PAD(igb, A5EBE0, A5EBE0, 0x3C);
ALIAS(gViewportCallbacks, pad_A5EBE0, 0);
PAD(igb, A5EBE0, A5EC1C, 0x44);
ALIAS(gViewportDrawn, pad_A5EC1C, 0);
BSS(A5EBE0) int gViewportCount;
BSS(A5EBE0) char gViewportReady;
PAD(igb, A5EBE0, A5EC65, 0x3);
BSS(A5EBE0) Viewport *gViewportCurrent;
BSS(A5EBE0) int gViewportDrawnCount;
