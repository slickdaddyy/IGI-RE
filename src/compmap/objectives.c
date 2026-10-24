#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "compmap/compmap.h"

/* one mission objective (0xCC bytes) */
typedef struct Objective {
    char name[0x20];
    int taskId;          /* 0x20, -1 = none */
    char unk24[0xA0];
    int complete;        /* 0xC4 */
    int failed;          /* 0xC8 */
} Objective;

ORIG(0x488700, void, SubLevel_Run, (void *task));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x467590, Objective *, ComputerObjective_Get, (int i));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));

static const float kTabX = 0x1.99999ap-4f;
static const float kMapTabY = 0x1.9ddddep-1f;
static const float kTabY = 0x1.8cccccp-1f;
static const float kButtonX = -0x1p-3f;
static const float kButtonY = -0x1.555556p-3f;
static const double kTextX = 0.15;
static const double kTextXNoMap = 0.1;
static const double kTextY = 0.1;
static const float kBackZ = 0x1.0f3d70p+9f;
static const float kTextZ = 0x1.053d70p+9f;

void ComputerObjectives_Create(ComputerObjectives *o);
void ComputerObjectives_Draw(ComputerObjectives *o);
void ComputerObjectives_OnTab(ComputerItem *item, int *id);
void ComputerObjectives_OnSelect(ComputerButtonItem *b, int *id);

void ComputerObjectives_Init(void)
{
    uint16_t type = TaskType_New("ComputerObjectives", sizeof(ComputerObjectives), Task_GetRootType(), 0);

    gObjectivesType = type;
    TaskType_SetHandler(type, 0, ComputerObjectives_Create);
    TaskType_SetHandler2(gObjectivesType, 1, SubLevel_Run);
    TaskType_SetHandler2(gObjectivesType, 2, Task_Nothing);
    TaskType_SetHandler2(gObjectivesType, Screen_GetDrawEvent(), ComputerObjectives_Draw);
}
HOOK(0x468E20, ComputerObjectives_Init);

void ComputerObjectives_Create(ComputerObjectives *o)
{
    ComputerRes *r = ComputerRes_Get();
    int w = Display_GetMode()->width;
    int h = Display_GetMode()->height;
    Task *panel = MenuItemTask_New(o);
    const char *s;
    Sprite *font;
    float fw, fh;

    ComputerMenu_SetPanel(panel);
    if (Computer_IsMapEnabled()) {
        s = Resource_Load("MAP", 0);
        font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
        ComputerTextItem_New(panel, 1, 0, 0, 0, 0, 0, w * kTabX, h * kMapTabY, font, s, 1,
                             ComputerObjectives_OnTab);
    }
    fh = h;
    fw = w;
    s = Resource_Load("LOG", 0);
    font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
    ComputerTextItem_New(panel, 2, 0, 0, 0, 0, 0, fw * kTabX, fh * kTabY, font, s, 1, ComputerObjectives_OnTab);
    if (Computer_IsMapEnabled()) {
        float y = fh * kButtonY;
        float x = fw * kButtonX;
        int i;

        for (i = 0; i < 6; i++)
            o->button[i] = ComputerButtonItem_New(panel, i + 3, 0, 0, 0, 0, 0, x, y, 0, NULL, gEmptyString,
                                                  (Sprite *)&r->objective[0], (Sprite *)&r->objective[1],
                                                  (Sprite *)&r->objective[1], ComputerObjectives_OnSelect);
    }
}
HOOK(0x468E90, ComputerObjectives_Create);

void ComputerObjectives_OnTab(ComputerItem *item, int *id)
{
    if (!(ComputerMenu_GetPressed() & 0x10))
        return;
    if (*id == 1)
        ComputerMenu_SetScreen(1);
    else if (*id == 2)
        ComputerMenu_SetScreen(3);
}
HOOK(0x469030, ComputerObjectives_OnTab);

/* the buttons next to the objectives show the objective's task on the map */
void ComputerObjectives_OnSelect(ComputerButtonItem *b, int *id)
{
    unsigned int buttons;
    Objective *obj;

    if (b->base.id != *id)
        return;
    buttons = ComputerMenu_GetButtons();
    b->wasPressed = b->pressed;
    b->pressed = buttons & 0x10;
    if (!(buttons & 0x10))
        return;
    if (b->base.id < 3 || b->base.id > 8)
        return;
    obj = ComputerObjective_Get(b->base.id - 3);
    if (obj->taskId != -1) {
        ComputerMap_SetStartTask(Task_FindById(obj->taskId));
        ComputerMenu_SetScreen(1);
    }
}
HOOK(0x469060, ComputerObjectives_OnSelect);

