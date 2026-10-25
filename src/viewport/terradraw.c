#include "hook.h"
#include "viewport/viewport.h"
#include "viewport/viewportvars.h"

/* "TerraDrawTask": draws its world with QCube_Render in the viewport being drawn. */

typedef struct TerraDraw {
    Task task;
    void *world;
} TerraDraw;

extern uint16_t gTerraDrawOn;

ORIG(0x4D2650, void, QCube_Render, (void *world, uint8_t *lists, int full));

Viewport *Viewport_GetCurrent(void);
int Viewport_GetDrawWorld(Viewport *v);
uint8_t *Viewport_GetLists(Viewport *v);

void TerraDraw_Draw(TerraDraw *t);

void TerraDraw_Init(void)
{
    gTerraDrawType = TaskType_New("TerraDrawTask", sizeof(TerraDraw), Task_GetRootType(), 0);
    TaskType_SetHandler2(gTerraDrawType, Screen_GetDrawEvent(), TerraDraw_Draw);
    TaskType_SetHandler2(gTerraDrawType, 2, TASK_NOTHING);
}
HOOK(0x4E82C0, TerraDraw_Init);

void TerraDraw_Draw(TerraDraw *t)
{
    int full;

    if (!gTerraDrawOn)
        return;
    full = Viewport_GetDrawWorld(Viewport_GetCurrent());
    QCube_Render(t->world, Viewport_GetLists(Viewport_GetCurrent()), full);
}
HOOK(0x4E8310, TerraDraw_Draw);

void TerraDraw_Shutdown(void)
{
    TaskType_Free(gTerraDrawType);
    gTerraDrawType = 0x181;
}
HOOK(0x4E8350, TerraDraw_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54D9A0) uint16_t gTerraDrawType INIT(385);
DATA(54D9A0) uint16_t gTerraDrawOn INIT(1);
