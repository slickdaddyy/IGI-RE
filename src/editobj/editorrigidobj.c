/* EditorRigidObj: an abstract task type with no handlers, the parent of the editor objects. */
#include "hook.h"
#include "editobj/editobj.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));

#define gEditorRigidObjType VAR(uint16_t, 0x54E634)

void EditorRigidObj_Open(void)
{
    uint16_t t = TaskType_New("EditorRigidObj", sizeof(Task), Task_GetRootType(), 0, 0);

    gEditorRigidObjType = t;
    TaskType_SetHandler(t, 4, NULL);
    TaskType_SetHandler(gEditorRigidObjType, 9, NULL);
    TaskType_SetHandler(gEditorRigidObjType, 10, NULL);
    TaskType_SetHandler(gEditorRigidObjType, 11, NULL);
    TaskType_SetHandler(gEditorRigidObjType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gEditorRigidObjType, 7, NULL);
    TaskType_SetHandler(gEditorRigidObjType, World_GetEvent1(), NULL);
    TaskType_SetHandler(gEditorRigidObjType, World_GetEvent0(), NULL);
    TaskType_SetHandler(gEditorRigidObjType, 8, NULL);
}
HOOK(0x5033F0, EditorRigidObj_Open);

void EditorRigidObj_Close(void)
{
    TaskType_Free(gEditorRigidObjType);
}
HOOK(0x5034C0, EditorRigidObj_Close);

uint16_t EditorRigidObj_GetType(void)
{
    return gEditorRigidObjType;
}
HOOK(0x5034D0, EditorRigidObj_GetType);
