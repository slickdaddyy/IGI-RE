#include "hook.h"
#include "igi.h"
#include "define/define.h"

FN_EXTERN(Task_Nothing);

/* the Scene of notes/transctx.md, only its rectangle is used here */
typedef struct FadeScene {
    void *next;
    void *prev;
    int renderMode;
    void *clip[3];
    float x, y, w, h;
    char rest[0x24];
} FadeScene;

/* "FadeOutStart" / "FadeOutEnd" (0x7C bytes): the start task's colour and
   alpha are filled in between its draw event and the end task's one */
typedef struct FadeOut {
    Task task;
    int r, g, b;
    int alpha;
    FadeScene scene;
} FadeOut;

_Static_assert(sizeof(FadeOut) == 0x7C, "FadeOut");

extern uint16_t gFadeOutStartType;
extern FadeOut *gFadeOut;
extern uint16_t gFadeOutEndType;
extern uint16_t gGlowTaskType;
extern const float gSceneRect[];

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

ORIG(0x497570, void, Scene_Init, (FadeScene *s, int queues));
ORIG(0x4975F0, void, Scene_Free, (FadeScene *s));
ORIG(0x497620, void, Scene_Select, (FadeScene *s, int add));
ORIG(0x497680, void, Scene_SetRect, (FadeScene *s, const float *rect));
ORIG(0x4B4820, void, Draw_Rect, (float x, float y, float w, float h, float r, float g, float b, int unk4, int unk8));
ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x512BA0, void, Glow_QueueAll, (void));

void FadeOut_SetRect(FadeOut *f);
void FadeOutStart_Draw(FadeOut *f);
void FadeOutStart_Delete(FadeOut *f);
void FadeOutEnd_Draw(void);
void GlowTask_Draw(void);

/* fistp with the current rounding mode, like the original */
static inline int fist(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

/* screen coordinates go through a short */
static inline float coord(float v)
{
    return (short)fist(v);
}

void FadeOut_InitTask(FadeOut *f)
{
    f->r = 0;
    f->g = 0;
    f->b = 0;
    f->alpha = 0xFF;
    Scene_Init(&f->scene, 1);
}
HOOK(0x4E9090, FadeOut_InitTask);

/* the rectangle of the scene being drawn */
void FadeOut_SetRect(FadeOut *f)
{
    float rect[4];

    rect[0] = gSceneRect[0];
    rect[1] = gSceneRect[1];
    rect[2] = gSceneRect[2];
    rect[3] = gSceneRect[3];
    Scene_SetRect(&f->scene, rect);
}
HOOK(0x4E90C0, FadeOut_SetRect);

/* darkens by (255 - alpha) / 2 and adds the colour */
void FadeOut_Draw(FadeOut *f, int r, int g, int b, int alpha)
{
    int dark = (0xFF - alpha) / 2;

    if (!dark && !r && !g && !b)
        return;
    Scene_Select(&f->scene, 1);
    if (dark) {
        float c = dark & 0xFF;

        Draw_Rect(coord(f->scene.x), coord(f->scene.y), coord(f->scene.w), coord(f->scene.h), c, c, c, 0x10, 0);
    }
    if (r || g || b)
        Draw_Rect(coord(f->scene.x), coord(f->scene.y), coord(f->scene.w), coord(f->scene.h), r & 0xFF, g & 0xFF,
                  b & 0xFF, 8, 0);
    Debug_Log();
}
HOOK(0x4E9110, FadeOut_Draw);

void FadeOut_FreeScene(FadeOut *f)
{
    Scene_Free(&f->scene);
}
HOOK(0x4E92B0, FadeOut_FreeScene);

void FadeOut_Init(void)
{
    gFadeOutStartType = TaskType_New("FadeOutStart", sizeof(FadeOut), Task_GetRootType(), 0);
    gFadeOutEndType = TaskType_New("FadeOutEnd", sizeof(FadeOut), Task_GetRootType(), 0);
    TaskType_SetHandler2(gFadeOutStartType, 2, FadeOutStart_Delete);
    TaskType_SetHandler2(gFadeOutStartType, Screen_GetDrawEvent(), FadeOutStart_Draw);
    TaskType_SetHandler2(gFadeOutEndType, Screen_GetDrawEvent(), FadeOutEnd_Draw);
    gFadeOut = NULL;
}
HOOK(0x4E92C0, FadeOut_Init);

void FadeOutStart_Draw(FadeOut *f)
{
    gFadeOut = f;
    f->alpha = 0xFF;
    f->r = 0;
    f->g = 0;
    f->b = 0;
    FadeOut_SetRect(f);
}
HOOK(0x4E9360, FadeOutStart_Draw);

void FadeOutStart_Delete(FadeOut *f)
{
    FadeOut_FreeScene(f);
}
HOOK(0x4E93A0, FadeOutStart_Delete);

void FadeOutEnd_Draw(void)
{
    FadeOut *f = gFadeOut;
    int r = f->r >= 0xFF ? 0xFF : f->r;
    int g = f->g >= 0xFF ? 0xFF : f->g;
    int b = f->b >= 0xFF ? 0xFF : f->b;

    FadeOut_Draw(f, r, g, b, f->alpha);
    gFadeOut = NULL;
}
HOOK(0x4E93B0, FadeOutEnd_Draw);

void FadeOut_Shutdown(void)
{
    TaskType_Free(gFadeOutStartType);
    TaskType_Free(gFadeOutEndType);
}
HOOK(0x4E9410, FadeOut_Shutdown);

FadeOut *FadeOutStart_New(void *parent)
{
    FadeOut *f = (FadeOut *)Task_New(parent, gFadeOutStartType, NULL);

    FadeOut_InitTask(f);
    return f;
}
HOOK(0x4E9430, FadeOutStart_New);

Task *FadeOutEnd_New(void *parent)
{
    return Task_New(parent, gFadeOutEndType, NULL);
}
HOOK(0x4E9460, FadeOutEnd_New);

void GlowTask_Init(void)
{
    gGlowTaskType = TaskType_New("GlowTask", sizeof(Task), Task_GetRootType(), 0);
    TaskType_SetHandler2(gGlowTaskType, Screen_GetDrawEvent(), GlowTask_Draw);
    TaskType_SetHandler2(gGlowTaskType, 2, TASK_NOTHING);
}
HOOK(0x4E9480, GlowTask_Init);

void GlowTask_Draw(void)
{
    Glow_QueueAll();
}
HOOK(0x4E94D0, GlowTask_Draw);

void GlowTask_Shutdown(void)
{
    TaskType_Free(gGlowTaskType);
    gGlowTaskType = TASK_NO_TYPE;
}
HOOK(0x4E94E0, GlowTask_Shutdown);

Task *GlowTask_New(void *parent)
{
    return Task_New(parent, gGlowTaskType, NULL);
}
HOOK(0x4E9500, GlowTask_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DA50) uint16_t gGlowTaskType INIT(385);
PAD(igd, 54DA50, 54DA52, 0x2);
BSS(A5EC70) uint16_t gFadeOutStartType;
PAD(igb, A5EC70, A5EC72, 0x2);
BSS(A5EC70) FadeOut *gFadeOut;
BSS(A5EC70) uint16_t gFadeOutEndType;
PAD(igb, A5EC70, A5EC7A, 0x2);
