/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "mission/mission.h"
#include "core/coretypes.h"

ORIG(0x416D50, char, Game_IsPaused, (void));
ORIG(0x418E40, void *, MenuManager_FindFont, (void *mm, const char *name));
ORIG(0x468DD0, void, ComputerLog_Clear, (void));
ORIG(0x468DE0, void, ComputerLog_Add, (const char *line));
ORIG(0x4B4AF0, void, Draw_RectAZ, (float x, float y, float w, float h, float r, float g, float b, float a, float z,
                                   int flags, int unk));
ORIG(0x4B4E00, void, Draw_Sprite, (void *s, float x, float y, float r, float g, float b, int frame, int unk4,
                                   int unk8));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x4B7050, int, Sprite_StringWidth, (void *font, const char *str));
ORIG(0x4E7200, void, Sound_PlayDefined, (const char *name, int unk, float *params));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

extern uint16_t gStatusMessageType;
extern void *gStatusMessageParams;
extern RangeF gStatusDurationRange;
extern uint16_t gStatusMessageScreenType;
extern void *gMessagesRes;

extern RenderMode gRenderModeCurrent;
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void *gQVertexPalette;

static const double kZeroD = 0.0;
static const double kLineStep = 42.0;
static const float kZero = 0.0f;
static const float kHalf = 0.5f;
static const float kTicksPerSecond = 30.0f;
static const float kWidthStep = 0x1.24924ap-3f; /* 1/7 */
static const float kWrapWidth = 229.0f;
static const float kPlainWrap = 0.96875f;
static const float kTextX = 0.015625f;
static const float kTextY = 0x1.555556p-6f; /* 1/48 */
static const float kPlainY = 0x1.baaaaap-1f; /* 0.8646 */
static const float kTextGap = 3.0f;
static const float kTextDown = 2.0f;
static const float kBoxX = 117.5f;
static const float kBoxHeight = 37.0f;
static const float kFadeScale = 0x1.333334p-1f; /* 0.6 */
static const float kFadeBase = 0x1.333334p-3f;  /* 0.15 */

/* --- StatusMessage ---------------------------------------------------------- */

void StatusMessage_Create(StatusMessage *m, void *arg);
void StatusMessage_Run(StatusMessage *m);
void StatusMessage_Delete(StatusMessage *m);
void StatusMessage_GetParams(void *task, void **out);
void StatusMessage_Changed(StatusMessage *m, void *arg);
void StatusMessage_Activate(StatusMessage *m);
void StatusMessage_Deactivate(StatusMessage *m);