void ComputerObjectives_Draw(ComputerObjectives *o)
{
    int w = Display_GetMode()->width;
    int h = Display_GetMode()->height;
    char status[64], text[256];
    int i, x, y;

    Draw_RectZ(0, 0, w, h, 0, 0, 0, 0x40040000, -1, kBackZ);
    ComputerRes_Get();
    Mem_PushMode(1);
    if (Computer_IsMapEnabled())
        x = round_int(w * kTextX);
    else
        x = round_int(w * kTextXNoMap);
    y = round_int(h * kTextY);
    for (i = 0; i < 6; i++) {
        Objective *obj = ComputerObjective_Get(i);
        const char *s;
        Sprite *font;
        int y2;

        if (!obj || !obj->name[0])
            continue;
        if (Computer_IsMapEnabled())
            ComputerButtonItem_SetPos(o->button[i], x - 34, y + 2);
        crt_sprintf(text, "%s %d: ", Resource_Load("OBJECTIVE", 0), i + 1);
        if (obj->failed)
            s = Resource_Load("FAILED", 0);
        else if (obj->complete)
            s = Resource_Load("COMPLETE", 0);
        else
            s = Resource_Load("INCOMPLETE", 0);
        crt_sprintf(status, "%s", s);
        strcpy(text + strlen(text), status);
        font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
        y2 = Computer_DrawWrapped(x, y, kTextZ, 400, 0x78, text, font, 2);
        if (Resource_Find(obj->name)) {
            s = Resource_Load(obj->name, 0);
        } else {
            crt_sprintf(text, "RM: %s", obj->name);
            s = text;
        }
        font = MenuManager_FindFont(MENU_MANAGER(), "font4.fnt");
        y = Computer_DrawWrapped(x, y2, kTextZ, 400, 0x78, s, font, 1) + 8;
    }
    Mem_PopMode();
    CALL_CHILDREN(&o->task, Screen_GetDrawEvent);
}
HOOK(0x469110, ComputerObjectives_Draw);

void ComputerObjectives_Shutdown(void)
{
    TaskType_Free(gObjectivesType);
}
HOOK(0x469420, ComputerObjectives_Shutdown);

Task *ComputerObjectives_New(void *parent)
{
    return Task_New(parent, gObjectivesType, 0);
}
HOOK(0x469430, ComputerObjectives_New);

/* draws text word by word, wrapping at width; returns the y below it */
int Computer_DrawWrapped(int x, int y, float z, int width, int unused, const char *text, Sprite *font, int color)
{
    char word[256], line[256];
    int done = 0, cx = x, cy = y, left = width;
    Text *t;

    line[0] = 0;
    Mem_PushMode(1);
    t = Text_New(font, 0, 0, gEmptyString);
    Mem_PopMode();
    t->flags |= 0x40000;
    t->z = z;
    t->color[0] = gItemColor[color][0];
    t->color[1] = gItemColor[color][1];
    t->color[2] = gItemColor[color][2];
    for (;;) {
        int n = 0, ww;

        word[0] = 0;
        for (;;) {
            char c = *text++;

            if (!c) {
                word[n] = 0;
                done = 1;
                break;
            }
            if (c == ' ') {
                word[n] = ' ';
                word[n + 1] = 0;
                break;
            }
            word[n++] = c;
        }
        ww = Sprite_StringWidth(font, word);
        if (ww < left) {
            strcpy(line + strlen(line), word);
        } else {
            cx = x;
            cy += Sprite_GetHeight(font, line);
            line[0] = 0;
            strcpy(line, word);
            left = width;
        }
        left -= ww;
        Text_SetString(t, word);
        t->unk20 = -1;
        t->x = cx;
        t->y = cy;
        Text_DrawAlt(t);
        cx += ww;
        if (done) {
            int lh = Sprite_GetHeight(font, line);

            Text_Free(t);
            return cy + lh;
        }
    }
}
HOOK(0x469450, Computer_DrawWrapped);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BDD70) uint16_t gObjectivesType;
PAD(igb, 5BDD70, 5BDD72, 0x2);
