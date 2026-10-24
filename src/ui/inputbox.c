#include "hook.h"
#include "igi.h"
#include "ui/ui.h"

ORIG(0x4B8030, void, Script_RegisterString, (const char *name, char *buf, int size));

typedef struct QVertex {
    float x, y, z;
    float u, v;
    float a, r, g, b;
} QVertex;

typedef struct QVertexStrip {
    int untextured;
    unsigned int flags;
    int unk8;
    int count;
    QVertex *verts;
} QVertexStrip;

extern void (*gRenderFuncs[])(QVertexStrip *);
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void *gQVertexPalette;

/* a one line text field; the script sees the text as "<name>.zData" */
typedef struct InputBox {
    MenuWindow w;
    char text[0x50];
    VarString init;
    VarString forward;
    VarString enter;
    char registered;
    char editing;
    char password;
    char pad197;
    Sprite *sprite;
    Text *t;
    int cursor;
    int maxLength;
} InputBox;

_Static_assert(sizeof(InputBox) == 0x1A8, "InputBox");

extern uint16_t gInputBoxType;
extern void *gInputBoxParams;
extern InputBox *gInputBoxFilling;

void InputBox_Create(InputBox *ib, void *arg);
void InputBox_Delete(InputBox *ib);
void InputBox_Changed(InputBox *ib, void *arg);
void InputBox_Run(InputBox *ib);
void InputBox_DeleteChar(InputBox *ib, int pos);
void InputBox_InsertChar(InputBox *ib, int pos, char c);
void InputBox_Draw(InputBox *ib);
void InputBox_GetParams(void *task, void **out);
void InputBox_Layout(InputBox *ib);
void InputBox_Forward918(InputBox *ib);
void InputBox_Activate(InputBox *ib);
void InputBox_Deactivate(InputBox *ib);

