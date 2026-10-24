#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"

ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B7FC0, void, Script_RegisterReal, (const char *name, double *value));
ORIG(0x4B50B0, void, QVertex_DrawFrame, (Sprite *s, float x, float y, float r, float g, float b, float alpha, float z,
                                         int frame, unsigned int flags, int unk20));
ORIG(0x4A55FE, double, crt_floor, (double x));

/* a slider with an arrow button at each end and a knob between them; the
   value goes from 0 to 1, or in steps when "steps" is set. The script reads
   "<name>.vValue" (0..1) and "<name>.nValue" (the step) */
typedef struct SlideBar {
    MenuWindow w;
    char name[0x40]; /* sprites are <name>_1.spr, _2.spr and _3.spr (knob) */
    Sprite *knob;
    MenuButton *dec;
    MenuButton *inc;
    double value;
    float step;
    int length;
    int pos;
    char horizontal;
    char registered;
    char padB6[2];
    VarString valueExpr;
    VarString forward;
    VarString action;
    VarString stepExpr;
    VarString stepsExpr;
    int steps;
    int unk24C;
} SlideBar;

_Static_assert(sizeof(SlideBar) == 0x250, "SlideBar");
_Static_assert(offsetof(SlideBar, value) == 0xA0, "SlideBar.value");
_Static_assert(offsetof(SlideBar, steps) == 0x248, "SlideBar.steps");

extern uint16_t gSlideBarType;
extern void *gSlideBarParams;

void SlideBar_Run(SlideBar *sb);
void SlideBar_Draw(SlideBar *sb);
void SlideBar_Delete(SlideBar *sb);
void SlideBar_Create(SlideBar *sb, void *arg);
void SlideBar_GetParams(void *task, void **out);
void SlideBar_Changed(SlideBar *sb, void *arg);
void SlideBar_Layout(SlideBar *sb);
void SlideBar_Forward918(SlideBar *sb);
void SlideBar_Activate(SlideBar *sb);
void SlideBar_Deactivate(SlideBar *sb);
int SlideBar_GetPos(SlideBar *sb);
void SlideBar_SetPos(SlideBar *sb, int pos);

