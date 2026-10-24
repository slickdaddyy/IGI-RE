#include <string.h>

#include "hook.h"
#include "editcam/editcam.h"

FN_EXTERN(Task_Nothing);

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x5183D0, void, TaskTypes_Open, (void));
ORIG(0x508FA0, void, AIGraph_Init, (void));

#define Task_Nothing (FN(0x4D3710, Task_Nothing))

/* a pixel block handed to Display_DrawPrimitive */
typedef struct SyncBlit {
    int unk0;
    int prim;
    int unk8;
    int pitch;
    int x, y;
    int w, h;
    void *pixels;
    int unk24;
} SyncBlit;

ORIG(0x491D70, void, Display_DrawPrimitive, (SyncBlit *prim));
ORIG(0x491DC0, int, Display_GetBlitPrim, (void));

typedef struct SyncRect {
    int x, y, w, h;
} SyncRect;

typedef struct SyncList {
    int count;
    SyncRect rects[50];
} SyncList;

typedef struct SyncImage {
    int width;
    int unk4;
    uint16_t *pixels;
} SyncImage;

/* copies the rectangles queued in one of two lists to the screen,
   alternating between the lists each frame */
typedef struct Sync {
    Task task;
    int flip;
    SyncList lists[3];
    SyncImage *image;
} Sync;

_Static_assert(sizeof(SyncBlit) == 0x28, "SyncBlit");
_Static_assert(offsetof(Sync, lists[1]) == 0x348, "Sync.lists");
_Static_assert(sizeof(Sync) == 0x994, "Sync");

#define gSyncEvent0 VAR(uint8_t, 0x54E830)
#define gSyncEvent1 VAR(uint8_t, 0x54E831)
#define gSyncType VAR(uint16_t, 0xA7B298)
#define gSync VAR(Sync *, 0xA7B2A0)

void Sync_Create(Sync *s);
void Sync_Delete(Sync *s);
void Sync_Draw(Sync *s);

void Sync_Init(void)
{
    gSyncType = TaskType_New("Sync", sizeof(Sync), Task_GetRootType(), 0, 0);
    gSyncEvent0 = TaskEvent_Alloc(0);
    gSyncEvent1 = TaskEvent_Alloc(0);
    TaskType_SetHandler(gSyncType, 0, Sync_Create);
    TaskType_SetHandler2(gSyncType, 1, Task_Nothing);
    TaskType_SetHandler2(gSyncType, 2, Sync_Delete);
    TaskType_SetHandler2(gSyncType, Screen_GetDrawEvent(), Sync_Draw);
}
HOOK(0x508D70, Sync_Init);

void Sync_Create(Sync *s)
{
    s->flip = 0;
    s->lists[0].count = 0;
    s->lists[1].count = 0;
    s->lists[2].count = 0;
    s->image = NULL;
    gSync = s;
}
HOOK(0x508E00, Sync_Create);

void Sync_Delete(Sync *s)
{
    (void)s;
    gSync = NULL;
}
HOOK(0x508E30, Sync_Delete);

void Sync_Draw(Sync *s)
{
    SyncList *l = s->flip ? &s->lists[1] : &s->lists[2];
    int i;

    s->flip ^= 1;
    for (i = 0; i < l->count; i++) {
        SyncRect *r = &l->rects[i];
        SyncBlit b;

        memset(&b, 0, sizeof(b));
        b.pixels = s->image->pixels + (r->y * s->image->width + r->x);
        b.prim = Display_GetBlitPrim();
        b.unk24 = 0;
        b.pitch = s->image->width * 4;
        b.x = r->x;
        b.y = r->y;
        b.w = r->w;
        b.h = r->h;
        Display_DrawPrimitive(&b);
    }
    l->count = 0;
}
HOOK(0x508E40, Sync_Draw);

void Sync_Shutdown(void)
{
    TaskEvent_Free(gSyncEvent0);
    TaskEvent_Free(gSyncEvent1);
    TaskType_Free(gSyncType);
}
HOOK(0x508F30, Sync_Shutdown);

uint8_t Sync_GetEvent0(void)
{
    return gSyncEvent0;
}
HOOK(0x508F60, Sync_GetEvent0);

uint8_t Sync_GetEvent1(void)
{
    return gSyncEvent1;
}
HOOK(0x508F70, Sync_GetEvent1);

void sub_508F80(void)
{
    TaskTypes_Open();
    AIGraph_Init();
}
HOOK(0x508F80, sub_508F80);
