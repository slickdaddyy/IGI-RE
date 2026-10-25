#include "hook.h"
#include "igi.h"
#include "ui/ui.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B16D0, int, File_Read, (void *f, void *buf, int size));
ORIG(0x4B1780, int, File_GetSize, (void *f));
ORIG(0x424300, void, MenuWindow_ForwardLoad, (void *task));

/* a scrollable text file, word wrapped into lines */
typedef struct AsciiBox {
    MenuWindow w;
    char font[0x40];
    char file[0x40];
    Sprite *sprite;
    Text *text;
    char *buf; /* starts with a 0 byte, then the lines, each ending in 0 */
    char *line;
    int lineCount;
    int lineIndex;
    int lineHeight;
    int active;
    MenuWindow *scrollBar;
} AsciiBox;

_Static_assert(sizeof(AsciiBox) == 0xF8, "AsciiBox");

extern uint16_t gAsciiBoxType;
extern void *gAsciiBoxParams;

void AsciiBox_Create(AsciiBox *ab, void *arg);
void AsciiBox_Delete(AsciiBox *ab);
void AsciiBox_Reload(AsciiBox *ab);
int AsciiBox_Wrap(Text *t, Sprite *font, char *s, int width);
void AsciiBox_Changed(AsciiBox *ab, void *arg);
void AsciiBox_Run(AsciiBox *ab);
void AsciiBox_Draw(AsciiBox *ab);
void AsciiBox_GetParams(void *task, void **out);
void AsciiBox_Activate(AsciiBox *ab);
void AsciiBox_Deactivate(AsciiBox *ab);