void StatusMessage_Init(void)
{
    StatusMessage m;

    gStatusMessageType = TaskType_New("StatusMessage", sizeof(StatusMessage), EditorRigidObj_GetType(), 0, 0);
    TaskType_SetHandler(gStatusMessageType, 0, StatusMessage_Create);
    TaskType_SetHandler2(gStatusMessageType, 1, StatusMessage_Run);
    TaskType_SetHandler2(gStatusMessageType, 2, StatusMessage_Delete);
    TaskType_SetHandler(gStatusMessageType, Param_GetTableEvent(), StatusMessage_GetParams);
    TaskType_SetHandler(gStatusMessageType, Param_GetChangedEvent(), StatusMessage_Changed);
    TaskType_SetHandler2(gStatusMessageType, Level_GetEventDA8(), StatusMessage_Activate);
    TaskType_SetHandler2(gStatusMessageType, Level_GetEventDAC(), StatusMessage_Deactivate);
    gStatusDurationRange.max = 100.0f;
    gStatusDurationRange.min = 0.0f;
    gStatusMessageParams = ParamTable_New(9);
    ParamTable_Add(gStatusMessageParams, &m, &m.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, &m.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, &m.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, &m.text, Param_VarString(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, m.portrait, Param_String256(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, m.sound, Param_String16(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, &m.once, Param_Bool8(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, &m.plain, Param_Bool8(), 7, NULL);
    ParamTable_Add(gStatusMessageParams, &m, &m.duration, Param_Real32(), 7, &gStatusDurationRange);
}
HOOK(0x485330, StatusMessage_Init);

void StatusMessage_Create(StatusMessage *m, void *arg)
{
    m->text = NULL;
    m->portrait[0] = 0;
    m->sound[0] = 0;
    m->once = 0;
    m->shown = 0;
    m->plain = 0;
    VarString_Init(&m->cond);
    m->tickSent = -100000;
    m->isSent = 0;
    m->registered = 0;
    m->tickFinished = -1;
    m->ticksSinceFinished = 0;
    m->isFinished = 0;
    m->duration = 2.0f;
}
HOOK(0x485570, StatusMessage_Create);

/* sends the message to the screen when its condition holds, at most every 7 seconds */
void StatusMessage_Run(StatusMessage *m)
{
    if (m->isFinished)
        m->ticksSinceFinished++;
    if (m->once && m->isSent && !m->shown)
        return;
    if (TICKS <= m->tickSent + 210)
        return;
    if (m->shown && m->isSent) {
        Task_Delete(m);
        return;
    }
    if (!VarString_HasExpr(&m->cond) && !m->shown)
        return;
    if (!m->shown && x87_eq(VarString_EvalReal(&m->cond, m), kZeroD))
        return;
    StatusMessageScreen_AddMessage(StatusScreen_Get()->messages, m);
    m->isSent = 1;
    m->tickSent = TICKS;
}
HOOK(0x4855F0, StatusMessage_Run);

void StatusMessage_Delete(StatusMessage *m)
{
    if (Task_GetId(m) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), m->task.type);

        if (fn)
            ((void (*)(void *))fn)(m);
    }
    VarString_Free(&m->cond);
    if (m->text) {
        Mem_Free(m->text);
        m->text = NULL;
    }
}
HOOK(0x4856D0, StatusMessage_Delete);

void StatusMessage_GetParams(void *task, void **out)
{
    *out = gStatusMessageParams;
}
HOOK(0x485740, StatusMessage_GetParams);

void StatusMessage_Changed(StatusMessage *m, void *arg)
{
    VarString_Bind(&m->cond, m);
}
HOOK(0x485750, StatusMessage_Changed);

void StatusMessage_Activate(StatusMessage *m)
{
    char name[64];
    char buf[128];

    if (m->registered)
        return;
    m->registered = 1;
    Task_GetName(name, m);
    crt_sprintf(buf, "%s.isSendt", name);
    Script_RegisterBool8(buf, &m->isSent);
    crt_sprintf(buf, "%s.nTickSendt", name);
    Script_RegisterInt(buf, &m->tickSent);
    crt_sprintf(buf, "%s.isFinishedDisplay", name);
    Script_RegisterBool8(buf, &m->isFinished);
    crt_sprintf(buf, "%s.nFinishedDisplay", name);
    Script_RegisterInt(buf, &m->tickFinished);
    crt_sprintf(buf, "%s.nTicksSinceFinishedDisplay", name);
    Script_RegisterInt(buf, &m->ticksSinceFinished);
}
HOOK(0x485770, StatusMessage_Activate);

void StatusMessage_Deactivate(StatusMessage *m)
{
    char name[64];
    char buf[128];

    if (!m->registered)
        return;
    Task_GetName(name, m);
    crt_sprintf(buf, "%s.isSendt", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTickSendt", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isFinishedDisplay", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nFinishedDisplay", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTicksSinceFinishedDisplay", name);
    Script_RemoveSymbol(buf);
    m->registered = 0;
}
HOOK(0x485870, StatusMessage_Deactivate);

void StatusMessage_Close(void)
{
    QTaskList_Free(gStatusMessageParams);
    TaskType_Free(gStatusMessageType);
}
HOOK(0x485950, StatusMessage_Close);

/* a message made by code, shown once and then deleted */
StatusMessage *StatusMessageShow(void *owner, const char *text, const char *portrait, const char *sound)
{
    StatusMessage *m;

    Mem_PushMode(1);
    m = (StatusMessage *)Task_New(owner, gStatusMessageType, NULL);
    m->text = Mem_Alloc(strlen(text) + 1, 4);
    strcpy(m->text, text);
    strcpy(m->portrait, portrait);
    strcpy(m->sound, sound);
    m->once = 1;
    m->shown = 1;
    Mem_PopMode();
    StatusMessage_Changed(m, NULL);
    return m;
}
HOOK(0x485970, StatusMessageShow);

/* --- StatusMessageScreen ---------------------------------------------------- */

void StatusMessageScreen_Create(StatusMessageScreen *s);
void StatusMessageScreen_Run(StatusMessageScreen *s);
void StatusMessageScreen_Delete(StatusMessageScreen *s);
void StatusMessageScreen_Draw(StatusMessageScreen *s);
ScriptValue *StatusMessageScreen_ClearMessages(ScriptValue *out, void *data, void *args);
void StatusMessageScreen_UpdateLine(StatusMessageScreen *s, int i);
StatusMessage *StatusMessageScreen_GetMessage(StatusMessageScreen *s, int i);
void StatusMessageScreen_RemoveLine(StatusMessageScreen *s, int i);
void StatusMessageScreen_WrapText(StatusMessageScreen *s, int i, float maxWidth);
void *StatusMessageScreen_GetFont(StatusMessageScreen *s, int i);
void StatusMessageScreen_DrawLine(StatusMessageScreen *s, int i, float x, float y);
void StatusMessageScreen_DrawBox(StatusMessageScreen *s, int i, float x, float y, int flags);
void StatusMessageScreen_DrawPlainLine(StatusMessageScreen *s, int i, float x, float y);

void StatusMessageScreen_Init(void)
{
    gStatusMessageScreenType = TaskType_New("StatusMessageScreen", sizeof(StatusMessageScreen), Task_GetRootType(),
                                            0, 0);
    TaskType_SetHandler(gStatusMessageScreenType, 0, StatusMessageScreen_Create);
    TaskType_SetHandler2(gStatusMessageScreenType, 1, StatusMessageScreen_Run);
    TaskType_SetHandler2(gStatusMessageScreenType, 2, StatusMessageScreen_Delete);
    TaskType_SetHandler2(gStatusMessageScreenType, Screen_GetDrawEvent(), StatusMessageScreen_Draw);
    Script_RegisterFunction("StatusMessageScreen_ClearMessages", StatusMessageScreen_ClearMessages, NULL, 0);
}
HOOK(0x485A40, StatusMessageScreen_Init);

ScriptValue *StatusMessageScreen_ClearMessages(ScriptValue *out, void *data, void *args)
{
    ScriptValue v;

    StatusMessageScreen_Clear(StatusScreen_Get()->messages);
    v.i = 1;
    v.d = 1.0;
    v.s = gEmptyString;
    *out = v;
    return out;
}
HOOK(0x485AD0, StatusMessageScreen_ClearMessages);

void StatusMessageScreen_Create(StatusMessageScreen *s)
{
    int i;

    for (i = 0; i < STATUS_LINES; i++)
        s->lines[i].used = 0;
    ComputerLog_Clear();
}
HOOK(0x485B20, StatusMessageScreen_Create);

void StatusMessageScreen_Run(StatusMessageScreen *s)
{
    int i;

    if (Game_IsPaused())
        return;
    for (i = 0; i < STATUS_LINES; i++) {
        if (s->lines[i].used)
            StatusMessageScreen_UpdateLine(s, i);
    }
}
HOOK(0x485B40, StatusMessageScreen_Run);

/* the box grows for 9 ticks, then the text is typed out two ticks per letter */
void StatusMessageScreen_UpdateLine(StatusMessageScreen *s, int i)
{
    StatusMessage *m = StatusMessageScreen_GetMessage(s, i);
    StatusLine *l;

    if (!m) {
        StatusMessageScreen_RemoveLine(s, i);
        return;
    }
    l = &s->lines[i];
    if (!m->plain) {
        if (l->ticks < 9) {
            if (l->ticks < 2)
                l->height += kHalf;
            else
                l->width += kWidthStep;
        } else if (*l->cursor) {
            if ((l->ticks - 9) % 2 == 0) {
                l->cursor++;
                StatusMessageScreen_WrapText(s, i, kWrapWidth);
            }
        } else if (++l->shownTicks > (int)round_ll(m->duration * kTicksPerSecond)) {
            StatusMessageScreen_RemoveLine(s, i);
        }
    } else {
        if (l->shownTicks == 0)
            StatusMessageScreen_WrapText(s, i, Display_GetMode()->width * kPlainWrap);
        if (l->shownTicks > (int)round_ll(m->duration * kTicksPerSecond))
            StatusMessageScreen_RemoveLine(s, i);
        l->shownTicks++;
    }
    l->ticks++;
}
HOOK(0x485B80, StatusMessageScreen_UpdateLine);

StatusMessage *StatusMessageScreen_GetMessage(StatusMessageScreen *s, int i)
{
    return (StatusMessage *)*s->lines[i].msg;
}
HOOK(0x485D00, StatusMessageScreen_GetMessage);

void StatusMessageScreen_RemoveLine(StatusMessageScreen *s, int i)
{
    StatusMessage *m = StatusMessageScreen_GetMessage(s, i);
    StatusLine *l;
    int k;

    if (m) {
        if (gTickInfo)
            m->tickFinished = TICKS;
        else
            m->tickFinished = 0;
        m->isFinished = 1;
        m->ticksSinceFinished = 0;
    }
    l = &s->lines[i];
    TaskRef_Remove(l->msg);
    l->msg = NULL;
    l->used = 0;
    for (k = 0; k < 3; k++) {
        Text_Free(l->texts[k]);
        l->texts[k] = NULL;
    }
}
HOOK(0x485D20, StatusMessageScreen_RemoveLine);

/* splits the typed part of the text into lines at spaces, keeps the last three */
void StatusMessageScreen_WrapText(StatusMessageScreen *s, int i, float maxWidth)
{
    char rows[10][0x400];
    void *font = StatusMessageScreen_GetFont(s, i);
    StatusLine *l = &s->lines[i];
    char *p = l->text;
    char *start = p;
    char *space = NULL;
    char *out;
    char saved, c;
    int n = 0, k, first;

    saved = *l->cursor;
    *l->cursor = 0;
    for (k = 0; k < 3; k++)
        Text_SetString(l->texts[k], gEmptyString);
    c = *p;
    if (c) {
        out = rows[0];
        do {
            int w, brk;

            if (n >= 9)
                break;
            *p = 0;
            w = Sprite_StringWidth(font, start);
            brk = !((long double)w < maxWidth) || c == '\n';
            *p = c;
            if (brk) {
                if (!space)
                    space = p - 1;
                c = *space;
                *space = 0;
                strcpy(out, start);
                n++;
                out += 0x400;
                *space = c;
                start = space + 1;
            }
            if (*p == ' ')
                space = p;
            c = *++p;
        } while (c);
    }
    if (p != start) {
        strcpy(rows[n], start);
        n++;
    }
    first = n > 3 ? n - 3 : 0;
    for (k = first; k < n; k++)
        Text_SetString(l->texts[k - first], rows[k]);
    *l->cursor = saved;
}
HOOK(0x485DB0, StatusMessageScreen_WrapText);

void *StatusMessageScreen_GetFont(StatusMessageScreen *s, int i)
{
    StatusMessage *m = StatusMessageScreen_GetMessage(s, i);

    if (!m)
        return NULL;
    if (m->plain)
        return MenuManager_FindFont(*(void **)(gTickInfo + 0x120), "font4.fnt");
    return MenuManager_FindFont(*(void **)(gTickInfo + 0x120), "font1.fnt");
}
HOOK(0x485FB0, StatusMessageScreen_GetFont);

void StatusMessageScreen_Delete(StatusMessageScreen *s)
{
    StatusMessageScreen_Clear(s);
}
HOOK(0x486010, StatusMessageScreen_Delete);

void StatusMessageScreen_Draw(StatusMessageScreen *s)
{
    float x, y, h;
    int width, height, i;

    Game_Get();
    Game_Get();
    width = Display_GetMode()->width;
    height = Display_GetMode()->height;
    x = width * kTextX;
    h = height;
    y = height * kTextY;
    for (i = 0; i < STATUS_LINES; i++) {
        if (!s->lines[i].used)
            continue;
        if (StatusMessageScreen_GetMessage(s, i)->plain)
            StatusMessageScreen_DrawPlainLine(s, i, x, h * kPlainY);
        else
            StatusMessageScreen_DrawLine(s, i, x, y);
        y = y + kLineStep;
    }
}
HOOK(0x486020, StatusMessageScreen_Draw);

void StatusMessageScreen_DrawLine(StatusMessageScreen *s, int i, float x, float y)
{
    StatusLine *l;
    float x0 = x, y0 = y;
    int k;

    Display_GetMode();
    Display_GetMode();
    l = &s->lines[i];
    if (l->portrait && l->ticks > 9)
        Draw_Sprite(l->portrait, x, y, 128.0f, 128.0f, 128.0f, 0, 0, 0);
    if (l->portrait)
        x = Sprite_Width(l->portrait) + x;
    x = x + kTextGap;
    y = y + kTextDown;
    for (k = 0; k < 3; k++) {
        Text *t = l->texts[k];

        t->color[0] = 0.0f;
        t->color[1] = 120.0f;
        t->color[2] = 0.0f;
        t->x = x;
        t->y = y;
        Text_DrawAlt(t);
        y = Text_GetHeight(t) + y;
    }
    StatusMessageScreen_DrawBox(s, i, x0, y0, 0);
}
HOOK(0x4860F0, StatusMessageScreen_DrawLine);

/* the green box behind a line: a filled quad, then its outline */
void StatusMessageScreen_DrawBox(StatusMessageScreen *s, int i, float x, float y, int flags)
{
    StatusLine *l;
    QVertexStrip strip;
    QVertex v[5];
    long double fade, size, half, hh;
    float alpha, xa, ws, x0, x1, yc, y0, y1;
    int k;

    Display_GetMode();
    Display_GetMode();
    l = &s->lines[i];
    fade = (9 - l->ticks) / 9;
    if (!(fade > kZero))
        fade = kZero;
    alpha = fade * kFadeScale + kFadeBase;
    if (l->portrait)
        size = Sprite_Height(l->portrait);
    else
        size = kBoxHeight;
    xa = x + kBoxX;
    half = size * kHalf;
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    yc = half + y;
    ws = l->width * kBoxX;
    x0 = xa - ws;
    hh = half * l->height;
    y0 = yc - hh;
    x1 = ws + xa;
    y1 = hh + yc;
    Draw_RectAZ(x0, y0, x1 - x0, y1 - y0, 0.0f, 0.0f, 0.0f, 0.33f, 1.0f, 0x80000, flags);

    for (k = 0; k < 4; k++) {
        v[k].z = 1.0f;
        v[k].u = 0.0f;
        v[k].v = 0.0f;
        v[k].a = alpha;
        v[k].r = 0.0f;
        v[k].g = k == 3 ? 1.0f : 0.5f;
        v[k].b = 0.0f;
    }
    v[0].x = x0;
    v[0].y = y0;
    v[1].x = x1;
    v[1].y = y0;
    v[2].x = x0;
    v[2].y = y1;
    v[3].x = x1;
    v[3].y = y1;
    strip.lines = 0;
    strip.flags = 0x80000;
    strip.queue = flags;
    strip.count = 4;
    strip.verts = v;
    gRenderModeCurrent.methods[gQVertexStrip](&strip);

    for (k = 0; k < 4; k++) {
        v[k].z = 1.0f;
        v[k].u = 0.0f;
        v[k].v = 0.0f;
        v[k].a = 0.5f;
        v[k].r = 0.0f;
        v[k].g = 0.9f;
        v[k].b = 0.0f;
    }
    v[0].x = x0;
    v[0].y = y0;
    v[1].x = x1;
    v[1].y = y0;
    v[2].x = x1;
    v[2].y = y1;
    v[3].x = x0;
    v[3].y = y1;
    v[4] = v[0];
    strip.lines = 1;
    strip.queue = flags;
    strip.count = 5;
    strip.verts = v;
    gRenderModeCurrent.methods[gQVertexStrip](&strip);
}
HOOK(0x486210, StatusMessageScreen_DrawBox);

void StatusMessageScreen_DrawPlainLine(StatusMessageScreen *s, int i, float x, float y)
{
    int k;

    for (k = 0; k < 3; k++) {
        Text *t = s->lines[i].texts[k];

        t->x = x;
        t->y = y;
        Text_DrawAlt(t);
        y = Text_GetHeight(t) + y;
    }
}
HOOK(0x4865B0, StatusMessageScreen_DrawPlainLine);

void StatusMessageScreen_Close(void)
{
    TaskType_Free(gStatusMessageScreenType);
}
HOOK(0x486610, StatusMessageScreen_Close);

StatusMessageScreen *StatusMessageScreen_New(void *parent)
{
    Task *t;

    Mem_PushMode(1);
    t = Task_New(parent, gStatusMessageScreenType, NULL);
    Mem_PopMode();
    return (StatusMessageScreen *)t;
}
HOOK(0x486620, StatusMessageScreen_New);

/* takes the first free line; the text is a resource name of LANGUAGE:messages.res */
void StatusMessageScreen_AddMessage(StatusMessageScreen *s, StatusMessage *m)
{
    StatusLine *l;
    void *font;
    int i, k;

    for (i = 0; i < STATUS_LINES; i++) {
        if (!s->lines[i].used)
            break;
    }
    if (i == STATUS_LINES)
        return;
    l = &s->lines[i];
    l->used = 1;
    l->msg = TaskRef_Add(m);
    l->ticks = 0;
    l->shownTicks = 0;
    l->width = 0.0f;
    l->height = 0.0f;
    l->portrait = NULL;
    if (m->portrait[0]) {
        const char **names = StatusScreen_GetPortraitNames();
        StatusScreen *ss = StatusScreen_Get();

        for (k = 0; k < 4; k++) {
            if (strcmp(m->portrait, names[k]) == 0) {
                l->portrait = &ss->portraits[k];
                break;
            }
        }
    }
    Mem_PushMode(1);
    font = StatusMessageScreen_GetFont(s, i);
    for (k = 0; k < 3; k++)
        l->texts[k] = Text_New(font, 0.0f, 0.0f, gEmptyString);
    Mem_PopMode();
    if (m->sound[0]) {
        float params[3] = {1.0f, 1.0f, 0.0f};

        Sound_PlayDefined(m->sound, 1, params);
    }
    if (strlen(m->text) < 0x80 && Resource_Find(m->text)) {
        char *text = Resource_Load(m->text, NULL);

        strcpy(l->text, text);
        if (!m->plain)
            ComputerLog_Add(text);
    } else {
        crt_sprintf(l->text, "RM: %s", m->text);
    }
    if (m->plain)
        l->cursor = l->text + strlen(l->text);
    else
        l->cursor = l->text;
}
HOOK(0x486650, StatusMessageScreen_AddMessage);

void StatusMessages_Load(void)
{
    gMessagesRes = Resource_Load("LANGUAGE:messages.res", NULL);
}
HOOK(0x4868A0, StatusMessages_Load);

void StatusMessages_Free(void)
{
    Resource_Flush(gMessagesRes);
}
HOOK(0x4868C0, StatusMessages_Free);

void StatusMessageScreen_Clear(StatusMessageScreen *s)
{
    int i;

    for (i = 0; i < STATUS_LINES; i++) {
        if (s->lines[i].used)
            StatusMessageScreen_RemoveLine(s, i);
    }
}
HOOK(0x4868D0, StatusMessageScreen_Clear);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(540A8C) uint16_t gStatusMessageType INIT(385);
PAD(igd, 540A8C, 540A8E, 0x2);
DATA(540B08) uint16_t gStatusMessageScreenType INIT(385);
PAD(igd, 540B08, 540B0A, 0x2);
BSS(5C8A30) RangeF gStatusDurationRange;
BSS(5C8A30) void *gStatusMessageParams;
PAD(igb, 5C8A30, 5C8A3C, 0x4);
BSS(5C8A30) void *gMessagesRes;