void SlideBar_Init(void)
{
    SlideBar sb;

    gSlideBarType = TaskType_New("SlideBar", sizeof(SlideBar), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gSlideBarType, 1, SlideBar_Run);
    TaskType_SetHandler2(gSlideBarType, Screen_GetDrawEvent(), SlideBar_Draw);
    TaskType_SetHandler2(gSlideBarType, 2, SlideBar_Delete);
    TaskType_SetHandler(gSlideBarType, 0, SlideBar_Create);
    TaskType_SetHandler(gSlideBarType, Param_GetTableEvent(), SlideBar_GetParams);
    TaskType_SetHandler(gSlideBarType, Param_GetChangedEvent(), SlideBar_Changed);
    TaskType_SetHandler2(gSlideBarType, MenuWindow_GetEvent914(), SlideBar_Layout);
    TaskType_SetHandler2(gSlideBarType, MenuWindow_GetEvent918(), SlideBar_Forward918);
    TaskType_SetHandler2(gSlideBarType, Level_GetEventDA8(), SlideBar_Activate);
    TaskType_SetHandler2(gSlideBarType, Level_GetEventDAC(), SlideBar_Deactivate);
    gSlideBarParams = ParamTable_New(8);
    ParamTable_Add(gSlideBarParams, &sb, sb.name, Param_String32(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.horizontal, Param_Bool8(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.length, Param_Int32(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.valueExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.forward, Param_VarString(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.action, Param_VarString(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.stepExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gSlideBarParams, &sb, &sb.stepsExpr, Param_VarString(), 7, NULL);
}
HOOK(0x41F340, SlideBar_Init);

void SlideBar_Create(SlideBar *sb, void *arg)
{
    CALL_BASE(0, sb, arg);
    if (arg)
        sb->w.menu = arg;
    sb->name[0] = 0;
    sb->knob = NULL;
    sb->length = 50;
    sb->horizontal = 1;
    sb->dec = MenuButton_New(NULL);
    sb->inc = MenuButton_New(NULL);
    sb->dec->w.menu = sb->w.menu;
    sb->inc->w.menu = sb->w.menu;
    sb->inc->w.unk40 = 0;
    sb->dec->w.unk40 = 0;
    sb->inc->w.unk3C = 0;
    sb->dec->w.unk3C = 0;
    sb->w.x = 300;
    sb->w.y = 230;
    sb->w.unk3C = 1;
    sb->w.unk40 = 1;
    VarString_Init(&sb->valueExpr);
    VarString_Init(&sb->forward);
    VarString_Init(&sb->action);
    VarString_Init(&sb->stepExpr);
    VarString_Init(&sb->stepsExpr);
    sb->value = 0;
    sb->registered = 0;
    sb->step = 0.05f;
    sb->steps = 0;
}
HOOK(0x41F5A0, SlideBar_Create);

void SlideBar_Delete(SlideBar *sb)
{
    VarString_Free(&sb->valueExpr);
    VarString_Free(&sb->forward);
    VarString_Free(&sb->action);
    VarString_Free(&sb->stepExpr);
    VarString_Free(&sb->stepsExpr);
    MenuManager_Free(sb->dec);
    MenuManager_Free(sb->inc);
    CALL_OWN(Level_GetEventDAC(), sb);
}
HOOK(0x41F6C0, SlideBar_Delete);

void SlideBar_Changed(SlideBar *sb, void *arg)
{
    MenuButton *inc = sb->inc;
    MenuButton *dec = sb->dec;
    uint8_t ev;
    int w, h;

    VarString_Bind(&sb->valueExpr, sb);
    VarString_Bind(&sb->forward, sb);
    VarString_Bind(&sb->action, sb);
    VarString_Bind(&sb->stepExpr, sb);
    VarString_Bind(&sb->stepsExpr, sb);
    if (VarString_HasExpr(&sb->stepExpr))
        sb->step = VarString_EvalReal(&sb->stepExpr, sb);
    else
        sb->step = 0.05f;
    if (sb->w.menu) {
        char buf[64];

        crt_sprintf(buf, "%s_1.spr", sb->name);
        str_copy(sb->dec->sprite, buf);
        crt_sprintf(buf, "%s_2.spr", sb->name);
        str_copy(sb->inc->sprite, buf);
        crt_sprintf(buf, "%s_3.spr", sb->name);
        sb->knob = MenuManager_FindSprite(sb->w.menu, buf);
    }
    ev = Param_GetChangedEvent();
    HANDLER(ev, sb->dec->w.task.type)(sb->dec, arg);
    ev = Param_GetChangedEvent();
    HANDLER(ev, sb->inc->w.task.type)(sb->inc, arg);
    if (sb->horizontal) {
        w = sb->length + inc->w.w + dec->w.w + 6;
        h = 4;
        if (sb->knob) {
            int m = dec->w.h > inc->w.h ? dec->w.h : inc->w.h;

            if (m > Sprite_Height(sb->knob))
                h = (dec->w.h > inc->w.h ? dec->w.h : inc->w.h) + 4;
            else
                h = Sprite_Height(sb->knob) + 4;
        }
    } else {
        h = sb->length + inc->w.h + dec->w.h + 6;
        w = 4;
        if (sb->knob) {
            int m = dec->w.w > inc->w.w ? dec->w.w : inc->w.w;

            if (m > Sprite_Width(sb->knob))
                w = (dec->w.w > inc->w.w ? dec->w.w : inc->w.w) + 4;
            else
                w = Sprite_Width(sb->knob) + 4;
        }
    }
    sb->w.w = w;
    sb->w.h = h;
    CALL_BASE(Param_GetChangedEvent(), sb, arg);
}
HOOK(0x41F740, SlideBar_Changed);

/* the arrows step the value, dragging the knob sets it */
void SlideBar_Run(SlideBar *sb)
{
    int changed = 0;
    float cur = sb->value;
    MenuButton *dec = sb->dec;
    MenuButton *inc = sb->inc;
    float step = sb->steps ? rf(1.0L / (sb->steps - 1)) : sb->step;
    long double v = cur;

    ((void (*)(void *))HANDLER(1, dec->w.task.type))(dec);
    ((void (*)(void *))HANDLER(1, inc->w.task.type))(inc);
    if (dec->w.clicked || sb->w.key == -1) {
        if (sb->value > 0.0) {
            Sound_PlayName("menu_high", 0, 0);
            v = (long double)cur - step;
            changed = 1;
        }
    } else if (inc->w.clicked || sb->w.key == -2) {
        if (sb->value < 1.0) {
            changed = 1;
            Sound_PlayName("menu_high", 0, 0);
            v = (long double)step + cur;
        }
    } else if (sb->knob) {
        char horizontal = sb->horizontal;
        int x = sb->w.x + 2;
        int y = sb->w.y + 2;
        int width, height, mx, my;

        if (horizontal) {
            x = dec->w.w + x + 1;
            width = sb->length;
            height = Sprite_Height(sb->knob);
        } else {
            y = y + dec->w.h + 1;
            width = Sprite_Width(sb->knob);
            height = sb->length;
        }
        MenuWindow_HitTest(sb, x, y, width, height, &mx, &my);
        if (sb->w.held) {
            if (horizontal)
                v = (long double)(mx - x) / (sb->length - Sprite_Width(sb->knob));
            else
                v = (long double)(my - y) / (sb->length - Sprite_Height(sb->knob));
            changed = 1;
        }
    }
    if (sb->steps) {
        int n = sb->steps - 1;

        v = crt_floor((long double)n * v + 0.5f);
        v /= sb->steps - 1;
    }
    if (v < 0.0f)
        v = 0.0f;
    else if (!(v <= 1.0f))
        v = 1.0f;
    sb->value = v;
    sb->pos = SlideBar_GetPos(sb);
    if (changed && VarString_HasExpr(&sb->action)) {
        VarString_EvalReal(&sb->action, sb);
        MenuManager_MarkBusy(sb->w.menu);
    }
    CALL_BASE(1, sb);
}
HOOK(0x41FA10, SlideBar_Run);

void SlideBar_Draw(SlideBar *sb)
{
    MenuButton *dec = sb->dec;
    MenuButton *inc = sb->inc;
    char horizontal = sb->horizontal;
    float alpha = FADE_ALPHA(sb->w.menu);
    int x = sb->w.x + 2;
    int y = sb->w.y + 2;
    uint16_t base;

    dec->w.x = x;
    dec->w.y = y;
    base = MenuWindow_GetType();
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(sb);
    if (horizontal) {
        x = dec->w.w + x + 1;
        inc->w.x = sb->length + x + 1;
        inc->w.y = y;
    } else {
        y = y + dec->w.h + 1;
        inc->w.x = x;
        inc->w.y = sb->length + y + 1;
    }
    dec = sb->dec;
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), dec->w.task.type))(dec);
    inc = sb->inc;
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), inc->w.task.type))(inc);
    if (sb->knob) {
        int ox, oy, sx, sy;

        if (horizontal) {
            ox = round_int((long double)(sb->length - Sprite_Width(sb->knob)) * sb->value);
            oy = 0;
        } else {
            ox = 0;
            oy = round_int((long double)(sb->length - Sprite_Height(sb->knob)) * sb->value);
        }
        sx = SCREEN_X(x);
        sy = SCREEN_Y(y);
        QVertex_DrawFrame(sb->knob, ox + sx, oy + sy, 128.0f, 128.0f, 128.0f, alpha, 1.0f, 0, 0x80000, 0);
    }
    MenuWindow_DrawEnd(sb, 0.5f, alpha, sb->w.focused);
}
HOOK(0x41FCE0, SlideBar_Draw);