void AsciiBox_Init(void)
{
    AsciiBox ab;

    gAsciiBoxType = TaskType_New("AsciiBox", sizeof(AsciiBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gAsciiBoxType, 1, AsciiBox_Run);
    TaskType_SetHandler2(gAsciiBoxType, Screen_GetDrawEvent(), AsciiBox_Draw);
    TaskType_SetHandler2(gAsciiBoxType, 2, AsciiBox_Delete);
    TaskType_SetHandler(gAsciiBoxType, 0, AsciiBox_Create);
    TaskType_SetHandler(gAsciiBoxType, Param_GetTableEvent(), AsciiBox_GetParams);
    TaskType_SetHandler(gAsciiBoxType, Param_GetChangedEvent(), AsciiBox_Changed);
    TaskType_SetHandler2(gAsciiBoxType, MenuWindow_GetLoadEvent(), MenuWindow_ForwardLoad);
    TaskType_SetHandler2(gAsciiBoxType, MenuWindow_GetSaveEvent(), MenuWindow_ForwardSave);
    TaskType_SetHandler2(gAsciiBoxType, Level_GetActivateEvent(), AsciiBox_Activate);
    TaskType_SetHandler2(gAsciiBoxType, Level_GetDeactivateEvent(), AsciiBox_Deactivate);
    TaskType_SetHandler2(gAsciiBoxType, MenuWindow_GetUpdateTextEvent(), AsciiBox_Reload);
    gAsciiBoxParams = ParamTable_New(4);
    ParamTable_Add(gAsciiBoxParams, &ab, ab.font, Param_String32(), 7, NULL);
    ParamTable_Add(gAsciiBoxParams, &ab, ab.file, Param_String32(), 7, NULL);
    ParamTable_Add(gAsciiBoxParams, &ab, &ab.w.w, Param_Int32(), 7, NULL);
    ParamTable_Add(gAsciiBoxParams, &ab, &ab.w.h, Param_Int32(), 7, NULL);
}
HOOK(0x41C700, AsciiBox_Init);

void AsciiBox_Create(AsciiBox *ab, void *arg)
{
    CALL_BASE(0, ab, arg);
    ab->w.w = 100;
    ab->w.h = 100;
    ab->w.centreX = 1;
    ab->w.centreY = 1;
    ab->active = 0;
    ab->text = NULL;
    ab->sprite = NULL;
    ab->buf = NULL;
    ab->font[0] = 0;
    ab->file[0] = 0;
    ab->scrollBar = SlideBar_NewVertical(NULL, ab->w.menu);
}
HOOK(0x41C8D0, AsciiBox_Create);

static void free_buf(AsciiBox *ab)
{
    Mem_PushMode(1);
    Mem_Free(ab->buf);
    ab->buf = NULL;
    Mem_PopMode();
}

void AsciiBox_Delete(AsciiBox *ab)
{
    MenuManager_Free(ab->scrollBar);
    if (ab->text) {
        Text_Free(ab->text);
        ab->text = NULL;
    }
    if (ab->buf) {
        free_buf(ab);
        ab->buf = NULL;
    }
    CALL_OWN(Level_GetDeactivateEvent(), ab);
}
HOOK(0x41C940, AsciiBox_Delete);

/* loads the file and wraps it to the box width */
void AsciiBox_Reload(AsciiBox *ab)
{
    void *f;

    if (ab->buf)
        free_buf(ab);
    ab->buf = NULL;
    ab->lineCount = 0;
    if (ab->w.menu && ab->sprite && (f = File_Open(ab->file, "r")) != NULL) {
        int size = File_GetSize(f);

        Mem_PushMode(1);
        ab->buf = Mem_Alloc(size + 2, 4);
        Mem_PopMode();
        File_Read(f, ab->buf + 1, size);
        File_Close(f);
        ab->buf[size + 1] = 0;
        ab->buf[0] = 0;
        ab->lineCount = AsciiBox_Wrap(ab->text, ab->sprite, ab->buf + 1, ab->w.w - 28);
    }
    ab->lineIndex = 0;
    ab->line = ab->buf + 1;
    CALL_CHILDREN(&ab->w.task, MenuWindow_GetUpdateTextEvent);
}
HOOK(0x41C9D0, AsciiBox_Reload);

/* splits s into lines in place (spaces at the wrap points become 0) and
   returns the line count */
int AsciiBox_Wrap(Text *t, Sprite *font, char *s, int width)
{
    char *line = s, *word = s, *end;
    int lines = 0;

    if (!s)
        return 0;
    while (*word) {
        char c;

        for (end = word; *end && *end != ' ' && *end != '\t' && *end != '\n'; end++)
            ;
        c = *end;
        *end = 0;
        Text_SetString(t, line);
        *end = c;
        if (Text_GetWidth(t) > width && word != line) {
            word[-1] = 0;
            lines++;
            line = word;
        }
        switch (*end) {
        case 0:
            lines++;
            word = end;
            break;
        case '\t':
        case ' ':
            word = end + 1;
            break;
        case '\n':
            word = end + 1;
            lines++;
            *end = 0;
            line = word;
            break;
        }
    }
    return lines;
}
HOOK(0x41CB40, AsciiBox_Wrap);

void AsciiBox_Changed(AsciiBox *ab, void *arg)
{
    MenuWindow *bar;

    if (ab->text) {
        Text_Free(ab->text);
        ab->text = NULL;
    }
    if (ab->w.menu) {
        ab->sprite = MenuManager_FindFont(ab->w.menu, ab->font);
        if (ab->sprite) {
            ab->text = Text_New(ab->sprite, 0, 0, gEmptyString);
            ab->text->unk20 = 0;
            Text_SetString(ab->text, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");
            ab->lineHeight = Text_GetHeight(ab->text) + 2;
        }
    }
    AsciiBox_Reload(ab);
    bar = ab->scrollBar;
    HANDLER(Param_GetChangedEvent(), bar->task.type)(bar, arg);
    CALL_BASE(Param_GetChangedEvent(), ab, arg);
}
HOOK(0x41CC40, AsciiBox_Changed);

/* follows the scroll bar one line at a time */
void AsciiBox_Run(AsciiBox *ab)
{
    if (ab->buf) {
        int pos = SlideBar_GetPos(ab->scrollBar);

        while (pos < ab->lineIndex) {
            ab->line--;
            while (ab->line[-1])
                ab->line--;
            ab->lineIndex--;
        }
        while (pos > ab->lineIndex) {
            while (*ab->line++)
                ;
            ab->lineIndex++;
        }
    }
    CALL_OWN(1, ab->scrollBar);
    CALL_BASE(1, ab);
}
HOOK(0x41CD20, AsciiBox_Run);

void AsciiBox_Draw(AsciiBox *ab)
{
    float alpha = ui_alpha(ab->w.menu);
    MenuWindow *bar;

    SlideBar_SetRange(ab->scrollBar, ab->w.w - 19 + ab->w.x, ab->w.y, 0, ab->w.h, ab->lineCount);
    {
        uint16_t base = MenuWindow_GetType();

        ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(ab);
    }
    bar = ab->scrollBar;
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), bar->task.type))(bar);
    if (ab->sprite && ab->buf) {
        const char *p = ab->line;
        int rows = (ab->w.h - 4) / ab->lineHeight;
        int left = ab->lineCount - ab->lineIndex;
        int n = rows < left ? rows : left;
        int x = (Display_GetMode()->width >> 1) - 316 + ab->w.x;
        int y = (Display_GetMode()->height >> 1) - 236 + ab->w.y;

        if (n > 0) {
            float xf = x;

            do {
                Text_SetString(ab->text, p);
                ab->text->x = xf;
                ab->text->y = y;
                ab->text->color[0] = 128.0f;
                ab->text->color[1] = 128.0f;
                ab->text->color[2] = 128.0f;
                ab->text->flags |= 0x80000;
                ab->text->alpha = alpha;
                Text_DrawAlt(ab->text);
                while (*p)
                    p++;
                p++;
                y += ab->lineHeight;
            } while (--n);
        }
    }
    /* the frame leaves room for the scroll bar */
    ab->w.w -= 20;
    MenuWindow_DrawEnd(ab, 0.5f, alpha, 0);
    ab->w.w += 20;
}
HOOK(0x41CDF0, AsciiBox_Draw);

void AsciiBox_GetParams(void *task, void **out)
{
    *out = gAsciiBoxParams;
}
HOOK(0x41D000, AsciiBox_GetParams);

void AsciiBox_Activate(AsciiBox *ab)
{
    if (!ab->active)
        ab->active = 1;
}
HOOK(0x41D010, AsciiBox_Activate);

void AsciiBox_Deactivate(AsciiBox *ab)
{
    if (ab->active)
        ab->active = 0;
}
HOOK(0x41D030, AsciiBox_Deactivate);

void AsciiBox_Shutdown(void)
{
    QTaskList_Free(gAsciiBoxParams);
    TaskType_Free(gAsciiBoxType);
    gAsciiBoxType = 0x181;
}
HOOK(0x41D050, AsciiBox_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B5F4) uint16_t gAsciiBoxType INIT(385);
PAD(igd, 53B5F4, 53B5F6, 0x2);
BSS(57BBBC) void *gAsciiBoxParams;
