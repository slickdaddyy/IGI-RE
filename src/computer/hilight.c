#include <string.h>

#include "hook.h"
#include "computer/computer.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x46B4D0, char *, ComputerRes_Get, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5490, int, crt_atoi, (const char *s));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F00, void *, Param_DropDownCombo, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

#define MAX_IDS 64
#define NUM_ICONS 19

/* marks a list of objects (by id, "1 4 7-9") on the map computer while
   cond is true: icon, marker shape and color, two labels */
typedef struct ComputerHilight {
    Task task;
    Vec3d pos;
    VarString cond;
    char *icon;
    char idText[256];
    int count;
    int id[MAX_IDS];
    void *iconSprite;
    char marker[32];
    int markerVal;
    char color[32];
    int colorVal;
    char text[256];
    char text2[256];
    int unk4DC;
} ComputerHilight;

_Static_assert(offsetof(ComputerHilight, icon) == 0x88, "ComputerHilight");
_Static_assert(offsetof(ComputerHilight, iconSprite) == 0x290, "ComputerHilight");
_Static_assert(offsetof(ComputerHilight, text2) == 0x3DC, "ComputerHilight");
_Static_assert(sizeof(ComputerHilight) == 0x4E0, "ComputerHilight");

typedef struct EnumDesc {
    int count;
    char **names;
} EnumDesc;

extern uint16_t gHilightType;
extern void *gHilightList;
extern void *gHilightParams;
extern EnumDesc gHilightIcons;

void ComputerHilight_Create(ComputerHilight *h);
void ComputerHilight_Delete(ComputerHilight *h);
void ComputerHilight_GetParams(void *task, void **out);
void ComputerHilight_Changed(ComputerHilight *h);
void ComputerHilight_DrawOnMap(ComputerHilight *h, MapQuery *q);

void ComputerHilight_Open(void)
{
    ComputerHilight h;
    uint8_t ev;

    gHilightIcons.count = NUM_ICONS;
    gHilightIcons.names = (char **)(ComputerRes_Get() + 0x4B0);
    gHilightType = TaskType_New("ComputerHilight", sizeof(ComputerHilight), EditorRigidObj_GetType(), 0);
    TaskType_SetHandler(gHilightType, 0, ComputerHilight_Create);
    TaskType_SetHandler2(gHilightType, 2, ComputerHilight_Delete);
    TaskType_SetHandler(gHilightType, Param_GetTableEvent(), ComputerHilight_GetParams);
    TaskType_SetHandler(gHilightType, Param_GetChangedEvent(), ComputerHilight_Changed);
    ev = ComputerObject_GetEvent0();
    TaskType_SetHandler(gHilightType, ev, ComputerHilight_DrawOnMap);
    gHilightParams = ParamTable_New(8);
    ParamTable_Add(gHilightParams, &h, &h.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gHilightParams, &h, &h.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gHilightParams, &h, h.idText, Param_String256(), 7, NULL);
    ParamTable_Add(gHilightParams, &h, &h.icon, Param_DropDownCombo(), 7, &gHilightIcons);
    ParamTable_Add(gHilightParams, &h, h.marker, Param_String32(), 7, NULL);
    ParamTable_Add(gHilightParams, &h, h.color, Param_String32(), 7, NULL);
    ParamTable_Add(gHilightParams, &h, h.text, Param_String256(), 7, NULL);
    ParamTable_Add(gHilightParams, &h, h.text2, Param_String256(), 7, NULL);
    gHilightList = QTaskList_New(32);
}
HOOK(0x4682F0, ComputerHilight_Open);

void ComputerHilight_Create(ComputerHilight *h)
{
    VarString_Init(&h->cond);
    h->idText[0] = 0;
    h->icon = NULL;
    h->count = 0;
    h->iconSprite = NULL;
    crt_sprintf(h->marker, "MARKER_NONE");
    h->markerVal = -1;
    crt_sprintf(h->color, "MARKER_COLOR_NONE");
    h->colorVal = -1;
    h->text[0] = 0;
    h->text2[0] = 0;
    QTaskList_Add(gHilightList, h);
}
HOOK(0x4684F0, ComputerHilight_Create);