void SlideBar_GetParams(void *task, void **out)
{
    *out = gSlideBarParams;
}
HOOK(0x41FF20, SlideBar_GetParams);

void SlideBar_Layout(SlideBar *sb)
{
    if (VarString_HasExpr(&sb->stepsExpr))
        sb->steps = round_int(VarString_EvalReal(&sb->stepsExpr, sb));
    if (VarString_HasExpr(&sb->valueExpr)) {
        if (!sb->steps)
            sb->value = VarString_EvalReal(&sb->valueExpr, sb);
        else
            SlideBar_SetPos(sb, round_int(VarString_EvalReal(&sb->valueExpr, sb)));
    }
    CALL_CHILDREN(&sb->w.task, MenuWindow_GetEvent914);
}
HOOK(0x41FF30, SlideBar_Layout);

void SlideBar_Forward918(SlideBar *sb)
{
    EVAL(&sb->forward, sb);
    CALL_CHILDREN(&sb->w.task, MenuWindow_GetEvent918);
}
HOOK(0x420020, SlideBar_Forward918);

void SlideBar_Activate(SlideBar *sb)
{
    char name[64];
    char buf[128];

    if (!sb->registered) {
        sb->registered = 1;
        Task_GetName(name, sb);
        crt_sprintf(buf, "%s.vValue", name);
        Script_RegisterReal(buf, &sb->value);
        crt_sprintf(buf, "%s.nValue", name);
        Script_RegisterInt2(buf, &sb->pos);
    }
}
HOOK(0x4200C0, SlideBar_Activate);