void InputBox_Init(void)
{
    InputBox ib;

    gInputBoxType = TaskType_New("InputBox", sizeof(InputBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gInputBoxType, 1, InputBox_Run);
    TaskType_SetHandler2(gInputBoxType, Screen_GetDrawEvent(), InputBox_Draw);
    TaskType_SetHandler2(gInputBoxType, 2, InputBox_Delete);
    TaskType_SetHandler(gInputBoxType, 0, InputBox_Create);
    TaskType_SetHandler(gInputBoxType, Param_GetTableEvent(), InputBox_GetParams);
    TaskType_SetHandler(gInputBoxType, Param_GetChangedEvent(), InputBox_Changed);
    TaskType_SetHandler2(gInputBoxType, MenuWindow_GetEvent914(), InputBox_Layout);
    TaskType_SetHandler2(gInputBoxType, MenuWindow_GetEvent918(), InputBox_Forward918);
    TaskType_SetHandler2(gInputBoxType, Level_GetEventDA8(), InputBox_Activate);
    TaskType_SetHandler2(gInputBoxType, Level_GetEventDAC(), InputBox_Deactivate);
    gInputBoxParams = ParamTable_New(6);
    ParamTable_Add(gInputBoxParams, &ib, &ib.w.w, Param_Int32(), 7, NULL);
    ParamTable_Add(gInputBoxParams, &ib, &ib.maxLength, Param_Int32(), 7, NULL);
    ParamTable_Add(gInputBoxParams, &ib, &ib.init, Param_VarString(), 7, NULL);
    ParamTable_Add(gInputBoxParams, &ib, &ib.forward, Param_VarString(), 7, NULL);
    ParamTable_Add(gInputBoxParams, &ib, &ib.enter, Param_VarString(), 7, NULL);
    ParamTable_Add(gInputBoxParams, &ib, &ib.password, Param_Bool8(), 7, NULL);
}
HOOK(0x41D080, InputBox_Init);

void InputBox_Create(InputBox *ib, void *arg)
{
    CALL_BASE(0, ib, arg);
    ib->w.h = 15;
    ib->text[0] = gEmptyString[0];
    ib->cursor = 4;
    ib->w.unk3C = 1;
    ib->w.unk40 = 1;
    ib->maxLength = 20;
    VarString_Init(&ib->init);
    VarString_Init(&ib->forward);
    VarString_Init(&ib->enter);
    ib->registered = 0;
    ib->editing = 0;
    ib->password = 0;
    ib->t = NULL;
    ib->w.selectable = 1;
}
HOOK(0x41D290, InputBox_Create);

void InputBox_Delete(InputBox *ib)
{
    if (ib->t) {
        Text_Free(ib->t);
        ib->t = NULL;
    }
    VarString_Free(&ib->init);
    VarString_Free(&ib->forward);
    VarString_Free(&ib->enter);
    CALL_OWN(Level_GetEventDAC(), ib);
}
HOOK(0x41D330, InputBox_Delete);

void InputBox_Changed(InputBox *ib, void *arg)
{
    VarString_Bind(&ib->init, ib);
    VarString_Bind(&ib->forward, ib);
    VarString_Bind(&ib->enter, ib);
    if (ib->t) {
        Text_Free(ib->t);
        ib->t = NULL;
    }
    CALL_BASE(Param_GetChangedEvent(), ib, arg);
    ib->sprite = MenuManager_FindFont(ib->w.menu, "font1.fnt");
    if (ib->sprite)
        ib->t = Text_New(ib->sprite, 0, 0, gEmptyString);
}
HOOK(0x41D3A0, InputBox_Changed);

static int text_len(const char *s)
{
    int n = 0;

    while (s[n])
        n++;
    return n;
}

void InputBox_Run(InputBox *ib)
{
    MenuInput *in = MenuManager_GetInput(ib->w.menu);

    if (in->entered) {
        ib->editing = 0;
        ib->w.key = 0;
    } else if (!ib->editing) {
        MenuWindow_HitTest(ib, ib->w.x, ib->w.y, ib->w.w, ib->w.h, NULL, NULL);
        if (ib->w.clicked || ib->w.key == -7) {
            MenuManager_Pause(ib->w.menu);
            ib->editing = 1;
            ib->cursor = text_len(ib->text);
            Sound_PlayName("menu_k01", 0, NULL);
        }
    } else {
        while (Keys_GetCount()) {
            int k = Keys_Pop();
            int len = text_len(ib->text);

            switch (k) {
            case -10:
                ib->cursor = len;
                break;
            case -9:
                ib->cursor = 0;
                break;
            case -8:
                MenuManager_Resume(ib->w.menu);
                ib->editing = 0;
                break;
            case -7:
                MenuManager_Resume(ib->w.menu);
                ib->editing = 0;
                if (VarString_HasExpr(&ib->enter))
                    VarString_EvalReal(&ib->enter, ib);
                break;
            case -6:
                if (ib->cursor < len)
                    InputBox_DeleteChar(ib, ib->cursor);
                break;
            case -5:
                if (ib->cursor > 0)
                    InputBox_DeleteChar(ib, ib->cursor - 1);
                break;
            case -4:
            case -3:
                break;
            case -2:
                ib->cursor = ib->cursor + 1 > len ? len : ib->cursor + 1;
                break;
            case -1:
                ib->cursor = ib->cursor - 1 < 0 ? 0 : ib->cursor - 1;
                break;
            default:
                if (len < ib->maxLength - 1 && k != 0 && (char)k != '"')
                    InputBox_InsertChar(ib, ib->cursor, k);
                break;
            }
        }
    }
    CALL_BASE(1, ib);
}
HOOK(0x41D450, InputBox_Run);

void InputBox_DeleteChar(InputBox *ib, int pos)
{
    char *p = &ib->text[pos];

    while (*p) {
        p[0] = p[1];
        p++;
    }
    ib->cursor = pos;
    Sound_PlayName("menu_k01", 0, NULL);
}
HOOK(0x41D660, InputBox_DeleteChar);

void InputBox_InsertChar(InputBox *ib, int pos, char c)
{
    char name[64];
    int i, n = c;

    for (i = text_len(ib->text); i >= pos; i--)
        ib->text[i + 1] = ib->text[i];
    ib->text[pos] = c;
    ib->cursor = pos + 1;
    if (n < 0)
        n = -n;
    crt_sprintf(name, "menu_k0%.1d", n % 7 + 1);
    Sound_PlayName(name, 0, NULL);
}
HOOK(0x41D6A0, InputBox_InsertChar);

static int blink_on(void)
{
    int t = (unsigned)Flow_GetFrames() * 2;

    return t / Flow_GetFps() & 1;
}

void InputBox_Draw(InputBox *ib)
{
    float alpha = ui_alpha(ib->w.menu);
    uint16_t base = MenuWindow_GetType();

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(ib);
    if (ib->t) {
        int x = ib->w.x + 2;
        int y = ib->w.y + 2;
        const char *s = ib->text;
        char stars[256];

        if (ib->password) {
            int n = text_len(ib->text), i;

            for (i = 0; i < n; i++)
                stars[i] = '*';
            stars[n] = 0;
            s = stars;
        }
        Text_SetString(ib->t, s);
        ib->t->x = x;
        ib->t->y = y;
        ib->t->flags |= 0x80000;
        ib->t->alpha = alpha;
        Text_DrawAlt(ib->t);
        /* blinking cursor, twice a second */
        if (ib->editing && blink_on()) {
            char tmp[44];
            QVertex v[2];
            QVertexStrip strip;
            int x0;

            crt_strncpy(tmp, s, ib->cursor);
            tmp[ib->cursor] = 'a';
            tmp[ib->cursor + 1] = 0;
            Text_SetString(ib->t, tmp);
            x0 = x + Text_GetWidth(ib->t) - 6;
            y += 10;
            /* the alpha of both vertices is left unset, like in the original */
            v[0].x = x0;
            v[0].y = y;
            v[0].z = 1.0f;
            v[0].u = 0;
            v[0].v = 0;
            v[0].r = 1.0f;
            v[0].g = 1.0f;
            v[0].b = 1.0f;
            v[1].x = x0 + 6;
            v[1].y = y;
            v[1].z = 1.0f;
            v[1].u = 0;
            v[1].v = 0;
            v[1].r = 1.0f;
            v[1].g = 1.0f;
            v[1].b = 1.0f;
            strip.untextured = 1;
            strip.flags = 0;
            strip.unk8 = 0;
            strip.count = 2;
            strip.verts = v;
            gQVertexTexture = NULL;
            gQVertexPalette = NULL;
            gRenderFuncs[gQVertexStrip](&strip);
        }
    }
    MenuWindow_DrawEnd(ib, 0.5f, alpha, ib->w.focused);
}
HOOK(0x41D720, InputBox_Draw);

void InputBox_GetParams(void *task, void **out)
{
    *out = gInputBoxParams;
}
HOOK(0x41D9D0, InputBox_GetParams);

void InputBox_Layout(InputBox *ib)
{
    ib->text[0] = 0;
    if (VarString_HasExpr(&ib->init)) {
        gInputBoxFilling = ib;
        VarString_EvalReal(&ib->init, ib);
        gInputBoxFilling = NULL;
    }
    CALL_CHILDREN(&ib->w.task, MenuWindow_GetEvent914);
}
HOOK(0x41D9E0, InputBox_Layout);

void InputBox_Forward918(InputBox *ib)
{
    if (VarString_HasExpr(&ib->forward))
        VarString_EvalReal(&ib->forward, ib);
    CALL_CHILDREN(&ib->w.task, MenuWindow_GetEvent918);
}
HOOK(0x41DA90, InputBox_Forward918);

void InputBox_Activate(InputBox *ib)
{
    char name[64];
    char buf[128];

    if (!ib->registered) {
        Task_GetName(name, ib);
        crt_sprintf(buf, "%s.zData", name);
        Script_RegisterString(buf, ib->text, 0x28);
        ib->registered = 1;
    }
}
HOOK(0x41DB30, InputBox_Activate);

void InputBox_Deactivate(InputBox *ib)
{
    char name[64];
    char buf[128];

    if (ib->registered) {
        Task_GetName(name, ib);
        crt_sprintf(buf, "%s.zData", name);
        Script_RemoveSymbol(buf);
        ib->registered = 0;
    }
}
HOOK(0x41DB90, InputBox_Deactivate);

void InputBox_Shutdown(void)
{
    QTaskList_Free(gInputBoxParams);
    TaskType_Free(gInputBoxType);
    gInputBoxType = 0x181;
}
HOOK(0x41DBF0, InputBox_Shutdown);

/* called by script functions while an InputBox runs its init expression */
void InputBox_SetText(const char *s)
{
    char *d = gInputBoxFilling->text;

    while ((*d++ = *s++) != 0)
        ;
}
HOOK(0x41DC20, InputBox_SetText);