void ComputerHilight_Delete(ComputerHilight *h)
{
    VarString_Free(&h->cond);
    QTaskList_Remove(gHilightList, h);
    if (h->icon) {
        Mem_Free(h->icon);
        h->icon = NULL;
    }
}
HOOK(0x468570, ComputerHilight_Delete);

void ComputerHilight_GetParams(void *task, void **out)
{
    *out = gHilightParams;
}
HOOK(0x4685B0, ComputerHilight_GetParams);

void ComputerHilight_ParseIds(ComputerHilight *h);

void ComputerHilight_Changed(ComputerHilight *h)
{
    int i;

    ComputerHilight_ParseIds(h);
    h->iconSprite = NULL;
    if (h->icon) {
        for (i = 0; i < NUM_ICONS; i++) {
            if (!strcmp(h->icon, ((char **)(ComputerRes_Get() + 0x4B0))[i])) {
                h->iconSprite = ComputerRes_Get() + 0x2E8 + i * 0x18;
                break;
            }
        }
    }
    Mem_PushMode(1);
    h->markerVal = Script_EvalExpr(h->marker);
    h->colorVal = Script_EvalExpr(h->color);
    Mem_PopMode();
    VarString_Bind(&h->cond, h);
}
HOOK(0x4685C0, ComputerHilight_Changed);

void ComputerHilight_ParseIds(ComputerHilight *h)
{
    /* a range can fill the list, the last number is then stored one past it */
    int *id = (int *)((char *)h + offsetof(ComputerHilight, id));
    char *p = h->idText;
    int range = 0;
    int v, n;

    h->count = 0;
    do {
        switch (*p) {
        case 0:
            return;
        case ' ':
            p++;
            break;
        case '-':
            range = 1;
            p++;
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            v = crt_atoi(p);
            if (range && h->count) {
                for (n = id[h->count - 1] + 1; n < v && h->count < MAX_IDS; n++)
                    id[h->count++] = n;
                range = 0;
            }
            id[h->count++] = v;
            for (; v; v /= 10)
                p++;
            break;
        default:
            range = 0;
            p++;
            break;
        }
    } while (h->count < MAX_IDS);
}
HOOK(0x468690, ComputerHilight_ParseIds);

void ComputerHilight_DrawOnMap(ComputerHilight *h, MapQuery *q)
{
    Task *t;
    TaskFn fn;
    int i;

    if (!(int)round_ll(VarString_EvalReal(&h->cond, h)))
        return;
    q->sprite = h->iconSprite;
    q->marker = h->markerVal;
    q->color = h->colorVal;
    q->text = h->text;
    q->text2 = h->text2;
    for (i = 0; i < h->count; i++) {
        t = Task_FindById(h->id[i]);
        if (!t)
            continue;
        fn = HANDLER(ComputerObject_GetEvent0(), t->type);
        if (fn)
            fn(t, q);
    }
}
HOOK(0x4687A0, ComputerHilight_DrawOnMap);

void ComputerHilight_Close(void)
{
    QTaskList_Free(gHilightParams);
    TaskType_Free(gHilightType);
    QTaskList_Free(gHilightList);
}
HOOK(0x468870, ComputerHilight_Close);

TaskArray *ComputerHilight_GetList(void)
{
    return gHilightList;
}
HOOK(0x4688A0, ComputerHilight_GetList);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53F6D0) uint16_t gHilightType INIT(385);
PAD(igd, 53F6D0, 53F6D2, 0x2);
BSS(5BDC80) void *gHilightList;
BSS(5BDC80) void *gHilightParams;
PAD(igb, 5BDC80, 5BDC88, 0x8);
BSS(5BDC80) EnumDesc gHilightIcons;