void SlideBar_Deactivate(SlideBar *sb)
{
    char name[64];
    char buf[128];

    if (sb->registered) {
        sb->registered = 0;
        Task_GetName(name, sb);
        crt_sprintf(buf, "%s.vValue", name);
        Script_RemoveSymbol(buf);
        crt_sprintf(buf, "%s.nValue", name);
        Script_RemoveSymbol(buf);
    }
}
HOOK(0x420140, SlideBar_Deactivate);

void SlideBar_Shutdown(void)
{
    QTaskList_Free(gSlideBarParams);
    TaskType_Free(gSlideBarType);
    gSlideBarType = 0x181;
}
HOOK(0x4201B0, SlideBar_Shutdown);

/* the vertical slider the list boxes use as a scroll bar */
SlideBar *SlideBar_NewVertical(void *parent, void *menu)
{
    SlideBar *sb = (SlideBar *)Task_New(parent, gSlideBarType, menu);

    str_copy(sb->name, "slidevert");
    sb->horizontal = 0;
    sb->length = 50;
    sb->steps = 3;
    return sb;
}
HOOK(0x4201E0, SlideBar_NewVertical);

/* w is not used, the bar is as wide as its sprites */
void SlideBar_SetRange(SlideBar *sb, int x, int y, int w, int h, int steps)
{
    sb->w.x = x;
    sb->w.y = y;
    sb->w.h = h;
    sb->length = h - sb->inc->w.h - sb->dec->w.h - 6;
    sb->steps = steps;
}
HOOK(0x420240, SlideBar_SetRange);

int SlideBar_GetPos(SlideBar *sb)
{
    if (sb->steps)
        return round_int((long double)(sb->steps - 1) * sb->value + 0.5);
    return 0;
}
HOOK(0x420290, SlideBar_GetPos);

void SlideBar_SetPos(SlideBar *sb, int pos)
{
    if (sb->steps)
        sb->value = ((long double)pos + 0.5f) / sb->steps;
}
HOOK(0x4202D0, SlideBar_SetPos);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B6BC) uint16_t gSlideBarType INIT(385);
PAD(igd, 53B6BC, 53B6BE, 0x2);
BSS(57BBE4) void *gSlideBarParams;
