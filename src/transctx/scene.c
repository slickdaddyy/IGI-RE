#include <string.h>

#include "hook.h"
#include "transctx/transctx.h"

ORIG(0x491DB0, int, Display_GetClipWindowPrim, (void));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, void *node));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B0EE0, void, RenderMode_ForEach, (void *fn, void *user));
ORIG(0x4B0F90, int, RenderMode_Pop, (void));
ORIG(0x4B0FD0, void, RenderMode_Push, (int mode));
ORIG(0x4C1780, void, Debug_Log, (void));

typedef struct SceneList {
    Scene *head;
    Scene *tail;
    Scene *tailpred;
} SceneList;

extern SceneList gSceneList;
extern int gSceneVersion;
extern char gFrameMem[];
extern char *gFramePtr;
extern unsigned gFramePeak;

F32(kHalf, 0x3F000000);

void Scene_SetBounds(Scene *s, const float *min, const float *max)
{
    s->minX = min[0];
    s->minY = min[1];
    s->maxX = max[0];
    s->maxY = max[1];
    gSceneVersion++;
}
HOOK(0x497520, Scene_SetBounds);

void Scene_InitList(void)
{
    gSceneList.head = (Scene *)&gSceneList.tail;
    gSceneList.tailpred = (Scene *)&gSceneList.head;
    gSceneList.tail = NULL;
}
HOOK(0x497550, Scene_InitList);

void Scene_SetRect(Scene *s, const float *rect);
void RenderQueue_Init(RenderNode *q, int n);

void Scene_Init(Scene *s, int queues)
{
    float rect[4];

    s->renderMode = -1;
    rect[0] = 0;
    rect[1] = 0;
    rect[2] = 0;
    rect[3] = 0;
    Scene_SetRect(s, rect);
    s->queueCount = queues;
    s->queues = Mem_Alloc(queues * sizeof(RenderNode), 4);
    s->extra = Mem_Alloc(sizeof(RenderNode), 4);
    RenderQueue_Init(s->queues, queues);
    RenderQueue_Init(s->extra, 1);
}
HOOK(0x497570, Scene_Init);

void Scene_Free(Scene *s)
{
    Mem_Free(s->extra);
    s->extra = NULL;
    Mem_Free(s->queues);
    s->queues = NULL;
}
HOOK(0x4975F0, Scene_Free);

/* Makes s the current scene; with add set it is also drawn by Scene_DrawAll. */
void Scene_Select(Scene *s, int add)
{
    gSceneVersion++;
    gScene = *s;
    if (add) {
        s->prev = NULL;
        s->next = NULL;
        List_AddTail(&gSceneList, s);
    }
}
HOOK(0x497620, Scene_Select);

int Scene_GetVersion(void)
{
    return gSceneVersion;
}
HOOK(0x497670, Scene_GetVersion);

void Scene_SetRect(Scene *s, const float *rect)
{
    float min[2], max[2];

    s->x = rect[0];
    s->y = rect[1];
    s->w = rect[2];
    s->h = rect[3];
    s->halfW = rect[2] * kHalf.f;
    s->halfH = rect[3] * kHalf.f;
    s->clip.type = Display_GetClipWindowPrim();
    min[0] = s->x;
    min[1] = s->y;
    max[0] = s->w + s->x;
    max[1] = s->h + s->y;
    Scene_SetBounds(s, min, max);
}
HOOK(0x497680, Scene_SetRect);

void Primitive_Draw(RenderNode *n);
void RenderQueue_Draw(RenderNode *n);

/* Draws every scene added this frame and empties the list. */
void Scene_DrawAll(void)
{
    Scene *s, *next;

    TransContext_Invalidate();
    for (s = gSceneList.head; (next = s->next) != NULL; s = next) {
        gScene = *s;
        if (s->renderMode != -1)
            RenderMode_Push(s->renderMode);
        Primitive_Draw(&s->clip);
        RenderQueue_Draw(&s->queues[s->queueCount - 1]);
        RenderQueue_Init(s->queues, s->queueCount);
        if (s->renderMode != -1)
            RenderMode_Pop();
    }
    Scene_InitList();
    TransContext_Invalidate();
    Debug_Log();
}
HOOK(0x497700, Scene_DrawAll);

void Scene_QueueClipWindow(const void *prim, int queue)
{
    RenderNode *n = Frame_Alloc(0x24);

    memcpy(n, prim, 0x24);
    n->type = Display_GetClipWindowPrim();
    RenderQueue_Add(queue, n);
}
HOOK(0x4977B0, Scene_QueueClipWindow);

void Frame_Reset(void)
{
    gFramePtr = gFrameMem;
}
HOOK(0x4977F0, Frame_Reset);

/* Memory that lives until the next Frame_Reset. */
void *Frame_Alloc(int size)
{
    char *p = gFramePtr;
    unsigned used;

    gFramePtr += size;
    used = gFramePtr - gFrameMem;
    if (used > gFramePeak)
        gFramePeak = used;
    return p;
}
HOOK(0x497800, Frame_Alloc);

void RenderQueue_Draw(RenderNode *n)
{
    for (; n; n = n->next)
        if (n->type != -1)
            gRenderFuncs[n->type](n);
}
HOOK(0x497830, RenderQueue_Draw);

void Primitive_Draw(RenderNode *n)
{
    gRenderFuncs[n->type](n);
}
HOOK(0x497860, Primitive_Draw);

/* Links the queue heads into one chain, each to the one before it. */
void RenderQueue_Init(RenderNode *q, int n)
{
    if (!n)
        return;
    q->next = NULL;
    q->type = -1;
    for (; --n; q++) {
        q[1].next = q;
        q[1].type = -1;
    }
}
HOOK(0x497880, RenderQueue_Init);

void RenderQueue_Add(int queue, void *node)
{
    RenderNode *q = &gScene.queues[queue];

    ((RenderNode *)node)->next = q->next;
    q->next = node;
}
HOOK(0x4978B0, RenderQueue_Add);

int Primitive_Register(const char *name)
{
    return RenderMode_CreateMethod(name);
}
HOOK(0x4978D0, Primitive_Register);

void Primitive_Unregister(int id)
{
    RenderMode_DeleteMethod(id);
}
HOOK(0x4978E0, Primitive_Unregister);

typedef struct PrimHandler {
    int id;
    void *fn;
} PrimHandler;

void Primitive_SetHandlerCB(const int *mode, const PrimHandler *h);

/* Sets the handler of a primitive in every render mode. */
void Primitive_SetHandler(int id, void *fn)
{
    PrimHandler h;

    h.id = id;
    h.fn = fn;
    RenderMode_ForEach(Primitive_SetHandlerCB, &h);
}
HOOK(0x4978F0, Primitive_SetHandler);

void Primitive_SetHandlerCB(const int *mode, const PrimHandler *h)
{
    RenderMode_SetMethod(*mode, h->id, h->fn);
}
HOOK(0x497920, Primitive_SetHandlerCB);
